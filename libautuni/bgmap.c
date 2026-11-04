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
#include <sys/mman.h>
#include <sys/stat.h>

#include <syslog.h>
#include <unistd.h>

#include "akbg.h"
#include "autuni.h"

/*
 * Backgrounds from .akbg files, mapped as they are: the pixels are
 * already in the format of the X server, so nothing is decoded or
 * converted.
 */

int
au_bg_map(struct au_bg *bg, int fd, const char *path, int w, int h)
{
	uint8_t hdr[AKBG_HDRLEN];
	struct akbg_header ah;
	struct stat st;
	void *map;
	int error;

	if (fstat(fd, &st) == -1 || !S_ISREG(st.st_mode) ||
	    pread(fd, hdr, sizeof(hdr), 0) != (ssize_t)sizeof(hdr)) {
		syslog(LOG_WARNING, "%s: cannot read the header", path);
		return -1;
	}
	if ((error = akbg_check(hdr, (uint64_t)st.st_size, (uint32_t)w,
	    (uint32_t)h, &ah)) != AKBG_OK) {
		syslog(LOG_WARNING, "%s: %s", path, akbg_strerror(error));
		return -1;
	}
	map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) {
		syslog(LOG_WARNING, "%s: mmap: %m", path);
		return -1;
	}
	bg->bg_map = map;
	bg->bg_maplen = (size_t)st.st_size;
	/* pixman only reads from a source image. */
	bg->bg_img = pixman_image_create_bits(PIXMAN_x8r8g8b8, w, h,
	    (uint32_t *)((uint8_t *)map + AKBG_HDRLEN), (int)ah.ah_stride);
	if (bg->bg_img == NULL) {
		au_bg_unmap(bg);
		return -1;
	}
	return 0;
}

void
au_bg_unmap(struct au_bg *bg)
{

	if (bg->bg_img != NULL)
		pixman_image_unref(bg->bg_img);
	if (bg->bg_map != NULL)
		(void)munmap(bg->bg_map, bg->bg_maplen);
	bg->bg_img = NULL;
	bg->bg_map = NULL;
	bg->bg_maplen = 0;
}
