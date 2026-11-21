#!/bin/bash
# localtest.sh [psimail-host] - (1.01) Find on this Psion (engine/local.c):
# with the Inbox synced and two texts downloaded, a search while offline
# looks at the downloaded mail - a word of a subject, a word only in a
# downloaded text, two words that must both be there, a word in a text
# not downloaded (not found), nothing - and "\x01L" words do the same
# while online, sending the server nothing.
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1185
STORE=$(mktemp -d /tmp/pmlocal.XXXXXX)
python3 "$HERE/fakeimap.py" $PORT > "$STORE/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$STORE"' EXIT
sleep 0.5
export PM_HOST=127.0.0.1 PM_PORT=$PORT PM_TLS=0 PM_USER=x PM_PASS=x
fail=0
run() { "$H" -s "$STORE" "$@" 2>/dev/null | grep -v '^\['; }
check() { # check "expected" "got"
	if [ "$2" != "$1" ]; then echo "FAIL: wanted '$1', got '$2'"; fail=1; else echo "ok: $2"; fi
}
uids() { cut -f1 "$STORE"/A0/F*/search.txt | grep -v '^#' | tr '\n' ' '; }
run sync INBOX > /dev/null
run body INBOX 102 > /dev/null
run body INBOX 104 > /dev/null
check "search: OK 1 found here" "$(PM_OFFLINE=1 run search INBOX hello)"
check "101 " "$(uids)"
check "search: OK 1 found here" "$(PM_OFFLINE=1 run search INBOX carrier)"
check "102 " "$(uids)"
# (101 says "short note" too, but its text is not on the Psion)
check "search: OK 1 found here" "$(PM_OFFLINE=1 run search INBOX "short note")"
check "104 " "$(uids)"
check "search: OK 0 found here" "$(PM_OFFLINE=1 run search INBOX "second carrier")"
check "search: OK 0 found here" "$(PM_OFFLINE=1 run search INBOX padding)"
check "search: FAILED Nothing to find" "$(PM_OFFLINE=1 run search INBOX " ")"
n=$(grep -c SEARCH "$STORE/server.log")
check "search: OK 1 found here" "$(run search INBOX "$(printf '\001L')WiRSa")"
check "$n" "$(grep -c SEARCH "$STORE/server.log")"
[ $fail = 0 ] && echo "localtest: all passed" || { echo "localtest: FAILED"; exit 1; }
