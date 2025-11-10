/*
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * Copyright © 2011 Benjamin Franzke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <yxw.h>
#include <yxw_def.h>
#include "drv.h"

int wayland_create_display(struct display *display);

static void wayland_destroy_display(struct display *display)
{
	struct wldisplay  *wldpy;

	if (display == NULL) {
		return;
	}

	wldpy = (struct wldisplay *)display-> _native;
	if (wldpy == NULL) {
		return;
	}

	wl_surface_destroy(wldpy->cursor_surface);

	if (wldpy->cursor_theme)
		wl_cursor_theme_destroy(wldpy->cursor_theme);

	if (wldpy->wm_base)
		xdg_wm_base_destroy(wldpy->wm_base);

	if (wldpy->compositor)
		wl_compositor_destroy(wldpy->compositor);

	wl_registry_destroy(wldpy->registry);
	wl_display_flush(wldpy->display);
	wl_display_disconnect(wldpy->display);

	free(wldpy);
}

int wayland_create_window(struct window  *window);

static void wayland_destroy_window(struct window  *window)
{
	struct wlwin *wlwin = NULL;
	if ((window == NULL) || (window->_native == 0)) {
		return;
	}

	wlwin = (struct wlwin*)window->_native;

	wl_egl_window_destroy(wlwin->eglwin);

	if (wlwin->xdg_toplevel)
		xdg_toplevel_destroy(wlwin->xdg_toplevel);
	if (wlwin->xdg_surface)
		xdg_surface_destroy(wlwin->xdg_surface);
	wl_surface_destroy(wlwin->surface);

	if (wlwin->callback)
		wl_callback_destroy(wlwin->callback);

	free(wlwin);
}

static int wayland_pre_draw(struct window  *window)
{
	if ((window == NULL) || (window->display == NULL) ||
						(window->display->_native == 0)) {
		return -EINVAL;
	}

	struct wldisplay *wldpy = (struct wldisplay *)window->display->_native;

	/* According to weston-simple-egl:
	 * The mainloop here is a little subtle.  Redrawing will cause
	 * EGL to read events so we can just call
	 * wl_display_dispatch_pending() to handle any events that got
	 * queued up as a side effect. */
	return wl_display_dispatch_pending(wldpy->display);	
}

static int wayland_post_draw(struct window  *window)
{
	struct wl_region *region;
	struct wldisplay  *wldpy = NULL;
	struct wlwin *wlwin = NULL;

	if ((window == NULL) || (window->display == NULL) ||
			(window->display-> _native == 0)) {
		return -1;
	}

	wldpy = (struct wldisplay *)window->display-> _native;
	wlwin = (struct wlwin *)window-> _native;

	if (window->opaque || window->fullscreen) {
		region = wl_compositor_create_region(wldpy->compositor);

		wl_region_add(region, 0, 0,  window->rect.width, window->rect.height);
		wl_surface_set_opaque_region(wlwin->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(wlwin->surface, NULL);
	}
	return 0;
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

struct yxw_ops wayland_ops = {
	wayland_create_display,
	wayland_destroy_display,

	wayland_create_window,
	wayland_destroy_window,

	wayland_pre_draw,
	wayland_post_draw,

	running,
	quit
};

