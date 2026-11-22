#!/bin/bash
# foldertest.sh [psimail-host] - folder commands against fakeimap.py:
# create (top level and under Work, a non-ASCII name sent as modified
# UTF-7), rename (the local files follow), delete (refused for the Inbox, a
# standard folder and a folder with children; offline). Then (1.18) a long
# list as Fastmail sends it, the standard folders last: they are all kept
# and put first, flagged (RFC 6154) or by their names, and a list longer
# than PsiMail keeps says how many are left out.
set -u
# the engine's text is Windows-1252: in a UTF-8 locale grep took the 0xE9 of
# the non-ASCII name for binary and the test failed with nothing wrong
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
PORT=1179
STORE=$(mktemp -d /tmp/pmft.XXXXXX)
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
check "folders: OK " "$(run folders)"
check "mkfolder: OK Folder \"Caf$(printf '\xe9') 2026\" created" "$(run mkfolder - "Caf$(printf '\xe9') 2026")"
grep -q 'CREATE "Caf&AOk- 2026"' "$STORE/server.log" && echo "ok: sent as modified UTF-7" || { echo "FAIL: not modified UTF-7"; fail=1; }
grep -q '^-.*Caf&AOk- 2026	Caf. 2026' "$STORE/A0/folders.txt" && echo "ok: in folders.txt" || { echo "FAIL: folders.txt"; fail=1; }
check "mkfolder: OK Folder \"Notes\" created" "$(run mkfolder Work Notes)"
grep -q 'CREATE "Work/Notes"' "$STORE/server.log" && echo "ok: under Work with the delimiter" || { echo "FAIL: child name"; fail=1; }
check "mkfolder: FAILED Mailbox already exists" "$(run mkfolder Work Notes)"
check "mkfolder: FAILED No folder name entered" "$(run mkfolder - "")"
check "sync: OK No new messages" "$(run sync "Work/Notes")"
[ -d "$STORE"/A0/F* ] && echo "ok: the folder has files" || { echo "FAIL: no store dir"; fail=1; }
before=$(ls -d "$STORE"/A0/F*)
check "renfolder: OK Folder renamed \"Old notes\"" "$(run renfolder "Work/Notes" "Old notes")"
after=$(ls -d "$STORE"/A0/F*)
[ "$before" != "$after" ] && [ -f "$after/index.txt" ] && echo "ok: the files followed the rename" || { echo "FAIL: store not moved ($before -> $after)"; fail=1; }
grep -q '^-.*Work/Old notes' "$STORE/A0/folders.txt" && echo "ok: renamed in folders.txt" || { echo "FAIL: folders.txt after rename"; fail=1; }
check "renfolder: FAILED The Inbox can't be renamed" "$(run renfolder INBOX Mail)"
check "renfolder: FAILED A standard folder can't be renamed" "$(run renfolder Trash Bin)"
check "delfolder: FAILED The Inbox can't be deleted" "$(run delfolder INBOX)"
check "delfolder: FAILED A standard folder can't be deleted" "$(run delfolder Trash)"
check "delfolder: FAILED Mailbox has children" "$(run delfolder Work)"
check "delfolder: OFFLINE Working offline" "$(PM_OFFLINE=1 run delfolder "Work/Old notes")"
check "delfolder: OK Folder deleted" "$(run delfolder "Work/Old notes")"
[ -z "$(ls -d "$STORE"/A0/F* 2>/dev/null)" ] && echo "ok: its files went too" || { echo "FAIL: store dir left"; fail=1; }
grep -q 'Old notes' "$STORE/A0/folders.txt" && { echo "FAIL: still in folders.txt"; fail=1; } || echo "ok: gone from folders.txt"
check "delfolder: FAILED No such folder" "$(run delfolder Nowhere)"
kill $SRV 2>/dev/null; wait $SRV 2>/dev/null

# (1.18) many folders. many PORT ARGS...: a fresh store, folders, the list
many() {
	local port=$1; shift
	rm -rf "$STORE/m"; mkdir -p "$STORE/m"
	python3 "$HERE/fakeimap.py" $port "$@" > "$STORE/m.log" 2>&1 &
	SRV=$!
	sleep 0.5
	PM_PORT=$port "$H" -s "$STORE/m" folders 2>/dev/null | grep -v '^\['
	kill $SRV 2>/dev/null; wait $SRV 2>/dev/null
}
FT=$STORE/m/A0/folders.txt
kinds() { grep -v '^#' "$FT" | head -${1:-6} | awk -F'\t' '{printf "%s:%s ", $1, $4}'; }
check "folders: OK " "$(many 1181 --fastmail 85)"
check "85" "$(grep -vc '^#' "$FT")"
check "I:INBOX D:Drafts S:Sent A:Archive J:Spam T:Trash " "$(kinds)"
grep -q '^#MORE' "$FT" && { echo "FAIL: #MORE with nothing left out"; fail=1; } || echo "ok: no #MORE"
check "N:Alerts -:Alerts/System Alerts " "$(grep -v '^#' "$FT" | sed -n 7,8p | awk -F'\t' '{printf "%s:%s ", $1, $4}')"
grep -q '	Snoozed	' "$FT" && grep -q '	Scheduled	' "$FT" && echo "ok: Snoozed and Scheduled listed" || { echo "FAIL: the last folders"; fail=1; }
grep -q 'STATUS "Trash"' "$STORE/m.log" && echo "ok: the standard folders counted first" || { echo "FAIL: Trash not counted"; fail=1; }

check "folders: OK " "$(many 1182 --fastmail 85 --names-only)"
check "I:INBOX D:Drafts S:Sent Items A:Archive J:Junk E-mail T:Deleted Items " "$(kinds)"

check "folders: OK " "$(many 1183 --fastmail 85 --list-status)"
grep -q 'LIST "" "\*" RETURN (SPECIAL-USE STATUS' "$STORE/m.log" && echo "ok: LIST RETURN (SPECIAL-USE STATUS)" || { echo "FAIL: no LIST-STATUS"; fail=1; }
check "T	0	0	Trash" "$(grep '^T' "$FT" | cut -f1-4)"
grep -q '< P0.. STATUS' "$STORE/m.log" && { echo "FAIL: STATUS sent though LIST gave it"; fail=1; } || echo "ok: no STATUS commands"

# more than PsiMail keeps (300): the standard ones still in, the rest counted
check "folders: OK " "$(many 1184 --fastmail 420)"
check "304" "$(grep -vc '^#' "$FT")"
check "I:INBOX D:Drafts S:Sent A:Archive J:Spam T:Trash " "$(kinds)"
check "#MORE	116" "$(grep '^#MORE' "$FT")"
check "Folder 296" "$(grep -v '^#' "$FT" | tail -1 | cut -f4)"

[ $fail = 0 ] && echo "ALL OK" || echo "FAILED"
exit $fail
