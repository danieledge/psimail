#!/usr/bin/env python3
"""caldrop.py [psimail-host] - (1.08) the calendar over a connection that drops
part-way through an answer (psion-core a144972 and pmnet.c's pmn_clean_end):
a fake CalDAV server on this PC (nothing real) resets, closes short of its
Content-Length, or ends the list early at a clean close; the sync fails,
events.txt and the ctag stay as they were, and the next good sync fetches
the change. An answer with no length and a clean close is used."""
import os, sys, socket, subprocess, threading, shutil, datetime, time, struct, signal

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'build', 'mail-host', 'psimail-host'))
WORK = os.path.join(HERE, '..', 'build', 'caldrop')
shutil.rmtree(WORK, ignore_errors=True)
os.makedirs(WORK)
PASSFILE = os.path.join(WORK, 'pass')
with open(PASSFILE, 'w') as f: f.write('not-a-real-password\n')

day = (datetime.date.today() + datetime.timedelta(days=2)).strftime('%Y%m%d')
def event(uid, summary, hour):
    return ('BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//fake//EN\r\nBEGIN:VEVENT\r\nUID:%s\r\n'
            'DTSTART:%sT%02d0000Z\r\nDTEND:%sT%02d3000Z\r\nSUMMARY:%s\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n'
            % (uid, day, hour, day, hour, summary))
def card(uid, name):
    return 'BEGIN:VCARD\r\nVERSION:3.0\r\nUID:%s\r\nFN:%s\r\nN:%s;;;;\r\nEND:VCARD\r\n' % (uid, name, name)

state = {
    'ctag': 'c1', 'events': [('1', 'e1', event('d1', 'Dentist', 9)), ('2', 'e2', event('d2', 'Lunch', 12)),
                             ('3', 'e3', event('d3', 'Choir', 18))],
    'btag': 'b1', 'cards': [('1', 'k1', card('u1', 'Ann Example')), ('2', 'k2', card('u2', 'Bob Example')),
                            ('3', 'k3', card('u3', 'Cat Example'))],
    'cut': {},           # (method, path) -> mode, for the next such request only
}
seen = []

def ms(body):
    return ('<?xml version="1.0" encoding="UTF-8"?><multistatus xmlns="DAV:" '
            'xmlns:C="urn:ietf:params:xml:ns:caldav" xmlns:A="urn:ietf:params:xml:ns:carddav" '
            'xmlns:CS="http://calendarserver.org/ns/">' + body + '</multistatus>')
def ok(href, props):
    return ('<response><href>%s</href><propstat><prop>%s</prop><status>HTTP/1.1 200 OK</status>'
            '</propstat></response>' % (href, props))

def answer(method, path, body):
    if method == 'PROPFIND' and path in ('/home/', '/.well-known/caldav'):
        return 207, ms(ok(path, '<C:calendar-home-set><href>/cals/</href></C:calendar-home-set>'))
    if method == 'PROPFIND' and path == '/.well-known/carddav':
        return 207, ms(ok(path, '<A:addressbook-home-set><href>/books/</href></A:addressbook-home-set>'))
    if method == 'PROPFIND' and path == '/cals/':
        return 207, ms(ok('/cals/', '<resourcetype><collection/></resourcetype>') +
                       ok('/cals/home/', '<resourcetype><collection/><C:calendar/></resourcetype>'
                          '<displayname>Home</displayname><CS:getctag>%s</CS:getctag>'
                          '<C:supported-calendar-component-set><C:comp name="VEVENT"/></C:supported-calendar-component-set>'
                          '<current-user-privilege-set><privilege><all/></privilege></current-user-privilege-set>'
                          % state['ctag']))
    if method == 'REPORT' and path == '/cals/home/':
        return 207, ms(''.join(ok('/cals/home/%s.ics' % n, '<getetag>"%s"</getetag><C:calendar-data>%s</C:calendar-data>'
                                  % (e, d)) for n, e, d in state['events']))
    if method == 'PROPFIND' and path == '/books/':
        return 207, ms(ok('/books/', '<resourcetype><collection/></resourcetype>') +
                       ok('/books/main/', '<resourcetype><collection/><A:addressbook/></resourcetype>'
                          '<displayname>Contacts</displayname><CS:getctag>%s</CS:getctag>'
                          '<current-user-privilege-set><privilege><all/></privilege></current-user-privilege-set>'
                          % state['btag']))
    if method == 'PROPFIND' and path == '/books/main/':
        return 207, ms(ok('/books/main/', '<resourcetype><collection/><A:addressbook/></resourcetype>') +
                       ''.join(ok('/books/main/%s.vcf' % n, '<resourcetype/><getetag>"%s"</getetag>' % e)
                               for n, e, d in state['cards']))
    if method == 'REPORT' and path == '/books/main/':
        want = body.decode(errors='replace')
        return 207, ms(''.join(ok('/books/main/%s.vcf' % n, '<getetag>"%s"</getetag><A:address-data>%s</A:address-data>'
                                  % (e, d)) for n, e, d in state['cards'] if '/books/main/%s.vcf' % n in want))
    return 404, ''

