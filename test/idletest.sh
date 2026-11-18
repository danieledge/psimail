#!/bin/bash
# idletest.sh [psimail-host] - (1.01) new mail at once (PM_CMD_IDLE,
# engine/imap.c imap_idle): IDLE on the Inbox until the server says EXISTS,
# then DONE and a sync that brings the message; with nothing coming, DONE
# when the time is up and "No new mail"; a server without IDLE is told
# apart ("Not available"). (1.08) PsiMail.app ending during the IDLE
# (PM_APP_PID names a process standing for it) is noticed within seconds:
# DONE, and no second IDLE.
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1189
STORE=$(mktemp -d /tmp/pmidle.XXXXXX)
fail=0
SRV=
trap 'kill $SRV 2>/dev/null; rm -rf "$STORE"' EXIT
export PM_HOST=127.0.0.1 PM_TLS=0 PM_USER=x PM_PASS=x
server() { kill $SRV 2>/dev/null; PORT=$((PORT + 1)); export PM_PORT=$PORT
	python3 "$HERE/fakeimap.py" $PORT "$@" > "$STORE/server.log" 2>&1 & SRV=$!; sleep 0.5; }
run() { "$H" -s "$STORE" "$@" 2>/dev/null | grep -v '^\['; }
check() { if [ "$2" != "$1" ]; then echo "FAIL: wanted '$1', got '$2'"; fail=1; else echo "ok: $2"; fi; }
server --idle-arrive 2
run sync INBOX > /dev/null
t0=$(date +%s)
check "idle: OK 1 new in the Inbox" "$(run idle 30)"
[ $(( $(date +%s) - t0 )) -lt 15 ] && echo "ok: ended when the mail came" || { echo "FAIL: waited too long"; fail=1; }
grep -q '^106	' "$STORE"/A0/F*/index.txt && echo "ok: 106 in the Inbox" || { echo "FAIL: 106 not synced"; fail=1; }
grep -q "< DONE" "$STORE/server.log" && echo "ok: DONE sent" || { echo "FAIL: no DONE"; fail=1; }
server --idle-arrive 0
t0=$(date +%s)
check "idle: OK No new mail" "$(run idle 3)"
d=$(( $(date +%s) - t0 ))
[ $d -ge 3 ] && [ $d -lt 12 ] && echo "ok: waited the time ($d s)" || { echo "FAIL: waited $d s"; fail=1; }
server --idle-arrive 0
sleep 300 & APP=$!
t0=$(date +%s)
( sleep 2; kill $APP ) &
out=$(PM_APP_PID=$APP run idle 120)
d=$(( $(date +%s) - t0 ))
wait $APP 2>/dev/null
check "idle: CANCELLED PsiMail has ended" "$out"
[ $d -lt 10 ] && echo "ok: the app's end noticed in $d s" || { echo "FAIL: the app's end took $d s"; fail=1; }
grep -q "< DONE" "$STORE/server.log" && echo "ok: DONE sent" || { echo "FAIL: no DONE"; fail=1; }
[ "$(grep -c '< P[0-9]* IDLE' "$STORE/server.log")" = 1 ] && echo "ok: one IDLE" || { echo "FAIL: IDLE sent again"; fail=1; }
sleep 300 & APP=$!
t0=$(date +%s)
check "idle: OK No new mail" "$(PM_APP_PID=$APP run idle 4)"
d=$(( $(date +%s) - t0 )); kill $APP; wait $APP 2>/dev/null
[ $d -ge 4 ] && echo "ok: with the app there, waited the time ($d s)" || { echo "FAIL: left after $d s with the app there"; fail=1; }
server
check "idle: FAILED Not available - the server cannot say at once" "$(run idle 3)"
[ $fail = 0 ] && echo "idletest: all passed" || { echo "idletest: FAILED"; exit 1; }
