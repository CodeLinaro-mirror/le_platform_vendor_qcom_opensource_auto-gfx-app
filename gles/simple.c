
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <sys/types.h>
#include <unistd.h>

#include "yxw.h"

#ifdef HAVE_BOOTKPI_H
#include <bootkpi/logging.h>
#define BOOTKPI_INIT bootkpi_log_init
#define BOOTKPI_LOG bootkpi_log_line
#else
#define BOOTKPI_INIT() ((void)0)
#define BOOTKPI_LOG printf
#endif

struct egl {
	EGLenum		platform;
	EGLDisplay 	dpy;
	EGLContext 	ctx;
	EGLConfig 	conf;
	EGLSurface egl_surface;
};

struct gl{
	GLuint rotation_uniform;
	GLuint pos;
	GLuint col;
};

static const char *vert_shader_text =
	"uniform mat4 rotation;\n"
	"attribute vec4 pos;\n"
	"attribute vec4 color;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_Position = rotation * pos;\n"
	"  v_color = color;\n"
	"}\n";

static const char *frag_shader_text =
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_FragColor = v_color;\n"
	"}\n";

static const uint32_t speed_div = 5, benchmark_interval = 5;

static GLuint create_shader(const char *source, GLenum shader_type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling %s: %.*s\n",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len, log);
		exit(1);
	}

	return shader;
}

void init_egl(struct display *display, struct window *window, struct egl *egl, int secure)
{
	int i = 0;
	EGLint context_attribs[32];

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	context_attribs[i++] = EGL_CONTEXT_CLIENT_VERSION;
	context_attribs[i++] = 2;

	if (secure == 1) {
		context_attribs[i++] = EGL_PROTECTED_CONTENT_EXT;
		context_attribs[i++] = EGL_TRUE;
	}

	context_attribs[i++] = EGL_NONE;
	context_attribs[i++] = EGL_NONE;

	EGLint major, minor, n, count, size;
	EGLConfig *configs;
	EGLBoolean ret;

	if ((display == NULL) || (window == NULL) || (egl == NULL))
		return;

	egl->platform = EGL_NO_DISPLAY ;

	if (yxw_get_win_type() == YXW_WAYLAND) {
		egl->platform = EGL_PLATFORM_WAYLAND_KHR;
	} else if ((yxw_get_win_type() == YXW_NULL) ||
					(yxw_get_win_type() == YXW_KMS)
				) {
		egl->platform = EGL_PLATFORM_GBM_KHR;
	}

	yxw_init_display(display, window);

	if (window->opaque || window->buffer_size == 16)
		config_attribs[9] = 0;

	egl->dpy = eglGetPlatformDisplay(egl->platform, 
							(EGLNativeDisplayType)display->hdl, NULL);
	assert(egl->dpy);

	ret = eglInitialize(egl->dpy, &major, &minor);
	if (!ret) {
		EGLint err = eglGetError();
		fprintf(stderr, "eglInitialize failed: 0x%04x\n", err);
		assert(0);
	}

	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	if (!eglGetConfigs(egl->dpy, NULL, 0, &count) || count < 1)
		assert(0);

	configs = calloc(count, sizeof *configs);
	assert(configs);
	ret = eglChooseConfig(egl->dpy, config_attribs,
			      configs, count, &n);
	assert(ret && n >= 1);
	for (i = 0; i < n; i++) {
		eglGetConfigAttrib(egl->dpy,
				   configs[i], EGL_BUFFER_SIZE, &size);
		if (window->buffer_size == size) {
			egl->conf = configs[i];
			break;
		}
	}
	free(configs);

	if (egl->conf == NULL) {
		fprintf(stderr, "did not find config with buffer size %d\n",
			window->buffer_size);
		exit(-1);
	}

	egl->ctx = eglCreateContext(egl->dpy,
					    egl->conf,
					    EGL_NO_CONTEXT,
						context_attribs);
	assert(egl->ctx);
}

void fini_egl(struct display *display, struct egl *egl)
{
	if ((display == NULL) || (egl == NULL))
		return;

	eglTerminate(egl->dpy);
	eglReleaseThread();

	yxw_destroy_display(display);
}

