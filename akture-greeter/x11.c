/*-
 * Copyright (c) 2026 Serusion
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <xcb/randr.h>
#include <xcb/shm.h>
#include <xcb/xcb.h>

#include "greeter.h"

/*
 * The X side of the greeter: one override-redirect window over the
 * whole screen, a frame buffer in shared memory and the outputs from
 * RandR.  Only MIT-SHM PutImage is used to draw; no core drawing
 * requests and no Render.
 */

#define XC_LEFT_PTR		68	/* from <X11/cursorfont.h> */
#define X11_GRAB_TRIES		20
#define X11_GRAB_DELAY		50000	/* microseconds */

static xcb_screen_t *
x11_screen(xcb_connection_t *conn, int n)
{
	xcb_screen_iterator_t it;

	for (it = xcb_setup_roots_iterator(xcb_get_setup(conn)); it.rem;
	    xcb_screen_next(&it)) {
		if (n-- == 0)
			return it.data;
	}
	return NULL;
}

static int
x11_check_visual(struct greeter *g)
{
	const xcb_setup_t *setup;
	xcb_format_iterator_t fi;
	xcb_depth_iterator_t di;
	xcb_visualtype_iterator_t vi;
	xcb_screen_t *s = g->g_x.x_screen;
	int bpp;

	setup = xcb_get_setup(g->g_x.x_conn);
	if (setup->image_byte_order != XCB_IMAGE_ORDER_LSB_FIRST ||
	    s->root_depth != 24)
		goto bad;
	bpp = 0;
	for (fi = xcb_setup_pixmap_formats_iterator(setup); fi.rem;
	    xcb_format_next(&fi)) {
		if (fi.data->depth == 24)
			bpp = fi.data->bits_per_pixel;
	}
	if (bpp != 32)
		goto bad;
	for (di = xcb_screen_allowed_depths_iterator(s); di.rem;
	    xcb_depth_next(&di)) {
		for (vi = xcb_depth_visuals_iterator(di.data); vi.rem;
		    xcb_visualtype_next(&vi)) {
			if (vi.data->visual_id == s->root_visual &&
			    vi.data->_class == XCB_VISUAL_CLASS_TRUE_COLOR &&
			    vi.data->red_mask == 0xff0000 &&
			    vi.data->green_mask == 0x00ff00 &&
			    vi.data->blue_mask == 0x0000ff)
				return 0;
		}
	}
bad:
	syslog(LOG_ERR, "need a 24-bit TrueColor visual in 32-bit pixels");
	return -1;
}

/*
 * (Re)allocate the shared frame buffer for the current screen size.
 */
static int
x11_alloc(struct greeter *g)
{
	struct x11 *x = &g->g_x;
	xcb_generic_error_t *err;
	size_t size;
	int id;

	if (x->x_frame != NULL) {
		pixman_image_unref(x->x_frame);
		xcb_shm_detach(x->x_conn, x->x_shmseg);
		(void)shmdt(x->x_shmaddr);
		x->x_frame = NULL;
	}

	size = (size_t)x->x_width * (size_t)x->x_height * 4;
	if ((id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600)) == -1) {
		syslog(LOG_ERR, "shmget: %m");
		return -1;
	}
	x->x_shmaddr = shmat(id, NULL, 0);
	if (x->x_shmaddr == (void *)-1) {
		syslog(LOG_ERR, "shmat: %m");
		(void)shmctl(id, IPC_RMID, NULL);
		return -1;
	}
	x->x_shmseg = xcb_generate_id(x->x_conn);
	err = xcb_request_check(x->x_conn, xcb_shm_attach_checked(x->x_conn,
	    x->x_shmseg, (uint32_t)id, 0));
	/* Both sides are attached (or failed); nobody else may attach. */
	(void)shmctl(id, IPC_RMID, NULL);
	if (err != NULL) {
		free(err);
		syslog(LOG_ERR, "MIT-SHM attach failed");
		(void)shmdt(x->x_shmaddr);
		return -1;
	}
	x->x_frame = pixman_image_create_bits(PIXMAN_x8r8g8b8, x->x_width,
	    x->x_height, x->x_shmaddr, x->x_width * 4);
	return x->x_frame != NULL ? 0 : -1;
}

