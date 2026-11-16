/* charset.c - character sets for PsiMail
 *
 * The Psion's EIKON UI is 8-bit and uses Windows-1252, so everything the app
 * shows is converted to that here: UTF-8, ISO-8859-1/-15, Windows-1252 and
 * US-ASCII are understood (that covers nearly all mail seen in practice);
 * characters with no cp1252 form become a close ASCII stand-in or '?'.
 * Outgoing mail is sent as UTF-8.
 *
 * UTF-8 to and from cp1252 (cs_utf8_to_cp1252, cs_cp1252_to_utf8, cs_put_ucs)
 * are psion-core's, core/mail/engine/cal/cp1252.c, shared with the calendar
 * engine.
 */
#include <string.h>
#include <stdio.h>
#include "pm.h"

int cs_is_utf8(const char *cs)
{
	return cs && (!pm_strcasecmp(cs, "utf-8") || !pm_strcasecmp(cs, "utf8"));
}

int cs_to_cp1252(const char *charset, const char *in, int n, char *out, int max)
{
	int i, k = 0;
	if (max <= 0) return 0;
	if (cs_is_utf8(charset))
		return cs_utf8_to_cp1252(in, n, out, max);
	if (charset && !pm_strcasecmp(charset, "iso-8859-15")) {
		for (i = 0; i < n && k < max - 1; i++) {
			unsigned char c = (unsigned char)in[i];
			unsigned int u = c;
			switch (c) {
			case 0xa4: u = 0x20ac; break; case 0xa6: u = 0x160; break; case 0xa8: u = 0x161; break;
			case 0xb4: u = 0x17d; break; case 0xb8: u = 0x17e; break; case 0xbc: u = 0x152; break;
			case 0xbd: u = 0x153; break; case 0xbe: u = 0x178; break;
			}
			k = cs_put_ucs(u, out, k, max);
		}
		out[k] = 0;
		return k;
	}
	/* us-ascii, iso-8859-1, windows-1252 and anything unknown: bytes as they are
	   (Latin-1 is cp1252 apart from the C1 controls, which never appear) */
	for (i = 0; i < n && k < max - 1; i++) out[k++] = in[i];
	out[k] = 0;
	return k;
}

/* ---------------------------------------------------------------- RFC 2047 */

