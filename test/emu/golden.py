#!/usr/bin/env python3
"""golden.py - PsiMail's screen tours in the emulator, and a pixel compare.

  python3 test/emu/golden.py run mx|nb OUTDIR [TOUR...]
  python3 test/emu/golden.py compare BASEDIR NEWDIR [DIFFDIR]

`run` drives the tours below (each its own emulator run, three at a time)
and saves the named screenshots as OUTDIR/<mx|nb>-<name>.png. `compare`
compares every <name>.png in BASEDIR with the one in NEWDIR, pixel for
pixel, with the toolbar's clock masked; it lists the files that differ (and
writes a picture of each difference to DIFFDIR) and exits 1 if any do.

The 5mx's 63 shots are the golden set (test/emu/golden/5mx/): after every
change, run the tours on the 5mx and compare. A difference is either
listed in the commit or a bug. The netBook's shots are looked through, not
compared: that screen is the one meant to change.

Cards (built from a TESTSEED build in a copy of the checkout, never in the
checkout itself; see test/README.md):
  CARD   the seeded card: 5mx, a card image with SEED, SEEDA0, SEEDX and
         SEEDCAL; netBook, a Coda OS.IMG with PsiMail and the seed in ROM
  CARD0  the same with an unseeded build, for the first-run tour (N)
The emulator's clock starts at 17 Nov 2026 09:40 (PSION_RTC_SEED) so that
the calendar's "today" and the clock stay the same from run to run.

STEP: +N waits N s; esc enter menu up down left right tab del space;
^X is Ctrl+X, ^^X Shift+Ctrl+X; text:hello; tap:X,Y (LCD coordinates);
shot:NAME saves a screenshot.
"""
import sys, os, subprocess, glob
from concurrent.futures import ThreadPoolExecutor
from PIL import Image, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.environ.get('PSION_APP') or os.path.join(HERE, '..', '..'))
CORE = os.path.abspath(os.environ.get('PSION_CORE') or os.path.join(REPO, 'core'))
EMU = os.path.abspath(os.environ.get('PSION_EMU') or os.path.join(REPO, '..', 'psionEmulators'))
RTC = os.environ.get('PSION_RTC_SEED', '328edd70')      # 2026-11-17 09:40:00
K = {'esc': 4, 'enter': 3, 'menu': 148, 'up': 16, 'down': 17, 'left': 14, 'right': 15,
     'tab': 2, 'del': 1, 'space': 5}
# (the 5mx's clock and the netBook's, masked in the compare)
CLOCK = {(640, 240): (571, 170, 640, 240), (640, 480): (571, 405, 640, 480)}


