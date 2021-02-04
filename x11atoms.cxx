// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "x11atoms.hxx"

#include <QDebug>
#include <QMetaProperty>
#include <QX11Info>

#ifdef Q_OS_LINUX

QDebug operator<<(QDebug debug, const xcb_generic_error_t *err) {
	if (err) {
		debug << err->error_code;
	} else {
		debug << "null";
	}
	return debug;
}

bool xcbErr(const void *data, xcb_generic_error_t *err, const char *msg) {
	if (!data || err) {
		qWarning() << msg << err;
		if (err) {
			free(err);
		}
		return true;
	}

	return false;
}

X11Atoms::X11Atoms(xcb_connection_t *con, QObject *parent)
	: QObject(parent) {
	const auto *meta = this->metaObject();
	QVector<std::tuple<QMetaProperty, xcb_intern_atom_cookie_t>> cookies;
	cookies.reserve(meta->propertyCount());
	for (int i = 0; i < meta->propertyCount(); i++) {
		const auto prop = meta->property(i);
		if (prop.typeName() == nullptr || strcmp(prop.typeName(), "xcb_atom_t") != 0) {
			continue;
		}
		const char *name = prop.name();
		if (name == nullptr) {
			continue;
		}
		cookies.push_back({prop, xcb_intern_atom(con, true, strlen(name), name)});
	}
	for (const auto &[prop, atomCookie] : cookies) {
		xcb_generic_error_t *err = nullptr;
		PodPtr<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(con, atomCookie, &err));
		if (!reply.data() || err) {
			qWarning() << "unable to get atom" << prop.name() << err;
			if (err) {
				free(err);
			}
			continue;
		}
		this->setProperty(prop.name(), QVariant(reply->atom));
	}
}

X11Atoms *X11Atoms::INSTANCE = nullptr;

const X11Atoms *X11Atoms::get() {
	if (INSTANCE == nullptr) {
		auto *con = QX11Info::connection();
		if (con == nullptr) {
			return nullptr;
		}
		INSTANCE = new X11Atoms(con);
	}
	return INSTANCE;
}

#endif