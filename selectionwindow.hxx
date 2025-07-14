// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef SELECTIONWINDOW_HXX
#define SELECTIONWINDOW_HXX

#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWidget>

#include "platform.hxx"

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

class ColorSwatch : public QWidget {
	Q_OBJECT
 private:
	Q_DISABLE_COPY(ColorSwatch)
	QColor color;

 protected:
	virtual void paintEvent(QPaintEvent *) override;
	virtual void resizeEvent(QResizeEvent *) override;

 public:
	ColorSwatch(QWidget *parent = nullptr);
	virtual ~ColorSwatch();
	virtual QSize sizeHint() const override;

	void setColor(QColor color);
};

class ColorCopyButton : public QPushButton {
	Q_OBJECT
 public:
	enum Format {
		CSS_HEX,
		CSS_RGB,
	};

 private:
	Q_DISABLE_COPY(ColorCopyButton)

	Format format;
	QColor color;
	QString formatString() const;
	void copyString() const;

 public:
	ColorCopyButton(QWidget *parent = nullptr);
	virtual ~ColorCopyButton();

	void setFormat(Format format);
	void setColor(QColor color);
	void copyStringIfActive() const;
};

class ZoomTooltip : public QWidget {
	Q_OBJECT
 private:
	Q_DISABLE_COPY(ZoomTooltip)
	QPixmap img;
	int scale;

	void doResize();

 protected:
	virtual void paintEvent(QPaintEvent *) override;

 public:
	ZoomTooltip(QWidget *parent = nullptr);
	virtual ~ZoomTooltip();

	void setImage(QPixmap image);
	void setScale(int scale);
};

class ShotItem : public QGraphicsPixmapItem {
 public:
	ShotItem(SelectionWindow *);
	virtual ~ShotItem();

 protected:
	virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
	virtual void mousePressEvent(QGraphicsSceneMouseEvent *) override;
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;

 private:
	SelectionWindow *win;
};

class PenDrawing : public QAbstractGraphicsShapeItem {
	QPainterPath rawPath;
	QRectF bounds;
	QPainterPath strokedPath;

	void updateStrokedPath();

 public:
	PenDrawing(QPen pen, QPoint start);
	virtual ~PenDrawing();

	void addPoint(QPoint);

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
};

class DrawingUndoItem : public QUndoCommand {
	SelectionWindow *parent;
	QGraphicsItem *item;
	bool owned;

 public:
	explicit DrawingUndoItem(SelectionWindow *parent, QGraphicsItem *item);
	virtual ~DrawingUndoItem();

	void undo() override;
	void redo() override;
};

class SelectionWindow : public QWidget {
	Q_OBJECT
 private:
	Q_DISABLE_COPY(SelectionWindow)

	friend ShotItem;
	friend DrawingUndoItem;

 public:
	explicit SelectionWindow(QWidget *parent = nullptr);
	virtual ~SelectionWindow();

	void setPicking(bool picking);
	virtual void setVisible(bool visible) override;

 protected:
	virtual bool event(QEvent *) override;
	virtual void moveEvent(QMoveEvent *) override;
	virtual void resizeEvent(QResizeEvent *) override;

 signals:
	void pickColorChanged(QColor color);
	void pickColorSelected(QColor color);

 private:
	void geometryChanged(QEvent *);

	QPixmap pixmap();
	void saveTo(QString path);
	QString savePath();

	bool picking;
	bool pickedLock;
	QPoint pickPos;
	void pickMoved();
	void pickSelected();
	ZoomTooltip *pickTooltip;

	QToolBar *shotToolbar;
	QAction *togglePicker;
	QAction *showCursor;

	QAction *selectArea;
	QAction *penTool;

	QToolBar *pickToolbar;

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

	QUndoStack *undoStack;
	PenDrawing *activeDrawing;
};
#endif
