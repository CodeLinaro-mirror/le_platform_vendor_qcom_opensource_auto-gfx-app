/*
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __YXW_H__
#define __YXW_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct window;
struct display {
	void *_native;
	void *hdl;
	struct window *window;
};

struct rect {
	int x,y;
	int width, height;
};

struct window {
	struct display *display;
	size_t hdl;

	bool is_secure;
	int sync_mode;  /* 0 for normal mode, 1 for performance mode*/
	struct rect rect;
	int fullscreen, opaque, buffer_size;

	void *_native;
};

enum yxw_type {
	YXW_NULL = 0,
	YXW_WAYLAND,
	YXW_KMS,
	YXW_MAX
};

int yxw_reg_win (enum yxw_type type);

int yxw_init_display(struct display *display, struct window  *window);
void yxw_destroy_display(struct display *display);

int yxw_init_window(struct window  *window);
void yxw_destroy_window(struct window  *window);

int yxw_pre_draw(struct window  *window);
int yxw_post_draw(struct window  *window);

enum yxw_type yxw_get_win_type();

int yxw_running();
void yxw_quit();

#ifdef __cplusplus
}
#endif

#endif /*__YXW_H__*/
