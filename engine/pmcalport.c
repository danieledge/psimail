/* pmcalport.c - what psimail.exe gives the shared calendar engine
 * (core/mail/engine/cal/calport.h): the calendar's server and account from
 * the shared chunk, the store, working offline, and a certificate to ask
 * the user about. The rest of the port (pm_log, pmn_*, st_changed ...) is
 * PsiMail's own functions of those names.
 */
#include <string.h>
#include "pm.h"
#include "calport.h"

/* the shared engine's results are PsiMail's own numbers */
typedef char pm_res_match[(CAL_RES_OK == PM_RES_OK && CAL_RES_FAILED == PM_RES_FAILED &&
	CAL_RES_OFFLINE == PM_RES_OFFLINE && CAL_RES_CANCELLED == PM_RES_CANCELLED &&
	CAL_RES_UNTRUSTED == PM_RES_UNTRUSTED && CAL_RES_NEED_PASS == PM_RES_NEED_PASS &&
	CAL_RES_LOGIN_FAILED == PM_RES_LOGIN_FAILED) ? 1 : -1];

void calp_target(CalTarget *t)
{
	PmShared *s = pm_shared();
	PmCalendar *k = &s->cal;
	PmAccount *a = &s->acct[k->acct >= 0 && k->acct < PM_MAX_ACCOUNTS ? k->acct : 0];
	t->host = k->host;
	t->port = k->port;
	t->tls = !k->plain;
	t->path = k->path;
	/* the calendar's own user name and password, else its account's */
	t->user = k->user[0] ? k->user : a->user;
	t->pass = k->pass[0] ? k->pass : a->pass;
	t->zone = k->zone;
	t->days_back = k->days_back;
	t->days_ahead = k->days_ahead;
}

const char *calp_dir(void) { return pm_shared()->store_dir; }

int calp_offline(void) { return pm_shared()->offline; }

void calp_untrusted(const char *hostport, const char *why, const char *fingerprint)
{
	PmShared *s = pm_shared();
	pm_copy(s->trust_host, hostport, sizeof(s->trust_host));
	pm_copy(s->trust_why, why, sizeof(s->trust_why));
	pm_copy(s->trust_fp, fingerprint, sizeof(s->trust_fp));
}
