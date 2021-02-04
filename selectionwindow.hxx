// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef SELECTIONWINDOW_HXX
#define SELECTIONWINDOW_HXX

#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QLabel>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

class SelectionWindow;

class DragHandle : public QLabel {
	Q_OBJECT
 private:
	Q_DISABLE_COPY(DragHandle)

 public:
	explicit DragHandle(QWidget *moveParent);
	virtual ~DragHandle();

	virtual void mousePressEvent(QMouseEvent *) override;
	virtual void mouseMoveEvent(QMouseEvent *) override;

	QWidget *moveParent;
	QPoint dragStart;
};

class ShotItem : public QGraphicsPixmapItem {
 public:
	ShotItem(SelectionWindow *);
	~ShotItem();

 protected:
	virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
	virtual void mousePressEvent(QGraphicsSceneMouseEvent *) override;
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;

 private:
	SelectionWindow *win;
};

struct OpenWindow {
 public:
	QRect geometry;
	QString name;
};
QDebug operator<<(QDebug, const OpenWindow &);

class SelectionWindow : public QWidget {
	Q_OBJECT
 private:
	Q_DISABLE_COPY(SelectionWindow)

	friend ShotItem;

 public:
	explicit SelectionWindow(QWidget *parent = nullptr);
	virtual ~SelectionWindow();

 protected:
	virtual void moveEvent(QMoveEvent *) override;
	virtual void resizeEvent(QResizeEvent *) override;

 private:
	void geometryChanged(QEvent *);

	void saveTo(QString path);
	QString savePath();

	QToolBar *toolbar;
	QGraphicsView *selectionView;
	QGraphicsScene *scene;
	QGraphicsPathItem *selectionItem;
	ShotItem *shotItem;
	QGraphicsPixmapItem *cursorItem;

	QRect desktopGeometry;
	qint8 recursingGeometry;

	QList<OpenWindow> openWindows;

	QPoint selectionStart;
	QPoint selectionEnd;
	QRect selection;
	void selectionMoved();

	QPixmap shot;

	QPoint cursorPosition;
	QPixmap cursor;
};
#endif
