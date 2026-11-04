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
#include <xcb/shm.h>
#include <xcb/xcb.h>

#include "autuni.h"

/*
 * A frame buffer for a window: an image in MIT-SHM shared memory,
 * drawn with pixman and put to the window with ShmPutImage.  The parts
 * drawn are recorded as damage, and only those are sent.
 */

void
au_frame_init(struct au_frame *fr, xcb_connection_t *conn)
{

	(void)memset(fr, 0, sizeof(*fr));
	fr->fr_conn = conn;
	pixman_region32_init(&fr->fr_damage);
}

/*
 * (Re)allocate the shared buffer for a frame of w by h pixels.
 */
int
au_frame_alloc(struct au_frame *fr, int w, int h)
{
	xcb_generic_error_t *err;
	size_t size;
	int id;

	au_frame_free(fr);
	fr->fr_width = w;
	fr->fr_height = h;

	size = (size_t)w * (size_t)h * 4;
	if ((id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600)) == -1) {
		syslog(LOG_ERR, "shmget: %m");
		return -1;
	}
	fr->fr_shmaddr = shmat(id, NULL, 0);
	if (fr->fr_shmaddr == (void *)-1) {
		syslog(LOG_ERR, "shmat: %m");
		(void)shmctl(id, IPC_RMID, NULL);
		return -1;
	}
	fr->fr_shmseg = xcb_generate_id(fr->fr_conn);
	err = xcb_request_check(fr->fr_conn, xcb_shm_attach_checked(
	    fr->fr_conn, fr->fr_shmseg, (uint32_t)id, 0));
	/* Both sides are attached (or failed); nobody else may attach. */
	(void)shmctl(id, IPC_RMID, NULL);
	if (err != NULL) {
		free(err);
		syslog(LOG_ERR, "MIT-SHM attach failed");
		(void)shmdt(fr->fr_shmaddr);
		return -1;
	}
	fr->fr_img = pixman_image_create_bits(PIXMAN_x8r8g8b8, w, h,
	    fr->fr_shmaddr, w * 4);
	return fr->fr_img != NULL ? 0 : -1;
}

void
au_frame_free(struct au_frame *fr)
{

	if (fr->fr_img == NULL)
		return;
	pixman_image_unref(fr->fr_img);
	xcb_shm_detach(fr->fr_conn, fr->fr_shmseg);
	(void)shmdt(fr->fr_shmaddr);
	fr->fr_img = NULL;
}

void
au_frame_damage(struct au_frame *fr, const struct au_rect *r)
{

	if (r->r_w <= 0 || r->r_h <= 0)
		return;
	pixman_region32_union_rect(&fr->fr_damage, &fr->fr_damage,
	    r->r_x, r->r_y, (unsigned int)r->r_w, (unsigned int)r->r_h);
}

/*
 * Send the damaged parts of the frame to the drawable and wait until
 * the server has taken them, so that the next paint cannot race with
 * the copy.
 */
void
au_frame_present(struct au_frame *fr, xcb_drawable_t d, xcb_gcontext_t gc)
{
	pixman_box32_t *b;
	int i, n;

	pixman_region32_intersect_rect(&fr->fr_damage, &fr->fr_damage, 0, 0,
	    (unsigned int)fr->fr_width, (unsigned int)fr->fr_height);
	b = pixman_region32_rectangles(&fr->fr_damage, &n);
	for (i = 0; i < n; i++) {
		xcb_shm_put_image(fr->fr_conn, d, gc,
		    (uint16_t)fr->fr_width, (uint16_t)fr->fr_height,
		    (uint16_t)b[i].x1, (uint16_t)b[i].y1,
		    (uint16_t)(b[i].x2 - b[i].x1),
		    (uint16_t)(b[i].y2 - b[i].y1),
		    (int16_t)b[i].x1, (int16_t)b[i].y1, 24,
		    XCB_IMAGE_FORMAT_Z_PIXMAP, 0, fr->fr_shmseg, 0);
	}
	free(xcb_get_input_focus_reply(fr->fr_conn,
	    xcb_get_input_focus(fr->fr_conn), NULL));
	pixman_region32_clear(&fr->fr_damage);
}
