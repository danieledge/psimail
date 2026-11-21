/* store.c - PsiMail's files on the Psion (normally on the CF card)
 *
 *   <store> is <disk>:\System\Data\PsiMail\ (app/pmstore.cpp; \PsiMail\ before 0.74)
 *   <store>\pins.txt                 trusted certificates: host:port TAB sha256
 *   <store>\A<n>\account.txt         whose files these are (user@host)
 *   <store>\A<n>\folders.txt         folder list (see imap.c)
 *   <store>\A<n>\pending.txt         changes made offline, replayed later
 *   <store>\A<n>\outbox\<id>.txt     messages written in the app, to send
 *   <store>\A<n>\F<hash>\index.txt   the folder's messages, one per line
 *   <store>\A<n>\F<hash>\search.txt  results of the last search there
 *   <store>\A<n>\F<hash>\<uid>.txt   a downloaded message (text)
 *   <store>\A<n>\F<hash>\<uid>.att   its attachments: part TAB size TAB name TAB type
 *
 * index.txt:  "#PSIMAIL1 TAB uidvalidity TAB uidnext TAB exists", then
 *   uid TAB flags TAB date TAB size TAB from TAB subject TAB to TAB message-id TAB in-reply-to
 * sorted by uid. flags are letters: S seen, F flagged, A answered, D deleted,
 * T has attachments, B text downloaded. date is seconds since 1970 (UTC).
 * All text is Windows-1252, the Psion's character set.
 *
 * Only the engine writes these files (the app writes outbox messages), and
 * always to a temporary file first, renamed into place when complete.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "pm.h"

static unsigned long fnv(const char *s)
{
	unsigned long h = 2166136261UL;
	while (*s) { h ^= (unsigned char)*s++; h = (h * 16777619UL) & 0xffffffffUL; }
	return h;
}

/* PsiMail's folder, for tls/pmrandom.c's seed (it does not see PmShared) */
const char *st_store_dir(void)
{
	return pm_shared()->store_dir;
}

void st_acct_dir(int acct, char *out, int max)
{
	snprintf(out, max, "%sA%d%s", pm_shared()->store_dir, acct, PM_SEP);
}

void st_folder_dir(int acct, const char *folder, char *out, int max)
{
	snprintf(out, max, "%sA%d%sF%08lX%s", pm_shared()->store_dir, acct, PM_SEP, fnv(folder), PM_SEP);
}

void st_msg_path(int acct, const char *folder, unsigned int uid, const char *ext, char *out, int max)
{
	char d[160];
	st_folder_dir(acct, folder, d, sizeof(d));
	snprintf(out, max, "%s%u.%s", d, uid, ext);
}

void st_changed(void)
{
	pm_shared()->changed_seq++;
}

/* has anything come from the server into this account's files? */
static int st_fetched(int acct)
{
	char path[200];
	FILE *f;
	st_acct_dir(acct, path, sizeof(path));
	pm_copy(path + strlen(path), "folders.txt", sizeof(path) - (int)strlen(path));
	if ((f = fopen(path, "r")) != 0) { fclose(f); return 1; }
	st_folder_dir(acct, "INBOX", path, sizeof(path));
	pm_copy(path + strlen(path), "index.txt", sizeof(path) - (int)strlen(path));
	if ((f = fopen(path, "r")) != 0) { fclose(f); return 1; }
	return 0;
}

/* ---- (1.11) another login in a slot: nothing unsent is lost (review R11)
 *
 *   <store>\K<hash>\                 a login's files kept aside (hash: fnv of
 *                                    "user@host"; K<hash>2..9 if taken), with
 *                                    their own account.txt
 *
 * Before 1.11 a slot set up for another login lost its whole folder, the
 * Outbox (drafts are kept there) and pending.txt with it: after a cleared C:
 * two accounts typed in again the other way round lost their unsent mail.
 * Now the new login's files are found first (another slot not set up for
 * that login now, or a kept folder) and swapped or brought back; the old
 * login's go aside if anything of them is unsent, and are only removed when
 * nothing is (what is left is a copy of the server's). The app is told
 * (store_note).
 */

