
/*
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __YXW_DEF_H__
#define __YXW_DEF_H__

#include "yxw.h"

struct yxw_ops {
	int (*create_display)(struct display *display);
	void (*destroy_display)(struct display *display);

	int (*create_window)(struct window  *window);
	void (*destroy_window)(struct window  *window);

	int (*pre_draw)(struct window  *window);
	int (*post_draw)(struct window  *window);

	int (*running)(void);
	void (*quit)(void);
};

#endif /*__YXW_DEF_H__*/
