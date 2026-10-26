# login.net.py - loaded by core/tools/emu/net.py: "net.py login".
#
# Issue 1: the server refuses the login because the user name is wrong (a
# name where the server wants the address, as Proton Bridge does). A new
# account with user name "dan" against fakeimap.py --user dan@test.lan:
#  - one "Log in to ..." dialog for the check (the folders and the Inbox
#    fail together), with the server's words;
#  - Account... (Ctrl+A) opens the whole account dialog; Esc there: no more
#    dialogs, and "Not logged in - change the account in Tools > Accounts >
#    Settings".
# (The emulator's modem does not answer again after a hang-up, so a second
# connection, the retry, can't be tested here: test/logintest.sh has the
# engine's side.)
# Screenshots in build/emu/net/login/ (one a second): look at them.

def test_login(ctx):
    port = free_port()
    log = os.path.join(WORK, 'login-imap.log')
    ctx.procs.start('imap', [sys.executable, '-u', os.path.join(REPO, 'test', 'fakeimap.py'), str(port),
                             '--user', 'dan@test.lan'], log)
    if not wait_port(port):
        return False, 'fakeimap.py did not start'
    card = os.path.join(WORK, 'login.img')
    make_card('psimail', card, 'EMU_PKG_MAIL')
    ctx.modem('login', redirect='127.0.0.1:%d' % port)
    s = Script(90).key(DOWN).key(DOWN).type('dan@test.lan', gap=0.35)
    s.tap(170, 36).wait(1.5)                                   # the Incoming page
    s.key(DOWN, gap=0.8).key(DOWN, gap=0.8).type('143')        # Port 143
    s.key(DOWN, gap=0.8).key(LEFT, gap=0.8)                   # Security None
    s.key(DOWN, gap=0.8).type('dan', gap=0.35)                # User name: not the address
    s.key(DOWN, gap=0.8).type('secret')
    s.key(ENTER, gap=8)                                       # OK: PsiMail checks at once
    s.key(ESC, gap=float(os.environ.get('EMU_LOGIN_WAIT', 30)))   # "Use the Email icon for PsiMail?": No
    marks = {'asked': s.t}
    s.ctrl('a', gap=8)                                        # Account...: the whole account dialog
    marks['account'] = s.t
    s.key(ESC, gap=30)                                        # cancelled: "Not logged in - ..."
    marks['held'] = s.t
    out = emulate('login', card, ctx.sock, s, secs=45)
    print('login: times', ' '.join('%s %.0f s' % kv for kv in sorted(marks.items(), key=lambda kv: kv[1])))
    try:
        lines = open(log, 'rb').read().decode('latin-1').splitlines()
    except OSError:
        return False, out
    logins = [l for l in lines if ' LOGIN ' in l]
    print('login: the server saw', len(logins), 'logins:', '; '.join(l.split('< ', 1)[-1] for l in logins))
    wrong = [l for l in logins if 'LOGIN "dan" ' in l]
    # (the emulator's modem does not come back after a hang-up - the "+++ATH"
    # comes in one piece - so only the first connection reaches the server:
    # the folders' LOGIN; the Inbox's is "no answer from modem")
    ok = len(wrong) == 1
    return ok, out

TESTS['login'] = test_login
