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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "akutil.h"
#include "greeter.h"

/*
 * Keyboard input with libxkbcommon.  The keymap is the one of the X
 * server, unless keyboard_layouts lists layouts to switch between; in
 * that case a keymap is compiled from the server's rules with those
 * layouts.  The layout in use is tracked locally, so switching it does
 * not touch the server.
 */

#define KBD_RULES_ATOM	"_XKB_RULES_NAMES"

union xkb_event {
	struct {
		uint8_t		response_type;
		uint8_t		xkbType;
		uint16_t	sequence;
		xcb_timestamp_t	time;
		uint8_t		deviceID;
	} any;
	xcb_xkb_new_keyboard_notify_event_t new_keyboard;
	xcb_xkb_map_notify_event_t map_notify;
	xcb_xkb_state_notify_event_t state_notify;
};

/*
 * Short label for a layout, as shown in the top right corner.
 */
static void
kbd_label_for(const char *layout, size_t len, char *buf)
{
	static const struct {
		const char	*layout;
		const char	*label;
	} names[] = {
		{ "jp", "JIS" },
		{ "us", "US" },
		{ "gb", "UK" },
	};
	size_t i;

	for (i = 0; i < __arraycount(names); i++) {
		if (strlen(names[i].layout) == len &&
		    strncmp(names[i].layout, layout, len) == 0) {
			(void)strlcpy(buf, names[i].label, KBD_LABELMAX);
			return;
		}
	}
	for (i = 0; i < len && i < KBD_LABELMAX - 1; i++)
		buf[i] = (char)toupper((unsigned char)layout[i]);
	buf[i] = '\0';
}

static void
kbd_set_labels(struct kbd *k, const char *list)
{
	const char *p, *e;

	k->k_nlayouts = 0;
	for (p = list; *p != '\0' && k->k_nlayouts < KBD_MAXLAYOUTS; p = e) {
		e = p + strcspn(p, ",");
		if (e > p)
			kbd_label_for(p, (size_t)(e - p),
			    k->k_labels[k->k_nlayouts++]);
		if (*e == ',')
			e++;
	}
}

/*
 * Read the RMLVO the server was configured with.  names points into
 * buf afterwards.
 */
static int
kbd_rules(struct greeter *g, struct xkb_rule_names *names, char *buf,
    size_t len)
{
	xcb_connection_t *conn = g->g_x.x_conn;
	xcb_intern_atom_reply_t *ar;
	xcb_get_property_reply_t *pr;
	const char *field[5];
	size_t n, off;
	int i;

	ar = xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 1,
	    strlen(KBD_RULES_ATOM), KBD_RULES_ATOM), NULL);
	if (ar == NULL)
		return -1;
	pr = xcb_get_property_reply(conn, xcb_get_property(conn, 0,
	    g->g_x.x_screen->root, ar->atom, XCB_ATOM_STRING, 0, 1024), NULL);
	free(ar);
	if (pr == NULL)
		return -1;
	n = (size_t)xcb_get_property_value_length(pr);
	if (n == 0 || n >= len) {
		free(pr);
		return -1;
	}
	(void)memcpy(buf, xcb_get_property_value(pr), n);
	buf[n] = '\0';
	free(pr);

	/* rules, model, layout, variant, options: NUL separated */
	for (i = 0, off = 0; i < 5; i++) {
		field[i] = off < n ? buf + off : "";
		off += strlen(field[i]) + 1;
	}
	names->rules = field[0];
	names->model = field[1];
	names->layout = field[2];
	names->variant = field[3];
	names->options = field[4];
	return 0;
}

