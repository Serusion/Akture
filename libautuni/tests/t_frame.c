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

#include <stddef.h>

#include <atf-c.h>

#include "autuni.h"

/*
 * What a frame does without an X server: its damage.  Allocating the
 * buffer and presenting it need a server with MIT-SHM.
 */

ATF_TC(damage);
ATF_TC_HEAD(damage, tc)
{
	atf_tc_set_md_var(tc, "descr", "damage adds up, empty rectangles not");
}
ATF_TC_BODY(damage, tc)
{
	struct au_frame fr;
	struct au_rect r;
	pixman_box32_t *e;

	au_frame_init(&fr, NULL);
	ATF_CHECK(fr.fr_img == NULL);
	ATF_CHECK(!pixman_region32_not_empty(&fr.fr_damage));

	r.r_x = 0;
	r.r_y = 0;
	r.r_w = 10;
	r.r_h = 10;
	au_frame_damage(&fr, &r);
	r.r_x = 5;
	r.r_y = 5;
	au_frame_damage(&fr, &r);
	r.r_w = 0;			/* empty: ignored */
	r.r_x = 100;
	au_frame_damage(&fr, &r);
	r.r_w = 10;
	r.r_h = -1;
	au_frame_damage(&fr, &r);

	e = pixman_region32_extents(&fr.fr_damage);
	ATF_CHECK_EQ(e->x1, 0);
	ATF_CHECK_EQ(e->y1, 0);
	ATF_CHECK_EQ(e->x2, 15);
	ATF_CHECK_EQ(e->y2, 15);

	au_frame_free(&fr);		/* no buffer: nothing done */
	ATF_CHECK(fr.fr_img == NULL);
	pixman_region32_fini(&fr.fr_damage);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, damage);
	return atf_no_error();
}
