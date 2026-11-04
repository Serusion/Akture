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
#include <sys/utsname.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "akutil.h"
#include "greeter.h"
#include "msgs.h"

/*
 * The login screen ("plan D"): the photo is the subject, the controls
 * sit in the lower left and the status in the upper right.
 *
 * Geometry is given at scale 1 for a 1366x768 output, measured from
 * the mock-up, and multiplied by the scale factor.  The screen is
 * drawn in blocks (clock, login, top right, logo); a change damages
 * the block it belongs to and only damaged pixels are drawn and sent.
 */

#define L_MARGIN	96	/* left edge of the clock and login block */
#define L_CLOCK_BASE	198	/* baseline of the time */
#define L_CLOCK_SIZE	112
#define L_DATE_BASE	236
#define L_DATE_SIZE	19
#define L_FIELD_BOTTOM	134	/* field bottom, up from the output bottom */
#define L_FIELD_W	360
#define L_FIELD_H	52
#define L_FIELD_PAD	25	/* text inset of the field */
#define L_FIELD_SIZE	16
#define L_DOT_R		4	/* password dots */
#define L_DOT_STEP	14
#define L_BUTTON_R	20
#define L_BUTTON_INSET	6
#define L_AVATAR_R	17
#define L_AVATAR_DY	30	/* avatar centre above the field */
#define L_AVATAR_SIZE	13
#define L_NAME_DX	49
#define L_NAME_SIZE	20
#define L_PROMPT_DY	64	/* PAM text baseline above the field */
#define L_PROMPT_SIZE	14
#define L_NOTE_DY	21	/* note baseline below the field */
#define L_NOTE_SIZE	13
#define L_NOTE_SHIFT	16	/* the session row moves down for a note */
#define L_ROW_DY	42	/* session row baseline below the field */
#define L_ROW_DX	8
#define L_ROW_SIZE	15
#define L_ROW_GAP	28
#define L_CHEVRON	17	/* room for the chevron after the session */
#define L_TOP_CY	43	/* centre line of the top right items */
#define L_RIGHT		44	/* right margin of the top right items */
#define L_POWER_R	7
#define L_PILL_H	29
#define L_PILL_PAD	12
#define L_PILL_SIZE	13
#define L_TOP_GAP	21
#define L_HOST_GAP	16
#define L_HOST_SIZE	15
#define L_LOGO_RIGHT	34
#define L_LOGO_BASE	38
#define L_LOGO_SIZE	17
#define L_STAR_R	7
#define L_MENU_ROW	34
#define L_MENU_PAD	6
#define L_MENU_INSET	16
#define L_MENU_SIZE	15
#define L_FOCUS_PAD	6
#define L_BLOCK_W	500	/* width of the right hand blocks */

#define C_FIELD		0xffede7dcU	/* kinari */
#define C_PLACEHOLDER	0xff8e857bU
#define C_INK		0xff2b2420U	/* text in the field */
#define C_BUTTON_INK	0xfffff8f0U

#define W_THIN		300
#define W_REGULAR	400
#define W_MEDIUM	500
#define W_SEMIBOLD	600
#define W_BOLD		700

static const char *const ui_weekdays[] = { MSG_WEEKDAYS };

static void	ui_menu_close(struct greeter *);

static double
px(const struct greeter *g, double v)
{

	return v * g->g_scale;
}

static void
ui_font(const struct greeter *g, struct au_font *f, int clock, double size,
    int weight)
{

	f->f_face = clock ? g->g_fclock : g->g_fui;
	f->f_fallback = clock ? g->g_fui : g->g_fclock;
	f->f_size = px(g, size);
	f->f_weight = weight;
}

/* Offset from a centre line to the baseline of vertically centred text. */
static double
ui_vcenter(const struct au_font *f)
{

	return (au_font_ascent(f) - au_font_descent(f)) / 2;
}

static int
ui_inside(const struct au_rect *r, int x, int y)
{

	return x >= r->r_x && x < r->r_x + r->r_w &&
	    y >= r->r_y && y < r->r_y + r->r_h;
}

static void
ui_setrect(struct au_rect *r, double x, double y, double w, double h)
{

	r->r_x = (int)floor(x);
	r->r_y = (int)floor(y);
	r->r_w = (int)ceil(x + w) - r->r_x;
	r->r_h = (int)ceil(y + h) - r->r_y;
}

static int
ui_power_allowed(const struct greeter *g)
{

	return (g->g_flags & AKINIT_POWER) != 0;
}

/*
 * The note under the field, if any: an error, or the Caps Lock warning
 * while a secret is typed.
 */
static const char *
ui_note(const struct greeter *g)
{

	switch (g->g_ui.u_note) {
	case UI_NOTE_BADPASS:
		return MSG_BADPASS;
	case UI_NOTE_SESSIONFAIL:
		return MSG_SESSIONFAIL;
	default:
		return g->g_kbd.k_caps && g->g_ui.u_secret ? MSG_CAPSLOCK : NULL;
	}
}

/*
 * Something in the login block changed.  The note moves the session
 * row, so the geometry is computed again; an open menu is damaged at
 * its old and its new place.
 */
static void
ui_damage_login(struct greeter *g)
{
	struct ui *u = &g->g_ui;

	if (u->u_menu != UI_MENU_NONE)
		x11_damage(g, &u->u_rmenu);
	ui_layout(g);
	x11_damage(g, &u->u_blogin);
	if (u->u_menu != UI_MENU_NONE)
		x11_damage(g, &u->u_rmenu);
}

