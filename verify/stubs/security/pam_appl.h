/*
 * <security/pam_appl.h> for Frama-C: the part of the OpenPAM API the
 * master uses, with the same values as OpenPAM, and ACSL contracts.
 * PAM and its modules are trusted (see SPECIFICATION.md): the contracts
 * only state what the master relies on, namely which memory a call may
 * change and that it returns a PAM status.
 */

#ifndef _VERIFY_PAM_APPL_H_
#define _VERIFY_PAM_APPL_H_

#include <string.h>		/* valid_read_string */

#define PAM_SUCCESS			0
#define PAM_BUF_ERR			5
#define PAM_CONV_ERR			6
#define PAM_AUTH_ERR			9
#define PAM_NEW_AUTHTOK_REQD		10
#define PAM_ABORT			26
#define PAM_NUM_ERRORS			34

#define PAM_PROMPT_ECHO_OFF		1
#define PAM_PROMPT_ECHO_ON		2
#define PAM_ERROR_MSG			3
#define PAM_TEXT_INFO			4
#define PAM_MAX_NUM_MSG			32

#define PAM_ESTABLISH_CRED		0x1
#define PAM_DELETE_CRED			0x2
#define PAM_CHANGE_EXPIRED_AUTHTOK	0x4

#define PAM_TTY				3

struct pam_handle;
typedef struct pam_handle pam_handle_t;

struct pam_message {
	int	 msg_style;
	char	*msg;
};

struct pam_response {
	char	*resp;
	int	 resp_retcode;
};

struct pam_conv {
	int	(*conv)(int, const struct pam_message **,
	    struct pam_response **, void *);
	void	*appdata_ptr;
};

/*@ predicate pam_status(integer r) = 0 <= r < PAM_NUM_ERRORS; */

/*@
  requires valid_read_string(service) && valid_read_string(user);
  requires \valid_read(conv) && \valid(pamh);
  assigns *pamh;
  ensures pam_status(\result);
  ensures \result == PAM_SUCCESS ==> *pamh != \null;
*/
int	pam_start(const char *service, const char *user,
	    const struct pam_conv *conv, pam_handle_t **pamh);

/*@ requires pamh != \null; assigns \nothing; ensures pam_status(\result); */
int	pam_authenticate(pam_handle_t *pamh, int flags);

/*@ requires pamh != \null; assigns \nothing; ensures pam_status(\result); */
int	pam_acct_mgmt(pam_handle_t *pamh, int flags);

/*@ requires pamh != \null; assigns \nothing; ensures pam_status(\result); */
int	pam_chauthtok(pam_handle_t *pamh, int flags);

/*@ requires pamh != \null; assigns \nothing; ensures pam_status(\result); */
int	pam_setcred(pam_handle_t *pamh, int flags);

/*@ requires pamh != \null; assigns \nothing; ensures pam_status(\result); */
int	pam_open_session(pam_handle_t *pamh, int flags);

/*@ requires pamh != \null; assigns \nothing; ensures pam_status(\result); */
int	pam_close_session(pam_handle_t *pamh, int flags);

/*@ requires pamh != \null; assigns \nothing; ensures pam_status(\result); */
int	pam_set_item(pam_handle_t *pamh, int item, const void *value);

/*@ requires pamh != \null; assigns \nothing; ensures pam_status(\result); */
int	pam_end(pam_handle_t *pamh, int status);

/*@ assigns \nothing; ensures \result == \null || \valid_read(\result); */
char	**pam_getenvlist(pam_handle_t *pamh);

/*@ assigns \nothing; ensures valid_read_string(\result); */
const char *pam_strerror(const pam_handle_t *pamh, int error);

#endif /* !_VERIFY_PAM_APPL_H_ */
