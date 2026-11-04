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

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "akbg.h"
#include "autuni.h"

#define W	64
#define H	32

static uint32_t pixels[W * H];

/* Write an .akbg of W by H pixels with a pattern, and open it. */
static int
make_akbg(const char *path)
{
	uint8_t hdr[AKBG_HDRLEN];
	struct akbg_header ah;
	int fd, i;

	for (i = 0; i < W * H; i++)
		pixels[i] = 0x00ffffffU & ((uint32_t)i * 2654435761U);
	akbg_header_init(&ah, W, H);
	akbg_header_encode(&ah, hdr);
	fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd != -1);
	ATF_REQUIRE_EQ(write(fd, hdr, sizeof(hdr)), (ssize_t)sizeof(hdr));
	ATF_REQUIRE_EQ(write(fd, pixels, sizeof(pixels)),
	    (ssize_t)sizeof(pixels));
	return fd;
}

ATF_TC(map);
ATF_TC_HEAD(map, tc)
{
	atf_tc_set_md_var(tc, "descr", "an .akbg is mapped pixel for pixel");
}
ATF_TC_BODY(map, tc)
{
	struct au_bg bg;
	const uint32_t *img;
	int fd, i;

	fd = make_akbg("bg.akbg");
	(void)memset(&bg, 0, sizeof(bg));
	ATF_REQUIRE_EQ(au_bg_map(&bg, fd, "bg.akbg", W, H), 0);
	ATF_REQUIRE(bg.bg_img != NULL);
	ATF_CHECK_EQ(pixman_image_get_width(bg.bg_img), W);
	ATF_CHECK_EQ(pixman_image_get_height(bg.bg_img), H);
	img = (const uint32_t *)pixman_image_get_data(bg.bg_img);
	for (i = 0; i < W * H && img[i] == pixels[i]; i++)
		continue;
	ATF_CHECK_EQ(i, W * H);

	au_bg_unmap(&bg);
	ATF_CHECK(bg.bg_img == NULL);
	ATF_CHECK(bg.bg_map == NULL);
	ATF_CHECK_EQ(bg.bg_maplen, 0);
	au_bg_unmap(&bg);		/* nothing mapped: nothing done */
	(void)close(fd);
}

ATF_TC(reject);
ATF_TC_HEAD(reject, tc)
{
	atf_tc_set_md_var(tc, "descr", "files that do not fit are not mapped");
}
ATF_TC_BODY(reject, tc)
{
	struct au_bg bg;
	int fd;

	fd = make_akbg("bg.akbg");
	(void)memset(&bg, 0, sizeof(bg));
	/* Made for another size of output. */
	ATF_CHECK_EQ(au_bg_map(&bg, fd, "bg.akbg", W + 1, H), -1);
	ATF_CHECK(bg.bg_img == NULL);
	ATF_CHECK(bg.bg_map == NULL);
	/* Pixels missing, then even the header. */
	ATF_REQUIRE_EQ(ftruncate(fd, AKBG_HDRLEN + 4), 0);
	ATF_CHECK_EQ(au_bg_map(&bg, fd, "bg.akbg", W, H), -1);
	ATF_REQUIRE_EQ(ftruncate(fd, 16), 0);
	ATF_CHECK_EQ(au_bg_map(&bg, fd, "bg.akbg", W, H), -1);
	(void)close(fd);
	/* Not a regular file. */
	ATF_REQUIRE((fd = open(".", O_RDONLY)) != -1);
	ATF_CHECK_EQ(au_bg_map(&bg, fd, ".", W, H), -1);
	ATF_CHECK(bg.bg_img == NULL);
	(void)close(fd);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, map);
	ATF_TP_ADD_TC(tp, reject);
	return atf_no_error();
}
