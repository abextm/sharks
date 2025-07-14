// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "selectionwindow.hxx"

#include <QActionGroup>
#include <QBitmap>
#include <QButtonGroup>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMetaObject>
#include <QMetaProperty>
#include <QOpenGLWidget>
#include <QPainterPath>
#include <QProcess>
#include <QResizeEvent>
#include <QScreen>
#include <QShortcut>
#include <QStandardPaths>

#include "config.hxx"
#include "platform.hxx"

// #define NO_FULLSCREEN

SelectionWindow::SelectionWindow(QWidget *parent)
	: QWidget(parent),
		picking(false),
		pickedLock(false),
		recursingGeometry(-1),
		openWindows(),
		selectionStart(),
		selectionEnd(),
		selection(),
		shot(),
		cursor() {
	this->cursorPosition = QCursor::pos();
	QScreen *screen = QGuiApplication::screenAt(this->cursorPosition);
	if (screen == nullptr) {
		screen = QGuiApplication::primaryScreen();
	}
	this->desktopGeometry = screen->virtualGeometry();

	this->shot = platform->getScreenshot(this->desktopGeometry);

	auto cursorImage = platform->getCursorImage();
	this->cursor = QPixmap::fromImage(cursorImage);
	this->cursorPosition -= cursorImage.offset();

	this->openWindows = platform->getOpenWindows();

#ifndef NO_FULLSCREEN
	if (platform->isWayland()) {
		this->setWindowFlags(Qt::FramelessWindowHint);
	} else {
		this->setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
	}
#else
	this->setWindowFlags(Qt::Window);
#endif
	this->setAttribute(Qt::WA_DeleteOnClose);

	this->scene = new QGraphicsScene(this);

	this->selectionView = new QGraphicsView(this);
	this->selectionView->setViewport(new QOpenGLWidget(this->selectionView));
	this->selectionView->setScene(this->scene);
	this->selectionView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	this->selectionView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	this->selectionView->setStyleSheet("border: 0px;");

	this->shotItem = new ShotItem(this);
	this->scene->addItem(this->shotItem);
	this->shotItem->setOffset(0, 0);
	this->cursorItem = this->scene->addPixmap(this->cursor);
	this->cursorItem->setOffset(this->cursorPosition);
	this->selectionItem = this->scene->addPath(QPainterPath(), QPen(), QColor(0, 0, 0, 175));
	this->selectionItem->setPos(0, 0);

	this->shotToolbar = new QToolBar(this);
	this->shotToolbar->setAutoFillBackground(true);

	this->pickToolbar = new QToolBar(this);
	this->pickToolbar->setAutoFillBackground(true);
	this->pickToolbar->setHidden(true);

	{
		QRect geo = screen->geometry();
		QPoint pt(
			qBound(geo.left(), geo.center().x() - (shotToolbar->width() / 2), geo.right()),
			geo.top() + 40);

		this->shotToolbar->move(pt);
		this->pickToolbar->move(pt);
	}

	this->shotToolbar->addWidget(new DragHandle(this->shotToolbar));
	this->pickToolbar->addWidget(new DragHandle(this->pickToolbar));

	this->togglePicker = new QAction(QIcon::fromTheme("find-location-symbolic"), "Color Picker", this);
	this->togglePicker->setCheckable(true);
	connect(this->togglePicker, &QAction::toggled, this, &SelectionWindow::setPicking);
	this->shotToolbar->addAction(this->togglePicker);
	this->pickToolbar->addAction(this->togglePicker);

	this->showCursor = new QAction(QIcon::fromTheme("input-mouse"), "Show cursor", this);
	this->showCursor->setCheckable(true);
	connect(this->showCursor, &QAction::toggled, this, [this](bool enabled) {
		this->cursorItem->setVisible(enabled);
	});
	this->showCursor->setChecked(true);
	this->shotToolbar->addAction(this->showCursor);

	this->shotToolbar->addSeparator();

	auto toolGroup = new QActionGroup(this->shotToolbar);

	this->selectArea = new QAction(QIcon::fromTheme("view-restore"), "Select area", this);
	toolGroup->addAction(this->selectArea);
	this->selectArea->setCheckable(true);
	this->selectArea->setChecked(true);

	this->penTool = new QAction(QIcon::fromTheme("xapp-edit-symbolic"), "Draw", this);
	toolGroup->addAction(this->penTool);
	this->penTool->setCheckable(true);
	this->penTool->setShortcuts(Config::parseHotkeys(config->root["pen"]["key"]));
	this->shotToolbar->addActions(toolGroup->actions());

	this->shotToolbar->addSeparator();

	auto actions = Config::get<toml::table>(&config->root, "action", "action should be a table of tables");
	if (actions) {
		for (auto &entry : *actions) {
			if (entry.second.is_table()) {
				auto tab = entry.second.as_table();
				if (!Config::get<bool>(tab, "enabled", "enabled should be a boolean").value_or(true)) {
					continue;
				}

				auto name = Config::get<std::string>(tab, "name", "name must be a string", "name must be set");
				auto action = Config::get<std::string>(tab, "action", "action must be a string", "action must be set");
				if (!name || !action) {
					continue;
				}

				auto *qAction = new QAction(QString::fromStdString(*name), this);

				auto icon = Config::get<std::string>(tab, "icon", "icon must be a string");
				if (icon) {
					qAction->setIcon(QIcon::fromTheme(QString::fromStdString(*icon)));
				}

				qAction->setShortcuts(Config::parseHotkeys((*tab)["key"]));

				if (*action == "copy") {
					connect(qAction, &QAction::triggered, this, [this]() {
						this->close();
						auto pixmap = this->pixmap();
						pixmap.save(this->savePath());
						auto *clipboard = QGuiApplication::clipboard();
						clipboard->setPixmap(pixmap);
					});
				} else if (*action == "save-default") {
					connect(qAction, &QAction::triggered, this, [this]() {
						this->close();
						this->saveTo(this->savePath());
					});
				} else if (*action == "save-as") {
					connect(qAction, &QAction::triggered, this, [this]() {
						auto filename = QFileDialog::getSaveFileName(this, "Save screenshot", this->savePath(), "*.png");
						if (!filename.isEmpty()) {
							this->close();
							this->saveTo(filename);
						}
					});
				} else if (*action == "exec") {
					QList<QString> args;
					auto tomlArgs = Config::get<toml::array>(tab, "args", "args must be an array of strings for exec");
					if (!tomlArgs) {
						delete qAction;
						continue;
					}

					for (auto &arg : *tomlArgs) {
						auto *str = arg.as_string();
						if (!str) {
							Config::complain(&arg, "args must be an array of string");
							continue;
						}
						args.emplaceBack(QString::fromStdString(**str));
					}

					if (args.size() < 1) {
						Config::complain((*tab)["args"], "args must have at least one entry");
						delete qAction;
						continue;
					}
					QString process = args.takeFirst();
					args.append("");
					connect(qAction, &QAction::triggered, this, [this, process, args]() mutable {
						this->close();
						QString path = this->savePath();
						this->saveTo(path);
						args[args.size() - 1] = path;
						auto *proc = new QProcess();
						proc->setProcessChannelMode(QProcess::ForwardedChannels);
						proc->start(process, args);
						connect(proc, &QProcess::finished, proc, [proc](int, QProcess::ExitStatus) {
							proc->deleteLater();
						});
					});
				} else {
					Config::complain((*tab)["action"], QString("action must be one of copy, save-default, save-as, or exec"));
					delete qAction;
					continue;
				}

				this->shotToolbar->addAction(qAction);
			} else {
				Config::complain(&entry.second, "action should be a table of tables");
			}
		}
	}

	auto *pickSwatch = new ColorSwatch(this->pickToolbar);
	this->pickToolbar->addWidget(pickSwatch);
	connect(this, &SelectionWindow::pickColorChanged, pickSwatch, &ColorSwatch::setColor);

	auto *formatBtnHolder = new QWidget(this->pickToolbar);
	auto *formatBtnLayout = new QGridLayout(formatBtnHolder);
	formatBtnLayout->setSpacing(0);
	formatBtnLayout->setContentsMargins(0, 0, 0, 0);
	formatBtnHolder->setLayout(formatBtnLayout);
	auto *formatBtnGroup = new QButtonGroup(formatBtnHolder);
	formatBtnGroup->setExclusive(true);
	ColorCopyButton::Format formats[] = {
		ColorCopyButton::Format::CSS_HEX,
		ColorCopyButton::Format::CSS_RGB,
	};
	for (size_t i = 0; i < sizeof(formats) / sizeof(*formats); i++) {
		auto *btn = new ColorCopyButton(this->pickToolbar);
		btn->setCheckable(true);
		if (i == 0) {
			btn->setChecked(true);
		}
		formatBtnGroup->addButton(btn);
		btn->setFormat(formats[i]);
		connect(this, &SelectionWindow::pickColorChanged, btn, &ColorCopyButton::setColor);
		connect(this, &SelectionWindow::pickColorSelected, btn, &ColorCopyButton::copyStringIfActive);
		formatBtnLayout->addWidget(btn, i % 2, i / 2);
	}
	this->pickToolbar->addWidget(formatBtnHolder);

	this->shotToolbar->addSeparator();
	this->pickToolbar->addSeparator();

	auto *close = new QAction(QIcon::fromTheme("exit"), "Close", this);
	connect(close, &QAction::triggered, this, &QWidget::close);
	close->setShortcut(QKeySequence(Qt::Key_Escape));
	this->shotToolbar->addAction(close);
	this->pickToolbar->addAction(close);

	this->pickTooltip = new ZoomTooltip(this);

	this->undoStack = new QUndoStack(this);

	{
		auto undo = this->undoStack->createUndoAction(this);
		undo->setShortcut(QKeySequence("Ctrl+Z"));
		this->addAction(undo);

		auto redo = this->undoStack->createRedoAction(this);
		redo->setShortcut(QKeySequence("Ctrl+Shift+Z"));
		this->addAction(redo);
	}

	this->selectionMoved();

	this->recursingGeometry = 0;
#ifndef NO_FULLSCREEN
	this->setGeometry(desktopGeometry);
#endif
}

