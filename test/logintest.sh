#!/bin/bash
# logintest.sh [psimail-host] - a login the server refuses (issue 1), against
# fakeimap.py --user: each command fails with PM_RES_LOGIN_FAILED and the
# server's own words (the app shows them in its Log in dialog, pmlogin.cpp),
# not a bare "failed"; an account with no password asks for one
# (NEED_PASS) without connecting; and with the user name put right, the
# mail comes and what was written meanwhile is kept (engine/store.c: no
# mail came under the wrong name, so it is not another account's store).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1183
fail=0
check() { # check "expected" "got"
	if [ "$2" != "$1" ]; then echo "FAIL: wanted '$1', got '$2'"; fail=1; else echo "ok: $2"; fi
}
STORE=$(mktemp -d /tmp/pmlogin.XXXXXX)
python3 "$HERE/fakeimap.py" $PORT --user dan@example.com > "$STORE/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; [ $fail = 0 ] && rm -rf "$STORE"' EXIT
sleep 0.5
export PM_HOST=127.0.0.1 PM_PORT=$PORT PM_TLS=0 PM_PASS=secret
run() { "$H" -s "$STORE" "$@" 2>/dev/null | grep -v '^\['; }

echo "== the user name, not the address"
got=$(PM_USER=dan run folders , sync INBOX)
check "folders: LOGIN_FAILED no such user" "$(echo "$got" | sed -n 1p)"
check "sync: LOGIN_FAILED no such user" "$(echo "$got" | sed -n 2p)"
check "2" "$(grep -c 'LOGIN "dan"' "$STORE/server.log")"
check "body: LOGIN_FAILED no such user" "$(PM_USER=dan run body INBOX 101)"

echo "== no password saved"
n=$(grep -c LOGIN "$STORE/server.log")
check "sync: NEED_PASS Password needed for Test" "$(PM_USER=dan PM_PASS= run sync INBOX)"
check "$n" "$(grep -c LOGIN "$STORE/server.log")"     # (not even connected)

echo "== written meanwhile"
mkdir -p "$STORE/A0/outbox"
printf 'To: x@example.com\nSubject: kept\nDraft: 1\n\nwritten while the login was refused\n' > "$STORE/A0/outbox/1.txt"

echo "== put right"
export PM_USER=dan@example.com
check "folders: OK " "$(run folders)"
check "sync: OK 5 new" "$(run sync INBOX)"
[ -f "$STORE/A0/outbox/1.txt" ] && echo "ok: the draft is still in the Outbox" || { echo "FAIL: the store was started afresh"; fail=1; }

echo "== another user name once mail has come: another account, started afresh"
check "sync: LOGIN_FAILED no such user" "$(PM_USER=someone run sync INBOX)"
[ ! -f "$STORE/A0/outbox/1.txt" ] && echo "ok: another account's files" || { echo "FAIL: kept another account's files"; fail=1; }

[ $fail = 0 ] && echo "logintest: all passed" || echo "logintest: FAILED"
exit $fail
