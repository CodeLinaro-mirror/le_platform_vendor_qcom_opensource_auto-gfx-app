/*
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "yxw.h"
#include "yxw_def.h"

struct window;
static struct yxw_ops *_ops_ = NULL;
static enum yxw_type win_type = YXW_NULL;

int yxw_reg_win (enum yxw_type type)
{
	if (type >= YXW_MAX)
		return -1;

	win_type = type;

	if (type == YXW_WAYLAND) {
		extern struct yxw_ops wayland_ops;
		_ops_ = &wayland_ops;
	} else if (type == YXW_KMS) {
		extern struct yxw_ops kms_ops;
		_ops_ = &kms_ops;
	} else {
		extern struct yxw_ops _ops;
		_ops_ = &_ops;
	}

	return 0;
}

enum yxw_type yxw_get_win_type()
{
	return win_type;
}

int yxw_init_display(struct display *display, struct window  *window)
{
	int ret = -EINVAL;

	if (display == NULL || window == NULL)
		return -EINVAL;

	display->window = window;
	window->display = display;

	if(_ops_ && _ops_->create_display) {
		ret = _ops_->create_display(display);
	}

	return ret;
}

void yxw_destroy_display(struct display *display)
{
	if (display == NULL)
		return;

	if(_ops_ && _ops_->destroy_display)
		_ops_->destroy_display(display);
}

int yxw_init_window(struct window  *window)
{
	if (window == NULL)
		return EINVAL;

	if(_ops_ && _ops_->create_window)
		return _ops_->create_window(window);

	return EINVAL;
}

void yxw_destroy_window(struct window  *window)
{
	if (window != NULL) {
		if(_ops_ && _ops_->create_window)
			_ops_->create_window(window);
	}
}

int yxw_pre_draw(struct window  *window)
{
	if (window != NULL) {
		if(_ops_ && _ops_->pre_draw)
			return _ops_->pre_draw(window);
	}

	return EINVAL;
}

int yxw_post_draw(struct window  *window)
{
	if (window != NULL) {
		if(_ops_ && _ops_->post_draw)
			return _ops_->post_draw(window);
	}

	return EINVAL;
}

int yxw_running()
{
	if(_ops_ && _ops_->running)
		return _ops_->running();
	else
		return 1;
}

void yxw_quit()
{
	if(_ops_ && _ops_->quit)
		_ops_->quit();
}