static void
x11_add_output(struct x11 *x, int ox, int oy, int w, int h, int primary)
{
	struct output *o;
	int i;

	if (w <= 0 || h <= 0)
		return;
	for (i = 0; i < x->x_noutputs; i++) {
		o = &x->x_outputs[i];
		if (o->o_rect.r_x == ox && o->o_rect.r_y == oy &&
		    o->o_rect.r_w == w && o->o_rect.r_h == h) {
			/* A mirror of an output we have. */
			if (primary)
				x->x_primary = i;
			return;
		}
	}
	if (x->x_noutputs == G_MAXOUTPUTS)
		return;
	o = &x->x_outputs[x->x_noutputs];
	(void)memset(o, 0, sizeof(*o));
	o->o_rect.r_x = ox;
	o->o_rect.r_y = oy;
	o->o_rect.r_w = w;
	o->o_rect.r_h = h;
	if (primary)
		x->x_primary = x->x_noutputs;
	x->x_noutputs++;
}

/*
 * Find the active outputs.  Without RandR the screen is one output.
 */
static void
x11_outputs(struct greeter *g)
{
	struct x11 *x = &g->g_x;
	xcb_randr_get_screen_resources_current_reply_t *res;
	xcb_randr_get_output_primary_reply_t *pri;
	xcb_randr_get_output_info_reply_t *oi;
	xcb_randr_get_crtc_info_reply_t *ci;
	xcb_randr_output_t *outs, primary;
	int i, n;

	for (i = 0; i < x->x_noutputs; i++)
		bg_free(&x->x_outputs[i]);
	x->x_noutputs = 0;
	x->x_primary = 0;

	if (x->x_randr_base != 0 && (res =
	    xcb_randr_get_screen_resources_current_reply(x->x_conn,
	    xcb_randr_get_screen_resources_current(x->x_conn,
	    x->x_screen->root), NULL)) != NULL) {
		primary = XCB_NONE;
		pri = xcb_randr_get_output_primary_reply(x->x_conn,
		    xcb_randr_get_output_primary(x->x_conn, x->x_screen->root),
		    NULL);
		if (pri != NULL) {
			primary = pri->output;
			free(pri);
		}
		outs = xcb_randr_get_screen_resources_current_outputs(res);
		n = xcb_randr_get_screen_resources_current_outputs_length(res);
		for (i = 0; i < n; i++) {
			oi = xcb_randr_get_output_info_reply(x->x_conn,
			    xcb_randr_get_output_info(x->x_conn, outs[i],
			    res->config_timestamp), NULL);
			if (oi == NULL)
				continue;
			if (oi->connection == XCB_RANDR_CONNECTION_CONNECTED &&
			    oi->crtc != XCB_NONE && (ci =
			    xcb_randr_get_crtc_info_reply(x->x_conn,
			    xcb_randr_get_crtc_info(x->x_conn, oi->crtc,
			    res->config_timestamp), NULL)) != NULL) {
				x11_add_output(x, ci->x, ci->y, ci->width,
				    ci->height, outs[i] == primary);
				free(ci);
			}
			free(oi);
		}
		free(res);
	}
	if (x->x_noutputs == 0)
		x11_add_output(x, 0, 0, x->x_width, x->x_height, 1);

	for (i = 0; i < x->x_noutputs; i++)
		bg_load(g, &x->x_outputs[i]);
}

