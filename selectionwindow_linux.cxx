// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "selectionwindow.hxx"

#ifdef Q_OS_LINUX
#include <errno.h>
#include <sys/shm.h>
#include <xcb/shm.h>
#include <xcb/xfixes.h>

#include <QDebug>
#include <QVector>
#include <tuple>

#include "x11atoms.hxx"

QImage getX11Cursor() {
	auto *con = getXCBConnection();
	if (!con) {
		return QImage();
	}

	auto imgCookie = xcb_xfixes_get_cursor_image(con);

	xcb_generic_error_t *err = nullptr;
	PodPtr<xcb_xfixes_get_cursor_image_reply_t> evr(xcb_xfixes_get_cursor_image_reply(con, imgCookie, &err));
	if (err) {
		free(err);
		return QImage();
	}

	uint32_t *pixdata = xcb_xfixes_get_cursor_image_cursor_image(evr.data());
	QImage img(evr->width, evr->height, QImage::Format_ARGB32_Premultiplied);
	for (int y = 0; y < evr->height; y++) {
		uint32_t *line = reinterpret_cast<uint32_t *>(img.scanLine(y));
		memcpy(line, &pixdata[y * evr->width], evr->width * 4);
	}
	img.setOffset(QPoint(evr->xhot, evr->yhot));
	return img;
}

static void walkWindowTree(QList<OpenWindow> &out, xcb_connection_t *con, QPoint parent, xcb_query_tree_reply_t *parentGeo) {
	const auto *atoms = X11Atoms::get();
	const auto *children = xcb_query_tree_children(parentGeo);
	const auto len = xcb_query_tree_children_length(parentGeo);
	QVector<std::tuple<xcb_window_t, xcb_query_tree_cookie_t, xcb_get_geometry_cookie_t, xcb_get_window_attributes_cookie_t, xcb_get_property_cookie_t, xcb_get_property_cookie_t, xcb_get_property_cookie_t>> cookies;
	cookies.reserve(len);
	for (int i = 0; i < len; i++) {
		auto win = children[i];
		cookies.push_back({win,
			xcb_query_tree(con, win),
			xcb_get_geometry(con, win),
			xcb_get_window_attributes(con, win),
			xcb_get_property(con, false, win, atoms->_NET_WM_WINDOW_TYPE, XCB_ATOM_ATOM, 0, 32),
			xcb_get_property(con, false, win, atoms->_NET_WM_STATE, XCB_ATOM_ATOM, 0, 32),
			xcb_get_property(con, false, win, XCB_ATOM_WM_NAME, XCB_ATOM_ANY, 0, 64)});
	}
	for (const auto &[win, treeCookie, geoCookie, attribCookie, winTypeCookie, winStateCookie, winNameCookie] : cookies) {
		bool invalid = false;
		xcb_generic_error_t *err = nullptr;
		PodPtr<xcb_query_tree_reply_t> treeReply(xcb_query_tree_reply(con, treeCookie, &err));
		invalid |= xcbErr(treeReply.data(), err, "unable to query entire window tree");

		PodPtr<xcb_get_geometry_reply_t> geoReply(xcb_get_geometry_reply(con, geoCookie, &err));
		invalid |= xcbErr(geoReply.data(), err, "unable to get window geometry");

		PodPtr<xcb_get_window_attributes_reply_t> attribReply(xcb_get_window_attributes_reply(con, attribCookie, &err));
		invalid |= xcbErr(attribReply.data(), err, "unable to get window attributes");

		PodPtr<xcb_get_property_reply_t> winTypeReply(xcb_get_property_reply(con, winTypeCookie, &err));
		invalid |= xcbErr(winTypeReply.data(), err, "unable to get window type");

		PodPtr<xcb_get_property_reply_t> winStateReply(xcb_get_property_reply(con, winStateCookie, &err));
		invalid |= xcbErr(winStateReply.data(), err, "unable to get window State");

		PodPtr<xcb_get_property_reply_t> winNameReply(xcb_get_property_reply(con, winNameCookie, &err));
		invalid |= xcbErr(winTypeReply.data(), err, "unable to get window name");

		if (invalid) {
			continue;
		}

		if (attribReply->map_state != XCB_MAP_STATE_VIEWABLE) {
			continue;
		}

		QPoint loc = parent + QPoint(geoReply->x, geoReply->y);

		bool shouldBeSelectable = false;
		auto winTypes = reinterpret_cast<xcb_atom_t *>(xcb_get_property_value(winTypeReply.data()));
		for (int i = 0; i < xcb_get_property_value_length(winTypeReply.data()); i++) {
			const auto winType = winTypes[i];
			shouldBeSelectable |= winType == atoms->_NET_WM_WINDOW_TYPE_DIALOG
				|| winType == atoms->_NET_WM_WINDOW_TYPE_DOCK
				|| winType == atoms->_NET_WM_WINDOW_TYPE_MENU
				|| winType == atoms->_NET_WM_WINDOW_TYPE_NORMAL
				|| winType == atoms->_NET_WM_WINDOW_TYPE_NOTIFICATION
				|| winType == atoms->_NET_WM_WINDOW_TYPE_SPLASH
				|| winType == atoms->_NET_WM_WINDOW_TYPE_TOOLBAR
				|| winType == atoms->_NET_WM_WINDOW_TYPE_UTILITY;
		}

		bool isHidden = false;
		auto winStates = reinterpret_cast<xcb_atom_t *>(xcb_get_property_value(winStateReply.data()));
		for (int i = 0; i < xcb_get_property_value_length(winStateReply.data()); i++) {
			const auto winState = winStates[i];
			isHidden |= winState == atoms->_NET_WM_STATE_HIDDEN;
		}

		if (isHidden) {
			continue;
		}

		QByteArray name((const char *)xcb_get_property_value(winNameReply.data()), xcb_get_property_value_length(winNameReply.data()));

		if (shouldBeSelectable) {
			OpenWindow w;
			w.geometry = QRect(loc, QSize(geoReply->width, geoReply->height));
			w.name = QString::fromLocal8Bit(name);
			out.push_back(w);
			continue;
		}

		if (xcb_query_tree_children_length(treeReply.data()) > 0) {
			walkWindowTree(out, con, loc, treeReply.data());
		}
	}
}

