# CLAUDE.md: PsiMail for the Psion Series 5mx

This file is the project brief for Claude Code. Read it before changing
anything. User-facing docs are in `README.md`; the tests in
`test/README.md`; build details in core's `docs/BUILDING.md`.

## What this is

PsiMail: IMAP/SMTP email with TLS, a CalDAV calendar synced with the
Psion's Agenda, and Contacts, for the **Psion Series 5mx** (EPOC Release 5,
ARM710T at 36 MHz with no FPU, about 16 MB RAM, 640x240 in 16 greys, gcc 3.0
Psion 98r2). UID 0x01000A7C (the engine 0x01000A7D). The version is
`KVersion` in `app/psimail.cpp` and `pkg/psimail.pkg`. The quality bar is
first-party Psion engineering: it should look and behave like Psion's own
programs.

The code shared with the other programs (PsiTerm, PsiWeb, PsiCast...) is
[psion-core](https://github.com/danieledge/psion-core), the git submodule
`core/`: `core/ssh/psiglue.cpp` (the modem and Psion Internet routes),
`core/ssh/psishared.h`, `core/ssh/psilink.h` (the shared connection
settings, `C:\System\Data\PsiLink.ini`), `core/ssh/pglinktest.cpp` (the Test
button), `core/ssh/psihold.cpp` (`psihold.exe`), `core/ssh/tls13.c`,
`core/mail/engine/img` (the picture decoders), `core/mail/compat`,
`core/web/compat` (the ESTLIB shims) and `core/tools` (the build container,
the emulator tools, release signing). Core changes go to core first; then
move the submodule pointer here. Read `core/CLAUDE.md` too.

## Architecture: two processes

1. **PsiMail.app** (`app/`, an EIKON DLL): the screen, keys, menus, dialogs,
   settings.
2. **psimail.exe** (`engine/`, plain C on ESTLIB): the networking, TLS and
   parsing, so the app never blocks on the network.

They talk through the global chunk `PsiMailShared` (`psimail.h`, which
embeds `core/ssh/psishared.h`): single-writer rings, a command queue
(`PM_CMD_*`), progress strings, heartbeats both ways. The engine quits only
if the app's process has actually gone; a missed heartbeat alone is not
enough, because the app can be starved of CPU.

**App** (`app/`): `psimail.cpp` (AppUi, menus, commands, settings, compose,
dialogs), `pmnative.cpp` (the main view on EIKON controls: folder tree,
message list, reader on `CEikRichTextEditor` with a custom scroll bar,
calendar pane), and one file per feature:

| Feature | Files |
|---|---|
| Calendar | `pmcal.cpp`, `pmcalview.cpp`, `pmwrite.cpp` |
| Contacts | `pmcontacts.cpp` |
| Undo | `pmundo.cpp` |
| Print | `pmprint.cpp` |
| Alerts, timed check, auto-send | `pmauto.cpp` |
| Invitations | `pminvite.cpp` |
| vCards | `pmvcard.cpp` |
| Save as Word | `pmsaveword.cpp` |
| Store location and migration | `pmstore.cpp` |
| Pictures | `pmpict.cpp` |
| Web pictures | `pmwebpic.cpp` |
| Quiet or detailed progress | `pmstatus.cpp` |
| Reader header | `pmheader.cpp` |
| Engine restart | `pmrecover.cpp` |
| A refused login (asked once, the user name too) | `pmlogin.cpp` |
| Help | `pmhelp.cpp` |

`ui/pmquote.cpp` is reply quoting, `ui/pmcalmodel.cpp` the calendar model,
`ui/pmicons.h` the toolbar pictures (`tools/mkicons.py` draws them).

**Engine** (`engine/`): `pmmain.c` (the command loop); `imap.c`,
`imapparse.c`; `smtp.c`, `compose.c`; `mime.c`, `charset.c`, `html.c` (HTML
becomes the reader's line format); `store.c` (the local store); `caldav.c`,
`ics.c`, `caltz.c`, `xmlscan.c`; `pictures.c` (on `core/mail/engine/img`,
producing 16-grey `.pmi` files); `webpics.c`, `http.c`; `invite.c` (iTIP);
`undo.c`; `pmupdate.c`; `certcheck.c`, `pmrsa.c`; `pmnet.c`; `pmepoc.cpp`
(the EPOC side). `tls/` configures core's TLS for PsiMail (`includes.h`,
`options.h`, `x25519.c`). `host/pmhost.c` is the PC build of the engine, on
`core/mail/host/pgnet_host.c`.

`button/`: `pmbutton.exe`, the optional helper that makes the Email
silkscreen icon open PsiMail. `pmbtnrec.cpp`, a boot recogniser, is built
but deliberately not installed: starting a program during boot hung the
emulator.

**Data:** the store is `<disk>:\System\Data\PsiMail\A<n>\F<folder-hash>\`
(`<uid>.txt`, index files, `.pic`, `.pmi`, `.inv`, `.vcd`); an old
`\PsiMail\` is moved there once at start. Settings are `TPmSettings`, saved
as a **fixed-size struct** in `C:\System\Apps\PsiMail\PsiMail.ini`. **Never
change its size**: new options use the spare fields and bits marked in
`app/pmapp.h` (most are bits in `iView` and `iSpare[]`). Logs: `psimail.log`
(lines timestamped mm:ss.t, with `link:`, `picture ...`, `web picture ...`
and `cmd N ... (T ms, heap K KB)`) and `psimail.old`.

## Building and testing

```
git clone --recurse-submodules https://github.com/danieledge/psimail.git
core/tools/docker/psibuild setup                  # once: image + SDK beside the checkout
core/tools/docker/psibuild "bash build.sh host"   # dist/PsiMail.sis + build/mail-host/psimail-host
core/tools/docker/psibuild "cd test && bash foldertest.sh"
```

Host tests (`test/`, after `bash build.sh host` or `make TARGET=host`):
`foldertest.sh`, `undotest.sh`, `logintest.sh`, `invtest.py`, `invitehost.py`, `htmltest.py`,
`imgtest.py` (ASan/UBSan, fuzzing the picture decoders), `parsefuzz.py`
(MIME/IMAP/ICS/XML/HTML fuzzer), `certtest.py`. `fakeimap.py` is a
scriptable fake server. All network input is untrusted: run `parsefuzz.py`
and `imgtest.py` after changing a parser, and use bounded copies everywhere.

psimail.exe has a 64 KB stack; big buffers are static. Long work in the
engine must not starve the UI: decode at low priority, keep heartbeats
going. The engine must not use ESTLIB's `sprintf` or stdio (they corrupted
the stack); `core/web/compat/nsprintf.c` replaces them. The emulator's CF
card can wedge on many small writes, which is why `pm_write_whole` exists.

**Emulator** (`core/tools/emu`, see its README; the psionEmulators checkout
sits beside this one):
```
python3 core/tools/emu/seed.py on && bash build.sh && python3 core/tools/emu/seed.py off
node --experimental-strip-types core/tools/emu/mkcard.ts psimail --seed
core/tools/emu/run.sh reader "88 4" "92 17" "94 3" "96 3"     # -> build/emu/reader/end.png
```
Look at the screenshots with the Read tool. PsiMail's window appears at
about 78 s simulated time. **TESTSEED must never be committed**: after a
seeded build, `seed.py off` and rebuild before releasing
(`grep TESTSEED app/psimail.cpp` must find nothing).

The SDK keeps every program's objects in one folder
(`$EPOCROOT/epoc32/build/.../ptproj/marmd/rel`): delete the `*.o` there after
switching branches or checkouts (stale objects have caused USER 19 panics).

## Releasing

1. Bump `KVersion` in `app/psimail.cpp` and the version in `pkg/psimail.pkg`
   (two digits: 0.84 = `0,84,0`, because the EPOC installer compares
   major.minor only).
2. Build with the seed off; `grep TESTSEED app/psimail.cpp` finds nothing.
3. `release.sh <version>`: signs `dist/PsiMail.sis` with
   `core/tools/release/sign.py --product PsiMail` (the key comes from
   `$PSITERM_RELEASE_KEY` or `~/.psiterm-signing/release.key`; never in a
   repository), writes `dist/PsiMail-version.txt`, and copies the three
   files into the psiterm checkout beside this one (`../psiterm/dist/`, or
   `$PSITERM`). **Installed PsiMails read psiterm's `dist/`** on GitHub
   (`dev` for test builds, `main` for releases); do not change the updater's
   URLs in code.
4. Commit here on **dev** and push; commit the three files in psiterm's
   `dist/` on its dev and push. Never push or merge to `main`; the owner
   does that.

## UI rules: the EIKON Application Style Guide

Use the `epoc-eikon-ui-guide` skill if it is available. The essentials:
- Menu bar: File, Edit, Message, View, Tools (Event replaces Message in the
  calendar). At most 8 items per pane, cascades low in the pane; "..." only
  on commands that open a dialog; tick boxes rather than flip-flop names;
  dim unavailable items rather than hiding them. In `DynInitMenuPaneL`,
  only dim or tick items that exist in that pane, or EIKON panics (EIKON 8).
- Shortcuts: the standard ones (Ctrl+E Close, Ctrl+K Preferences, Ctrl+M /
  Shift+Ctrl+M zoom, Ctrl+T toolbar, Ctrl+Z Undo, Ctrl+P Print,
  Shift+Ctrl+A About, Shift+Ctrl+H Help). Don't reuse standard letters.
- Dialogs fit 640x240; validation messages like "No ... entered"; queries
  state the fact first, then ask a question ending in "?", names in double
  quotes.
- Busy messages go bottom-left ("Checking mail..."), outcomes are
  infoprints: no "error", no "please", no full stops; "Not available ..." for
  things that can't be done.
- Toolbar on the right: the title, 4 buttons (24x20 pictures, dense font)
  and the clock; it can be hidden. 16-grey conventions, black on white, grey
  only for secondary text; text that doesn't fit is clipped with "..."
  (`TextUtils::ClipToFit`).

## Other rules

- Work on the **dev** branch. Push to dev only.
- Don't change the connection settings (the dialog, its fields, formats or
  defaults) unless Dan asks; they are shared with every program
  (`core/ssh/psilink.h`).
- **Never commit:** keys, passwords, the SDK, the emulator or its ROMs, the
  TESTSEED block, `dist/` (releases live in psiterm), anything private: LAN
  addresses, home paths, SSIDs, `*_local.h`.
- Writing style: British English, plain short sentences, the EIKON glossary
  terms, no em dashes. Commit messages are plain British English.
