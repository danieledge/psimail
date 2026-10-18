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
grep -q "TESTSEED" "$TOP/app/psimail.cpp" && { echo "app/psimail.cpp still has the TESTSEED block: core/tools/emu/seed.py off, then rebuild"; exit 1; }
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