def tours(kind):
    """The tours, as (name, card, steps). R1 is the y of the list's first row."""
    nb = kind == 'nb'
    R1 = 64 if nb else 67
    O = 120 if nb else 0
    y = lambda v: v + O
    ST = ['esc', '+2', 'tap:350,88', 'up'] if nb else ['esc', '+2']
    S = 'CARD'
    t = [
        ('A', S, 'esc +2 shot:inbox tap:350,%d enter +7 shot:reader-pic down down down down down down shot:reader-pic2 '
                 'esc down enter +8 shot:reader-gif down down down down down down shot:reader-gif2 esc down enter +8 '
                 'shot:reader-news down down down down down down down down shot:reader-news2 esc down enter +4 '
                 'shot:reader-quotes esc down down down down down down shot:list2 enter +4 shot:reader-flagged esc' % R1),
        ('B', S, 'esc +2 tap:350,%d menu shot:menu-file right shot:menu-edit right shot:menu-message right shot:menu-view '
                 'down down down down right shot:menu-show esc esc esc ^k +1 shot:prefs1 tab shot:prefs2 tab shot:prefs3 '
                 'esc ^^q +1 shot:status esc ^^b shot:sort esc ^f shot:find esc ^x shot:move esc ^^h +1 shot:help '
                 'down down down shot:help2 esc ^^a +1 shot:about esc' % R1),
        ('C1', S, 'esc +2 tap:350,%d menu right right right right shot:menu-tools down enter +2 shot:conn-link tab +1 '
                  'shot:conn-network esc' % R1),
        ('C2', S, 'esc +2 tap:350,%d menu right right right right down down enter +2 shot:cal-settings tab '
                  'shot:cal-settings2 esc' % R1),
        ('C3', S, 'esc +2 tap:350,%d menu right right right right down down down right shot:menu-accounts enter +2 '
                  'shot:acct1 tab shot:acct2 tab shot:acct3 tab shot:acct4 esc' % R1),
        ('D', S, 'esc +2 tap:350,%d ^^n +4 shot:cal-week down down shot:cal-week2 enter +2 shot:cal-event esc ^q +3 '
                 'shot:cal-month right right down shot:cal-month2 ^m +1 shot:cal-month-zoom ^m ^m menu shot:cal-menu '
                 'right right shot:cal-menu-event right shot:cal-menu-view esc esc ^n +3 shot:cal-new esc' % R1),
        ('E', S, 'esc +2 tap:350,%d ^m +1 shot:zoom-b ^m +1 shot:zoom-c ^m +1 shot:zoom-a ^t +1 shot:no-toolbar ^t '
                 '^^l +1 shot:no-folders ^^l ^^t +1 shot:no-title ^^t enter +5 ^m +1 shot:reader-zoom-b ^m +1 '
                 'shot:reader-zoom-c ^m esc' % R1),
        ('F', S, 'esc +2 tap:350,%d ^n +3 shot:compose text:alice down down down text:lunch down text:see space text:you '
                 '+1 shot:compose2 esc +1 shot:compose-close' % R1),
        ('G', S, 'esc +2 tap:350,%d down down down enter +4 ^r +3 shot:reply esc +1 esc menu right right down down down '
                 'down down right shot:menu-attach esc down right shot:menu-web esc esc esc' % R1),
        ('P', S, ST + ('^k +1 tap:%d,%d +1 shot:prefs-newmail tap:%d,%d +1 shot:prefs-screen esc +1 ^^q +2 shot:status esc'
                       % ((172, y(33), 246, y(33)) if nb else (172, 46, 246, 46))).split()),
        ('C', S, ST + ('menu right right right right down enter +2 tap:97,%d +1 shot:conn-network ^t +8 shot:conn-test'
                       % y(38)).split()),
        ('S', S, ST + ('menu right right right right down down enter +2 tap:%d,%d +1 shot:cal-agenda tap:%d,%d +1 '
                       'shot:cal-dates esc' % ((145, y(39), 210, y(39)) if nb else (145, 48, 210, 48))).split()),
        ('T', S, ST + ('menu right right right right down down down right enter +2 tap:130,%d +1 shot:acct-incoming '
                       'tap:213,%d +1 shot:acct-outgoing tap:285,%d +1 shot:acct-limits esc' % (y(34), y(34), y(34))).split()),
        ('Z', S, ST + ('^m +1 shot:list-zoom-b ^m +1 shot:list-zoom-c ^m +1 ^t +1 shot:list-no-toolbar ^t ^^l +1 '
                       'shot:list-no-folders ^^l ^^t +1 shot:list-no-title ^^t menu right right right down down down down '
                       'right down down down enter +2 shot:no-preview').split()),
        # (1.03: a new user is asked how the Psion gets online first; Esc goes on to the account)
        ('N', 'CARD0', ('+2 shot:first-setup tab +1 shot:first-setup-helper esc +3 shot:first-run +1 tap:130,%d +1 '
                        'shot:first-run-incoming esc +2 shot:first-run-cancelled esc +2 '
                        'shot:empty menu right right right right down enter +2 shot:first-conn' % y(34)).split()),
        # (1.02) next unread, the rules, the signatures, Check for new mail: At once
        ('J', S, ST + '^^j +2 shot:next-unread ^^j +2 shot:next-unread2 enter +4 ^^j +4 shot:next-unread-reader esc'.split()),
        ('R1', S, ST + 'menu right right right right down down down right up up +1 shot:menu-accounts2 enter +2 shot:rules '
                       'right right right down +1 shot:rules-move esc'.split()),
        ('R2', S, ST + 'menu right right right right down down down right up enter +2 shot:signatures esc'.split()),
    ]
    if nb:
        t += [
            ('A2', S, ST + ('shot:inbox enter +7 shot:reader-pic esc +2 tap:350,112 up +1 shot:preview-gif enter +9 shot:reader-gif '
                            'down down down down down down down down +1 shot:reader-gif2 esc +2 tap:350,136 up +2 shot:preview-news '
                            'enter +9 shot:reader-news down down down down down down down down down down +1 shot:reader-news2 '
                            'esc tap:350,160 up +1 shot:preview-quotes enter +4 shot:reader-quotes esc tap:350,208 down down +2 '
                            'shot:list2 enter +4 shot:reader-flagged').split()),
            ('D2', S, ST + ('^^n +5 shot:cal-week down down +1 shot:cal-week2 enter +2 shot:cal-event esc ^q +12 shot:cal-month '
                            'right +10 shot:cal-month2 ^n +4 shot:cal-new esc +1 menu right right +1 shot:cal-menu-event').split()),
            # (1.01) the week's time grid, and Ctrl+Q round the views to the day list
            ('W', S, ST + ('^^n +5 shot:cal-grid right +2 shot:cal-grid2 left down +2 shot:cal-grid3 ^q +10 ^q +4 '
                           'shot:cal-daylist').split()),
            # (1.01) the preview's actions: Reply from the strip
            ('R', S, ST + 'tap:200,466 +4 shot:strip-reply esc +2 esc +1'.split()),
        ]
    out = []
    for name, card, steps in t:
        if isinstance(steps, str):
            steps = steps.split()
        out.append((name, card, steps))
    return out


