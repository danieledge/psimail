#!/bin/bash
# synctest.sh [psimail-host] - (1.08) a folder sync removes a message from
# the Psion only on the server's own word. A sync that runs out of memory,
# gets an answer too big to read whole, or loses the connection part way
# through the server's list must keep every message it has (index.txt and
# the downloaded texts), and a message really gone from the server must
# still go.
#
# The Inbox holds 305 messages (fakeimap.py --many 300), more than the 256
# the flag check's list starts with, so it has to grow. PM_FAILALLOC=N
# makes the host engine's Nth allocation fail (N+: that one and every
# later one), for every N a sync makes.
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1289
TMPD=$(mktemp -d /tmp/pmsync.XXXXXX)
STORE=$TMPD/store
fail=0
SRV=
trap 'kill $SRV 2>/dev/null; rm -rf "$TMPD"' EXIT
export PM_HOST=127.0.0.1 PM_TLS=0 PM_USER=x PM_PASS=x PM_COUNT=400
server() { kill $SRV 2>/dev/null; wait $SRV 2>/dev/null; PORT=$((PORT + 1)); export PM_PORT=$PORT
	python3 "$HERE/fakeimap.py" $PORT "$@" > "$TMPD/server.log" 2>&1 & SRV=$!; sleep 0.4; }
# run STORE CMD...: the results on stdout, the engine's log in $TMPD/log
run() { local st=$1; shift; "$H" -s "$st" "$@" 2> "$TMPD/log" | grep -v '^\['; }
bad() { echo "FAIL: $*"; fail=$((fail + 1)); }
uids() { cut -f1 "$1"/A0/F*/index.txt | grep -v "^#" | sort; }

# what every check compares with: all 305 messages, three with their text
server --many 300
mkdir -p "$STORE"
run "$STORE" sync INBOX > /dev/null
for u in 101 104 1150; do run "$STORE" body INBOX $u > /dev/null; done
ALL=$(uids "$STORE")
[ "$(echo "$ALL" | wc -l)" = 305 ] && echo "ok: 305 messages in the store" || bad "the first sync got $(echo "$ALL" | wc -l) messages"
for u in 101 104 1150; do [ -f "$STORE"/A0/F*/$u.txt ] || bad "no text for $u"; done
cp -a "$STORE" "$TMPD/clean"
fresh() { rm -rf "$STORE"; cp -a "$TMPD/clean" "$STORE"; }
# intact WHAT [GONE...]: every message is still there but the GONE ones
# (which may or may not be), and the texts too
intact() {
	local what=$1 u miss; shift
	miss=$(comm -23 <(echo "$ALL" | grep -vxF -f <(printf '%s\n' "$@" 0)) <(uids "$STORE") | head -3 | tr '\n' ' ')
	[ -z "$miss" ] || { bad "$what: lost $miss"; return 1; }
	for u in 101 104 1150; do
		case " $* " in *" $u "*) continue ;; esac
		[ -f "$STORE"/A0/F*/$u.txt ] || { bad "$what: the text of $u was removed"; return 1; }
	done
	return 0
}

fresh
check=$(run "$STORE" sync INBOX)
[ "$check" = "sync: OK No new messages" ] && echo "ok: $check" || bad "an ordinary sync: $check"
intact "an ordinary sync" && echo "ok: an ordinary sync keeps them all"

# every allocation of a sync failing in turn, once and from then on
for mode in "" "+"; do
	n=1; bad0=$fail
	while :; do
		fresh
		out=$(PM_FAILALLOC=$n$mode run "$STORE" sync INBOX)
		grep -q 'alloc test' "$TMPD/log" || break
		intact "allocation $n$mode failing ($out)" || break
		n=$((n + 1))
	done
	[ $fail = $bad0 ] && echo "ok: each of $((n - 1)) allocations failing${mode:+ and every one after it}: nothing lost"
done
fresh
out=$(PM_FAILALLOC=1+ run "$STORE" sync INBOX)
case "$out" in "sync: OK"*) bad "out of memory but '$out'" ;; *) echo "ok: out of memory says so: $out" ;; esac

# a first sync (all 305 new) out of memory part way: the next sync must
# bring every one of them, with none skipped
server --many 300
n=1; bad0=$fail
while :; do
	rm -rf "$STORE"; mkdir -p "$STORE"
	PM_FAILALLOC=$n run "$STORE" sync INBOX > /dev/null
	grep -q 'alloc test' "$TMPD/log" || break
	run "$STORE" sync INBOX > /dev/null
	[ "$(uids "$STORE")" = "$ALL" ] || { bad "allocation $n failing in a first sync: $(uids "$STORE" | wc -l) after the next"; break; }
	n=$((n + 1))
done
[ $fail = $bad0 ] && echo "ok: a first sync with each of $((n - 1)) allocations failing: the next one brings them all"

# an answer too big for the parser (4000 keywords before the UID), and one
# longer than the 48 KB buffer: that message must not look gone
for n in 4000 9000; do
	server --many 300 --fat-flags 1100:$n
	fresh
	out=$(run "$STORE" sync INBOX)
	intact "$n keywords" && grep -q 'nothing removed' "$TMPD/log" && echo "ok: $n keywords before the UID: nothing removed ($out)" \
		|| bad "$n keywords: no 'nothing removed' in the log"
done

# the connection cut after 150 lines of the flag list
server --many 300 --cut-flags 150
fresh
out=$(run "$STORE" sync INBOX)
intact "the flag list cut short" && echo "ok: the flag list cut short: nothing lost ($out)"

# the connection dropped at every point of a sync, 250 bytes apart
server --many 300
fresh
run "$STORE" sync INBOX > /dev/null
bad0=$fail; k=0
for at in $(seq 60 250 14000); do
	server --many 300 --drop-at $at
	fresh
	out=$(run "$STORE" sync INBOX)
	intact "a drop at byte $at ($out)" || break
	k=$((k + 1))
done
[ $fail = $bad0 ] && echo "ok: dropped at $k points: nothing lost"

# and a message the server really has lost still goes
server --many 300 --hide 104,1150
fresh
out=$(run "$STORE" sync INBOX)
left=$(uids "$STORE" | wc -l)
[ "$left" = 303 ] && ! grep -qx '104' <(uids "$STORE") && [ ! -f "$STORE"/A0/F*/104.txt ] \
	&& echo "ok: two gone from the server are gone here ($out)" || bad "gone from the server: $left left, $out"
intact "two gone from the server" 104 1150 || true
n=1; bad0=$fail
while :; do
	fresh
	PM_FAILALLOC=$n run "$STORE" sync INBOX > /dev/null
	grep -q 'alloc test' "$TMPD/log" || break
	intact "allocation $n failing, two gone" 104 1150 || break
	n=$((n + 1))
done
[ $fail = $bad0 ] && echo "ok: with two gone, each of $((n - 1)) allocations failing: only those two may go"

[ $fail = 0 ] && echo "synctest: all passed" || { echo "synctest: FAILED"; exit 1; }