static void
x11_grab(struct greeter *g)
{
	struct x11 *x = &g->g_x;
	xcb_grab_keyboard_reply_t *r;
	int i, status;

	for (i = 0; i < X11_GRAB_TRIES; i++) {
		r = xcb_grab_keyboard_reply(x->x_conn, xcb_grab_keyboard(
		    x->x_conn, 1, x->x_win, XCB_CURRENT_TIME,
		    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC), NULL);
		status = r != NULL ? r->status : -1;
		free(r);
		if (status == XCB_GRAB_STATUS_SUCCESS)
			return;
		(void)usleep(X11_GRAB_DELAY);
	}
	syslog(LOG_WARNING, "cannot grab the keyboard");
}

int
x11_init(struct greeter *g)
{
	struct x11 *x = &g->g_x;
	const xcb_query_extension_reply_t *ext;
	xcb_shm_query_version_reply_t *shm;
	xcb_font_t font;
	xcb_cursor_t cursor;
	uint32_t mask, values[4];
	int scr;

	x->x_conn = xcb_connect(NULL, &scr);
	if (xcb_connection_has_error(x->x_conn)) {
		syslog(LOG_ERR, "cannot connect to the X server");
		return -1;
	}
	x->x_screen = x11_screen(x->x_conn, scr);
	if (x->x_screen == NULL || x11_check_visual(g) == -1)
		return -1;
	x->x_width = x->x_screen->width_in_pixels;
	x->x_height = x->x_screen->height_in_pixels;

	shm = xcb_shm_query_version_reply(x->x_conn,
	    xcb_shm_query_version(x->x_conn), NULL);
	if (shm == NULL) {
		syslog(LOG_ERR, "no MIT-SHM extension");
		return -1;
	}
	free(shm);
	ext = xcb_get_extension_data(x->x_conn, &xcb_randr_id);
	if (ext != NULL && ext->present)
		x->x_randr_base = ext->first_event;

	/* The standard arrow from the core cursor font. */
	font = xcb_generate_id(x->x_conn);
	xcb_open_font(x->x_conn, font, strlen("cursor"), "cursor");
	cursor = xcb_generate_id(x->x_conn);
	xcb_create_glyph_cursor(x->x_conn, cursor, font, font, XC_LEFT_PTR,
	    XC_LEFT_PTR + 1, 0, 0, 0, 0xffff, 0xffff, 0xffff);
	xcb_close_font(x->x_conn, font);

	x->x_win = xcb_generate_id(x->x_conn);
	mask = XCB_CW_BACK_PIXMAP | XCB_CW_OVERRIDE_REDIRECT |
	    XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	values[0] = XCB_BACK_PIXMAP_NONE;
	values[1] = 1;
	values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS |
	    XCB_EVENT_MASK_BUTTON_PRESS;
	values[3] = cursor;
	xcb_create_window(x->x_conn, XCB_COPY_FROM_PARENT, x->x_win,
	    x->x_screen->root, 0, 0, (uint16_t)x->x_width,
	    (uint16_t)x->x_height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
	    x->x_screen->root_visual, mask, values);
	xcb_free_cursor(x->x_conn, cursor);

	x->x_gc = xcb_generate_id(x->x_conn);
	values[0] = 0;
	xcb_create_gc(x->x_conn, x->x_gc, x->x_win,
	    XCB_GC_GRAPHICS_EXPOSURES, values);
	if (x->x_randr_base != 0)
		xcb_randr_select_input(x->x_conn, x->x_screen->root,
		    XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);

	pixman_region32_init(&x->x_damage);
	if (x11_alloc(g) == -1)
		return -1;
	x11_outputs(g);

	xcb_map_window(x->x_conn, x->x_win);
	xcb_set_input_focus(x->x_conn, XCB_INPUT_FOCUS_POINTER_ROOT,
	    x->x_win, XCB_CURRENT_TIME);
	x11_grab(g);
	return xcb_flush(x->x_conn) > 0 ? 0 : -1;
}

/*
 * The screen changed size or outputs: start over with a new buffer.
 */
