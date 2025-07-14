// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef SHARKS_SWAY_HXX
#define SHARKS_SWAY_HXX

#ifdef SHARKS_HAS_WAYLAND

#include <QByteArray>

class Sway {
	const char *socketPath;

	explicit Sway(const char *socketPath);

 public:
	static Sway *create();

	void sendPacket(quint32 type, QByteArray payload);

	void fullscreen();
};

#endif

#endif