void SelectionWindow::setPicking(bool picking) {
	if (this->picking == picking) {
		return;
	}

	this->picking = picking;
	this->togglePicker->setChecked(picking);
	this->pickToolbar->setVisible(picking);
	this->shotToolbar->setVisible(!picking);
	this->pickTooltip->setVisible(picking);
	if (picking) {
		this->pickToolbar->move(this->shotToolbar->pos());
		this->cursorItem->setVisible(false);
	} else {
		this->shotToolbar->move(this->pickToolbar->pos());
		this->cursorItem->setVisible(this->showCursor->isChecked());
	}
}

void SelectionWindow::setVisible(bool visible) {
	QWidget::setVisible(visible);

	if (visible) {
		platform->waylandFullscreen();
	}
}

bool SelectionWindow::event(QEvent *event) {
	if (event->type() == QEvent::LayoutRequest) {
		this->pickToolbar->resize(this->pickToolbar->sizeHint());
		this->shotToolbar->resize(this->shotToolbar->sizeHint());
		return true;
	}

	return QWidget::event(event);
}

QPixmap SelectionWindow::pixmap() {
	QRect selection = this->selection;
	if (selection.isEmpty()) {
		selection = this->desktopGeometry;
	}

	this->selectionItem->setVisible(false);

	QPixmap pixmap(selection.size());
	QPainter painter(&pixmap);
	this->scene->render(&painter, pixmap.rect(), selection);

	return pixmap;
}
void SelectionWindow::saveTo(QString path) {
	this->pixmap().save(path);
}
QString SelectionWindow::savePath() {
	QDateTime now = QDateTime::currentDateTime();
	QString month = now.toString("yyyy-MM");
	QDir monthDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/screenshots/" + month + "/");
	monthDir.mkpath(".");
	QString path = monthDir.filePath(now.toString("yyyy-MM-dd_hh-mm-ss") + ".png");
	return path;
}