/* the first line of dir's account.txt ("" none) */
static void st_login_of(const char *dir, char *have, int max)
{
	char path[200];
	FILE *f;
	snprintf(path, sizeof(path), "%saccount.txt", dir);
	have[0] = 0;
	if ((f = fopen(path, "r")) != 0) {
		if (!fgets(have, max, f)) have[0] = 0;
		fclose(f);
		have[strcspn(have, "\r\n")] = 0;
	}
}

/* user@host: a real login, not a damaged line */
static int st_is_login(const char *s)
{
	const char *at = strchr(s, '@');
	return at && at > s && at[1];
}

static void st_count_cb(const char *name, void *ctx)
{
	(void)name;
	(*(int *)ctx)++;
}

/* messages in dir's Outbox (drafts among them); with pending, +1 for a
   pending.txt of changes not yet sent */
static int st_unsent(const char *dir, int *pending)
{
	char path[200];
	FILE *f;
	int n = 0;
	snprintf(path, sizeof(path), "%soutbox%s", dir, PM_SEP);
	pm_list_dir(path, ".txt", st_count_cb, &n);
	snprintf(path, sizeof(path), "%spending.txt", dir);
	*pending = 0;
	if ((f = fopen(path, "r")) != 0) {
		*pending = fgetc(f) != EOF;
		fclose(f);
	}
	return n;
}

/* a folder renamed: both names end in the separator, which rename() must
   not have */
static int st_move_dir(const char *from, const char *to)
{
	char a[180], b[180];
	pm_copy(a, from, sizeof(a));
	pm_copy(b, to, sizeof(b));
	if (a[0]) a[strlen(a) - 1] = 0;
	if (b[0]) b[strlen(b) - 1] = 0;
	return rename(a, b);
}

/* the k'th kept folder's name for a login (k 1: K<hash>, then K<hash>2..) */
static void st_kept_dir(const char *login, int k, char *out, int max)
{
	if (k <= 1) snprintf(out, max, "%sK%08lX%s", pm_shared()->store_dir, fnv(login), PM_SEP);
	else snprintf(out, max, "%sK%08lX%d%s", pm_shared()->store_dir, fnv(login), k, PM_SEP);
}

#define ST_KEPT_MAX 9

/* a kept folder of this login -> 1 and its name */
static int st_find_kept(const char *login, char *out, int max)
{
	char have[180];
	int k;
	for (k = 1; k <= ST_KEPT_MAX; k++) {
		st_kept_dir(login, k, out, max);
		st_login_of(out, have, sizeof(have));
		if (!strcmp(have, login)) return 1;
	}
	return 0;
}

/* what the app shows once: "title TAB line" */
static void st_note(const char *title, const char *fmt, ...)
{
	PmShared *s = pm_shared();
	va_list ap;
	int n;
	snprintf(s->store_note, sizeof(s->store_note), "%s\t", title);
	n = (int)strlen(s->store_note);
	va_start(ap, fmt);
	vsnprintf(s->store_note + n, sizeof(s->store_note) - n, fmt, ap);
	va_end(ap);
	s->store_note_seq++;
	pm_log("store: %s", s->store_note);
}

static const char *st_plural(int n) { return n == 1 ? "" : "s"; }

/* a login as the user knows it: the user name, without "@host" */
static const char *st_shown(const char *login)
{
	static char buf[44];
	const char *at = strrchr(login, '@');
	int n = at ? (int)(at - login) : (int)strlen(login);
	if (n > 40) n = 40;
	memcpy(buf, login, n);
	buf[n] = 0;
	return buf;
}

/* another slot holding want's files and not set up for want now -> its
   number, or -1 */
static int st_slot_with(int acct, const char *want)
{
	PmShared *s = pm_shared();
	char other[160], login[180], theirs[180];
	int j;
	for (j = 0; j < PM_MAX_ACCOUNTS; j++) {
		if (j == acct) continue;
		st_acct_dir(j, other, sizeof(other));
		st_login_of(other, login, sizeof(login));
		if (strcmp(login, want)) continue;
		if (s->acct[j].used) {
			snprintf(theirs, sizeof(theirs), "%s@%s", s->acct[j].user, s->acct[j].imap_host);
			if (!strcmp(theirs, want)) continue;        /* (the same login in two slots: theirs) */
		}
		return j;
	}
	return -1;
}

