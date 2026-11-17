#!/bin/bash
# rulestest.sh [psimail-host] - (1.01) Tools > Rules (engine/pmmain.c,
# rules_run): rules.txt beside the account's mail is used on new Inbox mail
# only - not on the first sync, not on mail already there - and the first
# rule that matches acts: a subject moved to Archive (gone from the local
# Inbox, MOVE or COPY at the server), a sender's mail marked read (\Seen at
# the server and in the index); a rule with no folder is skipped.
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1187
STORE=$(mktemp -d /tmp/pmrules.XXXXXX)
python3 "$HERE/fakeimap.py" $PORT --arrive 3,4 > "$STORE/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$STORE"' EXIT
sleep 0.5
export PM_HOST=127.0.0.1 PM_PORT=$PORT PM_TLS=0 PM_USER=x PM_PASS=x
fail=0
run() { "$H" -s "$STORE" "$@" 2>/dev/null | grep -v '^\['; }
ok() { echo "ok: $1"; }
bad() { echo "FAIL: $1"; fail=1; }
inbox() { cut -f1,2 "$STORE"/A0/F*/index.txt 2>/dev/null; }
run folders > /dev/null
run sync INBOX > /dev/null
IDX=$(grep -l . "$STORE"/A0/F*/index.txt | head -1)
# the rules: a bad line (a move with no folder), then the two that act
printf 'S\tarrived\tM\t\nS\tarrived 106\tM\tArchive\nF\tann@example.com\tR\t\n' > "$STORE/A0/rules.txt"
run sync INBOX > /dev/null
grep -q "^10[1-5]	" "$IDX" && ok "the mail already there is left alone" || bad "old mail touched"
grep -qE 'UID (MOVE|COPY) 10[1-5] ' "$STORE/server.log" && bad "an old message was moved" || ok "nothing old moved"
run sync INBOX > /dev/null                 # 106 arrives: the subject rule moves it
grep -qE 'UID (MOVE|COPY) 106 "?Archive' "$STORE/server.log" && ok "106 moved to Archive at the server" || bad "106 not moved"
grep -q "^106	" "$IDX" && bad "106 still in the local Inbox" || ok "106 gone from the local Inbox"
run sync INBOX > /dev/null                 # 107 arrives from Ann: marked read
grep -q 'UID STORE 107 +FLAGS' "$STORE/server.log" && ok "107 marked read at the server" || bad "107 not marked read"
grep -q "^107	[^	]*S" "$IDX" && ok "107 read in the index" || bad "107 not read in the index"
[ $fail = 0 ] && echo "rulestest: all passed" || { echo "rulestest: FAILED"; tail -20 "$STORE/server.log"; exit 1; }
