#!/bin/bash
# randomtest.sh [psimail-host] - (1.13) the random seed kept between runs
# (tls/pmrandom.c, random.dat in the store), against openssl s_server:
#  - the first TLS connection writes random.dat, 32 bytes, and leaves no
#    random.da~ beside it;
#  - each later run's seed is new (made from the pool, which took in the
#    last one);
#  - a store that cannot be written does not stop the connection.
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1193
T=$(mktemp -d /tmp/pmrand.XXXXXX)
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout "$T/key.pem" -out "$T/cert.pem" \
	-days 2 -subj /CN=localhost > /dev/null 2>&1 || { echo "randomtest: needs openssl"; exit 2; }
openssl s_server -quiet -tls1_3 -accept $PORT -cert "$T/cert.pem" -key "$T/key.pem" > "$T/server.log" 2>&1 < /dev/zero &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
sleep 0.7
export PM_HOST=127.0.0.1 PM_PORT=$PORT PM_TLS=1 PM_USER=x PM_PASS=x
fail=0
check() { # check "expected" "got"
	if [ "$2" != "$1" ]; then echo "FAIL: wanted '$1', got '$2'"; fail=1; else echo "ok: $2"; fi
}
run() { timeout 20 "$H" -s "$1" folders > "$1.out" 2>&1; }
S=$T/s
mkdir -p "$S"
run "$S"
grep -qi "certificate\|trust\|key" "$S.out" || { echo "FAIL: no TLS handshake: $(tail -2 "$S.out")"; fail=1; }
check "32" "$(stat -c %s "$S/random.dat" 2>/dev/null)"
check "" "$(ls "$S" | grep 'random.da~')"
a=$(md5sum < "$S/random.dat")
run "$S"
b=$(md5sum < "$S/random.dat")
run "$S"
c=$(md5sum < "$S/random.dat")
if [ "$a" != "$b" ] && [ "$b" != "$c" ] && [ "$a" != "$c" ]; then echo "ok: a new seed each run"; else echo "FAIL: the seed did not change"; fail=1; fi
# a store that cannot be written: the handshake still happens
R=$T/ro
mkdir -p "$R"
run "$R" > /dev/null
rm -f "$R/random.dat"
chmod a-w "$R"
run "$R"
chmod u+w "$R"
grep -qi "certificate\|trust\|key" "$R.out" && echo "ok: TLS with a read-only store" || { echo "FAIL: read-only store: $(tail -2 "$R.out")"; fail=1; }
check "" "$(ls "$R" | grep random)"

[ $fail = 0 ] && echo "randomtest: all passed" || echo "randomtest: FAILED"
exit $fail
