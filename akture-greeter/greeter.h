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

#ifndef _GREETER_H_
#define _GREETER_H_

#include <sys/types.h>

#include <pixman.h>
#include <stdint.h>
#include <xcb/xcb.h>

#include "akmsg.h"
#include "akopt.h"
#include "autuni.h"

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

#define G_MAXOUTPUTS	8

struct output {
	struct au_rect	 o_rect;
	pixman_image_t	*o_bg;		/* NULL: solid background */
	void		*o_map;		/* mmap(2)ed .akbg */
	size_t		 o_maplen;
};

struct x11 {
	xcb_connection_t *x_conn;
	xcb_screen_t	*x_screen;
	xcb_window_t	 x_win;
	xcb_gcontext_t	 x_gc;
	uint32_t	 x_shmseg;
	void		*x_shmaddr;
	pixman_image_t	*x_frame;	/* the whole screen, in shm */
	int		 x_width, x_height;
	uint8_t		 x_randr_base;	/* 0: no RandR */
	struct output	 x_outputs[G_MAXOUTPUTS];
	int		 x_noutputs;
	int		 x_primary;
	pixman_region32_t x_damage;
};

#define KBD_MAXLAYOUTS	4
#define KBD_LABELMAX	8

struct kbd {
	struct xkb_context *k_ctx;
	struct xkb_keymap *k_keymap;
	struct xkb_state *k_state;
	int32_t		 k_device;
	uint8_t		 k_xkb_base;
	int		 k_custom;	/* keymap from keyboard_layouts */
	char		 k_labels[KBD_MAXLAYOUTS][KBD_LABELMAX];
	int		 k_nlayouts;
	int		 k_group;
	int		 k_caps;
};

/* Login state of the UI */
#define UI_MODE_PASSWORD	0	/* user known, field takes the password */
#define UI_MODE_USERNAME	1	/* field takes a user name */

#define UI_AUTH_IDLE		0
#define UI_AUTH_WAIT		1	/* request sent, busy */
#define UI_AUTH_PROMPT		2	/* answering a further PAM question */
#define UI_AUTH_CANCEL		3	/* cancelled, waiting for the result */

#define UI_FOCUS_FIELD		0
#define UI_FOCUS_SESSION	1
#define UI_FOCUS_OTHER		2
#define UI_FOCUS_LAYOUT		3
#define UI_FOCUS_POWER		4
#define UI_NFOCUS		5

#define UI_MENU_NONE		0
#define UI_MENU_SESSION		1
#define UI_MENU_POWER		2

#define UI_NOTE_NONE		0
#define UI_NOTE_BADPASS		1
#define UI_NOTE_SESSIONFAIL	2
#define UI_NOTE_PAM		3	/* PAM info or error text */

#define UI_FIELDMAX		AKMSG_RESPMAX

struct ui {
	int		 u_mode;
	int		 u_auth;
	int		 u_focus;
	int		 u_menu;
	int		 u_menusel;
	int		 u_note;
	char		 u_user[AKMSG_NAMEMAX + 1];
	char		 u_field[UI_FIELDMAX + 1];
	size_t		 u_fieldlen;
	int		 u_secret;	/* mask the field */
	int		 u_pending;	/* field holds the password to send */
	char		 u_prompt[AKMSG_TEXTMAX + 1];
	char		 u_pamtext[AKMSG_TEXTMAX + 1];
	char		 u_clock[64];
	char		 u_date[64];
	char		 u_host[64];

	/* Geometry of the last paint, for clicks and damage. */
	double		 u_scale;
	struct au_rect	 u_bclock, u_blogin, u_btop, u_blogo;
	struct au_rect	 u_rfield, u_rbutton, u_rsession, u_rother;
	struct au_rect	 u_rlayout, u_rpower, u_rmenu;
	double		 u_rowbase;	/* baseline of the session row */
	int		 u_menurow;	/* height of a menu row */
};

struct greeter {
	int		 g_sock;
	uint32_t	 g_sseq;
	uint32_t	 g_rseq;

	char		 g_opt[AKOPT_NKEYS][AKMSG_TEXTMAX + 1];
	uint32_t	 g_bgcolor;
	uint32_t	 g_accent;
	uint32_t	 g_text;
	double		 g_scale;
	int		 g_showlogo;

	char		 g_sessions[AK_MAXSESSIONS][AKMSG_NAMEMAX + 1];
	int		 g_nsessions;
	int		 g_session;
	char		 g_lastuser[AKMSG_NAMEMAX + 1];
	uint32_t	 g_flags;	/* AKINIT_* */

	struct au_face	*g_fui;
	struct au_face	*g_fclock;

	struct x11	 g_x;
	struct kbd	 g_kbd;
	struct ui	 g_ui;
};

/* bg.c */
void	bg_load(struct greeter *, struct output *);
void	bg_free(struct output *);

/* ipc.c */
int	ipc_init(struct greeter *);
int	ipc_recv(struct greeter *, struct akmsg *);
int	ipc_send(struct greeter *, struct akmsg *);

/* kbd.c */
int	kbd_init(struct greeter *);
void	kbd_key(struct greeter *, const xcb_key_press_event_t *);
void	kbd_event(struct greeter *, const xcb_generic_event_t *);
void	kbd_next_layout(struct greeter *);
const char *kbd_label(const struct greeter *);

/* ui.c */
void	ui_init(struct greeter *);
void	ui_layout(struct greeter *);
void	ui_paint(struct greeter *);
int	ui_tick(struct greeter *);
void	ui_message(struct greeter *, struct akmsg *);
void	ui_key(struct greeter *, uint32_t, const char *, int, int);
void	ui_click(struct greeter *, int, int);
void	ui_caps(struct greeter *);
void	ui_damage_all(struct greeter *);

/* x11.c */
int	x11_init(struct greeter *);
void	x11_dispatch(struct greeter *, xcb_generic_event_t *);
void	x11_damage(struct greeter *, const struct au_rect *);
void	x11_present(struct greeter *);
const struct output *x11_primary(const struct greeter *);

#endif /* !_GREETER_H_ */
