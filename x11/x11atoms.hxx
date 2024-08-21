// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef X11ATOMS_HXX
#define X11ATOMS_HXX

#ifdef SHARKS_HAS_X

#include <QObject>
#include <QScopedPointer>
#include <xcb/xcb.h>

template <class C>
using PodPtr = QScopedPointer<C, QScopedPointerPodDeleter>;

QDebug operator<<(QDebug debug, const xcb_generic_error_t *err);
bool xcbErr(const void *data, xcb_generic_error_t *err, const char *msg);

class X11Atoms final : public QObject {
	Q_OBJECT
 private:
	Q_DISABLE_COPY(X11Atoms)
 public:
	X11Atoms(xcb_connection_t *con, QObject *parent = nullptr);

#define ATOM_PROP(name) \
	Q_PROPERTY(xcb_atom_t name MEMBER name FINAL) \
	xcb_atom_t name;
	ATOM_PROP(_NET_WM_WINDOW_TYPE)
	ATOM_PROP(_NET_WM_WINDOW_TYPE_DOCK)
	ATOM_PROP(_NET_WM_WINDOW_TYPE_TOOLBAR)
	ATOM_PROP(_NET_WM_WINDOW_TYPE_MENU)
	ATOM_PROP(_NET_WM_WINDOW_TYPE_UTILITY)
	ATOM_PROP(_NET_WM_WINDOW_TYPE_SPLASH)
	ATOM_PROP(_NET_WM_WINDOW_TYPE_DIALOG)
	ATOM_PROP(_NET_WM_WINDOW_TYPE_NOTIFICATION)
	ATOM_PROP(_NET_WM_WINDOW_TYPE_NORMAL)

	ATOM_PROP(_NET_WM_STATE)
	ATOM_PROP(_NET_WM_STATE_HIDDEN)
#undef ATOM_PROP
};
Q_DECLARE_METATYPE(xcb_atom_t)
#endif

#endif	// X11ATOMS_HXX
