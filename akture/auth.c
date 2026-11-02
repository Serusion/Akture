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

#include <security/pam_appl.h>

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "akture.h"
#include "pathnames.h"

/*
 * Authentication.  Akture never looks at a password itself: PAM does
 * all of it, and the conversation function relays PAM's questions to
 * the greeter as AKMSG_PROMPT and its answers back as AKMSG_RESPONSE.
 *
 * The PAM API forces two exceptions to the rules of the master: the
 * conversation is called through a function pointer, and responses
 * must be allocated with malloc(3) because the modules free them.
 */

struct authctx {
	struct akture	*ac_a;
	int		 ac_interactive;	/* greeter present */
	int		 ac_result;		/* AUTH_CANCEL, AUTH_ABORT */
};

static int	auth_conv(int, const struct pam_message **,
		    struct pam_response **, void *);

/* PAM keeps a pointer to the conversation for the whole handle. */
static struct authctx auth_ctx;
static const struct pam_conv auth_pamconv = { auth_conv, &auth_ctx };

/*
 * Wait for the answer to a prompt.  Returns 0 with a RESPONSE in *m,
 * or AUTH_CANCEL or AUTH_ABORT.
 */
static int
auth_wait(struct akture *a, struct akmsg *m)
{
	time_t deadline;
	int left;

	deadline = time(NULL) + AK_AUTH_TIMEOUT;
	for (;;) {
		if ((left = (int)(deadline - time(NULL))) <= 0) {
			syslog(LOG_NOTICE, "no answer from the greeter");
			return AUTH_CANCEL;
		}
		switch (ev_wait(a, left * 1000)) {
		case EV_READ:
			if (greeter_recv(a, m) == -1)
				return AUTH_ABORT;
			if (!akstate_accepts(a->a_state, m->am_type) ||
			    akstate_next(a->a_state, (int)m->am_type,
			    &a->a_state) == -1) {
				syslog(LOG_WARNING, "unexpected message %u "
				    "during authentication", m->am_type);
				akmsg_clear(m);
				return AUTH_ABORT;
			}
			if (m->am_type == AKMSG_CANCEL)
				return AUTH_CANCEL;
			return 0;
		case EV_CHILD:
			if (!child_running(a, CHILD_GREETER) ||
			    !child_running(a, CHILD_X))
				return AUTH_ABORT;
			break;
		case EV_QUIT:
			return AUTH_ABORT;
		default:
			break;
		}
	}
}

static int
auth_prompt_style(int style)
{

	switch (style) {
	case PAM_PROMPT_ECHO_OFF:
		return AKPROMPT_SECRET;
	case PAM_PROMPT_ECHO_ON:
		return AKPROMPT_VISIBLE;
	case PAM_ERROR_MSG:
		return AKPROMPT_ERROR;
	case PAM_TEXT_INFO:
		return AKPROMPT_INFO;
	default:
		return -1;
	}
}

static void
auth_free_responses(struct pam_response *r, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (r[i].resp != NULL) {
			ak_explicit_zero(r[i].resp, strlen(r[i].resp));
			free(r[i].resp);
		}
	}
	free(r);
}

