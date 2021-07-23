// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "selectionwindow.hxx"

#include <QBitmap>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
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
#include <QX11Info>
QPixmap getX11Screenshot(QRect desktopGeometry);
QImage getX11Cursor();
QList<OpenWindow> getX11Windows();
#endif

SelectionWindow::SelectionWindow(QWidget *parent)
	: QWidget(parent),
		recursingGeometry(-1),
		openWindows(),
		selectionStart(),
		selectionEnd(),
		selection(),
		shot(),
		cursor() {
	this->cursorPosition = QCursor::pos();
	QScreen *screen = QGuiApplication::screenAt(this->cursorPosition);
	this->desktopGeometry = screen->virtualGeometry();

#ifdef Q_OS_LINUX
	if (QX11Info::isPlatformX11()) {
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
	if (QX11Info::isPlatformX11()) {
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

	this->setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
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

	this->toolbar = new QToolBar(this);
	this->toolbar->setAutoFillBackground(true);

	{
		QRect geo = screen->geometry();
		QPoint pt(
			qBound(geo.left(), geo.center().x() - (toolbar->width() / 2), geo.right()),
			geo.top() + 40);

		this->toolbar->move(pt);
	}

	auto *dragHandle = new DragHandle(this->toolbar);
	this->toolbar->addWidget(dragHandle);

	auto *showCursor = new QAction(QIcon::fromTheme("input-mouse"), "Show cursor", this);
	showCursor->setCheckable(true);
	connect(showCursor, &QAction::toggled, this, [this](bool enabled) {
		this->cursorItem->setVisible(enabled);
	});
	showCursor->setChecked(true);
	this->toolbar->addAction(showCursor);

	this->toolbar->addSeparator();

	auto *save = new QAction(QIcon::fromTheme("document-save"), "Save", this);
	connect(save, &QAction::triggered, this, [this]() {
		this->close();
		this->saveTo(this->savePath());
	});
	this->toolbar->addAction(save);

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
	this->toolbar->addAction(saveas);

	auto *upload = new QAction(QIcon::fromTheme("document-send"), "Upload", this);
	upload->setShortcut(QKeySequence(Qt::Key_Enter));
	connect(upload, &QAction::triggered, this, [this]() {
		this->setVisible(false);
		QString path = this->savePath();
		this->saveTo(path);
		qInfo() << QProcess::execute("ymup", QStringList({"-if", path}));
		this->close();
	});
	this->toolbar->addAction(upload);

	this->toolbar->addSeparator();

	auto *close = new QAction(QIcon::fromTheme("exit"), "Close", this);
	connect(close, &QAction::triggered, this, &QWidget::close);
	close->setShortcut(QKeySequence(Qt::Key_Escape));
	this->toolbar->addAction(close);

	this->selectionMoved();

	this->recursingGeometry = 0;
	this->setGeometry(desktopGeometry);
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

SelectionWindow::~SelectionWindow() {
}

DragHandle::DragHandle(QWidget *moveParent) : QLabel(" ⠿ ", moveParent), moveParent(moveParent) {
}
DragHandle::~DragHandle() {
}
void DragHandle::mousePressEvent(QMouseEvent *ev) {
	this->dragStart = this->moveParent->mapFromGlobal(ev->globalPos());
}
void DragHandle::mouseMoveEvent(QMouseEvent *ev) {
	if (ev->buttons().testFlag(Qt::LeftButton)) {
		QPoint newPos = qobject_cast<QWidget *>(this->moveParent->parent())->mapFromGlobal(ev->globalPos());
		this->moveParent->move(newPos - this->dragStart);
	}
}

ShotItem::ShotItem(SelectionWindow *parent) : QGraphicsPixmapItem(parent->shot), win(parent) {
	this->setCursor(QCursor(Qt::CursorShape::CrossCursor));
}
ShotItem::~ShotItem() {}
void ShotItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	win->selectionStart = event->screenPos();
	win->selectionEnd = QPoint();
	win->selectionMoved();
}
void ShotItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	if (event->buttons().testFlag(Qt::LeftButton)) {
		win->selectionEnd = event->screenPos();
		win->selectionMoved();
	}
}
void ShotItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
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

QDebug operator<<(QDebug debug, const OpenWindow &win) {
	QDebugStateSaver saver(debug);
	debug.nospace() << "Win(" << win.name << win.geometry << ')';
	return debug;
}
