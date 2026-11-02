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

#include <stdint.h>
#include <string.h>

#include "akmsg.h"
#include "akopt.h"
#include "akparse.h"
#include "pathnames.h"

#define G	AKOPT_F_GREETER

/*
 * The table of akture.conf(5) keys.  Adding a key means adding a line
 * here, a define in akopt.h and a paragraph in akture.conf.5.
 */
const struct akopt akopt_table[AKOPT_NKEYS] = {
	[AKOPT_BACKGROUND] =
	    { "background",		AKOPT_T_NAME,	 G, "" },
	[AKOPT_BACKGROUND_COLOR] =
	    { "background_color",	AKOPT_T_COLOR,	 G, "#1E1410" },
	[AKOPT_ACCENT] =
	    { "accent",			AKOPT_T_COLOR,	 G, "#C8441B" },
	[AKOPT_TEXT_COLOR] =
	    { "text_color",		AKOPT_T_COLOR,	 G, "#F5EFE4" },
	[AKOPT_FONT_UI] =
	    { "font_ui",		AKOPT_T_PATH,	 G,
	      _PATH_AKTURE_FONTDIR "/Outfit.ttf" },
	[AKOPT_FONT_CLOCK] =
	    { "font_clock",		AKOPT_T_PATH,	 G,
	      _PATH_AKTURE_FONTDIR "/ZenKakuGothicNew-Light.ttf" },
	[AKOPT_CLOCK_FORMAT] =
	    { "clock_format",		AKOPT_T_STRING,	 G, "%H:%M" },
	[AKOPT_SHOW_LOGO] =
	    { "show_logo",		AKOPT_T_BOOL,	 G, "yes" },
	[AKOPT_LOGO] =
	    { "logo",			AKOPT_T_STRING,	 G, "" },
	[AKOPT_SCALE] =
	    { "scale",			AKOPT_T_SCALE,	 G, "1" },
	[AKOPT_KEYBOARD_LAYOUTS] =
	    { "keyboard_layouts",	AKOPT_T_LAYOUTS, G, "" },
	[AKOPT_DEFAULT_SESSION] =
	    { "default_session",	AKOPT_T_STRING,	 0, "" },
	[AKOPT_REMEMBER_USER] =
	    { "remember_user",		AKOPT_T_BOOL,	 0, "yes" },
	[AKOPT_ALLOW_POWER] =
	    { "allow_power",		AKOPT_T_BOOL,	 0, "yes" },
	[AKOPT_AUTOLOGIN_USER] =
	    { "autologin_user",		AKOPT_T_NAME,	 0, "" },
	[AKOPT_X_SERVER] =
	    { "x_server",		AKOPT_T_PATH,	 0,
	      _PATH_AKTURE_XSERVER },
	[AKOPT_X_ARGS] =
	    { "x_args",			AKOPT_T_STRING,	 0, "-nolisten tcp" },
	[AKOPT_PREFER_XSESSION] =
	    { "prefer_xsession",	AKOPT_T_BOOL,	 0, "no" },
};

#undef G

/*
 * Return the key for name, or -1 if there is no such key.
 */
int
akopt_lookup(const char *name)
{
	int i;

	for (i = 0; i < AKOPT_NKEYS; i++) {
		if (strcmp(akopt_table[i].ao_name, name) == 0)
			return i;
	}
	return -1;
}

/*
 * The checks below loop up to the length rather than to the NUL, so
 * that Frama-C/Eva sees the bounds.
 */
/*@
  requires valid_read_string(s);
  assigns \nothing;
*/
static int
isprintable(const char *s)
{
	size_t i, len;
	unsigned char c;

	len = strlen(s);
	/*@
	  loop invariant 0 <= i <= len;
	  loop assigns i, c;
	  loop variant len - i;
	*/
	for (i = 0; i < len; i++) {
		c = (unsigned char)s[i];
		if (c < 0x20 || c == 0x7f)
			return 0;
	}
	return 1;
}

/*@
  requires valid_read_string(s);
  assigns \nothing;
*/
static int
isname(const char *s)
{
	size_t i, len;
	unsigned char c;

	len = strlen(s);
	/*@
	  loop invariant 0 <= i <= len;
	  loop assigns i, c;
	  loop variant len - i;
	*/
	for (i = 0; i < len; i++) {
		c = (unsigned char)s[i];
		if (c <= 0x20 || c == 0x7f || c == '/')
			return 0;
	}
	return 1;
}

/*@
  requires valid_read_string(s);
  assigns \nothing;
*/
static int
islayouts(const char *s)
{
	size_t i, len;
	int seen;
	char c;

	len = strlen(s);
	seen = 0;
	/*@
	  loop invariant 0 <= i <= len;
	  loop assigns i, c, seen;
	  loop variant len - i;
	*/
	for (i = 0; i < len; i++) {
		c = s[i];
		if (c == ',') {
			if (!seen)
				return 0;
			seen = 0;
		} else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
		    c == '_' || c == '-') {
			seen = 1;
		} else
			return 0;
	}
	return len == 0 || seen;
}

/*
 * Is val acceptable for key?  Every accepted value also fits into a
 * single AKMSG_INIT_OPTION message.
 */
int
akopt_valid(int key, const char *val)
{
	uint32_t color;
	unsigned int pct;
	int b;

	if (key < 0 || key >= AKOPT_NKEYS)
		return 0;
	if (strlen(val) > AKMSG_TEXTMAX || !isprintable(val))
		return 0;

	switch (akopt_table[key].ao_type) {
	case AKOPT_T_STRING:
		return 1;
	case AKOPT_T_NAME:
		return isname(val);
	case AKOPT_T_PATH:
		return val[0] == '/';
	case AKOPT_T_COLOR:
		return ak_parse_color(val, &color) == 0;
	case AKOPT_T_BOOL:
		return ak_parse_bool(val, &b) == 0;
	case AKOPT_T_SCALE:
		return ak_parse_scale(val, &pct) == 0;
	case AKOPT_T_LAYOUTS:
		return islayouts(val);
	default:
		return 0;
	}
}