void
ui_damage_all(struct greeter *g)
{
	struct au_rect r;

	r.r_x = r.r_y = 0;
	r.r_w = g->g_x.x_width;
	r.r_h = g->g_x.x_height;
	x11_damage(g, &r);
}

/*
 * Erase the input field.  Called on every path that leaves it: send,
 * cancel, failure, Esc, Ctrl+U and mode changes.
 */
static void
ui_clear_field(struct ui *u)
{

	ak_explicit_zero(u->u_field, sizeof(u->u_field));
	u->u_fieldlen = 0;
}

static void
ui_send(struct greeter *g, struct akmsg *m)
{

	if (ipc_send(g, m) == -1)
		exit(AKTURE_EXIT_ERROR);
}

/*
 * Update the clock strings.  Returns 1 if they changed.
 */
static int
ui_clock(struct greeter *g)
{
	struct ui *u = &g->g_ui;
	char clk[sizeof(u->u_clock)], date[sizeof(u->u_date)];
	struct tm tm;
	time_t now;

	now = time(NULL);
	if (localtime_r(&now, &tm) == NULL)
		return 0;
	/* The format is clock_format of akture.conf(5), checked there. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	if (strftime(clk, sizeof(clk), g->g_opt[AKOPT_CLOCK_FORMAT], &tm) ==
	    0)
		clk[0] = '\0';
#pragma GCC diagnostic pop
	(void)snprintf(date, sizeof(date), MSG_DATE_FMT, tm.tm_mon + 1,
	    tm.tm_mday, ui_weekdays[tm.tm_wday % 7]);
	if (strcmp(clk, u->u_clock) == 0 && strcmp(date, u->u_date) == 0)
		return 0;
	(void)strlcpy(u->u_clock, clk, sizeof(u->u_clock));
	(void)strlcpy(u->u_date, date, sizeof(u->u_date));
	return 1;
}

/*
 * Called when the poll(2) timeout expires.  Returns the milliseconds
 * until the next full minute.
 */
int
ui_tick(struct greeter *g)
{
	struct timespec ts;

	if (ui_clock(g))
		x11_damage(g, &g->g_ui.u_bclock);
	(void)clock_gettime(CLOCK_REALTIME, &ts);
	return (int)(60000 - (ts.tv_sec % 60) * 1000 - ts.tv_nsec / 1000000) +
	    20;
}

void
ui_init(struct greeter *g)
{
	struct ui *u = &g->g_ui;
	struct utsname un;

	(void)memset(u, 0, sizeof(*u));
	(void)strlcpy(u->u_user, g->g_lastuser, sizeof(u->u_user));
	u->u_mode = u->u_user[0] != '\0' ? UI_MODE_PASSWORD : UI_MODE_USERNAME;
	u->u_secret = u->u_mode == UI_MODE_PASSWORD;
	u->u_focus = UI_FOCUS_FIELD;
	if (g->g_flags & AKINIT_SESSIONFAIL)
		u->u_note = UI_NOTE_SESSIONFAIL;

	if (gethostname(u->u_host, sizeof(u->u_host)) == -1)
		u->u_host[0] = '\0';
	u->u_host[sizeof(u->u_host) - 1] = '\0';
	u->u_host[strcspn(u->u_host, ".")] = '\0';
	if (g->g_opt[AKOPT_LOGO][0] == '\0' && uname(&un) == 0)
		(void)strlcpy(g->g_opt[AKOPT_LOGO], un.sysname,
		    sizeof(g->g_opt[AKOPT_LOGO]));

	(void)ui_clock(g);
	ui_layout(g);
	ui_damage_all(g);
}

static int
ui_menu_items(const struct greeter *g, const char **items)
{
	int i;

	switch (g->g_ui.u_menu) {
	case UI_MENU_SESSION:
		for (i = 0; i < g->g_nsessions; i++)
			items[i] = g->g_sessions[i];
		return g->g_nsessions;
	case UI_MENU_POWER:
		items[0] = MSG_SHUTDOWN;
		items[1] = MSG_REBOOT;
		return 2;
	default:
		return 0;
	}
}

static void
ui_menu_layout(struct greeter *g)
{
	struct ui *u = &g->g_ui;
	const char *items[AK_MAXSESSIONS];
	struct au_font f;
	double w, h, x, y;
	int i, n;

	ui_font(g, &f, 0, L_MENU_SIZE, W_REGULAR);
	n = ui_menu_items(g, items);
	w = 0;
	for (i = 0; i < n; i++)
		w = fmax(w, au_font_width(&f, items[i]));
	w += 2 * px(g, L_MENU_INSET);
	h = n * px(g, L_MENU_ROW) + 2 * px(g, L_MENU_PAD);
	if (u->u_menu == UI_MENU_SESSION) {
		x = u->u_rsession.r_x - px(g, L_FOCUS_PAD);
		y = u->u_rsession.r_y - px(g, 10) - h;
	} else {
		x = u->u_rpower.r_x + u->u_rpower.r_w - w;
		y = u->u_rpower.r_y + u->u_rpower.r_h + px(g, 8);
	}
	ui_setrect(&u->u_rmenu, x, y, w, h);
	u->u_menurow = (int)lrint(px(g, L_MENU_ROW));
}

/*
 * Compute the geometry of the interactive parts on the primary output.
 */
void
ui_layout(struct greeter *g)
{
	struct ui *u = &g->g_ui;
	const struct au_rect *o;
	struct au_font f;
	double x, fy, base, asc, desc, w, cx, cy, right;

	o = &x11_primary(g)->o_rect;
	x = o->r_x + px(g, L_MARGIN);
	fy = o->r_y + o->r_h - px(g, L_FIELD_BOTTOM + L_FIELD_H);

	ui_setrect(&u->u_rfield, x, fy, px(g, L_FIELD_W), px(g, L_FIELD_H));
	ui_setrect(&u->u_rbutton,
	    x + px(g, L_FIELD_W - L_BUTTON_INSET - 2 * L_BUTTON_R),
	    fy + px(g, L_FIELD_H / 2 - L_BUTTON_R),
	    px(g, 2 * L_BUTTON_R), px(g, 2 * L_BUTTON_R));

	ui_setrect(&u->u_bclock, o->r_x, o->r_y,
	    fmin(o->r_w, px(g, L_MARGIN + 800)), px(g, L_DATE_BASE + 30));
	ui_setrect(&u->u_blogin, o->r_x,
	    fy - px(g, L_PROMPT_DY + 24),
	    fmin(o->r_w, px(g, L_MARGIN + L_FIELD_W + 300)),
	    px(g, L_PROMPT_DY + 24 + L_FIELD_H + L_ROW_DY + L_NOTE_SHIFT + 16));
	ui_setrect(&u->u_btop, o->r_x + o->r_w - px(g, L_BLOCK_W), o->r_y,
	    px(g, L_BLOCK_W), px(g, 2 * L_TOP_CY + 8));
	ui_setrect(&u->u_blogo, o->r_x + o->r_w - px(g, L_BLOCK_W),
	    o->r_y + o->r_h - px(g, L_LOGO_BASE + 40),
	    px(g, L_BLOCK_W), px(g, L_LOGO_BASE + 40));

	/* The session row. */
	ui_font(g, &f, 0, L_ROW_SIZE, W_REGULAR);
	asc = au_font_ascent(&f);
	desc = au_font_descent(&f);
	base = fy + px(g, L_FIELD_H + L_ROW_DY +
	    (ui_note(g) != NULL ? L_NOTE_SHIFT : 0));
	u->u_rowbase = base;
	w = au_font_width(&f, g->g_sessions[g->g_session]) + px(g, L_CHEVRON);
	ui_setrect(&u->u_rsession, x + px(g, L_ROW_DX), base - asc, w,
	    asc + desc);
	ui_setrect(&u->u_rother, u->u_rsession.r_x + w + px(g, L_ROW_GAP),
	    base - asc, au_font_width(&f, MSG_OTHERUSER), asc + desc);

	/* The top right corner, from the right. */
	cy = o->r_y + px(g, L_TOP_CY);
	right = o->r_x + o->r_w - px(g, L_RIGHT);
	(void)memset(&u->u_rpower, 0, sizeof(u->u_rpower));
	if (ui_power_allowed(g)) {
		cx = right - px(g, L_POWER_R);
		ui_setrect(&u->u_rpower, cx - px(g, L_POWER_R + 4),
		    cy - px(g, L_POWER_R + 4), px(g, 2 * L_POWER_R + 8),
		    px(g, 2 * L_POWER_R + 8));
		right = cx - px(g, L_POWER_R + L_TOP_GAP);
	}
	ui_font(g, &f, 0, L_PILL_SIZE, W_REGULAR);
	w = au_font_width(&f, kbd_label(g)) + 2 * px(g, L_PILL_PAD);
	ui_setrect(&u->u_rlayout, right - w, cy - px(g, L_PILL_H) / 2, w,
	    px(g, L_PILL_H));

	if (u->u_menu != UI_MENU_NONE)
		ui_menu_layout(g);
}

static void
ui_focus_ring(struct greeter *g, struct au_canvas *c, const struct au_rect *r,
    int item)
{
	double pad, radius;

	if (g->g_ui.u_focus != item || g->g_ui.u_menu != UI_MENU_NONE)
		return;
	switch (item) {
	case UI_FOCUS_LAYOUT:
	case UI_FOCUS_POWER:
		/* Round controls get a round ring. */
		pad = px(g, 4);
		radius = (r->r_h + pad) / 2;
		break;
	default:
		pad = px(g, L_FOCUS_PAD);
		radius = px(g, 9);
		break;
	}
	au_draw_rrect_stroke(c, r->r_x - pad, r->r_y - pad / 2,
	    r->r_w + 2 * pad, r->r_h + pad, radius, px(g, 1.5),
	    au_draw_alpha(g->g_text, 0xb3));
}

static void
ui_draw_clock(struct greeter *g, struct au_canvas *c)
{
	const struct au_rect *o;
	struct au_font f;
	double x;

	o = &x11_primary(g)->o_rect;
	x = o->r_x + px(g, L_MARGIN);
	ui_font(g, &f, 1, L_CLOCK_SIZE, W_THIN);
	(void)au_font_draw(c, &f, x, o->r_y + px(g, L_CLOCK_BASE),
	    g->g_ui.u_clock, g->g_text);
	ui_font(g, &f, 1, L_DATE_SIZE, W_REGULAR);
	(void)au_font_draw(c, &f, x + px(g, 4), o->r_y + px(g, L_DATE_BASE),
	    g->g_ui.u_date, au_draw_alpha(g->g_text, 0xeb));
}

/*
 * The contents of the field: placeholder, dots or visible text.
 * Returns the x coordinate of the caret.
 */
static double
ui_draw_field_text(struct greeter *g, struct au_canvas *c, double tx, double ty,
    double maxw)
{
	struct ui *u = &g->g_ui;
	struct au_font f;
	const char *ph;
	double step;
	size_t i, n, first, off;

	ui_font(g, &f, 0, L_FIELD_SIZE, W_REGULAR);
	if (u->u_fieldlen == 0) {
		if (u->u_auth == UI_AUTH_PROMPT)
			ph = MSG_ANSWER;
		else if (u->u_mode == UI_MODE_USERNAME)
			ph = MSG_USERNAME;
		else
			ph = MSG_PASSWORD;
		(void)au_font_draw(c, &f, tx, ty, ph, C_PLACEHOLDER);
		return tx - px(g, 3);	/* the caret goes before the hint */
	}

	if (u->u_secret) {
		step = px(g, L_DOT_STEP);
		n = au_utf8_len(u->u_field, u->u_fieldlen);
		first = (double)n * step > maxw ? n - (size_t)(maxw / step) : 0;
		for (i = first; i < n; i++)
			au_draw_circle(c,
			    tx + px(g, L_DOT_R) + (i - first) * step,
			    ty - ui_vcenter(&f), px(g, L_DOT_R), C_INK);
		return tx + (n - first) * step - px(g, 2);
	}

	/* Visible text: show the end if it does not fit. */
	for (off = 0; off < u->u_fieldlen &&
	    au_font_width(&f, u->u_field + off) > maxw;) {
		off++;
		while (off < u->u_fieldlen &&
		    ((unsigned char)u->u_field[off] & 0xc0) == 0x80)
			off++;
	}
	return au_font_draw(c, &f, tx, ty, u->u_field + off, C_INK) + px(g, 1);
}

static void
ui_draw_login(struct greeter *g, struct au_canvas *c)
{
	struct ui *u = &g->g_ui;
	struct au_font f;
	const char *text;
	char initial[8];
	double x, fy, fw, fh, cx, cy, tx, ty, caret, base, s;
	size_t n;

	x = u->u_rfield.r_x;
	fy = u->u_rfield.r_y;
	fw = px(g, L_FIELD_W);
	fh = px(g, L_FIELD_H);

	/* A question or message from PAM above everything. */
	text = NULL;
	if (u->u_auth == UI_AUTH_PROMPT)
		text = u->u_prompt;
	else if (u->u_note == UI_NOTE_PAM)
		text = u->u_pamtext;
	if (text != NULL) {
		ui_font(g, &f, 0, L_PROMPT_SIZE, W_REGULAR);
		(void)au_font_draw(c, &f, x, fy - px(g, L_PROMPT_DY), text,
		    g->g_text);
	}

	/* The user: initial in a ring, and the name. */
	cx = x + px(g, L_AVATAR_R + 3);
	cy = fy - px(g, L_AVATAR_DY);
	au_draw_ring(c, cx, cy, px(g, L_AVATAR_R), px(g, 1.5),
	    au_draw_alpha(g->g_text, 0xd9));
	if (u->u_mode == UI_MODE_PASSWORD) {
		n = 1;
		while (((unsigned char)u->u_user[n] & 0xc0) == 0x80)
			n++;
		(void)strlcpy(initial, u->u_user,
		    n + 1 < sizeof(initial) ? n + 1 : sizeof(initial));
		if (initial[0] >= 'A' && initial[0] <= 'Z')
			initial[0] = (char)(initial[0] - 'A' + 'a');
		ui_font(g, &f, 0, L_AVATAR_SIZE, W_MEDIUM);
		(void)au_font_draw(c, &f, cx - au_font_width(&f, initial) / 2,
		    cy + ui_vcenter(&f), initial, g->g_text);
		ui_font(g, &f, 0, L_NAME_SIZE, W_SEMIBOLD);
		(void)au_font_draw(c, &f, x + px(g, L_NAME_DX),
		    cy + ui_vcenter(&f), u->u_user, g->g_text);
	} else {
		ui_font(g, &f, 0, L_NAME_SIZE - 3, W_REGULAR);
		(void)au_font_draw(c, &f, x + px(g, L_NAME_DX),
		    cy + ui_vcenter(&f), MSG_ENTERUSER,
		    au_draw_alpha(g->g_text, 0xb3));
	}

	/* The field and its button. */
	au_draw_rrect(c, x, fy, fw, fh, fh / 2, C_FIELD);
	ui_font(g, &f, 0, L_FIELD_SIZE, W_REGULAR);
	tx = x + px(g, L_FIELD_PAD);
	ty = fy + fh / 2 + ui_vcenter(&f);
	caret = ui_draw_field_text(g, c, tx, ty, fw - px(g, L_FIELD_PAD +
	    L_BUTTON_INSET + 2 * L_BUTTON_R + 10));
	if (u->u_focus == UI_FOCUS_FIELD && u->u_menu == UI_MENU_NONE &&
	    u->u_auth != UI_AUTH_WAIT)
		au_draw_line(c, caret + px(g, 1), fy + fh / 2 - px(g, 10),
		    caret + px(g, 1), fy + fh / 2 + px(g, 10), px(g, 1.5),
		    C_INK);

	cx = u->u_rbutton.r_x + u->u_rbutton.r_w / 2.0;
	cy = u->u_rbutton.r_y + u->u_rbutton.r_h / 2.0;
	au_draw_circle(c, cx, cy, px(g, L_BUTTON_R), g->g_accent);
	s = px(g, 1);
	if (u->u_auth == UI_AUTH_WAIT || u->u_auth == UI_AUTH_CANCEL) {
		/* Busy: no second submission. */
		au_draw_circle(c, cx - 6 * s, cy, 2 * s, C_BUTTON_INK);
		au_draw_circle(c, cx, cy, 2 * s, C_BUTTON_INK);
		au_draw_circle(c, cx + 6 * s, cy, 2 * s, C_BUTTON_INK);
	} else {
		au_draw_line(c, cx - 6 * s, cy, cx + 6 * s, cy, 1.8 * s,
		    C_BUTTON_INK);
		au_draw_line(c, cx + 1 * s, cy - 5 * s, cx + 6 * s, cy, 1.8 * s,
		    C_BUTTON_INK);
		au_draw_line(c, cx + 1 * s, cy + 5 * s, cx + 6 * s, cy, 1.8 * s,
		    C_BUTTON_INK);
	}

	/* A note below the field. */
	if ((text = ui_note(g)) != NULL) {
		ui_font(g, &f, 0, L_NOTE_SIZE, W_REGULAR);
		(void)au_font_draw(c, &f, x + px(g, L_ROW_DX),
		    fy + fh + px(g, L_NOTE_DY), text, g->g_text);
	}

	/* Session and the other user. */
	ui_font(g, &f, 0, L_ROW_SIZE, W_REGULAR);
	base = u->u_rowbase;
	cx = au_font_draw(c, &f, u->u_rsession.r_x, base,
	    g->g_sessions[g->g_session], au_draw_alpha(g->g_text, 0xf2)) +
	    px(g, 8);
	cy = base - px(g, 4.5);
	au_draw_line(c, cx - 3.5 * s, cy - 2 * s, cx, cy + 1.5 * s, 1.4 * s,
	    g->g_text);
	au_draw_line(c, cx, cy + 1.5 * s, cx + 3.5 * s, cy - 2 * s, 1.4 * s,
	    g->g_text);
	(void)au_font_draw(c, &f, u->u_rother.r_x, base, MSG_OTHERUSER,
	    au_draw_alpha(g->g_text, 0xf2));
	ui_focus_ring(g, c, &u->u_rsession, UI_FOCUS_SESSION);
	ui_focus_ring(g, c, &u->u_rother, UI_FOCUS_OTHER);
}

static void
ui_draw_top(struct greeter *g, struct au_canvas *c)
{
	struct ui *u = &g->g_ui;
	const struct au_rect *r;
	struct au_font f;
	double cx, cy, s;

	s = px(g, 1);
	r = &u->u_rlayout;
	cy = r->r_y + r->r_h / 2.0;

	ui_font(g, &f, 0, L_HOST_SIZE, W_REGULAR);
	(void)au_font_draw(c, &f, r->r_x - px(g, L_HOST_GAP) -
	    au_font_width(&f, u->u_host), cy + ui_vcenter(&f), u->u_host,
	    au_draw_alpha(g->g_text, 0xeb));

	au_draw_rrect_stroke(c, r->r_x, r->r_y, r->r_w, r->r_h, r->r_h / 2.0,
	    1.2 * s, au_draw_alpha(g->g_text, 0xcc));
	ui_font(g, &f, 0, L_PILL_SIZE, W_REGULAR);
	(void)au_font_draw(c, &f, r->r_x + px(g, L_PILL_PAD),
	    cy + ui_vcenter(&f), kbd_label(g), g->g_text);
	ui_focus_ring(g, c, r, UI_FOCUS_LAYOUT);

	if (ui_power_allowed(g)) {
		r = &u->u_rpower;
		cx = r->r_x + r->r_w / 2.0;
		cy = r->r_y + r->r_h / 2.0;
		au_draw_arc(c, cx, cy + 0.5 * s, px(g, L_POWER_R), 1.6 * s, 0.7,
		    g->g_text);
		au_draw_line(c, cx, cy - 8.5 * s, cx, cy - 2 * s, 1.6 * s,
		    g->g_text);
		ui_focus_ring(g, c, r, UI_FOCUS_POWER);
	}
}

/*
 * The logo: an asterisk in the accent colour and the name, where text
 * between "**" is bold, e.g. "**Autumn**BSD".
 */
static void
ui_draw_logo(struct greeter *g, struct au_canvas *c)
{
	char buf[AKMSG_TEXTMAX + 1];
	char *seg[16];
	struct au_font f;
	const struct au_rect *o;
	double x, w, base, cx, cy, r, a, s;
	int i, n;

	if (!g->g_showlogo || g->g_opt[AKOPT_LOGO][0] == '\0')
		return;
	(void)strlcpy(buf, g->g_opt[AKOPT_LOGO], sizeof(buf));
	/* Split at "**"; odd segments are bold. */
	n = 0;
	seg[n++] = buf;
	for (i = 0; buf[i] != '\0' && n < (int)__arraycount(seg); i++) {
		if (buf[i] == '*' && buf[i + 1] == '*') {
			buf[i] = '\0';
			seg[n++] = &buf[i + 2];
			i++;
		}
	}

	w = 0;
	for (i = 0; i < n; i++) {
		ui_font(g, &f, 0, L_LOGO_SIZE, i % 2 ? W_BOLD : W_REGULAR);
		w += au_font_width(&f, seg[i]);
	}
	o = &x11_primary(g)->o_rect;
	x = o->r_x + o->r_w - px(g, L_LOGO_RIGHT) - w;
	base = o->r_y + o->r_h - px(g, L_LOGO_BASE);

	s = px(g, 1);
	cx = x - px(g, L_STAR_R + 9);
	cy = base - px(g, 6);
	r = px(g, L_STAR_R);
	for (i = 0; i < 4; i++) {
		a = i * M_PI / 4;
		au_draw_line(c, cx - r * cos(a), cy - r * sin(a),
		    cx + r * cos(a), cy + r * sin(a), 1.5 * s, g->g_accent);
	}
	for (i = 0; i < n; i++) {
		ui_font(g, &f, 0, L_LOGO_SIZE, i % 2 ? W_BOLD : W_REGULAR);
		x = au_font_draw(c, &f, x, base, seg[i], g->g_text);
	}
}

static void
ui_draw_menu(struct greeter *g, struct au_canvas *c)
{
	struct ui *u = &g->g_ui;
	const char *items[AK_MAXSESSIONS];
	const struct au_rect *r;
	struct au_font f;
	double y;
	int i, n;

	r = &u->u_rmenu;
	n = ui_menu_items(g, items);
	/* Nearly opaque: whatever lies below must not show through. */
	au_draw_rrect(c, r->r_x, r->r_y, r->r_w, r->r_h, px(g, 12),
	    au_draw_alpha(g->g_bgcolor, 0xf7));
	au_draw_rrect_stroke(c, r->r_x, r->r_y, r->r_w, r->r_h, px(g, 12),
	    px(g, 1), au_draw_alpha(g->g_text, 0x33));
	ui_font(g, &f, 0, L_MENU_SIZE, W_REGULAR);
	for (i = 0; i < n; i++) {
		y = r->r_y + px(g, L_MENU_PAD) + i * u->u_menurow;
		if (i == u->u_menusel)
			au_draw_rrect(c, r->r_x + px(g, 4), y,
			    r->r_w - px(g, 8), u->u_menurow, px(g, 8),
			    au_draw_alpha(g->g_text, 0x24));
		(void)au_font_draw(c, &f, r->r_x + px(g, L_MENU_INSET),
		    y + u->u_menurow / 2.0 + ui_vcenter(&f), items[i],
		    g->g_text);
	}
}

/*
 * Draw everything that intersects the damage, then send it.  The
 * order is background, clock, login block, top right and logo.
 */
void
ui_paint(struct greeter *g)
{
	struct x11 *x = &g->g_x;
	struct au_canvas c;
	const struct output *o;
	int i;

	if (!pixman_region32_not_empty(&x->x_damage))
		return;
	c.c_img = x->x_frame;
	pixman_region32_init(&c.c_clip);
	pixman_region32_copy(&c.c_clip, &x->x_damage);
	pixman_image_set_clip_region32(c.c_img, &c.c_clip);

	for (i = 0; i < x->x_noutputs; i++) {
		o = &x->x_outputs[i];
		if (o->o_bg != NULL)
			au_draw_image(&c, o->o_bg, o->o_rect.r_x,
			    o->o_rect.r_y);
		else
			au_draw_fill(&c, &o->o_rect, g->g_bgcolor);
	}
	ui_draw_clock(g, &c);
	ui_draw_login(g, &c);
	ui_draw_top(g, &c);
	ui_draw_logo(g, &c);
	if (g->g_ui.u_menu != UI_MENU_NONE)
		ui_draw_menu(g, &c);

	pixman_image_set_clip_region32(c.c_img, NULL);
	pixman_region32_fini(&c.c_clip);
	x11_present(g);
}

static void
ui_cancel(struct greeter *g)
{
	struct ui *u = &g->g_ui;
	struct akmsg m;

	if (u->u_auth == UI_AUTH_WAIT || u->u_auth == UI_AUTH_PROMPT) {
		akmsg_init(&m, AKMSG_CANCEL);
		ui_send(g, &m);
		u->u_auth = UI_AUTH_CANCEL;
	}
	ui_clear_field(u);
	u->u_pending = 0;
	u->u_prompt[0] = '\0';
	u->u_note = UI_NOTE_NONE;
	u->u_focus = UI_FOCUS_FIELD;
}

/*
 * Esc: back to the start, with the last user.
 */
static void
ui_reset(struct greeter *g)
{
	struct ui *u = &g->g_ui;

	ui_cancel(g);
	(void)strlcpy(u->u_user, g->g_lastuser, sizeof(u->u_user));
	u->u_mode = u->u_user[0] != '\0' ? UI_MODE_PASSWORD : UI_MODE_USERNAME;
	u->u_secret = u->u_mode == UI_MODE_PASSWORD;
	ui_damage_login(g);
}

static void
ui_other_user(struct greeter *g)
{
	struct ui *u = &g->g_ui;

	ui_cancel(g);
	u->u_user[0] = '\0';
	u->u_mode = UI_MODE_USERNAME;
	u->u_secret = 0;
	ui_damage_login(g);
}

static void
ui_submit(struct greeter *g)
{
	struct ui *u = &g->g_ui;
	struct akmsg m;

	switch (u->u_auth) {
	case UI_AUTH_IDLE:
		if (u->u_mode == UI_MODE_USERNAME) {
			if (u->u_fieldlen == 0)
				return;
			(void)strlcpy(u->u_user, u->u_field,
			    sizeof(u->u_user));
			ui_clear_field(u);
			u->u_mode = UI_MODE_PASSWORD;
			u->u_secret = 1;
			u->u_note = UI_NOTE_NONE;
			break;
		}
		akmsg_init(&m, AKMSG_BEGIN_AUTH);
		m.am_args[0] = (uint32_t)g->g_session;
		(void)akmsg_setstr(&m, u->u_user);
		ui_send(g, &m);
		u->u_auth = UI_AUTH_WAIT;
		u->u_pending = 1;
		u->u_note = UI_NOTE_NONE;
		break;
	case UI_AUTH_PROMPT:
		akmsg_init(&m, AKMSG_RESPONSE);
		(void)akmsg_setstr(&m, u->u_field);
		ui_send(g, &m);
		ui_clear_field(u);
		u->u_prompt[0] = '\0';
		u->u_auth = UI_AUTH_WAIT;
		break;
	default:
		return;
	}
	ui_damage_login(g);
}

/*
 * PAM text may contain newlines and tabs; it is shown on one line.
 */
static void
ui_flatten(char *s)
{

	for (; *s != '\0'; s++) {
		if (*s == '\n' || *s == '\t')
			*s = ' ';
	}
}

/*
 * A message from the master.
 */
void
ui_message(struct greeter *g, struct akmsg *m)
{
	struct ui *u = &g->g_ui;
	struct akmsg r;

	switch (m->am_type) {
	case AKMSG_PROMPT:
		if (u->u_auth != UI_AUTH_WAIT)
			break;		/* cancelled meanwhile */
		switch (m->am_args[0]) {
		case AKPROMPT_SECRET:
			if (u->u_pending) {
				/* The password typed before submitting. */
				akmsg_init(&r, AKMSG_RESPONSE);
				(void)akmsg_setstr(&r, u->u_field);
				ui_send(g, &r);
				ui_clear_field(u);
				u->u_pending = 0;
				break;
			}
			/* FALLTHROUGH */
		case AKPROMPT_VISIBLE:
			ui_clear_field(u);
			u->u_pending = 0;
			(void)strlcpy(u->u_prompt, m->am_str,
			    sizeof(u->u_prompt));
			ui_flatten(u->u_prompt);
			u->u_secret = m->am_args[0] == AKPROMPT_SECRET;
			u->u_auth = UI_AUTH_PROMPT;
			u->u_focus = UI_FOCUS_FIELD;
			break;
		default:
			(void)strlcpy(u->u_pamtext, m->am_str,
			    sizeof(u->u_pamtext));
			ui_flatten(u->u_pamtext);
			u->u_note = UI_NOTE_PAM;
			break;
		}
		break;
	case AKMSG_AUTH_RESULT:
		if (m->am_args[0] == AKRESULT_OK) {
			ui_clear_field(u);
			exit(AKTURE_EXIT_OK);
		}
		if (u->u_auth != UI_AUTH_CANCEL)
			u->u_note = UI_NOTE_BADPASS;
		ui_clear_field(u);
		u->u_auth = UI_AUTH_IDLE;
		u->u_pending = 0;
		u->u_prompt[0] = '\0';
		u->u_secret = u->u_mode == UI_MODE_PASSWORD;
		break;
	default:
		syslog(LOG_ERR, "unexpected message %u", m->am_type);
		exit(AKTURE_EXIT_ERROR);
	}
	akmsg_clear(m);
	ui_damage_login(g);
}

static void
ui_set_focus(struct greeter *g, int focus)
{

	g->g_ui.u_focus = focus;
	ui_damage_login(g);
	x11_damage(g, &g->g_ui.u_btop);
}

static void
ui_cycle_focus(struct greeter *g, int dir)
{
	int f;

	f = g->g_ui.u_focus;
	do {
		f = (f + dir + UI_NFOCUS) % UI_NFOCUS;
	} while (f == UI_FOCUS_POWER && !ui_power_allowed(g));
	ui_set_focus(g, f);
}

static void
ui_menu_open(struct greeter *g, int menu)
{
	struct ui *u = &g->g_ui;

	if (menu == UI_MENU_POWER && !ui_power_allowed(g))
		return;
	u->u_menu = menu;
	u->u_menusel = menu == UI_MENU_SESSION ? g->g_session : 0;
	ui_menu_layout(g);
	x11_damage(g, &u->u_rmenu);
	ui_set_focus(g, menu == UI_MENU_SESSION ? UI_FOCUS_SESSION :
	    UI_FOCUS_POWER);
}

static void
ui_menu_close(struct greeter *g)
{
	struct ui *u = &g->g_ui;

	x11_damage(g, &u->u_rmenu);
	u->u_menu = UI_MENU_NONE;
	ui_damage_login(g);
	x11_damage(g, &u->u_btop);
}

static void
ui_menu_choose(struct greeter *g)
{
	struct ui *u = &g->g_ui;
	struct akmsg m;
	int menu;

	menu = u->u_menu;
	ui_menu_close(g);
	if (menu == UI_MENU_SESSION) {
		g->g_session = u->u_menusel;
		ui_layout(g);
		ui_damage_login(g);
	} else if (menu == UI_MENU_POWER) {
		akmsg_init(&m, AKMSG_POWER);
		m.am_args[0] = u->u_menusel == 0 ? AKPOWER_SHUTDOWN :
		    AKPOWER_REBOOT;
		ui_send(g, &m);
	}
}

static void
ui_next_layout(struct greeter *g)
{

	kbd_next_layout(g);
	x11_damage(g, &g->g_ui.u_btop);
	ui_layout(g);
	x11_damage(g, &g->g_ui.u_btop);
}

static void
ui_activate(struct greeter *g)
{

	switch (g->g_ui.u_focus) {
	case UI_FOCUS_FIELD:
		ui_submit(g);
		break;
	case UI_FOCUS_SESSION:
		ui_menu_open(g, UI_MENU_SESSION);
		break;
	case UI_FOCUS_OTHER:
		ui_other_user(g);
		break;
	case UI_FOCUS_LAYOUT:
		ui_next_layout(g);
		break;
	case UI_FOCUS_POWER:
		ui_menu_open(g, UI_MENU_POWER);
		break;
	}
}

static void
ui_menu_key(struct greeter *g, uint32_t sym)
{
	const char *items[AK_MAXSESSIONS];
	struct ui *u = &g->g_ui;
	int n;

	n = ui_menu_items(g, items);
	switch (sym) {
	case XKB_KEY_Up:
		u->u_menusel = (u->u_menusel + n - 1) % n;
		x11_damage(g, &u->u_rmenu);
		break;
	case XKB_KEY_Down:
		u->u_menusel = (u->u_menusel + 1) % n;
		x11_damage(g, &u->u_rmenu);
		break;
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
	case XKB_KEY_space:
		ui_menu_choose(g);
		break;
	case XKB_KEY_Escape:
	case XKB_KEY_Tab:
	case XKB_KEY_ISO_Left_Tab:
		ui_menu_close(g);
		break;
	}
}

static void
ui_insert(struct greeter *g, const char *s)
{
	struct ui *u = &g->g_ui;
	const unsigned char *p;
	size_t len, max;

	for (p = (const unsigned char *)s; *p != '\0'; p++) {
		if (*p < 0x20 || *p == 0x7f)
			return;
	}
	len = strlen(s);
	max = u->u_mode == UI_MODE_USERNAME && u->u_auth != UI_AUTH_PROMPT ?
	    AKMSG_NAMEMAX : UI_FIELDMAX;
	if (len == 0 || u->u_fieldlen + len > max)
		return;		/* over the limit: refused */
	(void)memcpy(u->u_field + u->u_fieldlen, s, len);
	u->u_fieldlen += len;
	u->u_field[u->u_fieldlen] = '\0';
	if (u->u_note == UI_NOTE_BADPASS || u->u_note == UI_NOTE_SESSIONFAIL)
		u->u_note = UI_NOTE_NONE;
	u->u_focus = UI_FOCUS_FIELD;
	ui_damage_login(g);
}

static void
ui_backspace(struct greeter *g)
{
	struct ui *u = &g->g_ui;
	size_t off;

	if (u->u_fieldlen == 0)
		return;
	off = au_utf8_prev(u->u_field, u->u_fieldlen);
	ak_explicit_zero(u->u_field + off, u->u_fieldlen - off);
	u->u_fieldlen = off;
	ui_damage_login(g);
}

/*
 * A key press, already translated by xkbcommon.
 */
void
ui_key(struct greeter *g, uint32_t sym, const char *utf8, int ctrl,
    int shift)
{
	struct ui *u = &g->g_ui;
	int editable;

	if (u->u_menu != UI_MENU_NONE) {
		ui_menu_key(g, sym);
		return;
	}
	editable = u->u_auth != UI_AUTH_WAIT;

	switch (sym) {
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		ui_activate(g);
		return;
	case XKB_KEY_Tab:
		ui_cycle_focus(g, shift ? -1 : 1);
		return;
	case XKB_KEY_ISO_Left_Tab:
		ui_cycle_focus(g, -1);
		return;
	case XKB_KEY_Escape:
		ui_reset(g);
		return;
	case XKB_KEY_BackSpace:
		if (editable && u->u_focus == UI_FOCUS_FIELD)
			ui_backspace(g);
		return;
	}
	if (ctrl) {
		if ((sym == XKB_KEY_u || sym == XKB_KEY_U) && editable) {
			ui_clear_field(u);
			ui_damage_login(g);
		}
		return;
	}
	if (editable && utf8[0] != '\0')
		ui_insert(g, utf8);
}

void
ui_click(struct greeter *g, int x, int y)
{
	const char *items[AK_MAXSESSIONS];
	struct ui *u = &g->g_ui;
	int row;

	if (u->u_menu != UI_MENU_NONE) {
		row = y - u->u_rmenu.r_y - (int)px(g, L_MENU_PAD);
		if (ui_inside(&u->u_rmenu, x, y) && row >= 0 &&
		    row / u->u_menurow < ui_menu_items(g, items)) {
			u->u_menusel = row / u->u_menurow;
			ui_menu_choose(g);
		} else
			ui_menu_close(g);
		return;
	}

	if (ui_inside(&u->u_rbutton, x, y)) {
		u->u_focus = UI_FOCUS_FIELD;
		ui_submit(g);
	} else if (ui_inside(&u->u_rfield, x, y))
		ui_set_focus(g, UI_FOCUS_FIELD);
	else if (ui_inside(&u->u_rsession, x, y))
		ui_menu_open(g, UI_MENU_SESSION);
	else if (ui_inside(&u->u_rother, x, y))
		ui_other_user(g);
	else if (ui_inside(&u->u_rlayout, x, y))
		ui_next_layout(g);
	else if (ui_power_allowed(g) && ui_inside(&u->u_rpower, x, y))
		ui_menu_open(g, UI_MENU_POWER);
}

void
ui_caps(struct greeter *g)
{

	ui_damage_login(g);
}
