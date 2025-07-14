// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "platform.hxx"

#include <QCursor>
#include <QScreen>

#include "wayland/waylandplatform.hxx"
#include "x11/x11platform.hxx"

Platform *platform = nullptr;

void Platform::init() {
#ifdef SHARKS_HAS_X
	if (platform == nullptr && X11Platform::available()) {
		platform = new X11Platform();
	}
#endif
#if SHARKS_HAS_WAYLAND
	if (platform == nullptr && WaylandPlatform::available()) {
		platform = new WaylandPlatform();
	}
#endif
	if (platform == nullptr) {
		platform = new Platform();
	}
}

Platform::Platform() {
}

QImage Platform::getCursorImage() {
	QCursor c{};
	auto p = c.pixmap();
	if (p.isNull()) {
		auto b = c.bitmap();
		if (b.isNull()) {
			qInfo() << "unable to get cursor image";
		} else {
			p = QPixmap(b);
		}
	}

	auto img = p.toImage();
	img.setOffset(c.hotSpot());
	return img;
}

QList<OpenWindow> Platform::getOpenWindows() {
	return {};
}
QPixmap Platform::getScreenshot(QRect geometry) {
	auto *screen = QGuiApplication::primaryScreen();

	// you can only grab relative to the screen, but it works to grab the whole virtual desktop
	auto screenGeometry = screen->geometry();
	return screen->grabWindow(0, -screenGeometry.x(), -screenGeometry.y(), geometry.width(), geometry.height());
}

void Platform::waylandFullscreen() {
}

QDebug operator<<(QDebug debug, const OpenWindow &win) {
	QDebugStateSaver saver(debug);
	debug.nospace() << "Win(" << win.name << win.geometry << ')';
	return debug;
}

bool Platform::isWayland() {
	return false;
}