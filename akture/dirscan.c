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

#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <syslog.h>

#include "akture.h"

/*
 * Collect the names in a directory, sorted, skipping dot files and
 * editor backups.  At most max names are returned; the rest are
 * reported and ignored.
 */
int
dir_scan(const char *path, char (*names)[NAME_MAX + 1], int max)
{
	char tmp[NAME_MAX + 1];
	struct dirent *dp;
	DIR *dirp;
	size_t len;
	int i, j, n, dropped;

	if ((dirp = opendir(path)) == NULL)
		return -1;
	n = dropped = 0;
	while ((dp = readdir(dirp)) != NULL) {
		len = strlen(dp->d_name);
		if (dp->d_name[0] == '.' || len == 0 ||
		    dp->d_name[len - 1] == '~' || len > NAME_MAX)
			continue;
		if (n == max) {
			dropped++;
			continue;
		}
		(void)strlcpy(names[n++], dp->d_name, NAME_MAX + 1);
	}
	(void)closedir(dirp);
	if (dropped > 0)
		syslog(LOG_WARNING, "%s: %d entries ignored", path, dropped);

	/* Insertion sort: the lists are tiny. */
	for (i = 1; i < n; i++) {
		(void)strlcpy(tmp, names[i], sizeof(tmp));
		for (j = i; j > 0 && strcmp(names[j - 1], tmp) > 0; j--)
			(void)strlcpy(names[j], names[j - 1], NAME_MAX + 1);
		(void)strlcpy(names[j], tmp, NAME_MAX + 1);
	}
	return n;
}
