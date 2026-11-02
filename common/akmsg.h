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

#ifndef _AKMSG_H_
#define _AKMSG_H_

#include <stddef.h>
#include <stdint.h>

/*
 * Messages exchanged between the master (root) and the greeter
 * (unprivileged) over a SOCK_SEQPACKET socketpair.
 *
 * Every message is exactly AKMSG_SIZE bytes.  All integers are
 * little-endian 32-bit unsigned.  The layout is
 *
 *	0	type
 *	4	sequence number (per direction, starting at 0)
 *	8	nargs integer arguments
 *	8+4n	string length, then the string bytes (if the type has one)
 *
 * and every byte after the used part must be zero.  The per-type
 * layout is described by akmsg_layouts[] in akmsg.c.
 */
#define AKMSG_SIZE	512
#define AKMSG_HDRLEN	8
#define AKMSG_MAXARGS	3

#define AKMSG_NAMEMAX	64	/* user and session names */
#define AKMSG_TEXTMAX	400	/* prompts, option values */
#define AKMSG_RESPMAX	256	/* answers to prompts (passwords) */
#define AKMSG_STRMAX	AKMSG_TEXTMAX

#define AKMSG_NONE		0	/* never sent */

/* greeter -> master */
#define AKMSG_BEGIN_AUTH	1	/* arg: session id, str: user */
#define AKMSG_RESPONSE		2	/* str: answer */
#define AKMSG_CANCEL		3
#define AKMSG_POWER		4	/* arg: AKPOWER_* */

/* master -> greeter */
#define AKMSG_INIT_OPTION	16	/* arg: AKOPT_* key, str: value */
#define AKMSG_INIT_SESSION	17	/* arg: session id, str: name */
#define AKMSG_INIT		18	/* args: flags, nsessions, default */
					/* str: last user */
#define AKMSG_PROMPT		19	/* arg: AKPROMPT_*, str: text */
#define AKMSG_AUTH_RESULT	20	/* arg: AKRESULT_* */

#define AKPOWER_SHUTDOWN	1
#define AKPOWER_REBOOT		2

#define AKPROMPT_SECRET		1	/* echo off */
#define AKPROMPT_VISIBLE	2	/* echo on */
#define AKPROMPT_INFO		3
#define AKPROMPT_ERROR		4

#define AKRESULT_FAIL		0
#define AKRESULT_OK		1

/* AKMSG_INIT flags */
#define AKINIT_POWER		0x01	/* power actions are allowed */
#define AKINIT_SESSIONFAIL	0x02	/* the last session ended at once */
#define AKINIT_ALL		0x03

/* Upper bound on the number of sessions offered to the greeter. */
#define AK_MAXSESSIONS		16

/* The greeter finds its end of the socketpair on this descriptor. */
#define AKTURE_GREETER_FD	3

/* Exit status of the greeter that the master acts upon. */
#define AKTURE_EXIT_OK		0
#define AKTURE_EXIT_ERROR	1	/* restart it */
#define AKTURE_EXIT_NOFONT	3	/* give up and leave the console */

struct akmsg {
	uint32_t	am_type;
	uint32_t	am_seq;
	uint32_t	am_args[AKMSG_MAXARGS];
	uint32_t	am_len;			/* length of am_str */
	char		am_str[AKMSG_STRMAX + 1]; /* always NUL terminated */
};

/* akmsg_decode() errors */
#define AKMSG_OK	0
#define AKMSG_EBADTYPE	1	/* unknown type */
#define AKMSG_EBADARG	2	/* argument out of range */
#define AKMSG_EBADLEN	3	/* string length inconsistent */
#define AKMSG_EBADSTR	4	/* forbidden byte in string */
#define AKMSG_EBADPAD	5	/* garbage after the used part */

void	akmsg_init(struct akmsg *, uint32_t);
int	akmsg_setstr(struct akmsg *, const char *);
int	akmsg_encode(const struct akmsg *, uint8_t *);
int	akmsg_decode(const uint8_t *, struct akmsg *);
void	akmsg_clear(struct akmsg *);
const char *akmsg_strerror(int);

#endif /* !_AKMSG_H_ */
