#!/usr/bin/env python3
"""putlogtest.py [psimail-host] - Send log to server (engine/webpics.c
pm_send_log) against the answers it may get: only PsiProxy's "Log saved:
..." is "Log sent"; the Atom modem's own proxy, which has no PsiProxy behind
it (501 and "X-PsiConnect: no-psiproxy"), or an older Atom's status page
(200, HTML), is "Needs PsiProxy on a computer"; PsiProxy's own failures say
the status. Also checks the request's Host: line ("psiproxy" to the Atom).
Needs the host build: core/tools/docker/psibuild "bash build.sh host"."""
import os, socket, subprocess, sys, tempfile, threading

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "build", "mail-host", "psimail-host")

PSIPROXY = (b"HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=us-ascii\r\nContent-Length: 37\r\n"
            b"Connection: close\r\n\r\nLog saved: PsiMail-1.log (123 bytes)\n")
ATOM_NEW = (b"HTTP/1.1 501 Not Implemented\r\nX-PsiConnect: no-psiproxy\r\nContent-Type: text/plain\r\n"
            b"Content-Length: 73\r\nConnection: close\r\n\r\n"
            b"Needs PsiProxy on a computer: set it in the modem's Web proxy settings (AT$PU)")
PAGE = b"<html><head><title>PsiConnect web proxy</title></head><body><h2>PsiConnect 2.6: web proxy</h2></body></html>"
ATOM_OLD = (b"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %d\r\n"
            b"Cache-Control: no-store\r\nConnection: close\r\n\r\n" % len(PAGE)) + PAGE
TOO_BIG = (b"HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/html\r\nContent-Length: 5\r\n"
           b"Connection: close\r\n\r\n<p>x\n")

CASES = [  # (answer, proxy host, expected result line, expected Host: line)
    (PSIPROXY, "127.0.0.1", "putlog: OK Log sent", "127.0.0.1"),
    (ATOM_NEW, "127.0.0.1", "putlog: FAILED Needs PsiProxy on a computer", "127.0.0.1"),
    (ATOM_OLD, "127.0.0.1", "putlog: FAILED Needs PsiProxy on a computer", "127.0.0.1"),
    (TOO_BIG, "127.0.0.1", "putlog: FAILED the log is too big", "127.0.0.1"),
]


def serve(sock, answer, got):
    c, _ = sock.accept()
    data = b""
    while b"\r\n\r\n" not in data:
        data += c.recv(4096)
    head, _, body = data.partition(b"\r\n\r\n")
    n = int([l.split(b":")[1] for l in head.split(b"\r\n") if l.lower().startswith(b"content-length:")][0])
    while len(body) < n:
        body += c.recv(65536)
    got["head"] = head.decode("latin-1")
    c.sendall(answer)
    c.close()


def main():
    fails = 0
    work = tempfile.mkdtemp()
    with open(os.path.join(work, "psimail.log"), "w") as f:
        f.write("00:01.0 a line of log\n" * 50)
    for answer, host, want, want_host in CASES:
        s = socket.socket()
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(("127.0.0.1", 0))
        s.listen(1)
        got = {}
        t = threading.Thread(target=serve, args=(s, answer, got), daemon=True)
        t.start()
        env = dict(os.environ, PM_PROXY_HOST=host, PM_PROXY_PORT=str(s.getsockname()[1]), PM_TLS="0")
        r = subprocess.run([HOST, "-s", work, "putlog"], cwd=work, env=env, capture_output=True, text=True, timeout=60)
        line = [l for l in r.stdout.splitlines() if l.startswith("putlog:")]
        line = line[0].strip() if line else "(none) " + r.stdout + r.stderr
        hostline = [l for l in got.get("head", "").split("\r\n") if l.lower().startswith("host:")]
        ok = line == want and hostline and hostline[0].split(":")[1].strip() == want_host
        print("%s %-55s %s" % ("ok  " if ok else "FAIL", line, hostline[0] if hostline else "(no Host:)"))
        fails += not ok
        s.close()
    print("%d cases, %d failed" % (len(CASES), fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
