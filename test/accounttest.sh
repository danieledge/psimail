#!/bin/bash
# accounttest.sh [psimail-host] - accounts set up again in another order
# (review R11, 1.11): a slot whose account.txt names another login never
# loses that login's Outbox (drafts are kept there) or pending.txt. The two
# slots swap; an old login's unsent mail is kept aside (K<hash>) and comes
# back when that login is set up again in any slot; a login with nothing
# unsent has its copy of the server's mail removed, as before.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1184
fail=0
check() { # check "expected" "got"
	if [ "$2" != "$1" ]; then echo "FAIL: wanted '$1', got '$2'"; fail=1; else echo "ok: $2"; fi
}
has() { [ -f "$1" ] && echo "ok: $2" || { echo "FAIL: $2 ($1 missing)"; fail=1; }; }
STORE=$(mktemp -d /tmp/pmacct.XXXXXX)
python3 "$HERE/fakeimap.py" $PORT > "$STORE/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; [ $fail = 0 ] && rm -rf "$STORE"' EXIT
sleep 0.5
export PM_HOST=127.0.0.1 PM_PORT=$PORT PM_TLS=0 PM_PASS=secret
run() { "$H" -s "$STORE" "$@" 2>/dev/null | grep -v '^\['; }
login() { head -1 "$STORE/A$1/account.txt" 2>/dev/null; }
draft() { # draft SLOT N TEXT
	mkdir -p "$STORE/A$1/outbox"
	printf 'To: x@example.com\nSubject: %s\nDraft: 1\n\n%s\n' "$3" "$3" > "$STORE/A$1/outbox/$2.txt"
}

echo "== alice in slot 0, bob in slot 1, a draft each"
check "sync: OK 5 new" "$(PM_ACCT=0 PM_USER=alice@x PM_USER1=bob@x run sync INBOX)"
check "sync: OK 5 new" "$(PM_ACCT=1 PM_USER=bob@x PM_USER0=alice@x run sync INBOX)"
draft 0 1 alice-draft
draft 1 1 bob-draft
check "alice@x@127.0.0.1" "$(login 0)"

echo "== set up again the other way round: bob in slot 0, alice in slot 1"
got=$(PM_ACCT=0 PM_USER=bob@x PM_USER1=alice@x run sync INBOX)
check "sync: OK No new messages" "$(echo "$got" | sed -n 1p)"     # (the cache came with the login)
check "bob@x@127.0.0.1" "$(login 0)"
check "alice@x@127.0.0.1" "$(login 1)"
grep -q bob-draft "$STORE/A0/outbox/1.txt" && echo "ok: bob's draft is in slot 0" || { echo "FAIL: bob's draft"; fail=1; }
grep -q alice-draft "$STORE/A1/outbox/1.txt" && echo "ok: alice's draft is in slot 1" || { echo "FAIL: alice's draft"; fail=1; }
got=$(PM_ACCT=1 PM_USER=alice@x PM_USER0=bob@x run sync INBOX)
check "sync: OK No new messages" "$(echo "$got" | sed -n 1p)"     # (the cache came with the login)
grep -q alice-draft "$STORE/A1/outbox/1.txt" && echo "ok: alice's draft still in slot 1" || { echo "FAIL: alice's draft gone"; fail=1; }

echo "== carol in slot 0 (bob nowhere): bob's unsent mail kept aside"
got=$(PM_ACCT=0 PM_USER=carol@x PM_USER1=alice@x run sync INBOX)
echo "$got"
check "  note: Unsent mail kept	1 message for bob@x: set it up again to send" "$(echo "$got" | sed -n 2p)"
check "carol@x@127.0.0.1" "$(login 0)"
[ -f "$STORE/A0/outbox/1.txt" ] && { echo "FAIL: bob's draft left in carol's Outbox"; fail=1; } || echo "ok: carol's Outbox is her own"
K=$(grep -l '^bob@x@127.0.0.1$' "$STORE"/K*/account.txt 2>/dev/null | head -1)
[ -n "$K" ] && grep -q bob-draft "$(dirname "$K")/outbox/1.txt" && echo "ok: kept in $(basename "$(dirname "$K")")" || { echo "FAIL: bob's draft not kept"; fail=1; }

echo "== bob set up again, in slot 2: his draft back"
got=$(PM_ACCT=2 PM_USER=bob@x PM_USER0=carol@x PM_USER1=alice@x run sync INBOX)
echo "$got"
check "  note: Unsent mail back	1 message for bob@x in the Outbox again" "$(echo "$got" | sed -n 2p)"
grep -q bob-draft "$STORE/A2/outbox/1.txt" && echo "ok: bob's draft in slot 2" || { echo "FAIL: bob's draft not back"; fail=1; }
check "0" "$(ls -d "$STORE"/K* 2>/dev/null | wc -l)"

echo "== a change not yet sent (pending.txt) is kept too"
echo "x" > "$STORE/A1/pending.txt"
PM_ACCT=1 PM_USER=erin@x PM_USER0=carol@x PM_OFFLINE=1 run sync INBOX > /dev/null
check "1" "$(ls "$STORE"/K*/pending.txt 2>/dev/null | wc -l)"

echo "== carol, nothing unsent, replaced by dave: her copy of the server's mail goes"
PM_ACCT=0 PM_USER=dave@x run sync INBOX > /dev/null
check "dave@x@127.0.0.1" "$(login 0)"
check "" "$(grep -l '^carol' "$STORE"/K*/account.txt 2>/dev/null)"

[ $fail = 0 ] && echo "accounttest: all passed" || echo "accounttest: FAILED"
exit $fail