def handle(c):
    f = c.makefile('rb')
    try:
        while True:
            line = f.readline()
            if not line: return
            method, path = line.decode().split(' ')[:2]
            n = 0
            while True:
                h = f.readline()
                if h in (b'\r\n', b'\n', b''): break
                if h.lower().startswith(b'content-length:'): n = int(h.split(b':')[1])
            body = f.read(n) if n else b''
            seen.append((method, path))
            code, xml = answer(method, path, body)
            data = xml.encode()
            mode = state['cut'].pop((method, path), None)
            head = 'HTTP/1.1 %d %s\r\nContent-Type: application/xml; charset=utf-8\r\n' % (code, 'Multi-Status' if code == 207 else 'Not Found')
            if mode == 'reset':
                c.sendall((head + '\r\n').encode() + data[:len(data) // 2])
                time.sleep(0.3)
                c.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
                c.close()
                return
            if mode == 'short':
                c.sendall((head + 'Content-Length: %d\r\n\r\n' % len(data)).encode() + data[:len(data) // 2])
                time.sleep(0.3)
                c.close()
                return
            if mode == 'cutxml':
                # whole elements, but not the end of the list: a clean close
                k = data.rfind(b'<response>')
                c.sendall((head + 'Connection: close\r\n\r\n').encode() + data[:k])
                c.shutdown(socket.SHUT_WR)
                time.sleep(0.3)
                c.close()
                return
            if mode == 'hang':
                time.sleep(30)
                c.close()
                return
            if mode == 'whole':
                c.sendall((head + 'Connection: close\r\n\r\n').encode() + data)
                c.shutdown(socket.SHUT_WR)
                time.sleep(0.3)
                c.close()
                return
            c.sendall((head + 'Content-Length: %d\r\n\r\n' % len(data)).encode() + data)
    except (OSError, ValueError):
        pass
    finally:
        try: c.close()
        except OSError: pass

srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(('127.0.0.1', 0))
srv.listen(8)
PORT = srv.getsockname()[1]
def serve():
    while True:
        try: c, _ = srv.accept()
        except OSError: return
        threading.Thread(target=handle, args=(c,), daemon=True).start()
threading.Thread(target=serve, daemon=True).start()

fails = 0
def check(what, good):
    global fails
    print(('ok   ' if good else 'FAIL ') + what)
    if not good: fails += 1

S = os.path.join(WORK, 'store')
ENV = dict(os.environ, PM_CAL_HOST='127.0.0.1', PM_CAL_PORT=str(PORT), PM_CAL_PLAIN='1', PM_CAL_PATH='/home/',
           PM_USER='test@example.com', PM_PASS='not-a-real-password')
def sync():
    r = subprocess.run([EXE, '-s', S, 'cal'], env=ENV, capture_output=True, text=True, timeout=120)
    print('  exit %d' % r.returncode)
    return r.returncode
def read(path):
    for base in (S + '/cal/', S + '/'):
        try: return open(base + path, encoding='latin-1').read()
        except OSError: pass
    return ''
def ctag():
    lines = read('calendars.txt').splitlines()[1:]
    return lines[0].split('\t')[2] if lines else None

r = sync()
before = read('events.txt')
check('the first sync: three events, ctag c1', r == 0 and all(s in before for s in ('Dentist', 'Lunch', 'Choir')) and ctag() == 'c1')
state['ctag'] = 'c2'
state['events'].append(('4', 'e4', event('d4', 'Walk', 15)))
for mode in ('reset', 'short', 'cutxml'):
    state['cut'][('REPORT', '/cals/home/')] = mode
    r = sync()
    check('events answer cut (%s): fails, events.txt and the ctag as they were' % mode,
          r != 0 and read('events.txt') == before and ctag() == 'c1')
state['cut'][('PROPFIND', '/cals/')] = 'reset'
r = sync()
check('the list of calendars cut: fails, nothing lost', r != 0 and read('events.txt') == before)
state['cut'][('REPORT', '/cals/home/')] = 'whole'
r = sync()
check('no length, whole, a clean close: used; the next sync fetched the change', r == 0 and 'Walk' in read('events.txt') and ctag() == 'c2')
srv.close()
print('caldrop: %s' % ('all passed' if not fails else '%d failed' % fails))
sys.exit(1 if fails else 0)
