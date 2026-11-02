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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "akture.h"

/*
 * Reader for akture.conf(5).  conf_line() is a pure function over one
 * line and is shared with the fuzzer; conf_load() only adds the file
 * handling and the warnings.
 */

void
conf_defaults(struct akconf *ac)
{
	int i;

	for (i = 0; i < AKOPT_NKEYS; i++)
		(void)strlcpy(ac->ac_val[i], akopt_table[i].ao_default,
		    sizeof(ac->ac_val[i]));
}

/*
 * Apply one line.  Returns 0 if the line was empty or applied, or -1
 * with *why set if it was ignored; the previous value is kept then.
 */
int
conf_line(struct akconf *ac, const char *line, const char **why)
{
	char key[AKPARSE_LINEMAX], val[AKPARSE_LINEMAX];
	int k;

	switch (ak_parse_line(line, key, val)) {
	case AKPARSE_EMPTY:
		return 0;
	case AKPARSE_PAIR:
		break;
	default:
		*why = "syntax error";
		return -1;
	}
	if ((k = akopt_lookup(key)) == -1) {
		*why = "unknown key";
		return -1;
	}
	if (!akopt_valid(k, val)) {
		*why = "invalid value";
		return -1;
	}
	(void)strlcpy(ac->ac_val[k], val, sizeof(ac->ac_val[k]));
	return 0;
}

/*
 * Read the configuration file.  A missing file is not an error: the
 * defaults are used.  Bad lines are logged and skipped.
 */
int
conf_load(struct akconf *ac, const char *path)
{
	char line[AKPARSE_LINEMAX];
	const char *why;
	FILE *fp;
	size_t len;
	unsigned int lineno;
	int skip;

	conf_defaults(ac);
	if ((fp = fopen(path, "re")) == NULL) {
		if (errno == ENOENT)
			return 0;
		syslog(LOG_ERR, "%s: %m", path);
		return -1;
	}

	/*
	 * fgets(3) terminates the line; the explicit zeros only make
	 * that visible to Frama-C/Eva, which checks this function.
	 */
	(void)memset(line, 0, sizeof(line));
	lineno = 0;
	skip = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		line[sizeof(line) - 1] = '\0';
		len = strlen(line);
		if (len > 0 && line[len - 1] != '\n' && !feof(fp)) {
			/* Too long: drop the rest of it. */
			if (!skip)
				syslog(LOG_WARNING, "%s:%u: line too long",
				    path, lineno + 1);
			skip = 1;
			continue;
		}
		lineno++;
		if (skip) {
			skip = 0;
			continue;
		}
		why = NULL;
		if (conf_line(ac, line, &why) == -1)
			syslog(LOG_WARNING, "%s:%u: %s, using the default",
			    path, lineno, why);
	}
	if (ferror(fp))
		syslog(LOG_ERR, "%s: %m", path);
	(void)fclose(fp);
	return 0;
}

const char *
conf_str(const struct akture *a, int key)
{

	return a->a_conf.ac_val[key];
}

int
conf_bool(const struct akture *a, int key)
{
	int b;

	if (ak_parse_bool(a->a_conf.ac_val[key], &b) == -1)
		return 0;
	return b;
}
