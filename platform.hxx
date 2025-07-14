// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef PLATFORM_HXX
#define PLATFORM_HXX

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QGuiApplication>
#include <QDebug>

struct OpenWindow {
 public:
	QRect geometry;
	QString name;
};
QDebug operator<<(QDebug, const OpenWindow &);

class Platform : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(Platform)

 public:
	Platform();

	static void init();

	template <class T>
	static T* nativeObject() {
		auto *guiApp = qobject_cast<QGuiApplication*>(QGuiApplication::instance());
		return guiApp->nativeInterface<T>();
	}

	virtual QImage getCursorImage();
	virtual QList<OpenWindow> getOpenWindows();
	virtual QPixmap getScreenshot(QRect geometry);
	virtual void waylandFullscreen();
	virtual bool isWayland();
};

extern Platform *platform;

#endif	// PLATFORM_HXX
