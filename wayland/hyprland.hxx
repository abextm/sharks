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
