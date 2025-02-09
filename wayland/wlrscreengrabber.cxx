// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "wlrscreengrabber.hxx"

#ifdef SHARKS_HAS_WAYLAND

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <QDataStream>
#include <QGuiApplication>
#include <QPainter>

#include "wayland-wlr-screencopy-unstable-v1-client-protocol.h"
#include "wayland-xdg-output-unstable-v1-client-protocol.h"

struct OutputGrab {
	zwlr_screencopy_frame_v1 *frame = nullptr;

	uint32_t format = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t stride = 0;
	wl_buffer *buffer = nullptr;

	int shmFD = 0;
	void *shm = nullptr;
	size_t shmSize = 0;

	uint32_t flags = 0;

	bool failed = false;

	OutputGrab() {
	}

	~OutputGrab() {
		if (this->shm) munmap(this->shm, this->shmSize);
		if (this->shmFD) close(this->shmFD);
		if (this->buffer) wl_buffer_destroy(this->buffer);
		if (this->frame) zwlr_screencopy_frame_v1_destroy(this->frame);
	}

	QImage asQImage() {
		if (this->failed || !this->shm) {
			return {};
		}

		QImage::Format qfmt = QImage::Format_Invalid;
		switch (this->format) {
			//case WL_SHM_FORMAT_ARGB8888: qfmt = QImage::Format_ARGB32_Premultiplied; break;
			case WL_SHM_FORMAT_XRGB8888:
				qfmt = QImage::Format_ARGB32;
				break;
			case WL_SHM_FORMAT_RGB888:
				qfmt = QImage::Format_BGR888;
				break;
			case WL_SHM_FORMAT_BGR888:
				qfmt = QImage::Format_RGB888;
				break;
			// https://doc.qt.io/qt-6/qimage.html#Format-enum
			// https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_shm-enum-format
			default:
				qInfo() << "unsupported pixel format 0x" << Qt::hex << this->format;
		}

		return QImage((uchar *)this->shm, this->width, this->height, this->stride, qfmt);
	}
};

struct WLROutput {
	WLRScreengrabber *parent = nullptr;
	wl_output *output = nullptr;
	uint32_t name = 0;

	zxdg_output_v1 *xdgOutput = nullptr;

	int32_t x = 0;
	int32_t y = 0;
	int32_t transform = 0;

	OutputGrab *grab = nullptr;

	WLROutput(WLRScreengrabber *g, wl_output *output, uint32_t name)
		: parent(g),
			output(output),
			name(name) {
	}

	~WLROutput() {
		if (this->xdgOutput) zxdg_output_v1_destroy(this->xdgOutput);
		if (this->grab) delete this->grab;
		wl_output_release(this->output);
	}
};

WLRScreengrabber::WLRScreengrabber(wl_display *dpy)
	: outputs() {
	this->dpy = dpy;
	auto wrappedDPY = (wl_display *)wl_proxy_create_wrapper(this->dpy);
	this->q = wl_display_create_queue(this->dpy);
	wl_proxy_set_queue((wl_proxy *)wrappedDPY, this->q);
	this->reg = wl_display_get_registry(wrappedDPY);
	wl_proxy_wrapper_destroy(wrappedDPY);
}

WLRScreengrabber::~WLRScreengrabber() {
	for (auto output : this->outputs) {
		delete output;
	}
	if (this->reg) wl_registry_destroy(this->reg);
	if (this->shm) wl_shm_destroy(this->shm);
	if (this->copyMan) zwlr_screencopy_manager_v1_destroy(this->copyMan);
	if (this->xdgOutputMan) zxdg_output_manager_v1_destroy(this->xdgOutputMan);
	if (this->q) wl_event_queue_destroy(this->q);
}

static const wl_output_listener outputListener{
	.geometry = [](void *data, wl_output *, int32_t x, int32_t y, int32_t, int32_t, int32_t, const char *, const char *, int32_t transform) {
		WLROutput *o = (WLROutput*) data;
		o->transform = transform; },
	.mode = [](void *, wl_output *, uint32_t, int32_t, int32_t, int32_t) {},
	.done = [](void *, wl_output *) {},
	.scale = [](void *, wl_output *, int32_t) {},
	.name = [](void *, wl_output *, const char *) {},
	.description = [](void *, wl_output *, const char *) {},
};

static const zxdg_output_v1_listener xdgOutputListener{
	.logical_position = [](void *data, zxdg_output_v1 *, int32_t x, int32_t y) {
		WLROutput *o = (WLROutput*) data;
		o->x = x;
		o->y = y; },
	.logical_size = [](void *, zxdg_output_v1 *, int32_t, int32_t) {},
	.done = [](void *, zxdg_output_v1 *) {},
	.name = [](void *, zxdg_output_v1 *, const char *) {},
	.description = [](void *, zxdg_output_v1 *, const char *) {},
};

