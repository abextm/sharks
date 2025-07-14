// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "sway.hxx"

#ifdef SHARKS_HAS_WAYLAND

#include <QCoreApplication>
#include <QLocalSocket>

Sway::Sway(const char *socketPath) : socketPath(socketPath) {
}

Sway *Sway::create() {
	const char *swaysockPath = ::getenv("SWAYSOCK");
	if (swaysockPath == nullptr || swaysockPath[0] == '\0') {
		return nullptr;
	}

	return new Sway(swaysockPath);
}

static QByteArray swayPacketize(quint32 type, QByteArray payload) {
	QByteArray out;
	QDataStream str(&out, QIODevice::WriteOnly | QIODevice::Append);
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
	str.setByteOrder(QDataStream::BigEndian);
#else
	str.setByteOrder(QDataStream::LittleEndian);
#endif
	str.writeRawData("i3-ipc", 6);
	str << quint32(payload.length());
	str << quint32(type);
	out.append(payload);
	return out;
}

void Sway::sendPacket(quint32 type, QByteArray payload) {
	auto *sock = new QLocalSocket;

	sock->connectToServer(this->socketPath, QIODevice::ReadWrite);
	sock->waitForConnected();

	sock->write(swayPacketize(type, payload));
	sock->flush();

	// wait for a response so we don't close the socket before the request is written
	QObject::connect(sock, &QLocalSocket::readyRead, sock, &QObject::deleteLater);
}

void Sway::fullscreen() {
	this->sendPacket(0 /* RUN_COMMAND */, QString("for_window [pid=%1 title=\"^Sharks$\"] fullscreen enable global").arg(QCoreApplication::applicationPid()).toUtf8());
}

#endif