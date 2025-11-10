/*
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <gbm.h>

#include <yxw.h>
#include <yxw_def.h>

struct gbmdisplay {
	struct gbm_device *display;
};

static int _create_display(struct display *display)
{
	struct gbmdisplay  *ndpy;
	struct gbm_device *gbm;

	if (display == NULL) {
		return -EINVAL;
	}

	ndpy = malloc(sizeof(*ndpy));
	if (ndpy == NULL) {
		return -ENOMEM;
	}

	memset(ndpy, 0, sizeof(*ndpy));
	display->_native = ndpy;

	/* +++++++++++ native related +++++ */
	int fd = open("/dev/dri/renderD128", O_RDONLY | O_CLOEXEC);

	gbm = gbm_create_device(fd);
	
	ndpy->display = gbm;
	/* --------- Native ------ related */

	display->hdl = (size_t)ndpy->display;
	
	return 0;
}

static void _destroy_display(struct display *display)
{
	struct gbmdisplay  *ndpy;

	if (display == NULL) {
		return;
	}

	ndpy = (struct gbmdisplay *)display-> _native;
	if (ndpy == NULL) {
		return;
	}

	gbm_device_destroy(ndpy->display);

	free(ndpy);
}

/************************************ Public  Interface *************************/
static int _create_window(struct window  *window)
{
	if (window == NULL) {
		return -EINVAL;
	}

	window->_native = (size_t)0x0;

	window->hdl = 0x0;

	return 0;
}

static void _destroy_window(struct window  *window)
{
	
}


static int _pre_draw(struct window  *window)
{
	return 0;
}

static int _post_draw(struct window  *window)
{
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

struct yxw_ops _ops = {
	_create_display,
	_destroy_display,

	_create_window,
	_destroy_window,

	_pre_draw,
	_post_draw,

	running,
	quit
};