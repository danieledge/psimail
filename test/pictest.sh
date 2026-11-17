#!/bin/bash
# pictest.sh [psimail-host] - (1.04) the pictures in a message, in greys
# and in colour, and the cache kept across the change:
#  - no screen named (the 5mx): every .pmi is "PMI1", 16 greys, two pixels
#    a byte, rows ((w + 7) / 8) * 4 bytes, as always;
#  - a colour screen (PM_SCREEN=640x480x256, the netBook): "PMI2", one
#    byte a pixel (EPOC's 256 colours), rows ((w + 3) / 4) * 4 bytes, more
#    than 16 colours used, a progressive JPEG too (prog_colour);
#  - a "PMI1" already there on the colour screen is the cache: left as it
#    is (not set out again), and counted in the message's budget.
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1187
T=$(mktemp -d /tmp/pmpic.XXXXXX)
python3 "$HERE/newspics.py" "$T/pics" > /dev/null || { echo "pictest: needs Pillow"; exit 2; }
python3 "$HERE/fakeimap.py" $PORT --news "$T/pics" > "$T/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
sleep 0.5
export PM_HOST=127.0.0.1 PM_PORT=$PORT PM_TLS=0 PM_USER=x PM_PASS=x
fail=0
check() { # check "expected" "got"
	if [ "$2" != "$1" ]; then echo "FAIL: wanted '$1', got '$2'"; fail=1; else echo "ok: $2"; fi
}
# pmi FILE: magic w h status, and whether the size matches the layout
pmi() {
	python3 - "$1" <<'PY'
import sys, struct
d = open(sys.argv[1], 'rb').read()
m = d[:4].decode('latin-1'); w, h, sw, sh, st = struct.unpack('<HHHHB', d[4:13])
if st == 2: print(m, 'reason', d[16:].split(b'\0')[0].decode()); sys.exit()
stride = (w + 3) // 4 * 4 if m == 'PMI2' else (w + 7) // 8 * 4
body = d[16:]
cols = len(set(body)) if m == 'PMI2' else len(set([b & 15 for b in body] + [b >> 4 for b in body]))
print(m, 'ok' if len(body) == stride * h else 'BADSIZE', 'many' if cols > 16 else 'few')
PY
}
run() { "$1" -s "$2" "${@:3}" 2>/dev/null | grep -v '^\['; }
for s in grey colour; do
	mkdir -p "$T/$s"
	[ $s = colour ] && export PM_SCREEN=640x480x256 || unset PM_SCREEN
	run "$H" "$T/$s" sync INBOX > /dev/null
	run "$H" "$T/$s" body INBOX 205 > /dev/null
	run "$H" "$T/$s" body INBOX 206 > /dev/null
done
D() { ls -d "$T/$1"/A0/F*/ | head -1; }
# the 5mx: greys, as always (205's part 2 is a progressive JPEG)
unset PM_SCREEN
check "pictures: OK 1 picture" "$(run "$H" "$T/grey" pictures INBOX 205 2)"
check "pictures: OK 2 pictures" "$(run "$H" "$T/grey" pictures INBOX 206 "2 3")"
check "PMI1 ok few" "$(pmi "$(D grey)/205_2.pmi")"
check "PMI1 ok few" "$(pmi "$(D grey)/206_2.pmi")"
check "PMI1 ok few" "$(pmi "$(D grey)/206_3.pmi")"
# the netBook: 206's part 2 set out in greys before 1.04 (copied in), the rest new
export PM_SCREEN=640x480x256
cp "$(D grey)/206_2.pmi" "$(D colour)/206_2.pmi"
sum=$(md5sum < "$(D colour)/206_2.pmi")
check "pictures: OK 1 picture" "$(run "$H" "$T/colour" pictures INBOX 205 2)"
check "pictures: OK 1 picture" "$(run "$H" "$T/colour" pictures INBOX 206 "2 3")"
check "PMI2 ok many" "$(pmi "$(D colour)/205_2.pmi")"
check "PMI1 ok few" "$(pmi "$(D colour)/206_2.pmi")"
check "$sum" "$(md5sum < "$(D colour)/206_2.pmi")"
check "PMI2 ok many" "$(pmi "$(D colour)/206_3.pmi")"
# the 5 MP progressive photo, asked for: in colour within the memory
check "pictures: OK 1 picture" "$(run "$H" "$T/colour" pictures INBOX 206 '!4')"
check "PMI2 ok many" "$(pmi "$(D colour)/206_4.pmi")"
[ $fail = 0 ] && echo "pictest: all passed" || { echo "pictest: FAILED"; exit 1; }
