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
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(AKTURE_CAPSICUM)
#include <sys/capsicum.h>
#endif

#include "akbg.h"
#include "akparse.h"
#include "akutil.h"
#include "conv.h"
#include "pathnames.h"

/*
 * akture-conv: convert a PNG or JPEG picture into an .akbg background.
 *
 * The picture is decoded and processed by a child that has dropped
 * root privileges (when run as root) and only returns raw pixels over
 * a pipe; the parent merely writes the file, through a temporary name
 * and rename(2).
 */

#define CONV_BUFSIZE	65536

static __dead void
usage(void)
{

	(void)fprintf(stderr, "usage: %s -s WxH [-m fill|fit|center] "
	    "[-b radius] [-d percent]\n"
	    "\t[-c #RRGGBB] -o output.akbg input.png|jpg\n", getprogname());
	exit(EXIT_FAILURE);
}

static void
parse_size(const char *s, struct convopt *co)
{
	unsigned long w, h;
	char *ep;

	errno = 0;
	w = strtoul(s, &ep, 10);
	if (ep == s || *ep != 'x' || errno != 0)
		errx(EXIT_FAILURE, "%s: size must be WxH", s);
	s = ep + 1;
	h = strtoul(s, &ep, 10);
	if (ep == s || *ep != '\0' || errno != 0)
		errx(EXIT_FAILURE, "%s: size must be WxH", s);
	if (w < 1 || w > CONV_DIMMAX || h < 1 || h > CONV_DIMMAX ||
	    (uint64_t)w * h > CONV_PIXMAX)
		errx(EXIT_FAILURE, "%lux%lu: size out of range", w, h);
	co->co_width = (uint32_t)w;
	co->co_height = (uint32_t)h;
}

static double
parse_number(const char *s, double max, const char *what)
{
	double v;
	char *ep;

	errno = 0;
	v = strtod(s, &ep);
	if (ep == s || *ep != '\0' || errno != 0 || !(v >= 0 && v <= max))
		errx(EXIT_FAILURE, "%s: %s must be between 0 and %g", s, what,
		    max);
	return v;
}

/*
 * Decode and process in the child, writing the pixels to fd.
 */
static __dead void
convert(int in, int out, const struct convopt *co)
{
	struct akcred cred;
	struct image im;
	uint8_t magic[8], *pix;
	size_t len, off;
	ssize_t n;
	FILE *fp;
	int rv;

	if (geteuid() == 0) {
		if (ak_cred_user(AKTURE_USER, &cred) == -1)
			errx(EXIT_FAILURE, "no user %s to drop to", AKTURE_USER);
		cred.ac_ngroups = 1;
		cred.ac_groups[0] = cred.ac_gid;
		if (ak_privdrop(&cred) == -1)
			err(EXIT_FAILURE, "cannot drop privileges");
	}
	if ((fp = fdopen(in, "r")) == NULL)
		err(EXIT_FAILURE, "fdopen");
#if defined(AKTURE_CAPSICUM)
	if (cap_enter() == -1)
		err(EXIT_FAILURE, "cap_enter");
#endif

	if (fread(magic, 1, sizeof(magic), fp) != sizeof(magic))
		errx(EXIT_FAILURE, "input too short");
	rewind(fp);
	if (memcmp(magic, "\x89PNG\r\n\x1a\n", 8) == 0)
		rv = png_decode(fp, &im, co->co_bg);
	else if (magic[0] == 0xff && magic[1] == 0xd8)
		rv = jpeg_decode(fp, &im);
	else
		errx(EXIT_FAILURE, "input is neither PNG nor JPEG");
	if (rv == -1)
		errx(EXIT_FAILURE, "cannot decode the input");
	(void)fclose(fp);

	len = (size_t)co->co_width * co->co_height * 4;
	if ((pix = malloc(len)) == NULL)
		err(EXIT_FAILURE, "malloc");
	if (image_render(&im, co, pix) == -1)
		errx(EXIT_FAILURE, "out of memory");
	image_free(&im);

	for (off = 0; off < len; off += (size_t)n) {
		if ((n = write(out, pix + off, len - off)) <= 0)
			err(EXIT_FAILURE, "write");
	}
	_exit(EXIT_SUCCESS);
}

