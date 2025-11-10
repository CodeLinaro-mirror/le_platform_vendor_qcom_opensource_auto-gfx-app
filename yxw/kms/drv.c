/*
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "kmsdrv.h"

static int kms_create_display(struct display *display)
{
	struct drmdisplay  *native;
	struct gbm_device *gbm;
	uint64_t modifier = 0;
	int ret;

#if defined(GBM_FORMAT_MOD_LINEAR)
	 modifier = GBM_FORMAT_MOD_LINEAR;

#endif
	if (display == NULL) {
		return -EINVAL;
	}

	native = calloc(1, sizeof(*native));
	if (native == NULL) {
		return -ENOMEM;
	}

	if (drm_init_display(native)) {
		free_native(native);
		return -1;
	}

	display->_native = native;
	ret = init_gbm(&native->gbm, native->mode->hdisplay, native->mode->vdisplay, modifier);
	if(ret == 0) {
		native->display = native->gbm.dev;
		display->hdl = (void *)native->display;
	}

	return ret;
}

static void kms_destroy_display(struct display *display)
{
	struct drmdisplay  *native;

	if (display == NULL) {
		return;
	}

	native = (struct drmdisplay *)display-> _native;
	if (native == NULL) {
		return;
	}

	if (native->card_fd > 0) {
		drmClose(native->card_fd);
		native->card_fd = -1;
	}

	gbm_device_destroy(native->display);

	free_native(native);
}

/************************************ Public  Interface *************************/
static int kms_create_window(struct window  *window)
{
	if((window == NULL) || (window->display == NULL) || (window->display->_native == NULL)) {
		return -1;
	}
	struct drmdisplay *native = (struct drmdisplay *)window->display->_native;

	window->_native = (size_t)0x0;

	window->hdl = native->gbm.surface;

	return 0;
}

static void kms_destroy_window(struct window  *window)
{
	
}

static int kms_pre_draw(struct window  *window)
{
	if((window == NULL) || (window->display == NULL) || (window->display->_native == NULL)) {
		return -1;
	}

	return 0;
}

static int kms_post_draw(struct window  *window)
{
	struct gbm_bo *bo = NULL;
	struct drmdisplay *native = NULL;
	struct gbm *gbm = NULL;
	int ret = 0;
	int i = 0;

	if((window == NULL) || (window->display == NULL) || (window->display->_native == NULL)) {
		return -1;
	}

	native = (struct drmdisplay *)window->display->_native;
	gbm = &native->gbm;

	/* assmue 60fps */
	while ((bo == NULL) && i < 16) {
		bo = gbm_surface_lock_front_buffer(gbm->surface);
		if (bo == NULL) {
			usleep(1000);
			i++;
			printf("fuck i=%d\n", i);
		}
	}

	if (bo == NULL) {
		printf("Failed to lock frontbuffer\n");
		return -1;
	}

	struct drm_fb *fb = drm_fb_get_from_bo(native, bo);
	if (fb == NULL) {
		printf("Failed to get a new framebuffer BO\n");
		return -1;
	}

	ret = drm_atomic_commit(native, fb->fb_id);
	if (ret) {
		printf("failed to commit: %s\n", strerror(errno));
		return -1;
	}

	//ret = drm_wait_vblank(native);
	if (ret < 0) {
		printf("failed to wait DRM: %s\n", strerror(errno));
	}

	/* release last buffer to render on again: */
	if (gbm->next_bo != NULL) {
		gbm_surface_release_buffer(gbm->surface, gbm->next_bo);
	}

	gbm->next_bo = bo;

	/* Allow a modeset change for the first commit only. */
	native->flags &= ~(DRM_MODE_ATOMIC_ALLOW_MODESET);

	return ret;
}

static int _run = 1;
static int running()
{
	return _run;
}

static void quit()
{
	_run = 0;
}

struct yxw_ops kms_ops = {
	kms_create_display,
	kms_destroy_display,

	kms_create_window,
	kms_destroy_window,

	kms_pre_draw,
	kms_post_draw,

	running,
	quit
};
