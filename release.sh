#!/bin/bash
# Signs a PsiMail build and puts it where the installed programs look.
#   release.sh <version>            e.g. release.sh 0.84
# Signs dist/PsiMail.sis (core/tools/release/sign.py --product PsiMail; the
# key is $PSITERM_RELEASE_KEY, else ~/.psiterm-signing/release.key, never in
# a repository), writes dist/PsiMail-version.txt, and copies the three files
# together into the psiterm checkout beside this one ($PSITERM, default
# ../psiterm) - its dist/ is what every installed PsiMail's updater reads
# (https://github.com/danieledge/psiterm, the branch's dist/). Commit and
# push them there. Nothing is copied unless all three are ready.
set -e
VERSION=${1:?version, e.g. 0.84}
TOP=$(cd "$(dirname "$0")" && pwd)
CORE=${PSION_CORE:-$TOP/core}
SIS=$TOP/dist/PsiMail.sis
[ -f "$SIS" ] || { echo "no $SIS: build first (bash build.sh)"; exit 1; }
# (1.13, review CR-18) the published source must be what the .sis was built
# from: core is the submodule itself (not a link to another checkout, not
# $PSION_CORE), clean, and at the commit this checkout records for it
[ -z "${PSION_CORE:-}" ] || { echo "PSION_CORE is set: build and sign with the core/ submodule"; exit 1; }
[ -L "$TOP/core" ] && { echo "core is a link to $(readlink "$TOP/core"): check out the submodule (git submodule update --init) and rebuild"; exit 1; }
REC=$(git -C "$TOP" ls-tree HEAD core | awk '{print $3}')
CUR=$(git -C "$TOP/core" rev-parse HEAD 2>/dev/null)
[ -n "$REC" ] && [ "$REC" = "$CUR" ] || { echo "core is at ${CUR:-?}, this checkout records ${REC:-?}: commit the submodule pointer, rebuild, then sign"; exit 1; }
[ -z "$(git -C "$TOP/core" status --porcelain --untracked-files=no)" ] || { echo "core has changes not committed: commit them in psion-core first"; exit 1; }
grep -q "TESTSEED" "$TOP/app/psimail.cpp" && { echo "app/psimail.cpp still has the TESTSEED block: core/tools/emu/seed.py off, then rebuild"; exit 1; }
# (1.13) the built .sis itself, not only the source: a seeded test build
# (core/tools/emu/seed.py) leaves its marker and the folders it copies in
# PsiMail.app, and a .sis made from an old seeded object would pass the grep
# above. An EPOC R5 .sis holds its files as they are, so a byte scan finds them.
python3 - "$SIS" <<'PY' || exit 1
import sys
data = open(sys.argv[1], 'rb').read()
found = [w for w in (b'CODA-TEST-SEED', b'TESTSEED', b'D:\\SEED\\', b'D:\\SEEDA0\\', b'imap.example.com', b'SyncSeed') if w in data]
if found:
	print("dist/PsiMail.sis is a seeded test build (%s): seed.py off, delete the build's objects, rebuild" % ', '.join(w.decode() for w in found))
	sys.exit(1)
PY
# and Coda's guard for its public card, when a Coda checkout is beside this
# one ($CODA_NETBOOK): what must never be in a public build, seeds included
GUARD=${CODA_NETBOOK:-$TOP/../psion-netbook}/tools/publicguard.mts
if [ -f "$GUARD" ]; then
	node --experimental-strip-types "$GUARD" --no-webpass-check "$SIS" || { echo "publicguard refused dist/PsiMail.sis"; exit 1; }
fi
python3 "$CORE/tools/release/sign.py" --product PsiMail "$SIS" "$VERSION"
echo "$VERSION" > "$TOP/dist/PsiMail-version.txt"
echo "signed: dist/PsiMail.sis, dist/PsiMail.sis.sig, dist/PsiMail-version.txt ($VERSION)"
PT=${PSITERM:-$TOP/../psiterm}
if [ -d "$PT/dist" ]; then
	cp "$SIS" "$SIS.sig" "$TOP/dist/PsiMail-version.txt" "$PT/dist/"
	echo "copied to $PT/dist/: commit and push PsiMail.sis, PsiMail.sis.sig and PsiMail-version.txt there (the updaters read psiterm's dist/)"
else
	echo "no psiterm checkout at $PT (set PSITERM): copy the three files into its dist/ yourself"
fi
