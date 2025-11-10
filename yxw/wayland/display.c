
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

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface,
		     wl_fixed_t sx, wl_fixed_t sy)
{
	struct wldisplay  *wldpy = NULL;
	struct display *display = data;
	struct wl_buffer *buffer;
	struct wl_cursor *cursor = NULL;
	struct wl_cursor_image *image;

	if (display == NULL) {
		return;
	}
	wldpy = (struct wldisplay *)display-> _native;
	if (wldpy == NULL) {
		return;
	}

	cursor = wldpy->default_cursor;

	if (display->window->fullscreen)
		wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
	else if (cursor) {
		image = wldpy->default_cursor->images[0];
		buffer = wl_cursor_image_get_buffer(image);
		if (!buffer)
			return;
		wl_pointer_set_cursor(pointer, serial,
				      wldpy->cursor_surface,
				      image->hotspot_x,
				      image->hotspot_y);
		wl_surface_attach(wldpy->cursor_surface, buffer, 0, 0);
		wl_surface_damage(wldpy->cursor_surface, 0, 0,
				  image->width, image->height);
		wl_surface_commit(wldpy->cursor_surface);
	}
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
	struct wldisplay  *wldpy = NULL;
	struct wlwin *wlwin = NULL;
	struct display *display = data;
	struct window *window = NULL;

	if (display == NULL || window == NULL) {
		return;
	}

	wldpy = (struct wldisplay *)display-> _native;
	if (wldpy == NULL) {
		return;
	}

	window = display->window;
	wlwin = (struct wlwin *)window-> _native;
	if (wlwin == NULL) {
		return;
	}

	if (!wlwin->xdg_toplevel)
		return;

	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
		xdg_toplevel_move(wlwin->xdg_toplevel,
				  wldpy->seat, serial);
}

static void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static void touch_handle_down(void *data, struct wl_touch *wl_touch,
		  uint32_t serial, uint32_t time, struct wl_surface *surface,
		  int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct display *display = (struct display *)data;
	struct window *window = NULL;
	struct wldisplay  *wldpy = NULL;
	struct wlwin *wlwin = NULL;

	if ((display == NULL) || (display->window == NULL) ||
			(display-> _native == 0) || display->window->_native == 0) {
		return;
	}

	wldpy = (struct wldisplay *)display-> _native;
	window = display->window;
	wlwin = (struct wlwin *)window-> _native;

	if (!wldpy->wm_base)
		return;

	xdg_toplevel_move(wlwin->xdg_toplevel, wldpy->seat, serial);
}

static void touch_handle_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id)
{
}

static void touch_handle_motion(void *data, struct wl_touch *wl_touch,
		    uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
}

static void touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
}

static void touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
}

static const struct wl_touch_listener touch_listener = {
	touch_handle_down,
	touch_handle_up,
	touch_handle_motion,
	touch_handle_frame,
	touch_handle_cancel,
};

static void keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
	/* Just so we don’t leak the keymap fd */
	close(fd);
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
}

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct display *display = data;
	struct window *window = NULL;
	struct wldisplay  *wldpy = NULL;
	struct wlwin *wlwin = NULL;

	if ((display == NULL) || (display->window == NULL) ||
			(display-> _native == 0) || display->window->_native == 0) {
		return;
	}

	wldpy = (struct wldisplay *)display-> _native;
	window = display->window;
	wlwin = (struct wlwin *)window-> _native;

	if (!wldpy->wm_base)
		return;

	if (key == KEY_F11 && state) {
		if (window->fullscreen)
			xdg_toplevel_unset_fullscreen(wlwin->xdg_toplevel);
		else
			xdg_toplevel_set_fullscreen(wlwin->xdg_toplevel, NULL);
	} else if (key == KEY_ESC && state)
		yxw_quit();
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
			 enum wl_seat_capability caps)
{
	struct display *display = data;
	struct window *window = NULL;
	struct wldisplay  *wldpy = NULL;
	struct wlwin *wlwin = NULL;

	if ((display == NULL) || (display->window == NULL) ||
			(display-> _native == 0) || display->window->_native == 0) {
		return;
	}

	wldpy = (struct wldisplay *)display-> _native;
	window = display->window;
	wlwin = (struct wlwin *)window-> _native;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wldpy->pointer) {
		wldpy->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(wldpy->pointer, &pointer_listener, display);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wldpy->pointer) {
		wl_pointer_destroy(wldpy->pointer);
		wldpy->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wldpy->keyboard) {
		wldpy->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(wldpy->keyboard, &keyboard_listener, display);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wldpy->keyboard) {
		wl_keyboard_destroy(wldpy->keyboard);
		wldpy->keyboard = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !wldpy->touch) {
		wldpy->touch = wl_seat_get_touch(seat);
		wl_touch_set_user_data(wldpy->touch, display);
		wl_touch_add_listener(wldpy->touch, &touch_listener, display);
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && wldpy->touch) {
		wl_touch_destroy(wldpy->touch);
		wldpy->touch = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
	xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	xdg_wm_base_ping,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	struct display *display = data;
	struct wldisplay  *wldpy;

	if ((display == NULL) || (display-> _native == 0)) {
		return;
	}
	wldpy = (struct wldisplay *)display-> _native;

	if (strcmp(interface, "wl_compositor") == 0) {
		wldpy->compositor =
			wl_registry_bind(registry, name,
					 &wl_compositor_interface,
					 (version < 4? version:4));
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		wldpy->wm_base = wl_registry_bind(registry, name,
					      &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(wldpy->wm_base, &wm_base_listener, display);
	} else if (strcmp(interface, "wl_seat") == 0) {
		wldpy->seat = wl_registry_bind(registry, name,
					   &wl_seat_interface, 1);
		wl_seat_add_listener(wldpy->seat, &seat_listener, display);
	} else if (strcmp(interface, "wl_shm") == 0) {
		wldpy->shm = wl_registry_bind(registry, name,
					  &wl_shm_interface, 1);
		wldpy->cursor_theme = wl_cursor_theme_load(NULL, 32, wldpy->shm);
		if (!wldpy->cursor_theme) {
			fprintf(stderr, "unable to load default theme\n");
			return;
		}
		wldpy->default_cursor =
			wl_cursor_theme_get_cursor(wldpy->cursor_theme, "left_ptr");
		if (!wldpy->default_cursor) {
			fprintf(stderr, "unable to load default left pointer\n");
			// TODO: abort ?
		}
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

/************************************ Public  Interface *************************/
int wayland_create_display(struct display *display)
{
	struct wldisplay  *wldpy;

	if (display == NULL) {
		return -EINVAL;
	}

	wldpy = malloc(sizeof(*wldpy));
	if (wldpy == NULL) {
		return -ENOMEM;
	}

	memset(wldpy, 0, sizeof(*wldpy));
	wldpy->display = wl_display_connect(NULL);
	if(wldpy->display == NULL) {
		free(wldpy);
		return -ECONNREFUSED;
	}

	display-> _native = wldpy;
	wldpy->registry = wl_display_get_registry(wldpy->display);
	wl_registry_add_listener(wldpy->registry,
				 &registry_listener, display);

	wl_display_roundtrip(wldpy->display);

	wldpy->cursor_surface =
		wl_compositor_create_surface(wldpy->compositor);

	display->hdl = (size_t)wldpy->display;
	
	return 0;
}