/* dir and slot j's folder change places -> 0 */
static int st_swap(int acct, const char *dir, int j)
{
	char other[160], tmp[160];
	st_acct_dir(j, other, sizeof(other));
	snprintf(tmp, sizeof(tmp), "%sA%d.swp%s", pm_shared()->store_dir, acct, PM_SEP);
	pm_rmtree(tmp);                                   /* (a swap cut short earlier: only ever empty) */
	if (st_move_dir(dir, tmp) != 0) return -1;        /* (dir may not be there: then plainly moved) */
	if (st_move_dir(other, dir) != 0) { st_move_dir(tmp, dir); return -1; }
	if (st_move_dir(tmp, other) != 0) { st_move_dir(dir, other); st_move_dir(tmp, dir); return -1; }
	return 0;
}

/* the slot's folder has nothing of its own (none, or nothing fetched or
   unsent): want's files from another slot or kept aside, if there are
   any, take its place -> 1 if they did */
static int st_adopt(int acct, const char *dir, const char *want)
{
	char kept[160], other[160];
	int j = st_slot_with(acct, want), pending = 0, n;
	if (j >= 0) {
		st_acct_dir(j, other, sizeof(other));
		pm_rmtree(dir);
		if (st_move_dir(other, dir) != 0) return 0;
		pm_log("store: A%d has %s's files now (from A%d)", acct, want, j);
	} else if (st_find_kept(want, kept, sizeof(kept))) {
		pm_rmtree(dir);
		if (st_move_dir(kept, dir) != 0) return 0;
		pm_log("store: A%d: %s's kept files back", acct, want);
	} else
		return 0;
	if ((n = st_unsent(dir, &pending)) > 0)
		st_note("Unsent mail back", "%d message%s for %s in the Outbox again", n, st_plural(n), st_shown(want));
	return 1;
}

/* the slot's folder holds have's files (mail came for that login), the
   slot is now set up for want -> 0, or -1 and why (nothing changed: the
   old files must not be taken for the new login's, nor sent from it) */
static int st_new_login(int acct, const char *dir, const char *have, const char *want, char *why, int whymax)
{
	char kept[160], login[180];
	int j, k, pending = 0, unsent;

	/* 1. the new login's files in another slot that is not set up for it
	   now (accounts typed in again in another order): the two swap */
	if ((j = st_slot_with(acct, want)) >= 0 && st_swap(acct, dir, j) == 0) {
		pm_log("store: A%d has %s's files now, A%d %s's", acct, want, j, have);
		return 0;
	}

	/* 2. the old login's files: put aside if anything is unsent, else
	   removed (what is left is a copy of the server's) */
	unsent = st_unsent(dir, &pending);
	if (unsent || pending) {
		for (k = 1; k <= ST_KEPT_MAX; k++) {
			st_kept_dir(have, k, kept, sizeof(kept));
			st_login_of(kept, login, sizeof(login));
			if (!login[0] && (pm_rmtree(kept), st_move_dir(dir, kept) == 0)) break;
		}
		if (k > ST_KEPT_MAX) {
			pm_log("store: A%d: %s's unsent mail could not be put aside", acct, have);
			snprintf(why, whymax, "Unsent mail of %.60s is in the way: send or delete it first", have);
			return -1;
		}
		pm_log("store: A%d: %s's files kept as %s (%d unsent, pending %d)", acct, have, kept, unsent, pending);
		if (unsent)
			st_note("Unsent mail kept", "%d message%s for %s: set it up again to send",
				unsent, st_plural(unsent), st_shown(have));
	} else
		pm_rmtree(dir);

	/* 3. the new login's files kept aside earlier: back in this slot */
	st_adopt(acct, dir, want);
	return 0;
}