void SelectionWindow::moveEvent(QMoveEvent *event) {
	QWidget::moveEvent(event);
	this->geometryChanged(event);
}
void SelectionWindow::resizeEvent(QResizeEvent *event) {
	QWidget::resizeEvent(event);
	this->geometryChanged(event);
}
void SelectionWindow::geometryChanged(QEvent *) {
#ifndef NO_FULLSCREEN
	if (this->recursingGeometry >= 0) {
		if (this->geometry() != this->desktopGeometry) {
			if (this->recursingGeometry++ < 4) {
				this->setGeometry(this->desktopGeometry);
				return;
			} else {
				qInfo() << "adjustment failed; geometry " << this->geometry() << "desktop=" << this->desktopGeometry;
			}
		}

		this->recursingGeometry = 0;
	}
#endif
	this->selectionView->setGeometry(this->desktopGeometry);
	this->selectionView->resetTransform();
}

void SelectionWindow::selectionMoved() {
	QPainterPath path;
	path.addRect(this->desktopGeometry);
	if (!this->selectionStart.isNull() && !this->selectionEnd.isNull()) {
		QRect sel(this->selectionStart, this->selectionEnd);
		this->selection = sel.normalized();

		QPainterPath selection;
		selection.addRect(this->selection);

		path -= selection;
	} else {
		this->selection = QRect();
	}

	this->selectionItem->setPath(path);
}

