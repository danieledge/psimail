#!/bin/bash
# memtest.sh [psimail-host] - (1.15) what the engine leaves on the store,
# which with no card is C:, the Psion's memory, against fakeimap.py:
#  - a message's opened attachments (Open\<uid>\) and its invitation files
#    (.ics, .inv) go with it when it is moved away; an opened attachment
#    that a message waiting in the Outbox names stays;
#  - Open\ folders of messages no longer listed (left by earlier versions)
#    are swept after a sync;
#  - an F<hash> folder that matches no folder on the server (removed from
#    another program) is swept after a whole folder list; the listed
#    folders' stay;
#  - the pictures set out: past the folder's limit (PM_PIC_KEEP_KB here)
#    the older messages' go, never the one just read, and come back when
#    asked for again;
#  - an attachment, and a message to send with attachments, are refused
#    with no room on the disk (PM_FREE_KB), before anything is written;
#  - the decode of a picture is fitted to the memory free (PM_FREE_RAM_KB);
#  - the engine's loop trims after work and carries on.
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1193
T=$(mktemp -d /tmp/pmmem.XXXXXX)
python3 "$HERE/newspics.py" "$T/pics" > /dev/null || { echo "memtest: needs Pillow"; exit 2; }
python3 "$HERE/fakeimap.py" $PORT --crafted --invite --news "$T/pics" > "$T/server.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
sleep 0.5
export PM_HOST=127.0.0.1 PM_PORT=$PORT PM_TLS=0 PM_USER=x PM_PASS=x
fail=0
check() { # check "expected" "got"
	if [ "$2" != "$1" ]; then echo "FAIL: wanted '$1', got '$2'"; fail=1; else echo "ok: $2"; fi
}
run() { "$H" -s "$S" "$@" 2>/dev/null | grep -v '^\['; }
S=$T/s
mkdir -p "$S"
run folders > /dev/null
run sync INBOX > /dev/null
D=$(dirname "$(ls "$S"/A0/F*/index.txt | xargs grep -l '^303	' | head -1)")/
there() { [ -e "$1" ] && echo there || echo gone; }

# opened attachments and invitation files go with the message
run body INBOX 106 > /dev/null
check "there there" "$(there "$D/106.ics") $(there "$D/106.inv")"
mkdir -p "${D}Open/303" "${D}Open/301"     # (made by the app, as on the Psion)
check "attach: OK Saved ${D}Open/303/dup.txt" "$(run attach INBOX 303 "2	${D}Open/303/")"
check "move: OK " "$(run move INBOX 303 Trash)"
check "gone" "$(there "${D}Open/303")"
check "move: OK " "$(run move INBOX 106 Trash)"
check "gone gone" "$(there "$D/106.ics") $(there "$D/106.inv")"

# ...but not one a message in the Outbox will send
check "attach: OK Saved ${D}Open/301/evil line col.bin" "$(run attach INBOX 301 "2	${D}Open/301/")"
mkdir -p "$S/A0/outbox"
printf 'To: x@example.com\nSubject: fwd\nAttach: %s\n\nsee\n' "${D}Open/301/evil line col.bin" > "$S/A0/outbox/1.txt"
check "move: OK " "$(run move INBOX 301 Trash)"
check "there" "$(there "${D}Open/301/evil line col.bin")"

# Open\ folders of messages no longer listed: swept after a sync
mkdir -p "${D}Open/999" && echo x > "${D}Open/999/old.txt"
run sync INBOX > /dev/null
check "gone" "$(there "${D}Open/999")"
check "there" "$(there "${D}Open/301/evil line col.bin")"

# an F<hash> folder no folder on the server matches: swept after a list
mkdir -p "$S/A0/F12345678" && echo x > "$S/A0/F12345678/index.txt"
check "folders: OK " "$(run folders)"
check "gone" "$(there "$S/A0/F12345678")"
check "there" "$(there "$D/index.txt")"

# the pictures set out: the folder's limit
P=$D
run body INBOX 205 > /dev/null
run body INBOX 206 > /dev/null
check "pictures: OK 2 pictures" "$(run pictures INBOX 206 "2 3")"
check "2" "$(ls "$P" | grep -c '^206_.*\.pmi$')"
check "pictures: OK 1 picture" "$(PM_PIC_KEEP_KB=1 "$H" -s "$S" pictures INBOX 205 2 2> "$T/prune.log" | grep -v '^\[')"
check "0 1" "$(ls "$P" | grep -c '^206_.*\.pmi$') $(ls "$P" | grep -c '^205_.*\.pmi$')"
grep -q "older set out pictures removed" "$T/prune.log" && echo "ok: said in the log" || { echo "FAIL: not in the log"; fail=1; }

check "pictures: OK 2 pictures" "$(run pictures INBOX 206 "2 3")"
check "2" "$(ls "$P" | grep -c '^206_.*\.pmi$')"

# no room: refused before anything is written
check "attach: FAILED No room left for the attachment" "$(PM_FREE_KB=100 run attach INBOX 206 2 | sed 's/ (.*//')"
printf 'To: x@example.com\nSubject: big\nAttach: %s\n\nsee\n' "${D}Open/301/evil line col.bin" > "$S/A0/outbox/2.txt"
rm -f "$S/A0/outbox/1.txt"
check "1" "$(PM_FREE_KB=100 run send | grep -c 'No room left to send it')"

# a picture's decode fitted to the memory free: the progressive JPEG in
# colour is set out with memory to spare; with 1.6 MB free its
# whole-picture buffer is held to 64 KB, and it is refused as too big
rm -f "$P"/205_*.pmi
check "pictures: OK 1 picture" "$(PM_SCREEN=640x480x256 run pictures INBOX 205 2)"
rm -f "$P"/205_*.pmi
check "pictures: OK too big" "$(PM_FREE_RAM_KB=1600 PM_SCREEN=640x480x256 run pictures INBOX 205 2)"

# the engine's loop after work: the heap given back at the first idle
# moment, the watcher's level read (none here: -1), and it carries on
check "1" "$(PM_PREFETCH=3 run sync INBOX , loop | grep -c '^loop: done')"

[ $fail = 0 ] && echo "memtest: all passed" || echo "memtest: FAILED"
exit $fail