int st_check_account(int acct, char *why, int whymax)
{
	char dir[160], path[180], want[180], have[180];
	PmAccount *a = &pm_shared()->acct[acct];
	FILE *f;
	st_acct_dir(acct, dir, sizeof(dir));
	snprintf(want, sizeof(want), "%s@%s", a->user, a->imap_host);
	snprintf(path, sizeof(path), "%saccount.txt", dir);
	st_login_of(dir, have, sizeof(have));
	if (strcmp(have, want)) {
		char tmp[190];
		int pending = 0;
		/* another account's mail (1.11: swapped, kept aside or brought
		   back, never simply removed: st_new_login). Only when the file
		   really names one (user@host): a short or odd line is a damaged
		   file, not another account. Nor when nothing ever came from that
		   server under the old name (no folder list, no Inbox): a user name
		   put right after the server refused it (issue 1) keeps what was
		   written meanwhile */
		if (st_is_login(have) && st_fetched(acct)) {
			if (st_new_login(acct, dir, have, want, why, whymax) != 0) return -1;
		} else if (!st_fetched(acct) && !st_unsent(dir, &pending) && !pending)
			st_adopt(acct, dir, want);            /* (a new or empty slot: the login's own files, if any) */
		st_login_of(dir, have, sizeof(have));
		if (strcmp(have, want)) {
			pm_mkdir(pm_shared()->store_dir);
			pm_mkdir(dir);
			/* written whole, then put in place: a partial file would read as
			   "another account" at the next start */
			snprintf(tmp, sizeof(tmp), "%saccount.new", dir);
			if ((f = fopen(tmp, "w")) != 0) {
				fprintf(f, "%s\n", want);
				if (pm_fclose(f) != 0 || pm_replace(tmp, path) != 0) { remove(tmp); pm_log("store: could not write %s", path); }
			}
		}
		st_changed();
	}
	snprintf(path, sizeof(path), "%soutbox", dir);
	pm_mkdir(path);
	return 0;
}

/* ----------------------------------------------------------------- index */

void st_index_free(PmIndex *ix)
{
	free(ix->m);
	memset(ix, 0, sizeof(*ix));
}

static char *field(char **p)
{
	char *s = *p, *t;
	if (!s) return "";
	t = strchr(s, '\t');
	if (t) { *t = 0; *p = t + 1; } else *p = 0;
	return s;
}

PmMsg *st_index_find(PmIndex *ix, unsigned int uid)
{
	int lo = 0, hi = ix->n - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (ix->m[mid].uid == uid) return &ix->m[mid];
		if (ix->m[mid].uid < uid) lo = mid + 1; else hi = mid - 1;
	}
	return 0;
}

PmMsg *st_index_add(PmIndex *ix, unsigned int uid)
{
	int i;
	PmMsg *m = st_index_find(ix, uid);
	if (m) return m;
	if (ix->n >= ix->cap) {
		int nc = ix->cap ? ix->cap * 2 : 64;
		PmMsg *nm = (PmMsg *)realloc(ix->m, nc * sizeof(PmMsg));
		if (!nm) return 0;
		ix->m = nm; ix->cap = nc;
	}
	for (i = ix->n; i > 0 && ix->m[i - 1].uid > uid; i--)
		ix->m[i] = ix->m[i - 1];
	memset(&ix->m[i], 0, sizeof(PmMsg));
	ix->m[i].uid = uid;
	ix->n++;
	return &ix->m[i];
}

void st_index_remove(PmIndex *ix, unsigned int uid)
{
	PmMsg *m = st_index_find(ix, uid);
	int i;
	if (!m) return;
	i = (int)(m - ix->m);
	memmove(ix->m + i, ix->m + i + 1, (ix->n - i - 1) * sizeof(PmMsg));
	ix->n--;
}

