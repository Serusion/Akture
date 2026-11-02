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
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "akutil.h"

/*
 * Ghost state for Frama-C: records how far ak_privdrop() has come.
 * The ACSL contracts of setgroups(2), setgid(2) and setuid(2) in
 * verify/stubs/ require the previous step, so WP proves the order
 * groups -> gid -> uid.
 */
/*@ ghost int ak_pd_step = 0; */

/*
 * Clear memory in a way the compiler may not optimise away.
 */
/*@
  requires \valid((char *)p + (0 .. len - 1));
  assigns ((char *)p)[0 .. len - 1];
  ensures \forall integer i; 0 <= i < len ==> ((char *)p)[i] == 0;
*/
void
ak_explicit_zero(void *p, size_t len)
{

#if defined(__NetBSD__)
	explicit_memset(p, 0, len);
#else
	explicit_bzero(p, len);
#endif
}

/*
 * Fill in the credentials of the named user, including its
 * supplementary groups.
 */
int
ak_cred_user(const char *name, struct akcred *c)
{
	struct passwd *pw;
	int ngroups;

	if ((pw = getpwnam(name)) == NULL)
		return -1;

	c->ac_uid = pw->pw_uid;
	c->ac_gid = pw->pw_gid;
	ngroups = AK_NGROUPS_MAX;
	if (getgrouplist(name, pw->pw_gid, c->ac_groups, &ngroups) == -1)
		return -1;
	if (ngroups < 1 || ngroups > AK_NGROUPS_MAX)
		return -1;
	c->ac_ngroups = ngroups;
	return 0;
}

/*
 * Permanently drop root privileges: groups, then gid, then uid.
 * On failure the caller must not continue; it has to exit without
 * running anything on behalf of the user.
 */
/*@
  requires \valid_read(c);
  requires 1 <= c->ac_ngroups <= AK_NGROUPS_MAX;
  requires ak_pd_step == 0;
  assigns ak_pd_step;
  ensures \result == 0 || \result == -1;
  ensures \result == 0 ==> ak_pd_step == 3;
  ensures \result == -1 ==> ak_pd_step < 3;
*/
int
ak_privdrop(const struct akcred *c)
{

	if (setgroups(c->ac_ngroups, c->ac_groups) == -1)
		return -1;
	/*@ ghost ak_pd_step = 1; */
	if (setgid(c->ac_gid) == -1)
		return -1;
	/*@ ghost ak_pd_step = 2; */
	if (setuid(c->ac_uid) == -1)
		return -1;

	/* Do not trust the calls above blindly. */
	if (getuid() != c->ac_uid || geteuid() != c->ac_uid ||
	    getgid() != c->ac_gid || getegid() != c->ac_gid)
		return -1;
	if (c->ac_uid != 0 && setuid(0) != -1)
		return -1;
	/*@ ghost ak_pd_step = 3; */
	return 0;
}

/*
 * Atomically replace path with the given contents: write a temporary
 * file next to it, set owner and mode, and rename(2) it into place.
 */
int
ak_write_file(const char *path, const void *buf, size_t len, mode_t mode,
    uid_t uid, gid_t gid)
{
	char tmp[PATH_MAX];
	int fd, serrno;

	if (snprintf(tmp, sizeof(tmp), "%s.XXXXXX", path) >= (int)sizeof(tmp)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	if ((fd = mkstemp(tmp)) == -1)
		return -1;
	if (fchown(fd, uid, gid) == -1 || fchmod(fd, mode) == -1)
		goto bad;
	if (write(fd, buf, len) != (ssize_t)len)
		goto bad;
	if (fsync(fd) == -1)
		goto bad;
	if (close(fd) == -1) {
		fd = -1;
		goto bad;
	}
	if (rename(tmp, path) == -1) {
		fd = -1;
		goto bad;
	}
	return 0;

bad:
	serrno = errno;
	if (fd != -1)
		(void)close(fd);
	(void)unlink(tmp);
	errno = serrno;
	return -1;
}