static int
auth_conv(int n, const struct pam_message **msg, struct pam_response **resp,
    void *data)
{
	char text[AKMSG_TEXTMAX + 1];
	struct authctx *ctx;
	struct pam_response *r;
	struct akmsg m;
	int i, style;

	ctx = data;
	*resp = NULL;
	if (n <= 0 || n > PAM_MAX_NUM_MSG)
		return PAM_CONV_ERR;
	if ((r = calloc((size_t)n, sizeof(*r))) == NULL)
		return PAM_BUF_ERR;

	for (i = 0; i < n; i++) {
		if ((style = auth_prompt_style(msg[i]->msg_style)) == -1)
			goto fail;
		if (!ctx->ac_interactive) {
			/* Autologin, or session setup after the greeter. */
			if (style == AKPROMPT_SECRET ||
			    style == AKPROMPT_VISIBLE)
				goto fail;
			syslog(LOG_INFO, "PAM: %s", msg[i]->msg);
			continue;
		}

		akmsg_init(&m, AKMSG_PROMPT);
		m.am_args[0] = (uint32_t)style;
		(void)strlcpy(text, msg[i]->msg != NULL ? msg[i]->msg : "",
		    sizeof(text));
		(void)akmsg_setstr(&m, text);
		if (greeter_send(ctx->ac_a, &m) == -1) {
			ctx->ac_result = AUTH_ABORT;
			goto fail;
		}
		if (style == AKPROMPT_INFO || style == AKPROMPT_ERROR)
			continue;

		if ((ctx->ac_result = auth_wait(ctx->ac_a, &m)) != 0)
			goto fail;
		r[i].resp = strdup(m.am_str);
		akmsg_clear(&m);
		if (r[i].resp == NULL)
			goto fail;
	}
	*resp = r;
	return PAM_SUCCESS;

fail:
	auth_free_responses(r, n);
	return PAM_CONV_ERR;
}

/*
 * Run PAM for user with the given service.  pam_authenticate, then
 * pam_acct_mgmt, changing an expired password if asked to.  On
 * success the handle stays open for the session.
 */
static int
auth_run(struct akture *a, const char *service, const char *user)
{
	int rv;

	auth_ctx.ac_result = 0;
	if ((rv = pam_start(service, user, &auth_pamconv, &a->a_pamh)) !=
	    PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_start: %s", pam_strerror(NULL, rv));
		a->a_pamh = NULL;
		return AUTH_FAIL;
	}
	(void)pam_set_item(a->a_pamh, PAM_TTY, AKTURE_DISPLAY);
#ifdef PAM_XDISPLAY
	(void)pam_set_item(a->a_pamh, PAM_XDISPLAY, AKTURE_DISPLAY);
#endif

	rv = pam_authenticate(a->a_pamh, 0);
	if (rv == PAM_SUCCESS && auth_ctx.ac_result == 0) {
		rv = pam_acct_mgmt(a->a_pamh, 0);
		if (rv == PAM_NEW_AUTHTOK_REQD && auth_ctx.ac_result == 0)
			rv = pam_chauthtok(a->a_pamh,
			    PAM_CHANGE_EXPIRED_AUTHTOK);
	}
	auth_ctx.ac_interactive = 0;

	if (auth_ctx.ac_result != 0) {
		auth_end(a, rv);
		return auth_ctx.ac_result;
	}
	if (rv != PAM_SUCCESS) {
		/* The user name may be a mistyped password: omit it. */
		syslog(LOG_NOTICE, "authentication failed: %s",
		    pam_strerror(a->a_pamh, rv));
		auth_end(a, rv);
		return AUTH_FAIL;
	}
	return AUTH_OK;
}

/*
 * Authenticate the user named in a BEGIN_AUTH message, talking to the
 * greeter.  The caller has moved the state to AKS_AUTH.
 */
int
auth_greeter(struct akture *a, const struct akmsg *begin)
{

	(void)strlcpy(a->a_user, begin->am_str, sizeof(a->a_user));
	a->a_session = begin->am_args[0];
	auth_ctx.ac_a = a;
	auth_ctx.ac_interactive = 1;
	return auth_run(a, AKTURE_PAM_SERVICE, a->a_user);
}

/*
 * Log user in without asking, through the separate autologin service.
 */
int
auth_autologin(struct akture *a, const char *user)
{

	(void)strlcpy(a->a_user, user, sizeof(a->a_user));
	auth_ctx.ac_a = a;
	auth_ctx.ac_interactive = 0;
	return auth_run(a, AKTURE_PAM_AUTOLOGIN, a->a_user);
}

void
auth_end(struct akture *a, int status)
{

	if (a->a_pamh != NULL) {
		(void)pam_end(a->a_pamh, status);
		a->a_pamh = NULL;
	}
}
