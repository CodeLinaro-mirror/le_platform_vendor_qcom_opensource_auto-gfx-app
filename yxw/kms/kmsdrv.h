/*
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * DRM based mode setting test program
 * Copyright 2008 Tungsten Graphics
 *   Jakob Bornecrantz <jakob@tungstengraphics.com>
 * Copyright 2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * This fairly simple test program dumps output in a similar format to the
 * "xrandr" tool everyone knows & loves.  It's necessarily slightly different
 * since the kernel separates outputs into encoder and connector structures,
 * each with their own unique ID.  The program also allows test testing of the
 * memory management and mode setting APIs by allowing the user to specify a
 * connector and mode to use for mode setting.  If all works as expected, a
 * blue background should be painted on the monitor attached to the specified
 * connector after the selected mode is set.
 *
 * TODO: use cairo to write the mode info on the selected output once
 *       the mode has been programmed, along with possible test patterns.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gbm.h>
#include <gbm_priv.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <yxw.h>
#include <yxw_def.h>

#ifndef _YXW_DRMBE_H_
#define _YXW_DRMBE_H_

#define DEBUG(XX) printf("%s:%d, %s\n", __func__, __LINE__, XX);

struct drm_plane {
	drmModePlane *hdl;
	drmModeObjectProperties *props;
	drmModePropertyRes **prop_res;
};

struct drm_crtc {
	drmModeCrtc *hdl;
	drmModeObjectProperties *props;
	drmModePropertyRes **prop_res;
};

struct drm_conn {
	drmModeConnector *hdl;
	drmModeObjectProperties *props;
	drmModePropertyRes **prop_res;
};

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
};

struct gbm {
	struct gbm_device *dev;
	struct gbm_surface *surface;
	uint32_t format;
	int width, height;
	struct gbm_bo *next_bo;
};

struct drmdisplay {
	int card_fd;
	drmModeModeInfo *mode;

	struct drm_plane *plane;

	struct drm_crtc *crtc;
	uint32_t crtc_id;
	int crtc_index;

	struct drm_conn *conn;
	uint32_t conn_id;

	uint32_t flags;

	struct gbm_device *display;
	struct gbm gbm;
};

static void free_native(struct drmdisplay  *native)
{
	if (native) {
		if (native->crtc)
			free(native->crtc);

		if (native->plane)
			free(native->plane);

		if (native->conn)
			free(native->conn);
		
		free(native);
	}
}

static uint32_t find_crtc_for_encoder(const drmModeRes *resources,
		const drmModeEncoder *encoder)
{
	int i;

	for (i = 0; i < resources->count_crtcs; i++) {
		const uint32_t crtc_mask = 1 << i;
		const uint32_t crtc_id = resources->crtcs[i];
		if (encoder->possible_crtcs & crtc_mask) {
			return crtc_id;
		}
	}

	/* no match found */
	return -1;
}

static uint32_t find_crtc_for_connector(const struct drmdisplay *native,
									const drmModeRes *resources,
									const drmModeConnector *connector) {
	int i;

	for (i = 0; i < connector->count_encoders; i++) {
		const uint32_t encoder_id = connector->encoders[i];
		drmModeEncoder *encoder = drmModeGetEncoder(native->card_fd, encoder_id);

		if (encoder) {
			const uint32_t crtc_id = find_crtc_for_encoder(resources, encoder);

			drmModeFreeEncoder(encoder);
			if (crtc_id != 0) {
				return crtc_id;
			}
		}
	}

	/* no match found */
	return -1;
}

