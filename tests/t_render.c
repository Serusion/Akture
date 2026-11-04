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

#include <sha2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

#include "akparse.h"
#include "greeter.h"
#include "msgs.h"

/*
 * The login screen without an X server.  ui_layout() and ui_paint()
 * of the greeter draw into a 1366x768 image in memory, with fixed
 * fonts, user, clock and date on a solid background; the image is
 * written as PPM and its SHA-256 compared with the one the base commit
 * drew.  The X, keyboard and IPC parts of the greeter are replaced
 * below: ui_paint() ends with x11_present(), which sends the frame
 * through MIT-SHM.  The frame is an au_frame without a connection,
 * around an image in memory.
 *
 * The fonts are not in the repository.  AKTURE_TEST_FONT_UI and
 * AKTURE_TEST_FONT_CLOCK name them, and without them the test is
 * skipped.  The hashes hold for Outfit[wght].ttf and
 * ZenKakuGothicNew-Light.ttf from google/fonts 2fe1e8c, as installed
 * by autumn-fonts, with the pixman and FreeType of NetBSD-current:
 * other versions may draw other pixels.
 */

#define T_WIDTH		1366
#define T_HEIGHT	768

/* SHA-256 of the PPM files. */
#define T_LOGIN_SHA256	\
	"1153b325a2f9465db8056b036b3d6615d46d0c17d8ea7255851668d7cee29227"
#define T_MENU_SHA256	\
	"84d3f399694f24d396e776f4bd8ed3b4e4987fb38bf9080baf819fb72d523cce"

static struct greeter greeter;
static int npresent;

void
x11_damage(struct greeter *g, const struct au_rect *r)
{

	au_frame_damage(&g->g_x.x_frame, r);
}

/* Nothing to send to: the damage is taken, as by the server. */
void
x11_present(struct greeter *g)
{

	npresent++;
	pixman_region32_clear(&g->g_x.x_frame.fr_damage);
}

const struct output *
x11_primary(const struct greeter *g)
{

	return &g->g_x.x_outputs[g->g_x.x_primary];
}

const char *
kbd_label(const struct greeter *g)
{

	(void)g;
	return "US";
}

void
kbd_next_layout(struct greeter *g)
{

	(void)g;
}

int
ipc_send(struct greeter *g, struct akmsg *m)
{

	(void)g;
	(void)m;
	return -1;
}

/*
 * The greeter as ipc_init() and ui_init() leave it for user "user",
 * with the default options, the logo of AutumnBSD and two sessions,
 * at 10:09 on Wednesday, 4 November, on host "autumn".
 */
static void
render_setup(struct greeter *g)
{
	static const char *const weekdays[] = { MSG_WEEKDAYS };
	struct x11 *x = &g->g_x;
	struct ui *u = &g->g_ui;
	const char *fui, *fclock;
	unsigned int pct;
	int i;

	fui = getenv("AKTURE_TEST_FONT_UI");
	fclock = getenv("AKTURE_TEST_FONT_CLOCK");
	if (fui == NULL || fclock == NULL)
		atf_tc_skip("AKTURE_TEST_FONT_UI and AKTURE_TEST_FONT_CLOCK "
		    "name no fonts");

	(void)memset(g, 0, sizeof(*g));
	for (i = 0; i < AKOPT_NKEYS; i++)
		(void)strlcpy(g->g_opt[i], akopt_table[i].ao_default,
		    sizeof(g->g_opt[i]));
	(void)strlcpy(g->g_opt[AKOPT_LOGO], "**Autumn**BSD",
	    sizeof(g->g_opt[AKOPT_LOGO]));
	ATF_REQUIRE_EQ(ak_parse_color(g->g_opt[AKOPT_BACKGROUND_COLOR],
	    &g->g_theme.th_bgcolor), 0);
	ATF_REQUIRE_EQ(ak_parse_color(g->g_opt[AKOPT_ACCENT],
	    &g->g_theme.th_accent), 0);
	ATF_REQUIRE_EQ(ak_parse_color(g->g_opt[AKOPT_TEXT_COLOR],
	    &g->g_theme.th_text), 0);
	ATF_REQUIRE_EQ(ak_parse_scale(g->g_opt[AKOPT_SCALE], &pct), 0);
	g->g_theme.th_scale = pct / 100.0;
	g->g_showlogo = 1;
	g->g_flags = AKINIT_POWER;
	(void)strlcpy(g->g_sessions[0], "fluxbox", sizeof(g->g_sessions[0]));
	(void)strlcpy(g->g_sessions[1], "twm", sizeof(g->g_sessions[1]));
	g->g_nsessions = 2;
	g->g_session = 0;
	(void)strlcpy(g->g_lastuser, "user", sizeof(g->g_lastuser));

	ATF_REQUIRE_EQ(au_font_init(), 0);
	ATF_REQUIRE((g->g_fui = au_face_open(fui)) != NULL);
	ATF_REQUIRE((g->g_fclock = au_face_open(fclock)) != NULL);

	/* One output over the whole frame, without a background image. */
	x->x_width = T_WIDTH;
	x->x_height = T_HEIGHT;
	au_frame_init(&x->x_frame, NULL);
	x->x_frame.fr_img = pixman_image_create_bits(PIXMAN_x8r8g8b8, T_WIDTH,
	    T_HEIGHT, NULL, T_WIDTH * 4);
	ATF_REQUIRE(x->x_frame.fr_img != NULL);
	x->x_frame.fr_width = T_WIDTH;
	x->x_frame.fr_height = T_HEIGHT;
	x->x_outputs[0].o_rect.r_w = T_WIDTH;
	x->x_outputs[0].o_rect.r_h = T_HEIGHT;
	x->x_noutputs = 1;
	x->x_primary = 0;

	(void)strlcpy(u->u_user, g->g_lastuser, sizeof(u->u_user));
	u->u_mode = UI_MODE_PASSWORD;
	u->u_secret = 1;
	u->u_focus = UI_FOCUS_FIELD;
	(void)strlcpy(u->u_clock, "10:09", sizeof(u->u_clock));
	(void)snprintf(u->u_date, sizeof(u->u_date), MSG_DATE_FMT, 11, 4,
	    weekdays[3]);
	(void)strlcpy(u->u_host, "autumn", sizeof(u->u_host));
}

