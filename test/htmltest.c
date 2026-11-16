/* htmltest.c - the HTML converter (engine/html.c) on a PC, for htmltest.py
 *
 *   htmltest FILE [SPLIT]   FILE converted as the engine does, to stdout;
 *                           fed SPLIT bytes at a time (default 977), as
 *                           the message arrives in pieces
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../engine/pm.h"

/* pm_copy, pm_strcasecmp, pm_strncasecmp, pm_stristr: core/mail/engine/cal/calutil.c */
long pm_time(void) { return (long)time(0); }
void pm_log(const char *fmt, ...) { (void)fmt; }

static void sink(const char *s, int n, void *ctx) { fwrite(s, 1, n, (FILE *)ctx); }

int main(int argc, char **argv)
{
	FILE *f;
	static char buf[1 << 20];
	long n, pos;
	int split = argc > 2 ? atoi(argv[2]) : 977;
	HtmlConv *h;
	if (argc < 2 || !(f = fopen(argv[1], "rb"))) { fprintf(stderr, "usage: htmltest FILE [SPLIT]\n"); return 2; }
	n = (long)fread(buf, 1, sizeof(buf), f);
	fclose(f);
	if (split <= 0) split = 977;
	h = html_new();
	for (pos = 0; pos < n; pos += split)
		html_feed(h, buf + pos, (int)(n - pos < split ? n - pos : split), sink, stdout);
	html_end(h, sink, stdout);
	html_free(h);
	return 0;
}
