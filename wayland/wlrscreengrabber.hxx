// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef SHARKS_WLRSCREENGRABBER_HXX
#define SHARKS_WLRSCREENGRABBER_HXX

#ifdef SHARKS_HAS_WAYLAND

#include <wayland-client.h>

#include <QGuiApplication>
#include <QObject>
#include <QRect>

#include "wayland-wlr-screencopy-unstable-v1-client-protocol.h"
#include "wayland-xdg-output-unstable-v1-client-protocol.h"

struct WLROutput;

class WLRScreengrabber {
 public:
	wl_display *dpy = nullptr;
	wl_event_queue *q = nullptr;
	wl_registry *reg = nullptr;
	wl_shm *shm = nullptr;
	zwlr_screencopy_manager_v1 *copyMan = nullptr;
	zxdg_output_manager_v1 *xdgOutputMan = nullptr;
	QList<WLROutput *> outputs;
	int outstanding = 0;

	static WLRScreengrabber *create(wl_display *dpy);

	explicit WLRScreengrabber(wl_display *dpy);
	~WLRScreengrabber();

	bool init();
	QImage grab(QRect geom);
};

#endif

#endif