static const wl_registry_listener registryListener{
	.global = [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
		Q_UNUSED(version);

		WLRScreengrabber *self = (WLRScreengrabber*) data;
		if (strcmp(interface, wl_shm_interface.name) == 0) {
			self->shm = (wl_shm*) wl_registry_bind(registry, name, &wl_shm_interface, 1);
		} else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
			self->copyMan = (zwlr_screencopy_manager_v1*) wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, 1);
		} else if (strcmp(interface, wl_output_interface.name) == 0) {
			auto *output = new WLROutput(self, (wl_output*) wl_registry_bind(registry, name, &wl_output_interface, 3), name);
			if (self->xdgOutputMan) {
				output->xdgOutput = zxdg_output_manager_v1_get_xdg_output(self->xdgOutputMan, output->output);
				zxdg_output_v1_add_listener(output->xdgOutput, &xdgOutputListener, output);
			}
			self->outputs.push_back(output);
			wl_output_add_listener(output->output, &outputListener, output);
		} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
			self->xdgOutputMan = (zxdg_output_manager_v1*) wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 2);
		} },
	.global_remove = [](void *data, struct wl_registry *, uint32_t name) {
		WLRScreengrabber *self = (WLRScreengrabber*) data;
		self->outputs.removeIf([name](WLROutput *output){
			if (output->name == name) {
				delete output;
				return true;
			}
			return false;
		}); },
};

bool WLRScreengrabber::init() {
	wl_registry_add_listener(this->reg, &registryListener, this);
	wl_display_roundtrip_queue(this->dpy, this->q);
	wl_display_roundtrip_queue(this->dpy, this->q);
	if (!this->shm || !this->copyMan || !this->xdgOutputMan || this->outputs.isEmpty()) {
		return false;
	}
	for (auto *output : this->outputs) {
		if (!output->xdgOutput) {
			output->xdgOutput = zxdg_output_manager_v1_get_xdg_output(this->xdgOutputMan, output->output);
			zxdg_output_v1_add_listener(output->xdgOutput, &xdgOutputListener, output);
		}
	}
	wl_display_roundtrip_queue(this->dpy, this->q);
	return true;
}

static const zwlr_screencopy_frame_v1_listener listener{
	.buffer = [](void *data, zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
		WLROutput *o = (WLROutput*) data;
		if (o->grab == nullptr || o->grab->buffer != nullptr) {
			return;
		}
		auto g = o->grab;
		g->format = format;
		g->width = width;
		g->height = height;
		g->stride = stride;

		size_t size = stride * height;
		g->shmSize = size;

		g->shmFD = memfd_create("", MFD_CLOEXEC);
		ftruncate(g->shmFD, size);

		g->shm = mmap(nullptr,size,  PROT_READ | PROT_WRITE, MAP_SHARED, g->shmFD, 0);
		wl_shm_pool *pool = wl_shm_create_pool(o->parent->shm, g->shmFD, size);
		g->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
		wl_shm_pool_destroy(pool);

		zwlr_screencopy_frame_v1_copy(frame, g->buffer); },
	.flags = [](void *data, zwlr_screencopy_frame_v1 *, uint32_t flags) {
		WLROutput *o = (WLROutput*) data;
		o->grab->flags = flags; },
	.ready = [](void *data, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t) {
		WLROutput *o = (WLROutput*) data;
		o->parent->outstanding--; },
	.failed = [](void *data, zwlr_screencopy_frame_v1 *) {
		WLROutput *o = (WLROutput*) data;
		o->parent->outstanding--;
		o->grab->failed = true; },
	.damage = [](void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t, uint32_t) {},
	.linux_dmabuf = [](void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t) {},
	.buffer_done = [](void *, zwlr_screencopy_frame_v1 *) {},
};

QImage WLRScreengrabber::grab(QRect geom) {
	this->outstanding = this->outputs.size();
	for (auto *out : this->outputs) {
		out->grab = new OutputGrab();
		out->grab->frame = zwlr_screencopy_manager_v1_capture_output(this->copyMan, false, out->output);
		zwlr_screencopy_frame_v1_add_listener(out->grab->frame, &listener, out);
	}
	for (; this->outstanding && wl_display_dispatch_queue(this->dpy, this->q) != -1;);

	QImage out(geom.size(), QImage::Format_ARGB32_Premultiplied);
	QPainter p(&out);

	for (auto *output : this->outputs) {
		auto grab = output->grab;
		if (!grab) {
			qDebug() << "failed to grab display" << output->x << output->y;
			continue;
		}
		output->grab = nullptr;
		auto img = grab->asQImage();
		p.resetTransform();
		p.translate(output->x, output->y);
		switch (output->transform & 3) {
			// not handling flipped mode correctly
			case WL_OUTPUT_TRANSFORM_NORMAL:
				break;
			case WL_OUTPUT_TRANSFORM_90:
				p.translate(grab->height / 2.0, grab->width / 2.0);
				p.rotate(90);
				p.translate(grab->width / -2.0, grab->height / -2.0);
				break;
			case WL_OUTPUT_TRANSFORM_180:
				p.translate(grab->width / 2.0, grab->height / 2.0);
				p.rotate(180);
				p.translate(grab->width / -2.0, grab->height / -2.0);
				break;
			case WL_OUTPUT_TRANSFORM_270:
				p.translate(grab->height / 2.0, grab->width / 2.0);
				p.rotate(270);
				p.translate(grab->width / -2.0, grab->height / -2.0);
				break;
		}
		p.drawImage(0, 0, img);
		delete grab;
	}

	return out;
}
WLRScreengrabber *WLRScreengrabber::create(wl_display *dpy) {
	auto *g = new WLRScreengrabber(dpy);
	if (!g->init()) {
		delete g;
		return nullptr;
	}

	return g;
}

#endif