int st_index_load(int acct, const char *folder, const char *file, PmIndex *ix)
{
	char dir[160], path[190];
	static char line[1024];
	FILE *f;
	int broken = 0;
	memset(ix, 0, sizeof(*ix));
	st_folder_dir(acct, folder, dir, sizeof(dir));
	snprintf(path, sizeof(path), "%s%s", dir, file);
	if (!(f = fopen(path, "r"))) return ST_INDEX_NONE;
	{
		/* (1.15) the lines counted first and the list taken at its size in
		   one go: it grew by doubling (35, 70, 140 KB... each copy leaving
		   a hole in the engine's heap), and was loaded for every small job */
		int lines = 0, k;
		size_t got;
		while ((got = fread(line, 1, sizeof(line), f)) > 0)
			for (k = 0; k < (int)got; k++) if (line[k] == '\n') lines++;
		if (ferror(f) || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return ST_INDEX_BROKEN; }
		ix->cap = lines + 16;               /* (room for a few new ones before any copy) */
		if (!(ix->m = (PmMsg *)malloc(ix->cap * sizeof(PmMsg)))) {
			fclose(f);
			memset(ix, 0, sizeof(*ix));
			pm_log("store: could not read all of %s (no memory)", path);
			return ST_INDEX_BROKEN;
		}
	}
	while (fgets(line, sizeof(line), f)) {
		char *p = line, *uid;
		PmMsg *m;
		line[strcspn(line, "\r\n")] = 0;
		if (line[0] == '#') {
			field(&p);
			ix->uidvalidity = strtoul(field(&p), 0, 10);
			ix->uidnext = strtoul(field(&p), 0, 10);
			ix->exists = strtol(field(&p), 0, 10);
			continue;
		}
		uid = field(&p);
		if (!*uid) continue;
		m = st_index_add(ix, (unsigned int)strtoul(uid, 0, 10));
		if (!m) { broken = 1; break; }
		pm_copy(m->flags, field(&p), sizeof(m->flags));
		m->date = strtol(field(&p), 0, 10);
		m->size = strtol(field(&p), 0, 10);
		pm_copy(m->from, field(&p), sizeof(m->from));
		pm_copy(m->subject, field(&p), sizeof(m->subject));
		pm_copy(m->to, field(&p), sizeof(m->to));
		pm_copy(m->msgid, field(&p), sizeof(m->msgid));
		pm_copy(m->inreplyto, field(&p), sizeof(m->inreplyto));
		m->attach = st_flag_has(m->flags, 'T');
	}
	if (ferror(f)) broken = 2;
	fclose(f);
	if (broken) {
		/* half a list read as the whole would be saved as the whole, and
		   the rest of the folder's messages would be lost from it */
		pm_log("store: could not read all of %s (%s)", path, broken == 1 ? "no memory" : "a read error");
		st_index_free(ix);
		return ST_INDEX_BROKEN;
	}
	return 0;
}

/* (1.15) the newest uid in a folder's list (its last line: the list is in
   uid order) without reading it all; 0 if none */
unsigned int st_index_last_uid(int acct, const char *folder)
{
	char dir[160], path[190];
	static char tail[1100];
	FILE *f;
	long size, from;
	size_t n;
	unsigned int u = 0;
	char *p, *e;
	st_folder_dir(acct, folder, dir, sizeof(dir));
	snprintf(path, sizeof(path), "%sindex.txt", dir);
	if (!(f = fopen(path, "rb"))) return 0;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	from = size > (long)sizeof(tail) - 1 ? size - (long)sizeof(tail) + 1 : 0;
	fseek(f, from, SEEK_SET);
	n = fread(tail, 1, sizeof(tail) - 1, f);
	fclose(f);
	tail[n] = 0;
	/* the last whole line that is not the header */
	e = tail + n;
	while (e > tail && (e[-1] == '\n' || e[-1] == '\r')) e--;
	*e = 0;
	p = e;
	while (p > tail && p[-1] != '\n') p--;
	if (p == tail && from > 0) return 0;     /* (a line longer than the tail: cannot say) */
	if (*p == '#' || !*p) return 0;
	u = (unsigned int)strtoul(p, 0, 10);
	return u;
}

static void clean(char *s)
{
	for (; *s; s++) if (*s == '\t' || *s == '\r' || *s == '\n') *s = ' ';
}