static void
x11_reconfigure(struct greeter *g, int w, int h)
{
	struct x11 *x = &g->g_x;
	uint32_t values[2];

	x->x_width = w;
	x->x_height = h;
	values[0] = (uint32_t)w;
	values[1] = (uint32_t)h;
	xcb_configure_window(x->x_conn, x->x_win,
	    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
	if (x11_alloc(g) == -1)
		exit(AKTURE_EXIT_ERROR);
	x11_outputs(g);
	ui_layout(g);
	ui_damage_all(g);
}

void
x11_dispatch(struct greeter *g, xcb_generic_event_t *ev)
{
	struct x11 *x = &g->g_x;
	const xcb_expose_event_t *ex;
	const xcb_button_press_event_t *bp;
	const xcb_randr_screen_change_notify_event_t *sc;
	struct rect r;
	uint8_t type;

	type = ev->response_type & 0x7f;
	if (type == g->g_kbd.k_xkb_base) {
		kbd_event(g, ev);
		return;
	}
	if (x->x_randr_base != 0 &&
	    type == x->x_randr_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
		sc = (const xcb_randr_screen_change_notify_event_t *)ev;
		if (sc->rotation & (XCB_RANDR_ROTATION_ROTATE_90 |
		    XCB_RANDR_ROTATION_ROTATE_270))
			x11_reconfigure(g, sc->height, sc->width);
		else
			x11_reconfigure(g, sc->width, sc->height);
		return;
	}

	switch (type) {
	case XCB_EXPOSE:
		ex = (const xcb_expose_event_t *)ev;
		r.r_x = ex->x;
		r.r_y = ex->y;
		r.r_w = ex->width;
		r.r_h = ex->height;
		x11_damage(g, &r);
		break;
	case XCB_KEY_PRESS:
		kbd_key(g, (const xcb_key_press_event_t *)ev);
		break;
	case XCB_BUTTON_PRESS:
		bp = (const xcb_button_press_event_t *)ev;
		if (bp->detail == XCB_BUTTON_INDEX_1)
			ui_click(g, bp->event_x, bp->event_y);
		break;
	case 0:
		syslog(LOG_WARNING, "X error %u",
		    ((xcb_generic_error_t *)ev)->error_code);
		break;
	default:
		break;
	}
}

void
x11_damage(struct greeter *g, const struct rect *r)
{

	if (r->r_w <= 0 || r->r_h <= 0)
		return;
	pixman_region32_union_rect(&g->g_x.x_damage, &g->g_x.x_damage,
	    r->r_x, r->r_y, (unsigned int)r->r_w, (unsigned int)r->r_h);
}

/*
 * Send the damaged parts of the frame to the server and wait until it
 * has taken them, so that the next paint cannot race with the copy.
 */
void
x11_present(struct greeter *g)
{
	struct x11 *x = &g->g_x;
	pixman_box32_t *b;
	int i, n;

	pixman_region32_intersect_rect(&x->x_damage, &x->x_damage, 0, 0,
	    (unsigned int)x->x_width, (unsigned int)x->x_height);
	b = pixman_region32_rectangles(&x->x_damage, &n);
	for (i = 0; i < n; i++) {
		xcb_shm_put_image(x->x_conn, x->x_win, x->x_gc,
		    (uint16_t)x->x_width, (uint16_t)x->x_height,
		    (uint16_t)b[i].x1, (uint16_t)b[i].y1,
		    (uint16_t)(b[i].x2 - b[i].x1),
		    (uint16_t)(b[i].y2 - b[i].y1),
		    (int16_t)b[i].x1, (int16_t)b[i].y1, 24,
		    XCB_IMAGE_FORMAT_Z_PIXMAP, 0, x->x_shmseg, 0);
	}
	free(xcb_get_input_focus_reply(x->x_conn,
	    xcb_get_input_focus(x->x_conn), NULL));
	pixman_region32_clear(&x->x_damage);
}

const struct output *
x11_primary(const struct greeter *g)
{

	return &g->g_x.x_outputs[g->g_x.x_primary];
}