static void init_gl(struct gl *gl)
{
	GLuint frag, vert;
	GLuint program;
	GLint status;
	EGLBoolean ret;
	EGLint attribs[5] = { EGL_NONE };
	uint32_t num_attribs = 0;

	frag = create_shader(frag_shader_text, GL_FRAGMENT_SHADER);
	vert = create_shader(vert_shader_text, GL_VERTEX_SHADER);

	program = glCreateProgram();
	glAttachShader(program, frag);
	glAttachShader(program, vert);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%.*s\n", len, log);
		exit(1);
	}

	glUseProgram(program);

	gl->pos = 0;
	gl->col = 1;

	glBindAttribLocation(program, gl->pos, "pos");
	glBindAttribLocation(program, gl->col, "color");

	gl->rotation_uniform =
		glGetUniformLocation(program, "rotation");
}

void create_surface(struct window *window, struct egl *egl, int secure)
{
	int i = 0;
	EGLBoolean ret;
	struct display *display = NULL;
	EGLAttrib attribs[12];

	if (secure) {
		attribs[i++] = EGL_PROTECTED_CONTENT_EXT;
		attribs[i++] = EGL_TRUE;
	}

	attribs[i++] = EGL_NONE;

	if ((window == NULL) || (egl == NULL))
		return;

	if (yxw_init_window(window))
		return;

	display = window->display;
	egl->egl_surface = eglCreatePlatformWindowSurface(egl->dpy,
										egl->conf, window->hdl,
										attribs);

	if (window->sync_mode == 1) {
		eglSwapInterval(egl->dpy, 0);
	}

	//if (secure != 1)
	assert(eglMakeCurrent(egl->dpy,
				egl->egl_surface,
				egl->egl_surface,
				egl->ctx)== EGL_TRUE);

}

void create_psurface(struct window *window, struct egl *egl, int w, int h)
{
	EGLBoolean ret;
	struct display *display = NULL;

    EGLint surfaceAttrib[] =
    {
        EGL_WIDTH,  w,
        EGL_HEIGHT, h,
        EGL_NONE
    };

	if ((window == NULL) || (egl == NULL))
		return;

	display = window->display;

	if (yxw_init_window(window))
		return;

	egl->egl_surface = eglCreatePbufferSurface(egl->dpy, egl->conf, surfaceAttrib);

	if (window->sync_mode == 1) {
		eglSwapInterval(egl->dpy, 0);
	}

	assert(eglMakeCurrent(egl->dpy,
				egl->egl_surface,
				egl->egl_surface,
				egl->ctx)== EGL_TRUE);

}

void destroy_surface(struct window *window, struct egl *egl)
{
	if ((window == NULL) || (egl == NULL))
		return;

	eglMakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	eglDestroySurface(egl->dpy, egl->egl_surface);

	yxw_destroy_window(window);
}

static void draw_triangle(struct window *window, struct gl *gl)
{
	static const GLfloat verts[3][2] = {
		{ -0.5, -0.5 },
		{  0.5, -0.5 },
		{  0,    0.5 }
	};
	static const GLfloat colors[3][3] = {
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 }
	};

	glVertexAttribPointer(gl->pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(gl->col, 3, GL_FLOAT, GL_FALSE, 0, colors);
	glEnableVertexAttribArray(gl->pos);
	glEnableVertexAttribArray(gl->col);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(gl->pos);
	glDisableVertexAttribArray(gl->col);
}

static void redraw(struct window *window, struct gl *gl, uint32_t time)
{
	GLfloat angle;

	GLfloat mvp[4][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};

	angle = (time / speed_div)
			% 360 * M_PI / 180.0;

	mvp[0][0] =  cos(angle);
	mvp[2][0] =  sin(angle);
	mvp[0][2] = -sin(angle);
	mvp[2][2] =  cos(angle);

	glViewport(0, 0, window->rect.width, window->rect.height);

	glUniformMatrix4fv(gl->rotation_uniform, 1, GL_FALSE,
			   (GLfloat *) mvp);

	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT);

	draw_triangle(window, gl);
}