int st_index_save(int acct, const char *folder, const char *file, PmIndex *ix)
{
	char dir[160], path[190], tmp[190];
	FILE *f;
	int i;
	st_folder_dir(acct, folder, dir, sizeof(dir));
	pm_mkdir(dir);
	snprintf(path, sizeof(path), "%s%s", dir, file);
	snprintf(tmp, sizeof(tmp), "%s%s.new", dir, file);
	if (!(f = fopen(tmp, "w"))) return -1;
	fprintf(f, "#PSIMAIL1\t%lu\t%lu\t%ld\n", ix->uidvalidity, ix->uidnext, ix->exists);
	for (i = 0; i < ix->n; i++) {
		PmMsg *m = &ix->m[i];
		st_flag_set(m->flags, 'T', m->attach);
		clean(m->from); clean(m->subject); clean(m->to); clean(m->msgid); clean(m->inreplyto);
		fprintf(f, "%u\t%s\t%ld\t%ld\t%s\t%s\t%s\t%s\t%s\n", m->uid, m->flags, m->date, m->size,
			m->from, m->subject, m->to, m->msgid, m->inreplyto);
	}
	if (pm_fclose(f) != 0 || pm_replace(tmp, path) != 0) {
		pm_log("store: could not save %s", path);
		remove(tmp);                          /* the old list is still there */
		return -1;
	}
	st_changed();
	return 0;
}

void st_flag_set(char *flags, char f, int on)
{
	char *p = strchr(flags, f);
	if (on && !p) {
		int n = (int)strlen(flags);
		if (n < 10) { flags[n] = f; flags[n + 1] = 0; }
	} else if (!on && p) {
		memmove(p, p + 1, strlen(p));
	}
}

int st_flag_has(const char *flags, char f)
{
	return strchr(flags, f) != 0;
}

/* --------------------------------------------------------------- pending */

int st_pending_add(int acct, const char *line)
{
	char dir[160], path[190];
	FILE *f;
	st_acct_dir(acct, dir, sizeof(dir));
	snprintf(path, sizeof(path), "%spending.txt", dir);
	if (!(f = fopen(path, "a"))) { pm_log("store: could not note '%s' in pending.txt", line); return -1; }
	fprintf(f, "%s\n", line);
	if (pm_fclose(f) != 0) { pm_log("store: could not note '%s' in pending.txt (disk full?)", line); return -1; }
	return 0;
}

/* Takes one line out again - the change has reached the server. The first
   line equal to it goes (the newest copy would do as well: they are the
   same change); the file goes when nothing is left. 0 ok */
int st_pending_drop(int acct, const char *line)
{
	char dir[160], path[190], tmp[190];
	static char l[512];
	FILE *f, *out;
	int dropped = 0, kept = 0;
	st_acct_dir(acct, dir, sizeof(dir));
	snprintf(path, sizeof(path), "%spending.txt", dir);
	snprintf(tmp, sizeof(tmp), "%spending.new", dir);
	if (!(f = fopen(path, "r"))) return 0;
	if (!(out = fopen(tmp, "w"))) { fclose(f); return -1; }
	while (fgets(l, sizeof(l), f)) {
		l[strcspn(l, "\r\n")] = 0;
		if (!l[0]) continue;
		if (!dropped && !strcmp(l, line)) { dropped = 1; continue; }
		fprintf(out, "%s\n", l);
		kept++;
	}
	fclose(f);
	if (pm_fclose(out) != 0) { remove(tmp); return -1; }
	if (!kept) { remove(tmp); remove(path); return 0; }
	if (pm_replace(tmp, path) != 0) { remove(tmp); return -1; }
	return 0;
}

/* Replays changes made while offline, in order. Lines the server refuses are
   dropped (the message may have gone); a lost connection keeps the rest. */
