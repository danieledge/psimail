#!/bin/bash
# calua.sh [psimail-host] - (1.05) the calendar's requests name PsiMail's
# version and the machine in User-Agent (psion-core's http_user_agent), as
# mail and web pictures do: a CalDAV request to a local server that only
# notes the header (and refuses the login).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
H=${1:-$HERE/../build/mail-host/psimail-host}
T=$(mktemp -d /tmp/pmua.XXXXXX)
cat > "$T/srv.py" <<'PY'
import http.server, sys
class H(http.server.BaseHTTPRequestHandler):
    def any(self):
        sys.stderr.write('UA=' + self.headers.get('User-Agent', '') + '\n'); sys.stderr.flush()
        self.send_response(401); self.send_header('Content-Length', '0'); self.end_headers()
    do_PROPFIND = do_REPORT = do_GET = do_OPTIONS = any
    def log_message(self, *a): pass
http.server.HTTPServer(('127.0.0.1', int(sys.argv[1])), H).serve_forever()
PY
python3 "$T/srv.py" 1191 2> "$T/ua.log" &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
sleep 0.5
PM_VERSION=1.05 PM_CAL_HOST=127.0.0.1 PM_CAL_PORT=1191 PM_CAL_PLAIN=1 PM_CAL_PATH=/dav/ PM_USER=x PM_PASS=x \
	timeout 30 "$H" -s "$T/store" cal > /dev/null 2>&1
got=$(head -1 "$T/ua.log")
if [ "$got" = "UA=PsiMail/1.05 (Psion)" ]; then echo "ok: $got"; echo "calua: all passed"
else echo "FAIL: wanted 'UA=PsiMail/1.05 (Psion)', got '$got'"; echo "calua: FAILED"; exit 1; fi
