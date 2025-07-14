// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "waylandplatform.hxx"

#ifdef SHARKS_HAS_WAYLAND

#include <QTimer>

#include "hyprland.hxx"
#include "sway.hxx"
#include "wlrscreengrabber.hxx"

WaylandPlatform::WaylandPlatform()
	: qWayland(Platform::nativeObject<QNativeInterface::QWaylandApplication>()),
		sway(Sway::create()),
		hyprland(Hyprland::create()),
		wlrScreengrabber(WLRScreengrabber::create(qWayland->display())) {
}

bool WaylandPlatform::available() {
	return Platform::nativeObject<QNativeInterface::QWaylandApplication>() != nullptr;
}
void WaylandPlatform::waylandFullscreen() {
	if (this->sway) {
		this->sway->fullscreen();
	}
	if (this->hyprland) {
		QTimer::singleShot(250, this, [this]() { this->hyprland->fullscreen(); });
	}
}
QPixmap WaylandPlatform::getScreenshot(QRect geometry) {
	if (this->wlrScreengrabber) {
		return QPixmap::fromImage(this->wlrScreengrabber->grab(geometry));
	}

	qWarning() << "Unable to get wayland screenshot";

	return Platform::getScreenshot(geometry);
}

bool WaylandPlatform::isWayland() {
	return true;
}

#endif