static int drm_init_display(struct drmdisplay  *native)
{
	char *card_name = "msm_drm";
	int card_fd = -1;
	int ret;

	drmModeRes *resources;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int i, j, area;

	int primary_id = 0, found_primary = 0;

	native->card_fd = drmOpen(card_name, NULL);
	if (native->card_fd < 0) {
		printf("Failed to open kms card\n, error:%s\n", strerror(errno));
		return -errno;
	}

	drmSetClientCap(native->card_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	drmSetClientCap(native->card_fd, DRM_CLIENT_CAP_ATOMIC, 1);

	resources = drmModeGetResources(native->card_fd);
	if (!resources) {
		printf("drmModeGetResources failed: %s\n", strerror(errno));
		return -1;
	}

	/* find a connected connector: */
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(native->card_fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* it's connected, let's use this! */
			break;
		}
		drmModeFreeConnector(connector);
		connector = NULL;
	}

	if (!connector) {
		/* we could be fancy and listen for hotplug events and wait for
		 * a connector..
		 */
		printf("no connected connector!\n");
		return -1;
	}

	/* find preferred mode or the highest resolution mode: */
	for (i = 0, area = 0; i < connector->count_modes; i++) {
		drmModeModeInfo *current_mode = &connector->modes[i];

		if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
			native->mode = current_mode;
		}

		int current_area = current_mode->hdisplay * current_mode->vdisplay;
		if (current_area > area) {
			native->mode = current_mode;
			area = current_area;
		}
	}

	if (!native->mode) {
		printf("could not find mode!\n");
		return -1;
	}

	/* find encoder: */
	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(native->card_fd, resources->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id)
			break;
		drmModeFreeEncoder(encoder);
		encoder = NULL;
	}

	if (encoder) {
		native->crtc_id = encoder->crtc_id;
	} else {
		uint32_t crtc_id = find_crtc_for_connector(native, resources, connector);
		if (crtc_id == 0) {
			printf("no crtc found!\n");
			return -1;
		}

		native->crtc_id = crtc_id;
	}

	for (i = 0; i < resources->count_crtcs; i++) {
		if (resources->crtcs[i] == native->crtc_id) {
			native->crtc_index = i;
			break;
		}
	}

	drmModeFreeResources(resources);
	native->conn_id = connector->connector_id;

	/* DRM Atomic shall be trend,  so don't support legacy mode */
	ret = drmSetClientCap(native->card_fd, DRM_CLIENT_CAP_ATOMIC, 1);
	if (ret) {
		printf("Failed to enable atomic modeset: %s\n", strerror(errno));
		return -errno;
	}

	/* Find valid plane, better to be primary plane */
	drmModePlaneResPtr plan_res = drmModeGetPlaneResources(native->card_fd);
	if (!plan_res) {
		printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
		return -1;
	}
	for (i = 0; (i < plan_res->count_planes) && !found_primary; i++) {
		int id = plan_res->planes[i];
		drmModePlanePtr plane = drmModeGetPlane(native->card_fd, id);
		if (!plane)
			continue;

		if (plane->possible_crtcs & (1 << native->crtc_index)) {
			drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(native->card_fd, id, DRM_MODE_OBJECT_PLANE);
			primary_id = id;

			for (j = 0; j < props->count_props; j++) {
				drmModePropertyPtr prop = drmModeGetProperty(native->card_fd, props->props[j]);
				if ((strcmp(prop->name, "type") == 0) && (props->prop_values[j] == DRM_PLANE_TYPE_PRIMARY)) {
					found_primary = 1;
				}

				drmModeFreeProperty(prop);
			}

			drmModeFreeObjectProperties(props);
		}

		drmModeFreePlane(plane);
	}
	drmModeFreePlaneResources(plan_res);

	if (primary_id == 0) {
		printf("could not find a suitable plane\n");
		return ret;
	}

	native->plane = calloc(1, sizeof(*native->plane));
	native->crtc = calloc(1, sizeof(*native->crtc));
	native->conn = calloc(1, sizeof(*native->conn));

#define get_resource(NODE, NODETYPE, id) do {		\
		native->NODE->hdl = drmModeGet##NODETYPE(native->card_fd, id);		\
		if (!native->NODE->hdl) {		\
			printf("could not get %s %i: %s\n",		\
						#NODE, id, strerror(errno));		\
			return ret;			\
		}		\
	} while (0)

	get_resource(plane, Plane, primary_id);
	get_resource(crtc, Crtc, native->crtc_id);
	get_resource(conn, Connector, native->conn_id);

