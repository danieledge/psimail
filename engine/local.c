/* local.c - (1.01) Find on this Psion: the mail already downloaded, with
 * no connection.
 *
 * PM_CMD_SEARCH while working offline, or with its words after "\x01L"
 * (the app's "On this Psion"), comes here instead of to the server's
 * UID SEARCH (imap.c). Each word must be in the sender, the subject, the
 * recipient or the downloaded text (<uid>.txt; a message not downloaded is
 * matched by its list line only). The newest 50 matches go in search.txt,
 * as the server's search leaves them, so the app shows them the same way.
 * A long folder is gone through at a lower priority than the app, with
 * progress and Esc (the 5mx's 36 MHz).
 */
#include <string.h>
#include <stdlib.h>
#include "pm.h"

#define LS_TEXT 24000                    /* of each message's text: plenty for a match */
#define LS_MAX  50

/* the next word of s (space separated) into w; 0 when there are no more */
static const char *next_word(const char *s, char *w, int max)
{
	int n = 0;
	while (*s == ' ') s++;
	if (!*s) return 0;
	while (*s && *s != ' ') {
		if (n < max - 1) w[n++] = *s;
		s++;
	}
	w[n] = 0;
	return s;
}

static int in_text(const char *path, const char *word, char *buf)
{
	FILE *f = fopen(path, "rb");
	int n;
	if (!f) return 0;
	n = (int)fread(buf, 1, LS_TEXT - 1, f);
	fclose(f);
	if (n < 0) n = 0;
	buf[n] = 0;
	{
		/* (a NUL in the file ends the text: none in a store file) */
		int i;
		for (i = 0; i < n; i++) if (!buf[i]) buf[i] = ' ';
	}
	return pm_stristr(buf, word) != 0;
}

/* every word in the message's line or text */
static int msg_matches(int acct, const char *folder, const PmMsg *m, const char *words, char *buf, int *loaded)
{
	char w[64], path[190];
	const char *p = words;
	*loaded = 0;
	while ((p = next_word(p, w, sizeof(w))) != 0) {
		if (pm_stristr(m->from, w) || pm_stristr(m->subject, w) || pm_stristr(m->to, w))
			continue;
		if (!*loaded) {
			st_msg_path(acct, folder, m->uid, "txt", path, sizeof(path));
			*loaded = 1;
			if (!in_text(path, w, buf)) { buf[0] = 0; return 0; }
			continue;
		}
		if (!buf[0] || !pm_stristr(buf, w)) return 0;
	}
	return 1;
}

int local_search(int acct, const char *folder, const char *words, char *why, int whymax)
{
	static PmIndex fx, ix;
	static char buf[LS_TEXT];
	int i, found = 0, total;
	char w[64];
	if (!next_word(words, w, sizeof(w))) {
		snprintf(why, whymax, "Nothing to find");
		return PM_RES_FAILED;
	}
	memset(&ix, 0, sizeof(ix));
	if (st_index_load(acct, folder, "index.txt", &fx) != 0) {
		snprintf(why, whymax, "Nothing in this folder here");
		return PM_RES_FAILED;
	}
	total = fx.n;
	pm_cpu_low(1);
	/* newest first: the index is in uid order */
	for (i = fx.n - 1; i >= 0; i--) {
		int loaded;
		if (((fx.n - i) % 10) == 1) {
			pm_progress("Finding in the mail here (%d of %d)\x85", fx.n - i, total);
			pm_beat_reset();
		}
		if (pm_cancelled()) {
			pm_cpu_low(0);
			st_index_free(&fx);
			st_index_free(&ix);
			snprintf(why, whymax, "Stopped");
			return PM_RES_CANCELLED;
		}
		if (msg_matches(acct, folder, &fx.m[i], words, buf, &loaded)) {
			found++;
			if (ix.n < LS_MAX) {
				PmMsg *d = st_index_add(&ix, fx.m[i].uid);
				if (d) *d = fx.m[i];
			}
		}
	}
	pm_cpu_low(0);
	ix.uidvalidity = fx.uidvalidity;
	st_index_free(&fx);
	if (st_index_save(acct, folder, "search.txt", &ix) != 0) {
		char d[160];
		st_folder_dir(acct, folder, d, sizeof(d));
		pm_write_why(why, whymax, "the search results", d);
		st_index_free(&ix);
		return PM_RES_FAILED;
	}
	st_index_free(&ix);
	if (found > LS_MAX) snprintf(why, whymax, "%d found here - showing the newest %d", found, LS_MAX);
	else snprintf(why, whymax, "%d found here", found);
	return PM_RES_OK;
}
