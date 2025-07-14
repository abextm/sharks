// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef WAYLANDPLATFORM_HXX
#define WAYLANDPLATFORM_HXX

#ifdef SHARKS_HAS_WAYLAND

#include <QGuiApplication>
#include <platform.hxx>

class Sway;
class Hyprland;
class WLRScreengrabber;

class WaylandPlatform : public Platform {
	Q_OBJECT
	Q_DISABLE_COPY(WaylandPlatform)

	QNativeInterface::QWaylandApplication *qWayland;
	Sway *sway;
	Hyprland *hyprland;
	WLRScreengrabber *wlrScreengrabber;

 public:
	WaylandPlatform();

	static bool available();

	void waylandFullscreen() override;
	QPixmap getScreenshot(QRect geometry) override;
	bool isWayland() override;
};

#endif
#endif
