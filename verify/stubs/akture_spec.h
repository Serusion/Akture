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

/*
 * ACSL contracts of the library functions the verified parts of the
 * master call.  Only what is used is specified.  Included before every
 * file given to Frama-C (see verify/Makefile).
 */

#ifndef _AKTURE_SPEC_H_
#define _AKTURE_SPEC_H_

#include <sys/types.h>

#include <stddef.h>

/* POSIX, but missing from the Frama-C libc. */
#ifndef NAME_MAX
#define NAME_MAX	255
#endif

/*@ ghost extern int ak_pd_step; */

/*
 * Privilege dropping: each step requires the previous one, so that WP
 * proves the order setgroups -> setgid -> setuid in ak_privdrop().
 */
/* Declared with size_t as in the Frama-C libc; int on NetBSD. */
/*@
  requires ak_pd_step == 0;
  requires \valid_read(list + (0 .. size - 1));
  terminates \true;
  exits \false;
  assigns \nothing;
  ensures \result == 0 || \result == -1;
*/
int	setgroups(size_t size, const gid_t *list);

/*@
  requires ak_pd_step == 1;
  terminates \true;
  exits \false;
  assigns \nothing;
  ensures \result == 0 || \result == -1;
*/
int	setgid(gid_t gid);

/*@
  requires ak_pd_step >= 2;
  terminates \true;
  exits \false;
  assigns \nothing;
  ensures \result == 0 || \result == -1;
*/
int	setuid(uid_t uid);

/*@
  requires \valid((char *)b + (0 .. len - 1));
  terminates \true;
  exits \false;
  assigns ((char *)b)[0 .. len - 1];
  ensures \forall integer i; 0 <= i < len ==> ((char *)b)[i] == 0;
*/
void	explicit_bzero(void *b, size_t len);

/*
 * The library functions below always return.  These clauses are merged
 * with the contracts of the Frama-C libc, which leave them out.
 */
/*@ terminates \true; exits \false; */
void	*memset(void *s, int c, size_t n);
/*@ terminates \true; exits \false; */
size_t	strlen(const char *s);
/*@ terminates \true; exits \false; */
size_t	strnlen(const char *s, size_t n);
/*@ terminates \true; exits \false; */
int	strcmp(const char *s1, const char *s2);


/*@ terminates \true; exits \false; */
uid_t	getuid(void);
/*@ terminates \true; exits \false; */
uid_t	geteuid(void);
/*@ terminates \true; exits \false; */
gid_t	getgid(void);
/*@ terminates \true; exits \false; */
gid_t	getegid(void);

#endif /* !_AKTURE_SPEC_H_ */
