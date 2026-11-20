#!/bin/bash
# savetest.sh [psimail-host] - (1.13) what a message's files and headers
# can do to the store, against fakeimap.py --crafted:
#  - a full disk (here a 64 KB file size limit, ulimit -f) stops a
#    400 KB attachment's download at once: "Could not save", and only the
#    pieces up to the failure are fetched, not the whole file;
#  - an attachment far bigger than its BODYSTRUCTURE size is cut off (the
#    server is sending something else) and nothing is saved;
#  - a subject and a file name that decode to a newline and a tab stay on
#    one line: the .txt has one Subject line, the .att list one row of five
#    columns, the index one line per message;
#  - with dup.txt and dup(1).txt to dup(99).txt already there, a 101st
#    is refused and dup(99).txt is left as it was.
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1191
T=$(mktemp -d /tmp/pmsave.XXXXXX)
python3 "$HERE/fakeimap.py" $PORT --crafted > "$T/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
sleep 0.5
# (modem mode: 8 KB pieces, so the count of pieces says how much was fetched)
export PM_HOST=127.0.0.1 PM_PORT=$PORT PM_TLS=0 PM_USER=x PM_PASS=x PM_NETMODE=0
fail=0
check() { # check "expected" "got"
	if [ "$2" != "$1" ]; then echo "FAIL: wanted '$1', got '$2'"; fail=1; else echo "ok: $2"; fi
}
run() { "$H" -s "$T/s" "$@" 2>/dev/null | grep -v '^\['; }
pieces() { grep -c "BODY.PEEK\[$1\]" "$T/server.log"; }
S=$T/s
mkdir -p "$S"
run sync INBOX > /dev/null
D=$(ls -d "$S"/A0/F*/ | head -1)

# headers and file names: one line each
run body INBOX 301 > /dev/null
check "1" "$(grep -c '^Subject: ' "$D/301.txt")"
check "Subject: Hello X-Injected: yes tab" "$(grep '^Subject: ' "$D/301.txt")"
check "0" "$(grep -c '^X-Injected' "$D/301.txt")"
check "1 rows, 5 columns" "$(awk -F'\t' '{n++; c = NF} END {print n " rows, " c " columns"}' "$D/301.att")"
check "evil line col.bin" "$(cut -f3 "$D/301.att")"
check "0" "$(grep -c '^X-Injected' "$D/index.txt")"

# a full disk: the download stops at the first failed write
n0=$(pieces 2)
out=$( (trap '' XFSZ; ulimit -f 64; run attach INBOX 301 2) )
case "$out" in *"Could not save the attachment"*) echo "ok: $out";; *) echo "FAIL: attach on a full disk said '$out'"; fail=1;; esac
got=$(( $(pieces 2) - n0 ))
if [ "$got" -le 16 ]; then echo "ok: $got pieces fetched of about 50"; else echo "FAIL: $got pieces fetched (the whole attachment is about 50)"; fail=1; fi
check "" "$(ls "$S/attachments/" 2>/dev/null | grep 'col.bin')"
# and with room, all of it
out=$(run attach INBOX 301 2)
case "$out" in *"Saved"*) echo "ok: saved with room";; *) echo "FAIL: attach with room said '$out'"; fail=1;; esac
check "307200" "$(stat -c %s "$S/attachments/evil line col.bin" 2>/dev/null)"

# an attachment far bigger than it says
n0=$(pieces 2)
out=$(run attach INBOX 302 2)
case "$out" in *"more than the attachment holds"*) echo "ok: $out";; *) echo "FAIL: the oversized attachment said '$out'"; fail=1;; esac
got=$(( $(pieces 2) - n0 ))
if [ "$got" -le 12 ]; then echo "ok: $got pieces fetched of about 50"; else echo "FAIL: $got pieces of the oversized attachment fetched"; fail=1; fi
check "" "$(ls "$S/attachments/" | grep liar)"

# the 101st file of one name
A=$S/attachments
echo "keep me" > "$A/dup.txt"
for i in $(seq 1 99); do echo "keep me $i" > "$A/dup($i).txt"; done
out=$(run attach INBOX 303 2)
case "$out" in *"already 100 files"*) echo "ok: $out";; *) echo "FAIL: the 101st dup.txt said '$out'"; fail=1;; esac
check "keep me 99" "$(cat "$A/dup(99).txt")"
rm "$A/dup(42).txt"
out=$(run attach INBOX 303 2)
case "$out" in *"dup(42).txt"*) echo "ok: a free name is still found";; *) echo "FAIL: with dup(42).txt free: '$out'"; fail=1;; esac
check "new copy" "$(cat "$A/dup(42).txt" | tr -d '\r')"

[ $fail = 0 ] && echo "savetest: all passed" || echo "savetest: FAILED"
exit $fail
