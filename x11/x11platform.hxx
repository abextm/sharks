// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef X11PLATFORM_HXX
#define X11PLATFORM_HXX

#ifdef SHARKS_HAS_X
#include "platform.hxx"
#include "x11atoms.hxx"

class X11Platform : public Platform {
	Q_OBJECT
	Q_DISABLE_COPY(X11Platform)

	xcb_connection_t *conn;
	X11Atoms *atoms;

 public:
	X11Platform();
	
	static bool available();

	QImage getCursorImage() override;
	QList<OpenWindow> getOpenWindows() override;
	QPixmap getScreenshot(QRect geom) override;
};

#endif
#endif