int st_pending_replay(int acct, char *why, int whymax)
{
	char dir[160], path[190], tmp[190];
	static char line[512];
	FILE *f, *out = 0;
	int lost = 0, n = 0;
	st_acct_dir(acct, dir, sizeof(dir));
	snprintf(path, sizeof(path), "%spending.txt", dir);
	snprintf(tmp, sizeof(tmp), "%spending.new", dir);
	if (!(f = fopen(path, "r"))) return 0;
	while (fgets(line, sizeof(line), f)) {
		char *p = line, *op, *folder, *uid, *arg;
		int r = 0;
		line[strcspn(line, "\r\n")] = 0;
		if (!line[0]) continue;
		if (lost) {
			if (!out) out = fopen(tmp, "w");
			if (out) fprintf(out, "%s\n", line);
			continue;
		}
		op = field(&p); folder = field(&p); uid = field(&p); arg = field(&p);
		pm_progress("Sending changes made offline (%d)\x85", ++n);
		if (!strcmp(op, "FLAG")) r = imap_flag(acct, folder, (unsigned int)strtoul(uid, 0, 10), arg, why, whymax);
		else if (!strcmp(op, "MOVE")) r = imap_move(acct, folder, (unsigned int)strtoul(uid, 0, 10), arg, why, whymax);
		else if (!strcmp(op, "EXPUNGE")) r = imap_expunge(acct, folder, why, whymax);
		else if (!strcmp(op, "UNMOVE")) r = undo_replay(acct, folder, (unsigned int)strtoul(uid, 0, 10), arg, why, whymax);
		if (r == PM_RES_OFFLINE || r == PM_RES_CANCELLED || !pmn_is_open()) {
			/* keep it (and the rest) for next time */
			lost = 1;
			if (!out) out = fopen(tmp, "w");
			if (out) { fprintf(out, "%s\t%s\t%s\t%s\n", op, folder, uid, arg); }
		}
	}
	fclose(f);
	if (out) {
		/* the ones still to do take the file's place; if that fails the
		   whole file stays (doing a change twice is harmless, losing it is not) */
		if (pm_fclose(out) != 0 || pm_replace(tmp, path) != 0) { remove(tmp); return -1; }
	} else if (lost) {
		return -1;                            /* could not write the rest down: keep them all */
	} else remove(path);
	return lost ? -1 : 0;
}

/* ------------------------------------------------------------------ pins */

int st_pin_check(const char *hostport, const char *fp)
{
	char path[160];
	static char line[200];
	FILE *f;
	int r = 0;
	snprintf(path, sizeof(path), "%spins.txt", pm_shared()->store_dir);
	if (!(f = fopen(path, "r"))) return 0;
	while (fgets(line, sizeof(line), f)) {
		char *t = strchr(line, '\t');
		line[strcspn(line, "\r\n")] = 0;
		if (!t) continue;
		*t = 0;
		if (!strcmp(line, hostport)) r = strcmp(t + 1, fp) ? -1 : 1;
		if (r == 1) break;
	}
	fclose(f);
	return r;
}

/* The old pins and the new one go to a temporary file that takes pins.txt's
   place when complete: a pin cut short by a full card would read as "the
   certificate has changed" rather than as a first visit. 0 ok, -1 not saved */
int st_pin_save(const char *hostport, const char *fp)
{
	char path[160], tmp[160];
	static char line[200];
	FILE *f, *out;
	pm_mkdir(pm_shared()->store_dir);
	snprintf(path, sizeof(path), "%spins.txt", pm_shared()->store_dir);
	snprintf(tmp, sizeof(tmp), "%spins.new", pm_shared()->store_dir);
	if (!(out = fopen(tmp, "w"))) return -1;
	if ((f = fopen(path, "r")) != 0) {
		while (fgets(line, sizeof(line), f)) {
			char *t = strchr(line, '\t');
			if (t && (int)(t - line) == (int)strlen(hostport) && !strncmp(line, hostport, t - line))
				continue;                     /* this host's old pin: replaced below */
			fputs(line, out);
			if (line[strlen(line) - 1] != '\n') fputc('\n', out);
		}
		fclose(f);
	}
	fprintf(out, "%s\t%s\n", hostport, fp);
	if (pm_fclose(out) != 0 || pm_replace(tmp, path) != 0) { remove(tmp); return -1; }
	return 0;
}
