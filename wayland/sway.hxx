#ifndef SHARKS_SWAY_HXX
#define SHARKS_SWAY_HXX

#ifdef SHARKS_HAS_WAYLAND

#include <QByteArray>

class Sway {
	const char* socketPath;

	explicit Sway(const char* socketPath);
 public:
	static Sway *create();

	void sendPacket(quint32 type, QByteArray payload);

	void fullscreen();

};

#endif

#endif