QList<OpenWindow> getX11Windows() {
	QList<OpenWindow> out;

	auto *con = getXCBConnection();
	if (!con) {
		return out;
	}
	auto screen = xcb_setup_roots_iterator(xcb_get_setup(con)).data;

	auto treeQueryCookie = xcb_query_tree(con, screen->root);
	xcb_generic_error_t *err = nullptr;
	PodPtr<xcb_query_tree_reply_t> reply(xcb_query_tree_reply(con, treeQueryCookie, &err));
	if (xcbErr(reply.data(), err, "unable to query window tree root")) {
		return out;
	}

	walkWindowTree(out, con, QPoint(0, 0), reply.data());

	return out;
}

class SharedMemory {
 public:
	SharedMemory(int id) : id(id) {}
	void free() {
		if (id >= 0) {
			shmctl(id, IPC_RMID, nullptr);
			id = -1;
		}
	}
	~SharedMemory() {
		this->free();
	}
	int id;
};

static void detachShm(void *data) {
	shmdt(data);
}

QPixmap getX11Screenshot(QRect geometry) {
	auto *con = getXCBConnection();
	if (!con) {
		return QPixmap();
	}

	xcb_generic_error_t *err = nullptr;

	auto screen = xcb_setup_roots_iterator(xcb_get_setup(con)).data;

	if (screen->root_depth != 32 && screen->root_depth != 24) {
		qWarning() << "using slow screen grab because root is" << screen->root_depth << "bpp";
		return QPixmap();
	}

	int size = geometry.width() * 4 * geometry.height();
	SharedMemory shm(shmget(IPC_PRIVATE, size, IPC_CREAT | 0777));
	if (shm.id == -1) {
		qWarning() << "unable to create shared memory" << errno;
		return QPixmap();
	}

	xcb_shm_seg_t shmseg = xcb_generate_id(con);
	auto shmAttachCookie = xcb_shm_attach_checked(con, shmseg, shm.id, 0);
	auto shmgetCookie = xcb_shm_get_image(con, screen->root, geometry.x(), geometry.y(), geometry.width(), geometry.height(), ~0, XCB_IMAGE_FORMAT_Z_PIXMAP, shmseg, 0);
	xcb_shm_detach(con, shmseg);

	err = xcb_request_check(con, shmAttachCookie);
	if (err != nullptr) {
		qWarning() << "unable to attach shmem" << err;
		free(err);
	}

	PodPtr<xcb_shm_get_image_reply_t> shmgetReply(xcb_shm_get_image_reply(con, shmgetCookie, &err));
	if (xcbErr(shmgetReply.data(), err, "unable to get screenshot with xshm")) {
		return QPixmap();
	}

	if (shmgetReply->depth != 32 && shmgetReply->depth != 24) {
		qWarning() << "somehow got a" << shmgetReply->depth << "bpp image";
		return QPixmap();
	}

	void *data = shmat(shm.id, nullptr, 0);
	shm.free();

	QImage img((quint8 *)data, geometry.width(), geometry.height(), QImage::Format_RGB32, &detachShm, data);
	auto pixmap = QPixmap::fromImage(img);
	return pixmap;
}
#endif