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

#ifndef _AKTURE_H_
#define _AKTURE_H_

#include <sys/types.h>

#include <security/pam_appl.h>

#include <limits.h>
#include <stdint.h>
#include <time.h>

#include "akmsg.h"
#include "akopt.h"
#include "akparse.h"
#include "akutil.h"

/*
 * States of the master.  The transitions are in state.c; a session is
 * only started in AKS_SESSION, which is only entered from AKS_AUTH on
 * AKE_AUTH_OK.
 */
#define AKS_IDLE	0	/* greeter shown, waiting */
#define AKS_AUTH	1	/* PAM conversation running */
#define AKS_SESSION	2	/* user session running */

/*
 * Events.  Messages from the greeter use their AKMSG_* type; the
 * others are produced by the master itself.
 */
#define AKE_BEGIN_AUTH	AKMSG_BEGIN_AUTH
#define AKE_RESPONSE	AKMSG_RESPONSE
#define AKE_CANCEL	AKMSG_CANCEL
#define AKE_POWER	AKMSG_POWER
#define AKE_AUTH_OK	100	/* pam_authenticate and pam_acct_mgmt agreed */
#define AKE_AUTH_FAIL	101
#define AKE_SESSION_END	102

/* Failure delay: from the 3rd failure on, 2 more seconds each, max 30. */
#define AK_DELAY_FIRST	3
#define AK_DELAY_STEP	2
#define AK_DELAY_MAX	30

/* Timeouts and limits */
#define AK_XSTART_TRIES		3	/* attempts to start the X server */
#define AK_XSTART_TIMEOUT	30	/* seconds to wait for it */
#define AK_GREETER_FAILMAX	5	/* crashes ... */
#define AK_GREETER_FAILWIN	60	/* ... within this many seconds */
#define AK_AUTH_TIMEOUT		120	/* seconds to wait for an answer */
#define AK_KILL_TIMEOUT		5	/* seconds before SIGKILL */
#define AK_SESSION_MINTIME	10	/* shorter sessions count as failed */
#define AK_HOOK_TIMEOUT		10	/* seconds a hook may run */
#define AK_MAXHOOKS		32
#define AK_MAXARGS		32	/* argv of the X server, sessions */
#define AK_COOKIELEN		16	/* MIT-MAGIC-COOKIE-1 */

/* Children of the master */
#define CHILD_X		0
#define CHILD_GREETER	1
#define CHILD_SESSION	2
#define CHILD_NKINDS	3

struct child {
	pid_t		c_pid;		/* -1 when not running */
	int		c_status;	/* from waitpid(2) once exited */
	time_t		c_start;
};

struct session {
	char		s_id[NAME_MAX + 1];	/* file name, given to hooks */
	char		s_name[AKMSG_NAMEMAX + 1];	/* shown */
	char		s_exec[AKPARSE_LINEMAX];
};

struct akconf {
	char		ac_val[AKOPT_NKEYS][AKMSG_TEXTMAX + 1];
};

struct akture {
	int		 a_debug;
	int		 a_state;		/* AKS_* */
	int		 a_quit;		/* SIGTERM seen */
	int		 a_kq;

	struct akconf	 a_conf;
	struct session	 a_sessions[AK_MAXSESSIONS];
	int		 a_nsessions;
	struct akcred	 a_gcred;		/* the greeter user */

	struct child	 a_child[CHILD_NKINDS];
	uint8_t		 a_cookie[AK_COOKIELEN];

	int		 a_gsock;		/* -1 when not connected */
	uint32_t	 a_gsseq;		/* next sequence numbers */
	uint32_t	 a_grseq;
	int		 a_gfails;		/* recent greeter crashes */
	time_t		 a_gfailstart;
	uint32_t	 a_initflags;		/* AKINIT_SESSIONFAIL */
	int		 a_shown;		/* greeter-shown hook ran */

	pam_handle_t	*a_pamh;
	char		 a_user[AKMSG_NAMEMAX + 1];
	uint32_t	 a_session;		/* index into a_sessions */
	unsigned int	 a_nfail;		/* consecutive failures */
};

/* auth.c */
#define AUTH_OK		0
#define AUTH_FAIL	1
#define AUTH_CANCEL	2	/* the greeter asked to stop */
#define AUTH_ABORT	3	/* greeter gone or misbehaving */
int	auth_greeter(struct akture *, const struct akmsg *);
int	auth_autologin(struct akture *, const char *);
void	auth_end(struct akture *, int);

/* conf.c */
void	conf_defaults(struct akconf *);
int	conf_line(struct akconf *, const char *, const char **);
int	conf_load(struct akconf *, const char *);
const char *conf_str(const struct akture *, int);
int	conf_bool(const struct akture *, int);

/* dirscan.c */
int	dir_scan(const char *, char (*)[NAME_MAX + 1], int);

/* event.c */
#define EV_TIMEOUT	0
#define EV_READ		1	/* the greeter socket is readable */
#define EV_CHILD	2	/* a child changed state; reaped */
#define EV_QUIT		3	/* asked to terminate */
#define EV_XREADY	4	/* SIGUSR1 from the X server */
int	ev_init(struct akture *);
int	ev_watch(struct akture *, int, int);
int	ev_wait(struct akture *, int);
pid_t	child_spawn(struct akture *, int);
int	child_running(const struct akture *, int);
int	child_wait(struct akture *, int, int);
void	child_stop(struct akture *, int);

/* greeter.c */
int	greeter_start(struct akture *);
void	greeter_stop(struct akture *);
int	greeter_send(struct akture *, struct akmsg *);
int	greeter_recv(struct akture *, struct akmsg *);
int	greeter_result(struct akture *, uint32_t);

/* hook.c */
void	hook_run(struct akture *, const char *, const char *, const char *);

/* lastuser.c */
int	lastuser_read(char *, size_t);
void	lastuser_write(const char *);

/* power.c */
void	power_exec(struct akture *, uint32_t);

/* session.c */
int	sessions_load(struct akture *);
int	session_lookup(const struct akture *, const char *);
int	session_run(struct akture *);

/* state.c */
int	akstate_valid(int);
int	akstate_accepts(int, uint32_t);
int	akstate_next(int, int, int *);
unsigned int akstate_fail_delay(unsigned int);

/* xauth.c */
#define XAUTH_FAMILY_LOCAL	256
#define XAUTH_FAMILY_WILD	65535
#define XAUTH_BUFSIZE		65536
size_t	xauth_entries(uint8_t *, size_t, const char *, const char *,
	    const uint8_t *);
size_t	xauth_filter(const uint8_t *, size_t, const char *, uint8_t *,
	    size_t);
int	xauth_write(const char *, const uint8_t *, uid_t, gid_t);
int	xauth_merge(const char *, const uint8_t *);

/* xserver.c */
int	xserver_start(struct akture *);
void	xserver_stop(struct akture *);

/* md_*.c: machine dependent parts */
int	md_xserver_args(char **, int, int);
void	md_console(void);

#endif /* !_AKTURE_H_ */