static int
kbd_keymap(struct greeter *g)
{
	struct kbd *k = &g->g_kbd;
	struct xkb_rule_names names;
	struct xkb_state *st;
	char buf[1024];
	const char *layouts;

	xkb_state_unref(k->k_state);
	xkb_keymap_unref(k->k_keymap);
	k->k_state = NULL;
	k->k_keymap = NULL;
	k->k_custom = 0;
	k->k_group = 0;

	layouts = g->g_opt[AKOPT_KEYBOARD_LAYOUTS];
	if (kbd_rules(g, &names, buf, sizeof(buf)) == 0) {
		if (layouts[0] != '\0') {
			names.layout = layouts;
			names.variant = "";
			k->k_keymap = xkb_keymap_new_from_names(k->k_ctx,
			    &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
			if (k->k_keymap != NULL)
				k->k_custom = 1;
			else
				syslog(LOG_WARNING, "cannot compile layouts "
				    "%s, using the server keymap", layouts);
		}
		if (!k->k_custom)
			kbd_set_labels(k, names.layout);
	} else
		k->k_nlayouts = 0;

	if (k->k_keymap == NULL)
		k->k_keymap = xkb_x11_keymap_new_from_device(k->k_ctx,
		    g->g_x.x_conn, k->k_device, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (k->k_keymap == NULL)
		return -1;
	if (k->k_custom)
		kbd_set_labels(k, layouts);

	if ((k->k_state = xkb_state_new(k->k_keymap)) == NULL)
		return -1;
	/* Start from the server's modifiers and layout. */
	st = xkb_x11_state_new_from_device(k->k_keymap, g->g_x.x_conn,
	    k->k_device);
	if (st != NULL) {
		k->k_group = (int)xkb_state_serialize_layout(st,
		    XKB_STATE_LAYOUT_EFFECTIVE);
		(void)xkb_state_update_mask(k->k_state,
		    xkb_state_serialize_mods(st, XKB_STATE_MODS_DEPRESSED),
		    xkb_state_serialize_mods(st, XKB_STATE_MODS_LATCHED),
		    xkb_state_serialize_mods(st, XKB_STATE_MODS_LOCKED),
		    0, 0, (xkb_layout_index_t)k->k_group);
		xkb_state_unref(st);
	}
	if (k->k_group >= (int)xkb_keymap_num_layouts(k->k_keymap))
		k->k_group = 0;
	k->k_caps = xkb_state_mod_name_is_active(k->k_state,
	    XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED) > 0;
	return 0;
}

int
kbd_init(struct greeter *g)
{
	static const uint16_t events =
	    XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY |
	    XCB_XKB_EVENT_TYPE_MAP_NOTIFY |
	    XCB_XKB_EVENT_TYPE_STATE_NOTIFY;
	static const uint16_t nkn = XCB_XKB_NKN_DETAIL_KEYCODES;
	static const uint16_t parts =
	    XCB_XKB_MAP_PART_KEY_TYPES | XCB_XKB_MAP_PART_KEY_SYMS |
	    XCB_XKB_MAP_PART_MODIFIER_MAP |
	    XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS |
	    XCB_XKB_MAP_PART_KEY_ACTIONS | XCB_XKB_MAP_PART_VIRTUAL_MODS |
	    XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;
	static const uint16_t state =
	    XCB_XKB_STATE_PART_MODIFIER_BASE |
	    XCB_XKB_STATE_PART_MODIFIER_LATCH |
	    XCB_XKB_STATE_PART_MODIFIER_LOCK |
	    XCB_XKB_STATE_PART_GROUP_BASE |
	    XCB_XKB_STATE_PART_GROUP_LATCH |
	    XCB_XKB_STATE_PART_GROUP_LOCK;
	static const xcb_xkb_select_events_details_t details = {
		.affectNewKeyboard = nkn,
		.newKeyboardDetails = nkn,
		.affectState = state,
		.stateDetails = state,
	};
	struct kbd *k = &g->g_kbd;
	xcb_void_cookie_t ck;
	xcb_generic_error_t *err;

	if (!xkb_x11_setup_xkb_extension(g->g_x.x_conn,
	    XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
	    XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, NULL, NULL,
	    &k->k_xkb_base, NULL)) {
		syslog(LOG_ERR, "no XKB extension");
		return -1;
	}
	if ((k->k_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS)) == NULL)
		return -1;
	if ((k->k_device = xkb_x11_get_core_keyboard_device_id(
	    g->g_x.x_conn)) == -1)
		return -1;

	ck = xcb_xkb_select_events_aux_checked(g->g_x.x_conn,
	    (xcb_xkb_device_spec_t)k->k_device, events, 0, 0, parts, parts,
	    &details);
	if ((err = xcb_request_check(g->g_x.x_conn, ck)) != NULL) {
		free(err);
		return -1;
	}
	return kbd_keymap(g);
}

static void
kbd_caps(struct greeter *g)
{
	struct kbd *k = &g->g_kbd;
	int caps;

	caps = xkb_state_mod_name_is_active(k->k_state, XKB_MOD_NAME_CAPS,
	    XKB_STATE_MODS_LOCKED) > 0;
	if (caps != k->k_caps) {
		k->k_caps = caps;
		ui_caps(g);
	}
}

void
kbd_event(struct greeter *g, const xcb_generic_event_t *gev)
{
	const union xkb_event *ev = (const union xkb_event *)gev;
	struct kbd *k = &g->g_kbd;

	if (ev->any.deviceID != k->k_device)
		return;
	switch (ev->any.xkbType) {
	case XCB_XKB_NEW_KEYBOARD_NOTIFY:
		if ((ev->new_keyboard.changed & XCB_XKB_NKN_DETAIL_KEYCODES) ==
		    0)
			break;
		/* FALLTHROUGH */
	case XCB_XKB_MAP_NOTIFY:
		if (kbd_keymap(g) == -1) {
			syslog(LOG_ERR, "cannot reload the keymap");
			exit(AKTURE_EXIT_ERROR);
		}
		ui_damage_all(g);
		break;
	case XCB_XKB_STATE_NOTIFY:
		/* Modifiers from the server, the layout is our own. */
		(void)xkb_state_update_mask(k->k_state,
		    ev->state_notify.baseMods, ev->state_notify.latchedMods,
		    ev->state_notify.lockedMods, 0, 0,
		    (xkb_layout_index_t)k->k_group);
		kbd_caps(g);
		break;
	}
}

void
kbd_key(struct greeter *g, const xcb_key_press_event_t *ev)
{
	struct kbd *k = &g->g_kbd;
	char utf8[16];
	xkb_keysym_t sym;
	int ctrl, shift;

	sym = xkb_state_key_get_one_sym(k->k_state, ev->detail);
	ctrl = xkb_state_mod_name_is_active(k->k_state, XKB_MOD_NAME_CTRL,
	    XKB_STATE_MODS_EFFECTIVE) > 0;
	shift = xkb_state_mod_name_is_active(k->k_state, XKB_MOD_NAME_SHIFT,
	    XKB_STATE_MODS_EFFECTIVE) > 0;
	if (xkb_state_key_get_utf8(k->k_state, ev->detail, utf8,
	    sizeof(utf8)) >= (int)sizeof(utf8))
		utf8[0] = '\0';
	ui_key(g, sym, utf8, ctrl, shift);
	(void)memset(utf8, 0, sizeof(utf8));
}

void
kbd_next_layout(struct greeter *g)
{
	struct kbd *k = &g->g_kbd;
	xkb_layout_index_t n;

	n = xkb_keymap_num_layouts(k->k_keymap);
	if (n < 2)
		return;
	k->k_group = (int)(((xkb_layout_index_t)k->k_group + 1) % n);
	(void)xkb_state_update_mask(k->k_state,
	    xkb_state_serialize_mods(k->k_state, XKB_STATE_MODS_DEPRESSED),
	    xkb_state_serialize_mods(k->k_state, XKB_STATE_MODS_LATCHED),
	    xkb_state_serialize_mods(k->k_state, XKB_STATE_MODS_LOCKED),
	    0, 0, (xkb_layout_index_t)k->k_group);
}

const char *
kbd_label(const struct greeter *g)
{
	const struct kbd *k = &g->g_kbd;

	if (k->k_group < k->k_nlayouts)
		return k->k_labels[k->k_group];
	return "??";
}
