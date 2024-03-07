// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "selectionwindow.hxx"

#include <QBitmap>
#include <QButtonGroup>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMoveEvent>
#include <QOpenGLWidget>
#include <QPainterPath>
#include <QProcess>
#include <QResizeEvent>
#include <QScreen>
#include <QShortcut>
#include <QStandardPaths>
#ifdef Q_OS_LINUX
#include "wayland.hxx"
QPixmap getX11Screenshot(QRect desktopGeometry);
QImage getX11Cursor();
QList<OpenWindow> getX11Windows();
#endif

//#define NO_FULLSCREEN

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

#ifdef Q_OS_LINUX
	if (isWayland()) {
		this->shot = getWaylandScreenshot(this->desktopGeometry);
	} else {
		this->shot = getX11Screenshot(this->desktopGeometry);
	}
#endif
	if (this->shot.isNull()) {
		// you can only grab relative to the screen, but it works to grab the whole virtual desktop
		QRect screenGeometry = screen->geometry();
		this->shot = screen->grabWindow(0, -screenGeometry.x(), -screenGeometry.y(), this->desktopGeometry.width(), this->desktopGeometry.height());
	}

	QPixmap p;
#ifdef Q_OS_LINUX
	if (!isWayland()) {
		auto i = getX11Cursor();
		if (!i.isNull()) {
			p = QPixmap::fromImage(i);
			this->cursorPosition -= i.offset();
		}

		this->openWindows = getX11Windows();
	}
#endif
	if (p.isNull()) {
		QCursor c;
		p = c.pixmap();
		if (p.isNull()) {
			auto b = c.bitmap(Qt::ReturnByValue);
			if (b.isNull()) {
				qInfo() << "unable to get cursor image";
			} else {
				p = QPixmap(b);
			}
		}
		this->cursorPosition -= c.hotSpot();
	}
	this->cursor = p;
#ifndef NO_FULLSCREEN
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
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

	auto *save = new QAction(QIcon::fromTheme("document-save"), "Save", this);
	connect(save, &QAction::triggered, this, [this]() {
		this->close();
		this->saveTo(this->savePath());
	});
	this->shotToolbar->addAction(save);

	auto *saveas = new QAction(QIcon::fromTheme("document-save-as"), "Save-as", this);
	saveas->setShortcut(QKeySequence("Ctrl+S"));
	connect(saveas, &QAction::triggered, this, [this]() {
		auto filename = QFileDialog::getSaveFileName(this, "Save screenshot", this->savePath(), "*.png");
		if (!filename.isEmpty()) {
			this->setVisible(false);
			this->saveTo(filename);
			this->close();
		}
	});
	this->shotToolbar->addAction(saveas);

	auto *upload = new QAction(QIcon::fromTheme("document-send"), "Upload", this);
	upload->setShortcut(QKeySequence(Qt::Key_Enter));
	connect(upload, &QAction::triggered, this, [this]() {
		this->setVisible(false);
		QString path = this->savePath();
		this->saveTo(path);
		qInfo() << QProcess::execute("ymup", QStringList({"-if", path}));
		this->close();
	});
	this->shotToolbar->addAction(upload);

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

#ifdef Q_OS_LINUX
	if (visible && isWayland()) {
		swayFullscreen();
	}
#endif
}

bool SelectionWindow::event(QEvent *event) {
	if (event->type() == QEvent::LayoutRequest) {
		this->pickToolbar->resize(this->pickToolbar->sizeHint());
		this->shotToolbar->resize(this->shotToolbar->sizeHint());
		return true;
	}

	return QWidget::event(event);
}

void SelectionWindow::saveTo(QString path) {
	QRect selection = this->selection;
	if (selection.isEmpty()) {
		selection = this->desktopGeometry;
	}

	this->selectionItem->setVisible(false);

	QPixmap pixmap(selection.size());
	QPainter painter(&pixmap);
	this->scene->render(&painter, pixmap.rect(), selection);

	pixmap.save(path);
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
	this->repaint();
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
	this->repaint();
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
			win->pickPos = event->screenPos();
			win->pickMoved();
		} else if (event->buttons().testFlag(Qt::LeftButton)) {
			win->pickPos = event->screenPos();
			win->pickSelected();
		}
	} else {
		win->selectionStart = event->screenPos();
		win->selectionEnd = QPoint();
		win->selectionMoved();
	}
}
void ShotItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	if (win->picking) {
		if (event->buttons().testFlag(Qt::MiddleButton)) {
			win->pickPos = event->screenPos();
			win->pickMoved();
		}
	} else {
		if (event->buttons().testFlag(Qt::LeftButton)) {
			win->selectionEnd = event->screenPos();
			win->selectionMoved();
		}
	}
}
void ShotItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
	if (win->picking) {
		return;
	}
	QPoint pos = event->pos().toPoint();
	auto *win = this->win;
	for (const OpenWindow &w : qAsConst(win->openWindows)) {
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
			win->pickPos = event->screenPos();
			win->pickMoved();
		}
	}
}

QDebug operator<<(QDebug debug, const OpenWindow &win) {
	QDebugStateSaver saver(debug);
	debug.nospace() << "Win(" << win.name << win.geometry << ')';
	return debug;
}
