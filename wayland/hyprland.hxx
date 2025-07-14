// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef SHARKS_HYPRLAND_HXX
#define SHARKS_HYPRLAND_HXX

#include <QString>

#ifdef SHARKS_HAS_WAYLAND

class Hyprland {
	const QString socketPath;

	explicit Hyprland(QString socketPath);

 public:
	static Hyprland *create();

	void sendMessage(QByteArray message);
	void fullscreen();
};

#endif

#endif