def run_tour(kind, name, card, steps, outdir, work):
    T0 = float(os.environ.get('T0', 88 if kind == 'mx' else 125))
    t = T0; ev = []; shots = []
    def key(tt, k, hold=None):
        if kind == 'nb': ev.extend(['--press-key', '%.2f' % tt, str(k), str(hold or (6 if 32 <= k < 127 else 2))])
        else: ev.append('%.2f %d %d' % (tt, k, hold or 8))
    for s in steps:
        if s.startswith('+'): t += float(s[1:]); continue
        if s.startswith('shot:'): shots.append((t, s[5:])); t += 1; continue
        if s.startswith('^^'): key(t, 18, 30); key(t + 0.03, 22, 30); key(t + 0.15, ord(s[2].upper()), 3)
        elif s.startswith('^'): key(t, 22, 30); key(t + 0.1, ord(s[1].upper()), 3)
        elif s.startswith('text:'):
            for c in s[5:]: key(t, ord(c.upper()), 4); t += 0.4
        elif s.startswith('tap:'):
            x, yy = s[4:].split(',')
            if kind == 'nb': ev.extend(['--tap-seq', '%.2f' % t, x, yy])
            else: ev.append('%.2f tap %d %s' % (t, int(x) + 45, yy))
        else: key(t, K[s])
        t += 1.5
    frames_dir = os.path.join(work, kind, name)
    subprocess.run(['rm', '-rf', frames_dir]); os.makedirs(frames_dir)
    env = dict(os.environ, PSION_RTC_SEED=RTC)
    if kind == 'mx':
        if not ev: ev = ['%.2f 4 8' % T0]
        # MX_LAUNCH_AT=S (45): open PsiMail from Extras at S s rather than run.sh's
        # 37 s (a bigger card can keep the System screen busy past that)
        at = os.environ.get('MX_LAUNCH_AT', '45')
        if at:
            env['EMU_LAUNCH'] = '0'
            ev = ['%.2f tap 670 262' % float(at), '%.2f tap 335 215' % (float(at) + 2)] + ev
        last = max(float(e.split()[0]) for e in ev)
        env.update(EMU_CARD=card, EMU_OUT=frames_dir, PSION_APP=REPO, PSION_EMU=EMU, EMU_SECS=str(int(t - last) + 4))
        subprocess.run(['bash', os.path.join(CORE, 'tools', 'emu', 'run.sh'), name] + ev, env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        st = ['--press-key', '100', '22', '40', '--press-key', '100.2', '148']
        tt = 102.0
        for c in 'PSIMAIL': st.extend(['--press-key', '%.2f' % tt, str(ord(c)), '6']); tt += 0.4
        st.extend(['--press-key', '105.4', '3', '2'])
        env.update(PSION_NB_HOOK_MASK='0xdf', HARNESS_RGB='1')
        cmd = ['timeout', '3000', './harness/run', 'roms/netBook/BootLoader/netBook_BL_v011_eng/netBook_BL_v011_eng.bin',
               '--device', 'netbook', '--boot-seconds', '3', '--card-path', card, '--post-attach-seconds', '%.1f' % (t + 3),
               '--quiet-logs', '--screenshot-every', '1', frames_dir + '/s'] + st + ev
        with open(frames_dir + '/log', 'w') as log:
            subprocess.run(cmd, cwd=EMU, env=env, stdout=log, stderr=subprocess.STDOUT)
        for f in sorted(glob.glob(frames_dir + '/s-*.p?m')):
            Image.open(f).convert('RGB').save(f[:-4] + '.png'); os.remove(f)
    frames = sorted(glob.glob(frames_dir + '/s-*.png'))
    done = []
    for ts, n in shots:
        if not frames: break
        k = min(len(frames) - 1, max(0, int(ts)))
        Image.open(frames[k]).save(os.path.join(outdir, '%s-%s.png' % (kind, n)))
        done.append(n)
    return name, len(frames), done, len(shots)


def cmd_run(kind, outdir, only):
    outdir = os.path.abspath(outdir)
    os.makedirs(outdir, exist_ok=True)
    work = os.path.join(REPO, 'build', 'emu', 'golden')
    cards = {'CARD': os.environ.get('CARD'), 'CARD0': os.environ.get('CARD0')}
    jobs = [(n, cards[c], s) for n, c, s in tours(kind) if not only or n in only]
    for n, c, s in jobs:
        if not c or not os.path.exists(c):
            sys.exit('golden.py: no card for tour %s (set CARD and CARD0)' % n)
    with ThreadPoolExecutor(int(os.environ.get('JOBS', 3))) as ex:
        for name, nframes, done, want in ex.map(lambda j: run_tour(kind, j[0], j[1], j[2], outdir, work), jobs):
            print('%-3s %3d frames, %d of %d shots' % (name, nframes, len(done), want), flush=True)


def masked(im):
    im = im.convert('RGB')
    box = CLOCK.get(im.size)
    if box:
        im = im.copy(); im.paste((255, 0, 255), box)
    return im


def cmd_compare(base, new, diffdir=None):
    same = differ = missing = 0
    for f in sorted(glob.glob(os.path.join(base, '*.png'))):
        n = os.path.basename(f)
        g = os.path.join(new, n)
        if not os.path.exists(g):
            print('missing  %s' % n); missing += 1; continue
        a, b = masked(Image.open(f)), masked(Image.open(g))
        if a.size != b.size:
            print('size     %s %s -> %s' % (n, a.size, b.size)); differ += 1; continue
        bbox = ImageChops.difference(a, b).getbbox()
        if not bbox:
            same += 1; continue
        differ += 1
        print('differs  %s in %s' % (n, bbox))
        if diffdir:
            os.makedirs(diffdir, exist_ok=True)
            w, h = a.size
            o = Image.new('RGB', (w, h * 3 + 8), (255, 0, 0))
            o.paste(a, (0, 0)); o.paste(b, (0, h + 4))
            o.paste(ImageChops.invert(ImageChops.difference(a, b)), (0, 2 * h + 8))
            o.save(os.path.join(diffdir, n))
    print('%d the same, %d differ, %d missing' % (same, differ, missing))
    return 1 if differ or missing else 0


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) >= 3 and a[0] == 'run' and a[1] in ('mx', 'nb'):
        cmd_run(a[1], a[2], a[3:])
    elif len(a) >= 3 and a[0] == 'compare':
        sys.exit(cmd_compare(a[1], a[2], a[3] if len(a) > 3 else None))
    else:
        sys.exit(__doc__)