/* Write an x8r8g8b8 image as binary PPM. */
static void
write_ppm(pixman_image_t *img, const char *path)
{
	const uint8_t *data;
	const uint32_t *row;
	uint8_t *line;
	FILE *fp;
	int w, h, stride, x, y;

	w = pixman_image_get_width(img);
	h = pixman_image_get_height(img);
	stride = pixman_image_get_stride(img);
	data = (const uint8_t *)pixman_image_get_data(img);
	ATF_REQUIRE((line = malloc((size_t)w * 3)) != NULL);
	ATF_REQUIRE((fp = fopen(path, "wb")) != NULL);
	ATF_REQUIRE(fprintf(fp, "P6\n%d %d\n255\n", w, h) > 0);
	for (y = 0; y < h; y++) {
		row = (const uint32_t *)(const void *)(data +
		    (size_t)y * stride);
		for (x = 0; x < w; x++) {
			line[3 * x] = (uint8_t)(row[x] >> 16);
			line[3 * x + 1] = (uint8_t)(row[x] >> 8);
			line[3 * x + 2] = (uint8_t)row[x];
		}
		ATF_REQUIRE_EQ(fwrite(line, 3, (size_t)w, fp), (size_t)w);
	}
	ATF_REQUIRE_EQ(fclose(fp), 0);
	free(line);
}

/* Lay out, damage everything, paint once and compare the image. */
static void
render_check(struct greeter *g, const char *path, const char *want)
{
	char got[SHA256_DIGEST_STRING_LENGTH];

	npresent = 0;
	ui_layout(g);
	ui_damage_all(g);
	ui_paint(g);
	ATF_REQUIRE_EQ(npresent, 1);
	ATF_REQUIRE(!pixman_region32_not_empty(&g->g_x.x_frame.fr_damage));
	write_ppm(g->g_x.x_frame.fr_img, path);
	ATF_REQUIRE(SHA256_File(path, got) != NULL);
	ATF_CHECK_MSG(strcmp(got, want) == 0, "%s: SHA-256 %s, expected %s",
	    path, got, want);
}

ATF_TC(login);
ATF_TC_HEAD(login, tc)
{
	atf_tc_set_md_var(tc, "descr", "the login screen of user \"user\"");
}
ATF_TC_BODY(login, tc)
{
	struct greeter *g = &greeter;

	render_setup(g);
	render_check(g, "login.ppm", T_LOGIN_SHA256);
}

ATF_TC(menu);
ATF_TC_HEAD(menu, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "three characters typed, and the session menu open");
}
ATF_TC_BODY(menu, tc)
{
	struct greeter *g = &greeter;
	struct ui *u = &g->g_ui;

	render_setup(g);
	(void)strlcpy(u->u_field, "abc", sizeof(u->u_field));
	u->u_fieldlen = 3;
	u->u_menu = UI_MENU_SESSION;
	u->u_menusel = 1;
	render_check(g, "menu.ppm", T_MENU_SHA256);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, login);
	ATF_TP_ADD_TC(tp, menu);
	return atf_no_error();
}