/*
 * Copy exactly len bytes from the child into the output.
 */
static int
copy_pixels(int from, int to, size_t len)
{
	static uint8_t buf[CONV_BUFSIZE];
	size_t done;
	ssize_t n;

	for (done = 0; done < len; done += (size_t)n) {
		n = read(from, buf, len - done < sizeof(buf) ? len - done :
		    sizeof(buf));
		if (n <= 0)
			return -1;
		if (write(to, buf, (size_t)n) != n)
			return -1;
	}
	/* The child must not send more than announced. */
	return read(from, buf, 1) == 0 ? 0 : -1;
}

int
main(int argc, char **argv)
{
	struct convopt co;
	struct akbg_header h;
	uint8_t hdr[AKBG_HDRLEN];
	char tmp[PATH_MAX];
	const char *output;
	int ch, in, fd, p[2], status;
	pid_t pid;

	(void)memset(&co, 0, sizeof(co));
	co.co_mode = CONV_FILL;
	output = NULL;
	while ((ch = getopt(argc, argv, "b:c:d:m:o:s:")) != -1) {
		switch (ch) {
		case 'b':
			co.co_blur = parse_number(optarg, 500, "radius");
			break;
		case 'c':
			if (ak_parse_color(optarg, &co.co_bg) == -1)
				errx(EXIT_FAILURE, "%s: colour must be "
				    "#RRGGBB", optarg);
			co.co_bg &= 0xffffff;
			break;
		case 'd':
			co.co_dark = parse_number(optarg, 100, "darkness") /
			    100;
			break;
		case 'm':
			if (strcmp(optarg, "fill") == 0)
				co.co_mode = CONV_FILL;
			else if (strcmp(optarg, "fit") == 0)
				co.co_mode = CONV_FIT;
			else if (strcmp(optarg, "center") == 0)
				co.co_mode = CONV_CENTER;
			else
				usage();
			break;
		case 'o':
			output = optarg;
			break;
		case 's':
			parse_size(optarg, &co);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1 || output == NULL || co.co_width == 0)
		usage();

	if ((in = open(argv[0], O_RDONLY | O_CLOEXEC)) == -1)
		err(EXIT_FAILURE, "%s", argv[0]);
	if (pipe(p) == -1)
		err(EXIT_FAILURE, "pipe");
	if ((pid = fork()) == -1)
		err(EXIT_FAILURE, "fork");
	if (pid == 0) {
		(void)close(p[0]);
		convert(in, p[1], &co);
	}
	(void)close(p[1]);
	(void)close(in);

	if (snprintf(tmp, sizeof(tmp), "%s.XXXXXX", output) >=
	    (int)sizeof(tmp))
		errx(EXIT_FAILURE, "%s: name too long", output);
	if ((fd = mkstemp(tmp)) == -1)
		err(EXIT_FAILURE, "%s", tmp);
	akbg_header_init(&h, co.co_width, co.co_height);
	akbg_header_encode(&h, hdr);
	if (write(fd, hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr) ||
	    copy_pixels(p[0], fd, (size_t)h.ah_stride * h.ah_height) == -1) {
		(void)kill(pid, SIGTERM);
		(void)waitpid(pid, NULL, 0);
		(void)unlink(tmp);
		errx(EXIT_FAILURE, "conversion failed");
	}
	if (waitpid(pid, &status, 0) == -1 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0) {
		(void)unlink(tmp);
		errx(EXIT_FAILURE, "conversion failed");
	}
	if (fchmod(fd, 0644) == -1 || fsync(fd) == -1 || close(fd) == -1 ||
	    rename(tmp, output) == -1) {
		(void)unlink(tmp);
		err(EXIT_FAILURE, "%s", output);
	}
	return EXIT_SUCCESS;
}