void SelectionWindow::pickMoved() {
	int radius = 7;
	int dia = radius * 2 + 1;
	QPixmap subPx = this->shot.copy(QRect(this->pickPos - QPoint(radius, radius), QSize(dia, dia)));
	QImage subIm = subPx.toImage();

	QColor col = subIm.pixel(radius, radius);
	emit this->pickColorChanged(col);

	this->pickTooltip->move(this->pickPos + QPoint(5, 5));
	this->pickTooltip->setImage(subPx);
}
void SelectionWindow::pickSelected() {
	QPixmap onePx = this->shot.copy(QRect(this->pickPos, QSize(1, 1)));
	QImage oneIm = onePx.toImage();
	QColor col = oneIm.pixel(0, 0);
	emit this->pickColorSelected(col);
	this->close();
}

SelectionWindow::~SelectionWindow() {
}

DragHandle::DragHandle(QWidget *moveParent) : QLabel(" ⠿ ", moveParent), moveParent(moveParent) {
}
DragHandle::~DragHandle() {
}
void DragHandle::mousePressEvent(QMouseEvent *ev) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	this->dragStart = this->moveParent->mapFromGlobal(ev->globalPos());
#else
	this->dragStart = this->moveParent->mapFromGlobal(ev->globalPosition()).toPoint();
#endif
}
void DragHandle::mouseMoveEvent(QMouseEvent *ev) {
	if (ev->buttons().testFlag(Qt::LeftButton)) {
		QWidget *superParent = qobject_cast<QWidget *>(this->moveParent->parent());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		QPoint newPos = superParent->mapFromGlobal(ev->globalPos());
#else
		QPoint newPos = superParent->mapFromGlobal(ev->globalPosition()).toPoint();
#endif
		this->moveParent->move(newPos - this->dragStart);
	}
}

ColorSwatch::ColorSwatch(QWidget *parent) : QWidget(parent), color() {
	QSizePolicy sp(QSizePolicy::Fixed, QSizePolicy::Expanding);
	this->setSizePolicy(sp);
}
ColorSwatch::~ColorSwatch() {
}
void ColorSwatch::paintEvent(QPaintEvent *ev) {
	QWidget::paintEvent(ev);

	QPainter p(this);
	p.setBrush(this->color);
	p.drawRect(this->rect());
}
void ColorSwatch::resizeEvent(QResizeEvent *ev) {
	QWidget::resizeEvent(ev);
	if (ev->oldSize().height() != ev->size().height()) {
		this->updateGeometry();
	}
}
QSize ColorSwatch::sizeHint() const {
	int v = std::max(this->height(), 8);
	return QSize(v, 0);
}
void ColorSwatch::setColor(QColor color) {
	this->color = color;
	this->update();
}

