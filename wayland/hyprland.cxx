// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "hyprland.hxx"

#include <QCoreApplication>
#include <QDebug>
#include <QLocalSocket>

#ifdef SHARKS_HAS_WAYLAND

Hyprland::Hyprland(QString socketPath)
	: socketPath(socketPath) {
}

Hyprland *Hyprland::create() {
	auto his = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
	auto rtDir = qgetenv("XDG_RUNTIME_DIR");

	if (his.isEmpty() || rtDir.isEmpty()) {
		return nullptr;
	}

	QString socketPath = QString("%1/hypr/%2/.socket.sock").arg(rtDir, his);
	return new Hyprland(socketPath);
}
void Hyprland::sendMessage(QByteArray message) {
	auto *sock = new QLocalSocket;

	sock->connectToServer(this->socketPath, QIODevice::ReadWrite);
	sock->waitForConnected();

	sock->write(message);
	sock->flush();

	// wait for a response so we don't close the socket before the request is written
	QObject::connect(sock, &QLocalSocket::readyRead, sock, &QObject::deleteLater);
}

void Hyprland::fullscreen() {
	this->sendMessage(QString(
		"[[BATCH]]/"
		"dispatch moveoutofgroup pid:%1;"
		"dispatch setfloating pid:%1;"
		"dispatch movewindowpixel exact 0 0,pid:%1")
			.arg(QCoreApplication::applicationPid())
			.toUtf8());
	qDebug() << "Sent";
}

#endif