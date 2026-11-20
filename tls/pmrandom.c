/* pwrandom.c - random numbers for PsiMail's TLS key exchange
 *
 * The Psion has no hardware random source. As in PsiTerm, noise comes from
 * psiglue's pg_entropy() (timer/CPU jitter, the clock, free RAM, and the
 * key and pen timings PsiMail.app passes in) and is hashed into a SHA-256
 * pool; output blocks are SHA-256(pool, counter) and the pool is re-keyed
 * after every call, with fresh jitter mixed in each time. A seed kept
 * between runs (random.dat) is mixed in as well: see seed_file. */
#include "includes.h"
#include <stdio.h>

extern int pg_entropy(unsigned char *out, int max);
extern const char *st_store_dir(void);             /* engine/store.c: PsiMail's folder */
extern int pm_replace(const char *tmp, const char *path);   /* pmepoc.cpp, host/pmhost.c */

static unsigned char g_pool[32];
static unsigned long g_counter;
static int g_seeded;
static int g_noise;            /* pg_entropy has ever given a non-zero byte */

/* 1 while pg_entropy has given nothing but zeros: never on a Psion (its
   noise includes the clock and free RAM), only in the ARM harness's
   repeatable runs (web/links/emu/run_links.py --record/--replay), which
   tls13.c then keeps to the ClientHello the recordings were made with */
int tls_rng_repeatable(void) { return !g_noise; }

static void mix(const unsigned char *d, unsigned long n)
{
	hash_state h;
	sha256_init(&h);
	sha256_process(&h, g_pool, 32);
	sha256_process(&h, d, n);
	sha256_done(&h, g_pool);
}

/* (review CR-27, as PsiSync's R116) a seed kept between runs, random.dat in
   PsiMail's folder: after a reset the clock is back at its default and the
   ticks start from 0, so the first connection's key would rest on timing
   jitter alone. What the last run left is mixed in first; the next run's
   seed is made from the pool (never the pool itself) and written beside
   the file, then put in its place. */
static void seed_file(void)
{
	char path[120], tmp[124];
	unsigned char s[32];
	hash_state h;
	FILE *f;
	const char *dir = st_store_dir();
	int n;
	if (!dir || !dir[0] || strlen(dir) > sizeof(path) - 12) return;
	snprintf(path, sizeof(path), "%srandom.dat", dir);
	snprintf(tmp, sizeof(tmp), "%srandom.da~", dir);
	if ((f = fopen(path, "rb")) != 0) {
		n = (int)fread(s, 1, sizeof(s), f);
		fclose(f);
		if (n > 0) mix(s, (unsigned long)n);
	}
	sha256_init(&h);
	sha256_process(&h, g_pool, 32);
	sha256_process(&h, (const unsigned char *)"seed", 4);
	sha256_done(&h, s);
	mix((const unsigned char *)"seeded", 6);
	if ((f = fopen(tmp, "wb")) != 0) {
		int ok = fwrite(s, 1, sizeof(s), f) == sizeof(s);
		if (fflush(f) != 0) ok = 0;
		if (fclose(f) != 0) ok = 0;
		if (!ok || pm_replace(tmp, path) != 0) remove(tmp);
	}
	memset(s, 0, sizeof(s));
}

void genrandom(unsigned char *buf, unsigned int len)
{
	unsigned char e[256], blk[32];
	hash_state h;
	int n = pg_entropy(e, g_seeded ? 64 : (int)sizeof(e)), i;
	for (i = 0; i < n; i++) if (e[i]) g_noise = 1;
	mix(e, n);
	/* (not in the harness's repeatable runs, which must stay alike) */
	if (!g_seeded && g_noise) seed_file();
	g_seeded = 1;
	while (len) {
		unsigned int k = len < 32 ? len : 32;
		g_counter++;
		sha256_init(&h);
		sha256_process(&h, g_pool, 32);
		sha256_process(&h, (const unsigned char *)&g_counter, sizeof(g_counter));
		sha256_process(&h, (const unsigned char *)"out", 3);
		sha256_done(&h, blk);
		memcpy(buf, blk, k);
		buf += k; len -= k;
	}
	mix((const unsigned char *)"rekey", 5);
	memset(e, 0, sizeof(e));
	memset(blk, 0, sizeof(blk));
}