static void
signal_int(int signum)
{
	yxw_quit();
}

void help(char *app, int error_code)
{
	fprintf(stderr, "Usage: %s <Options>\n\n"
		"  -dly <us>\tBuffer swap delay in microseconds\n"
		"  -f\t\tRun in fullscreen mode\n"
		"  -opq\t\tCreate an opaque surface\n"
		"  -size\t\tUse a 16 bpp EGL config\n"
		"  -burst\tDon't sync to compositor redraw (eglSwapInterval 0)\n"
		"  -wm\t\tWindowing mode, 0: offscreen mode, 1: weston client(default), 2: kms\n"
		"  -secure\tSecure rendering mode, 0, disabled(default), 1 Enabled\n"
		"  -frames <num>\t Frames to be shown"
		"  -h\t\tThis help text\n\n", app);

	exit(error_code);
}

int main(int argc, char **argv)
{
	struct sigaction sigint;
	int i, ret = 0;
	struct display display = { 0 };
	struct window  window  = { 0 };
	int window_mode = YXW_WAYLAND;  // 0 is offscreen
	int one_frame_done = 0;
	int delay = 0;
	int is_secure = 0;
	uint32_t time, initial_frame_time = 0, benchmark_time = 0;
	uint32_t frames = 0, total_frames = 0, max_frames = 0;
	struct timeval tv;

	window.rect.width  = 1920;
	window.rect.height = 1080;
	window.buffer_size = 32;
	struct egl egl = {0};
	struct gl gl = {0};

	for (i = 1; i < argc; i++) {
		if (strcmp("-dly", argv[i]) == 0 && i+1 < argc)
			delay = atoi(argv[++i]);
		else if (strcmp("-wm", argv[i]) == 0)
			window_mode = atoi(argv[++i]);
		else if (strcmp("-b", argv[i]) == 0)
			window.sync_mode = 1;
		else if (strcmp("-frames", argv[i]) == 0 && i+1 < argc)
			max_frames = atoi(argv[++i]);
		else if (strcmp("-h", argv[i]) == 0)
			help(argv[0], EXIT_SUCCESS);
		else
			help(argv[0], EXIT_FAILURE);
	}
	BOOTKPI_INIT();
	BOOTKPI_LOG("qcom-bootkpi:GFX:%s, Started, with mode:%d\n", argv[0], window_mode);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	yxw_reg_win(window_mode);
	init_egl(&display, &window, &egl, is_secure);
	create_surface(&window, &egl, is_secure);
	init_gl(&gl);

	BOOTKPI_LOG("qcom-bootkpi:GFX:%s, Draw frames started.\n", argv[0]);
	while (yxw_running() && ret >= -1) {
		gettimeofday(&tv, NULL);
		time = tv.tv_sec * 1000 + tv.tv_usec / 1000;\
		if (frames == 0) {
			initial_frame_time = time;
			benchmark_time = time;
		}

		ret = yxw_pre_draw(&window);
		redraw(&window, &gl, (time - initial_frame_time));
		eglSwapBuffers(egl.dpy, egl.egl_surface);
		yxw_post_draw(&window);

		if (delay)
			usleep(delay);

		if (one_frame_done == 0) {
			one_frame_done = 1;
			BOOTKPI_LOG("qcom-bootkpi:GFX:%s, First frame render done.\n", argv[0]);
		}

		if (time - benchmark_time > (benchmark_interval * 1000)) {
			printf("%d frames in %d seconds: %f fps\n",
				frames,
				benchmark_interval,
				(float) frames / benchmark_interval);
			benchmark_time = time;
			frames = 0;
		}

		frames++;
		total_frames++;
		if ((max_frames > 0) && (total_frames >= max_frames)) {
			yxw_quit();
			break;
		}
	}

	BOOTKPI_LOG("%d frames rendered, exiting %s \n", total_frames, argv[0]);

	destroy_surface(&window, &egl);
	fini_egl(&display, &egl);

	return 0;
}
