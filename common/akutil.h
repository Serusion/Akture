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

#ifndef _AKUTIL_H_
#define _AKUTIL_H_

#include <sys/types.h>

#include <stddef.h>

#ifndef __arraycount
#define __arraycount(a)	(sizeof(a) / sizeof((a)[0]))
#endif

/*
 * Credentials applied by ak_privdrop().  The group list is fixed-size
 * so that the master never allocates memory.
 */
#define AK_NGROUPS_MAX	64

struct akcred {
	uid_t		ac_uid;
	gid_t		ac_gid;
	int		ac_ngroups;
	gid_t		ac_groups[AK_NGROUPS_MAX];
};

void	ak_explicit_zero(void *, size_t);
int	ak_cred_user(const char *, struct akcred *);
int	ak_privdrop(const struct akcred *);
int	ak_write_file(const char *, const void *, size_t, mode_t, uid_t,
	    gid_t);

#endif /* !_AKUTIL_H_ */
