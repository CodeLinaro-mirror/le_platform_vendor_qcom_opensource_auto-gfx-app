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
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <yxw.h>
#include <yxw_def.h>
#include "drv.h"


static void handle_surface_configure(void *data, struct xdg_surface *surface,
			 uint32_t serial)
{
	struct window *window = data;
	struct wlwin *wlwin = NULL;

	if ((window == NULL) || (window->_native == 0)) {
		return;
	}

	wlwin = (struct wlwin*)window->_native;

	xdg_surface_ack_configure(surface, serial);

	wlwin->wait_for_configure = 0;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_surface_configure
};

static void handle_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
			  int32_t width, int32_t height,
			  struct wl_array *states)
{
	struct window *window = data;
	uint32_t *p;
	struct wlwin *wlwin = NULL;

	if ((window == NULL) || (window->_native == 0)) {
		return;
	}

	wlwin = (struct wlwin*)window->_native;

	wl_array_for_each(p, states) {
		uint32_t state = *p;
		switch (state) {
		case XDG_TOPLEVEL_STATE_FULLSCREEN:
			//window->fullscreen = 1;
			break;
		case XDG_TOPLEVEL_STATE_MAXIMIZED:
			//window->maximized = 1;
			break;
		}
	}

	if (wlwin->eglwin)
		wl_egl_window_resize(wlwin->eglwin,
				     window->rect.width,
				     window->rect.height, 0, 0);
}

static void handle_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	yxw_quit();
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	handle_toplevel_configure,
	handle_toplevel_close,
};

/************************************ Public  Interface *************************/
int wayland_create_window(struct window  *window)
{
	struct wlwin *wlwin  = NULL;
	struct wldisplay *wldpy = NULL;
	struct display *display = NULL;

	if ((window == NULL) || (window->display == NULL)) {
		return -EINVAL;
	}

	display = window->display;

	if ((display == NULL) || (display->window == NULL)) {
		return -EINVAL;
	}

	wldpy = (struct wldisplay *)display-> _native;
	
	wlwin = malloc(sizeof(*wlwin));
	if (wlwin == NULL) {
		return -ENOMEM;
	}

	memset(wlwin, 0, sizeof(*wlwin));
	window->_native = (size_t)wlwin;
	wlwin->surface = wl_compositor_create_surface(wldpy->compositor);

	wlwin->eglwin =
		wl_egl_window_create(wlwin->surface,
				     window->rect.width,
				     window->rect.height);
	wlwin->xdg_surface = xdg_wm_base_get_xdg_surface(wldpy->wm_base,
							  wlwin->surface);
	xdg_surface_add_listener(wlwin->xdg_surface,
				 &xdg_surface_listener, window);

	wlwin->xdg_toplevel =
		xdg_surface_get_toplevel(wlwin->xdg_surface);
	xdg_toplevel_add_listener(wlwin->xdg_toplevel,
				  &xdg_toplevel_listener, window);

	xdg_toplevel_set_title(wlwin->xdg_toplevel, "simple-egl");

	wlwin->wait_for_configure = 1;
	wl_surface_commit(wlwin->surface);

	if (!wldpy->wm_base)
		return 0;

    while (wlwin->wait_for_configure)
        wl_display_roundtrip(wldpy->display);

	if (window->fullscreen)
		xdg_toplevel_set_fullscreen(wlwin->xdg_toplevel, NULL);

	window->hdl = wlwin->eglwin;

	return 0;
}

void wayland_destroy_window(struct window  *window)
{
	struct wlwin* wlwin = NULL;
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

