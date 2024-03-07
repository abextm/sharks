#ifndef WAYLAND_HXX
#define WAYLAND_HXX

#include <QWidget>
#include <QRect>
#include <QLocalSocket>

bool isWayland();
bool swayFullscreen();
QPixmap getWaylandScreenshot(QRect geometry);

#endif // WAYLAND_HXX