ColorCopyButton::ColorCopyButton(QWidget *parent) : QPushButton(parent), format(ColorCopyButton::Format::CSS_HEX), color(Qt::white) {
	connect(this, &QPushButton::clicked, this, &ColorCopyButton::copyString);
	this->setText(this->formatString());
}
ColorCopyButton::~ColorCopyButton() {
}
QString ColorCopyButton::formatString() const {
	if (!this->color.isValid()) {
		return QLatin1String("");
	}
	switch (this->format) {
		default:
		case CSS_HEX:
			return QStringLiteral("%1").arg(this->color.rgb() & 0xFF'FF'FF, 6, 16, QLatin1Char('0'));
		case CSS_RGB:
			return QStringLiteral("%1, %2, %3")
				.arg(this->color.red())
				.arg(this->color.green())
				.arg(this->color.blue());
	}
}
void ColorCopyButton::setFormat(Format format) {
	this->format = format;
	this->setText(this->formatString());
}
void ColorCopyButton::setColor(QColor color) {
	this->color = color;
	this->setText(this->formatString());
}
void ColorCopyButton::copyString() const {
	QGuiApplication::clipboard()->setText(this->formatString());
}
void ColorCopyButton::copyStringIfActive() const {
	if (this->isChecked()) {
		this->copyString();
	}
}

ZoomTooltip::ZoomTooltip(QWidget *parent) : QWidget(parent), img() {
	this->setAttribute(Qt::WA_TransparentForMouseEvents);
	this->setScale(8);
}
ZoomTooltip::~ZoomTooltip() {
}
void ZoomTooltip::paintEvent(QPaintEvent *ev) {
	QWidget::paintEvent(ev);

	QPainter p(this);

	QSize imSize = this->img.size();
	QRect imBounds(QPoint(1, 1), imSize * this->scale);
	p.drawPixmap(imBounds, this->img, this->img.rect());

	p.setPen(Qt::black);
	int w = imSize.width() * this->scale + 2;
	for (int i = 0; i <= imSize.height(); i++) {
		int y = i * this->scale;
		p.drawLine(0, y, w, y);
		if (i == imSize.height() / 2) {
			p.drawLine(0, y - 1, w, y - 1);
		} else if (i - 1 == imSize.height() / 2) {
			p.drawLine(0, y + 1, w, y + 1);
		}
	}
	int h = imSize.height() * this->scale + 2;
	for (int i = 0; i <= imSize.width(); i++) {
		int x = i * this->scale;
		p.drawLine(x, 0, x, h);
		if (i == imSize.width() / 2) {
			p.drawLine(x - 1, 0, x - 1, h);
		} else if (i - 1 == imSize.width() / 2) {
			p.drawLine(x + 1, 0, x + 1, h);
		}
	}
}
void ZoomTooltip::setImage(QPixmap image) {
	this->img = image;
	this->doResize();
	this->update();
}
void ZoomTooltip::setScale(int scale) {
	this->scale = scale;
	this->doResize();
}
void ZoomTooltip::doResize() {
	if (this->img.isNull()) {
		return;
	}
	resize(this->img.size() * this->scale + QSize(1, 1));
}