static int b64val(int c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

static int hexval(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

/* decodes one encoded-word's text into raw bytes */
static int ew_decode(int b, const char *t, int tn, char *raw, int rmax)
{
	int i, k = 0;
	if (b) {
		unsigned int acc = 0; int bits = 0;
		for (i = 0; i < tn && k < rmax; i++) {
			int v = b64val((unsigned char)t[i]);
			if (v < 0) continue;
			acc = (acc << 6) | v; bits += 6;
			if (bits >= 8) { bits -= 8; raw[k++] = (char)(acc >> bits); }
		}
	} else {
		for (i = 0; i < tn && k < rmax; i++) {
			if (t[i] == '_') raw[k++] = ' ';
			else if (t[i] == '=' && i + 2 < tn && hexval(t[i + 1]) >= 0 && hexval(t[i + 2]) >= 0) {
				raw[k++] = (char)(hexval(t[i + 1]) * 16 + hexval(t[i + 2])); i += 2;
			} else raw[k++] = t[i];
		}
	}
	return k;
}

void cs_decode_header(const char *in, char *out, int max)
{
	int k = 0, last_was_ew = 0;
	const char *p = in;
	/* bytes of a multi-byte character can be split across encoded-words:
	   collect runs of adjacent words with the same charset before converting */
	char raw[512]; int rn = 0; char rcs[32]; rcs[0] = 0;
	while (*p && k < max - 1) {
		if (p[0] == '=' && p[1] == '?') {
			const char *cs = p + 2, *q1 = strchr(cs, '?');
			if (q1 && q1[1] && q1[2] == '?') {
				const char *txt = q1 + 3, *end = strstr(txt, "?=");
				if (end) {
					char csname[32];
					int cl = (int)(q1 - cs), b = (q1[1] == 'B' || q1[1] == 'b');
					const char *star;
					if (cl > 31) cl = 31;
					memcpy(csname, cs, cl); csname[cl] = 0;
					star = strchr(csname, '*');        /* RFC 2231 language */
					if (star) *(char *)star = 0;
					if (rn && pm_strcasecmp(csname, rcs)) {
						k += cs_to_cp1252(rcs, raw, rn, out + k, max - k);
						rn = 0;
					}
					strcpy(rcs, csname);
					rn += ew_decode(b, txt, (int)(end - txt), raw + rn, (int)sizeof(raw) - rn);
					p = end + 2;
					last_was_ew = 1;
					continue;
				}
			}
		}
		if (last_was_ew) {
			/* whitespace between two encoded-words is dropped */
			const char *w = p;
			while (*w == ' ' || *w == '\t' || *w == '\r' || *w == '\n') w++;
			/* (only when that moved p: "=?" that is not an encoded-word
			   would otherwise come round here for ever) */
			if (w > p && w[0] == '=' && w[1] == '?') { p = w; continue; }
		}
		if (rn) { k += cs_to_cp1252(rcs, raw, rn, out + k, max - k); rn = 0; if (k >= max - 1) break; }
		last_was_ew = 0;
		if (*p == '\r' || *p == '\n') { p++; continue; }
		if (*p == '\t') { out[k++] = ' '; p++; continue; }
		if ((unsigned char)*p >= 0x80) {
			/* raw 8-bit header (not allowed, but common): try UTF-8 */
			const char *q = p;
			while ((unsigned char)*q >= 0x80) q++;
			k += cs_utf8_to_cp1252(p, (int)(q - p), out + k, max - k);
			p = q;
			continue;
		}
		out[k++] = *p++;
	}
	if (rn && k < max - 1) k += cs_to_cp1252(rcs, raw, rn, out + k, max - k);
	if (k > max - 1) k = max - 1;        /* (a converted run can fill out to the brim) */
	out[k] = 0;
}

void cs_encode_header(const char *in, char *out, int max)
{
	static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const unsigned char *s = (const unsigned char *)in;
	int need = 0, k = 0;
	char utf[600];
	int un, i;
	for (i = 0; s[i]; i++) if (s[i] >= 0x80 || (s[i] == '=' && s[i + 1] == '?')) need = 1;
	if (!need) { pm_copy(out, in, max); return; }
	/* one or more =?UTF-8?B?...?= words, each at most 75 characters and never
	   splitting a UTF-8 sequence */
	un = cs_cp1252_to_utf8(in, (int)strlen(in), utf, sizeof(utf));
	i = 0;
	while (i < un && k < max - 20) {
		int take = 0, j;
		while (i + take < un && take < 42) {
			int l = 1; unsigned char c = (unsigned char)utf[i + take];
			if (c >= 0xf0) l = 4; else if (c >= 0xe0) l = 3; else if (c >= 0xc0) l = 2;
			if (take + l > 42) break;
			take += l;
		}
		if (k) { if (k < max - 3) { out[k++] = '\r'; out[k++] = '\n'; out[k++] = ' '; } }
		memcpy(out + k, "=?UTF-8?B?", 10); k += 10;
		for (j = 0; j < take && k < max - 6; j += 3) {
			unsigned int v = ((unsigned char)utf[i + j]) << 16;
			int r = take - j;
			if (r > 1) v |= ((unsigned char)utf[i + j + 1]) << 8;
			if (r > 2) v |= (unsigned char)utf[i + j + 2];
			out[k++] = b64[(v >> 18) & 63];
			out[k++] = b64[(v >> 12) & 63];
			out[k++] = r > 1 ? b64[(v >> 6) & 63] : '=';
			out[k++] = r > 2 ? b64[v & 63] : '=';
		}
		out[k++] = '?'; out[k++] = '=';
		i += take;
	}
	out[k] = 0;
}

/* ------------------------------------------------------ modified UTF-7 (RFC 3501) */

void cs_mutf7_decode(const char *in, char *out, int max)
{
	int k = 0;
	const char *p = in;
	while (*p && k < max - 1) {
		if (*p == '&') {
			p++;
			if (*p == '-') { out[k++] = '&'; p++; continue; }
			{
				unsigned int acc = 0, hi = 0; int bits = 0;
				while (*p && *p != '-') {
					int c = *p == ',' ? 63 : b64val((unsigned char)*p);
					p++;
					if (c < 0) continue;
					acc = (acc << 6) | c; bits += 6;
					if (bits >= 16) {
						unsigned int u = (acc >> (bits - 16)) & 0xffff;
						bits -= 16;
						if (u >= 0xd800 && u < 0xdc00) { hi = u; continue; }
						if (u >= 0xdc00 && u < 0xe000 && hi) { u = 0x10000 + ((hi - 0xd800) << 10) + (u - 0xdc00); hi = 0; }
						k = cs_put_ucs(u, out, k, max);
					}
				}
				if (*p == '-') p++;
			}
			continue;
		}
		out[k++] = *p++;
	}
	out[k] = 0;
}

void cs_mutf7_encode(const char *in, char *out, int max)
{
	static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";
	const unsigned char *s = (const unsigned char *)in;
	int k = 0;
	while (*s && k < max - 1) {
		if (*s >= 0x20 && *s < 0x7f) {
			if (*s == '&') { if (k < max - 2) { out[k++] = '&'; out[k++] = '-'; } s++; continue; }
			out[k++] = (char)*s++;
			continue;
		}
		{
			unsigned int acc = 0; int bits = 0;
			if (k >= max - 1) break;
			out[k++] = '&';
			while (*s && (*s < 0x20 || *s >= 0x7f) && k < max - 4) {
				acc = (acc << 16) | cs_cp1252_to_ucs(*s++); bits += 16;
				while (bits >= 6) { bits -= 6; out[k++] = b64[(acc >> bits) & 63]; }
			}
			if (bits) out[k++] = b64[(acc << (6 - bits)) & 63];
			if (k < max - 1) out[k++] = '-';
		}
	}
	out[k] = 0;
}