#define get_properties(NODE, NODETYPE, id) do {					\
		uint32_t i;							\
		native->NODE->props = drmModeObjectGetProperties(native->card_fd,		\
				id, DRM_MODE_OBJECT_##NODETYPE);			\
		if (!native->NODE->props) {						\
			printf("could not get %s %u properties: %s\n", #NODE, id, strerror(errno));		\
			return ret;						\
		}								\
		native->NODE->prop_res = calloc(native->NODE->props->count_props,	\
				sizeof(*native->NODE->prop_res));			\
		for (i = 0; i < native->NODE->props->count_props; i++) {		\
			native->NODE->prop_res[i] = drmModeGetProperty(native->card_fd,	\
					native->NODE->props->props[i]);		\
		}								\
	} while (0)

	get_properties(plane, PLANE, primary_id);
	get_properties(conn, CONNECTOR, native->conn_id);
	get_properties(crtc, CRTC, native->crtc_id);

	native->flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_ATOMIC_ALLOW_MODESET;
}

/* Buffer switch */
static int add_connector_property(struct drm_conn *conn, drmModeAtomicReq *req,
					uint32_t obj_id, const char *name, uint64_t value)
{
	unsigned int i;
	int prop_id = 0;

	for (i = 0 ; i < conn->props->count_props ; i++) {
		if (strcmp(conn->prop_res[i]->name, name) == 0) {
			prop_id = conn->prop_res[i]->prop_id;
			break;
		}
	}

	if (prop_id < 0) {
		printf("no connector property: %s\n", name);
		return -EINVAL;
	}

	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

static int init_gbm(struct gbm *gbm, int w, int h, uint64_t modifier)
{

	char *render_name = "/dev/dri/renderD128";
	int render_fd = -1;
	struct stat tmp;

	gbm->format = GBM_FORMAT_XRGB8888;

	if (stat(render_name, &tmp) == 0) {
		render_fd = open(render_name, O_RDONLY | O_CLOEXEC);
		if (render_fd < 0) {
			printf("Invalid dri node\n");
		}
	}
	gbm->dev = gbm_create_device(render_fd);

	if (gbm_surface_create_with_modifiers) {
		gbm->surface = gbm_surface_create_with_modifiers(gbm->dev, w, h,
								gbm->format,
								&modifier, 1);
	}

	if (!gbm->surface) {
#if defined(GBM_FORMAT_MOD_LINEAR)
		if (modifier != GBM_FORMAT_MOD_LINEAR) {
			fprintf(stderr, "Modifiers requested but support isn't available\n");
			return -1;
		}
#endif
		gbm->surface = gbm_surface_create(gbm->dev, w, h,
						gbm->format,
						GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	}

	if (!gbm->surface) {
		printf("failed to create gbm surface\n");
		return -1;
	}

	gbm->width = w;
	gbm->height = h;

	return 0;
}

static int add_crtc_property(struct drm_crtc *crtc, drmModeAtomicReq *req, 
				uint32_t obj_id, const char *name, uint64_t value)
{
	unsigned int i;
	int prop_id = -1;

	for (i = 0 ; i < crtc->props->count_props ; i++) {
		if (strcmp(crtc->prop_res[i]->name, name) == 0) {
			prop_id = crtc->prop_res[i]->prop_id;
			break;
		}
	}

	if (prop_id < 0) {
		printf("no crtc property: %s\n", name);
		return -EINVAL;
	}

	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

static int add_plane_property(struct drm_plane *plane, drmModeAtomicReq *req,
				uint32_t obj_id, const char *name, uint64_t value)
{
	unsigned int i;
	int prop_id = -1;

	for (i = 0 ; i < plane->props->count_props ; i++) {
		if (strcmp(plane->prop_res[i]->name, name) == 0) {
			prop_id = plane->prop_res[i]->prop_id;
			break;
		}
	}

	if (prop_id < 0) {
		printf("no plane property: %s\n", name);
		return -EINVAL;
	}

	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

static int drm_atomic_commit(struct drmdisplay *native, uint32_t fb_id)
{
	uint32_t plane_id = native->plane->hdl->plane_id;
	uint32_t blob_id;
	int ret;

	drmModeAtomicReq *req = drmModeAtomicAlloc();

	if (native->flags & DRM_MODE_ATOMIC_ALLOW_MODESET) {
		if (add_connector_property(native->conn, req, native->conn_id, "CRTC_ID", native->crtc_id) < 0)
				return -1;

		if (drmModeCreatePropertyBlob(native->card_fd, native->mode, sizeof(*native->mode), &blob_id) != 0)
			return -1;

		if (add_crtc_property(native->crtc, req, native->crtc_id, "MODE_ID", blob_id) < 0)
			return -1;

		if (add_crtc_property(native->crtc, req, native->crtc_id, "ACTIVE", 1) < 0)
			return -1;
	}

	add_plane_property(native->plane, req, plane_id, "FB_ID", fb_id);
	add_plane_property(native->plane, req, plane_id, "CRTC_ID", native->crtc_id);
	add_plane_property(native->plane, req, plane_id, "SRC_X", 0);
	add_plane_property(native->plane, req, plane_id, "SRC_Y", 0);
	add_plane_property(native->plane, req, plane_id, "SRC_W", native->mode->hdisplay << 16);
	add_plane_property(native->plane, req, plane_id, "SRC_H", native->mode->vdisplay << 16);
	add_plane_property(native->plane, req, plane_id, "CRTC_X", 0);
	add_plane_property(native->plane, req, plane_id, "CRTC_Y", 0);
	add_plane_property(native->plane, req, plane_id, "CRTC_W", native->mode->hdisplay);
	add_plane_property(native->plane, req, plane_id, "CRTC_H", native->mode->vdisplay);

	ret = drmModeAtomicCommit(native->card_fd, req, native->flags, NULL);

	drmModeAtomicFree(req);

	return ret;
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
	struct drm_fb *fb = data;

	if (fb->fb_id)
		drmModeRmFB(drm_fd, fb->fb_id);

	free(fb);
}

struct drm_fb * drm_fb_get_from_bo(struct drmdisplay *native, struct gbm_bo *bo)
{
	int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
	struct drm_fb *fb = gbm_bo_get_user_data(bo);
	uint32_t width, height, format,
		 strides[4] = {0}, handles[4] = {0},
		 offsets[4] = {0}, flags = 0;
	int ret = -1;

	if (fb)
		return fb;

	fb = calloc(1, sizeof *fb);
	fb->bo = bo;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	format = gbm_bo_get_format(bo);

	uint64_t modifiers[4] = {0};
	modifiers[0] = gbm_bo_get_modifier(bo);
	const int num_planes = gbm_bo_get_plane_count(bo);
	for (int i = 0; i < num_planes; i++) {
		strides[i] = gbm_bo_get_stride_for_plane(bo, i);
		handles[i] = gbm_bo_get_handle(bo).u32;
		offsets[i] = gbm_bo_get_offset(bo, i);
		modifiers[i] = modifiers[0];
	}

	if (modifiers[0]) {
		flags = DRM_MODE_FB_MODIFIERS;
		printf("Using modifier %" PRIx64 "\n", modifiers[0]);
	}

	ret = drmModeAddFB2WithModifiers(native->card_fd, width, height,
			format, handles, strides, offsets,
			modifiers, &fb->fb_id, flags);

	if (ret) {
		if (flags)
			fprintf(stderr, "Modifiers failed!\n");

		memcpy(handles, (uint32_t [4]){gbm_bo_get_handle(bo).u32,0,0,0}, 16);
		memcpy(strides, (uint32_t [4]){gbm_bo_get_stride(bo),0,0,0}, 16);
		memset(offsets, 0, 16);
		ret = drmModeAddFB2(native->card_fd, width, height, format,
				handles, strides, offsets, &fb->fb_id, 0);
	}

	if (ret) {
		printf("failed to create fb: %s\n", strerror(errno));
		free(fb);
		return NULL;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}

static int drm_wait_vblank(struct drmdisplay *native)
{
	int ret;
	drmVBlank vbl = {0};

	vbl.request.type |= DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT;
	vbl.request.type |= (native->crtc_id << DRM_VBLANK_HIGH_CRTC_SHIFT) & DRM_VBLANK_HIGH_CRTC_MASK;
	ret = drmWaitVBlank(native->card_fd, &vbl);
}

#endif