ShotItem::ShotItem(SelectionWindow *parent) : QGraphicsPixmapItem(parent->shot), win(parent) {
	this->setAcceptHoverEvents(true);
	this->setCursor(QCursor(Qt::CursorShape::CrossCursor));
}
ShotItem::~ShotItem() {}
void ShotItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	if (win->picking) {
		if (event->buttons().testFlag(Qt::MiddleButton)) {
			win->pickedLock = !win->pickedLock;
			win->pickPos = event->scenePos().toPoint();
			win->pickMoved();
		} else if (event->buttons().testFlag(Qt::LeftButton)) {
			win->pickPos = event->scenePos().toPoint();
			win->pickSelected();
		}
	} else if (win->selectArea->isChecked()) {
		win->selectionStart = event->scenePos().toPoint();
		win->selectionEnd = QPoint();
		win->selectionMoved();
	} else if (win->penTool->isChecked()) {
		auto penTab = Config::get<toml::table>(&config->root, "pen", "missing pen table");
		if (penTab) {
			QColor color(Config::get<int64_t>(&*penTab, "color", "color should be an integer").value_or(0xFF0000));
			qreal width = Config::get<qreal>(&*penTab, "width", "width should be a number").value_or(4);
			win->activeDrawing = new PenDrawing(QPen(QBrush(color), width), event->scenePos().toPoint());
			win->scene->addItem(win->activeDrawing);
		}
	}
}
void ShotItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	if (win->picking) {
		if (event->buttons().testFlag(Qt::MiddleButton)) {
			win->pickPos = event->scenePos().toPoint();
			win->pickMoved();
		}
	} else if (win->selectArea->isChecked()) {
		if (event->buttons().testFlag(Qt::LeftButton)) {
			win->selectionEnd = event->scenePos().toPoint();
			win->selectionMoved();
		}
	} else if (win->penTool->isChecked()) {
		if (win->activeDrawing) {
			win->activeDrawing->addPoint(event->scenePos().toPoint());
		}
	}
}
void ShotItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
	if (!win->picking && win->penTool->isChecked()) {
		if (win->activeDrawing) {
			win->undoStack->push(new DrawingUndoItem(win, win->activeDrawing));
			win->activeDrawing = nullptr;
		}
	}
}
void ShotItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
	if (win->picking || !win->selectArea->isChecked()) {
		return;
	}
	QPoint pos = event->pos().toPoint();
	auto *win = this->win;
	for (const OpenWindow &w : std::as_const(win->openWindows)) {
		if (w.geometry.contains(pos)) {
			win->selectionStart = w.geometry.topLeft();
			win->selectionEnd = w.geometry.bottomRight();
			win->selectionMoved();
			return;
		}
	}
}
void ShotItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
	if (win->picking) {
		if (!win->pickedLock) {
			win->pickPos = event->scenePos().toPoint();
			win->pickMoved();
		}
	}
}

PenDrawing::PenDrawing(QPen pen, QPoint start)
	: rawPath(),
		bounds(start.x() - pen.widthF(), start.y() - pen.widthF(), pen.widthF() * 2, pen.widthF() * 2),
		strokedPath() {
	setPen(pen);
	this->rawPath.moveTo(start.x() - 1, start.y() - 1);
	this->rawPath.lineTo(start);
}
PenDrawing::~PenDrawing() {
}
QRectF PenDrawing::boundingRect() const {
	return this->bounds;
}
void PenDrawing::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
	this->updateStrokedPath();
	painter->fillPath(this->strokedPath, this->pen().brush());
}
void PenDrawing::addPoint(QPoint pt) {
	this->prepareGeometryChange();
	this->strokedPath.clear();
	this->rawPath.lineTo(pt);
	QRectF ptBounds(pt.x() - pen().widthF(), pt.y() - pen().widthF(), pen().widthF() * 2, pen().widthF() * 2);
	this->bounds = this->bounds.united(ptBounds);
}
void PenDrawing::updateStrokedPath() {
	if (this->strokedPath.isEmpty()) {
		QPainterPathStroker qpps(this->pen());
		qpps.setWidth(4);
		qpps.setCapStyle(Qt::PenCapStyle::RoundCap);
		qpps.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
		this->strokedPath = qpps.createStroke(this->rawPath);
	}
}

DrawingUndoItem::DrawingUndoItem(SelectionWindow *parent, QGraphicsItem *item)
	: parent(parent),
		item(item),
		owned(false) {
}
DrawingUndoItem::~DrawingUndoItem() {
	if (owned) {
		delete this->item;
	}
}
void DrawingUndoItem::undo() {
	owned = true;
	this->parent->scene->removeItem(this->item);
}
void DrawingUndoItem::redo() {
	// this is spuriously called by QUndoStack when added
	if (owned) {
		owned = false;
		this->parent->scene->addItem(this->item);
	}
}
