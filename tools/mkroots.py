#!/usr/bin/env python3
"""mkroots.py - writes psion-core's mail/engine/tlsv/roots.h, the root
certificates PsiMail and PsiWeb trust (RSA, and ECDSA on P-256 and P-384),
from PEM files.

    tools/mkroots.py tools/roots.pem > core/mail/engine/tlsv/roots.h

roots.pem: the first ten came from macOS's system roots (security
find-certificate -p): ISRG Root X1 (Let's Encrypt: Fastmail), GTS Root R1
(Google), DigiCert, USERTrust (Sectigo), GlobalSign, Amazon, Microsoft,
Go Daddy, Starfield. Added on 17 November 2026 from Mozilla's list
(Debian's ca-certificates, /etc/ssl/certs) for the web: RSA GlobalSign R3,
R6 and R46, Sectigo R46, DigiCert TLS RSA4096 G5; ECDSA ISRG Root X2, GTS
Root R3 and R4, GlobalSign ECC R4 and R5 and E46, DigiCert Global G3 and
TLS ECC P384 G5, USERTrust ECC, Sectigo E46, Amazon Root CA 3 and 4,
Microsoft ECC 2017. Keys of other kinds are left out.
"""
import sys
from cryptography import x509
from cryptography.hazmat.primitives.asymmetric import rsa, ec
from cryptography.hazmat.primitives import serialization

def carr(name, b):
    out = ["static const unsigned char %s[%d] = {" % (name, len(b))]
    for i in range(0, len(b), 16):
        out.append("\t" + "".join("0x%02x," % x for x in b[i:i + 16]))
    out.append("};")
    return "\n".join(out)

certs = []
for path in sys.argv[1:]:
    certs += x509.load_pem_x509_certificates(open(path, "rb").read())

print("/* roots.h - root certificates PsiMail and PsiWeb trust: RSA, and ECDSA on")
print(" * P-256 and P-384. Made by psimail's tools/mkroots.py from its tools/roots.pem */")
print("typedef struct { const char *name; const unsigned char *subject; int subjectlen;")
print("                 const unsigned char *n; int nlen; const unsigned char *e; int elen;")
print("                 int curve; const unsigned char *pt; } Root;      /* RSA: n, e; ECDSA: curve (256, 384), pt = X || Y */")
entries = []
for i, c in enumerate(certs):
    k = c.public_key()
    subj = c.subject.public_bytes()
    name = c.subject.rfc4514_string()
    if isinstance(k, ec.EllipticCurvePublicKey) and k.curve.name in ("secp256r1", "secp384r1"):
        bits = k.curve.key_size
        nb = bits // 8
        pn = k.public_numbers()
        pt = pn.x.to_bytes(nb, "big") + pn.y.to_bytes(nb, "big")
        print("/* %s, ECDSA P-%d, until %s */" % (name, bits, c.not_valid_after_utc.date()))
        print(carr("k_subj%d" % i, subj))
        print(carr("k_pt%d" % i, pt))
        entries.append('\t{ "%s", k_subj%d, %d, 0, 0, 0, 0, %d, k_pt%d },' % (name.replace('"', '').replace('\\', ''), i, len(subj), bits, i))
        continue
    if not isinstance(k, rsa.RSAPublicKey):
        continue
    nums = k.public_numbers()
    n = nums.n.to_bytes((nums.n.bit_length() + 7) // 8, "big")
    e = nums.e.to_bytes((nums.e.bit_length() + 7) // 8, "big")
    print("/* %s, %d bits, until %s */" % (name, nums.n.bit_length(), c.not_valid_after_utc.date()))
    print(carr("k_subj%d" % i, subj))
    print(carr("k_n%d" % i, n))
    print(carr("k_e%d" % i, e))
    entries.append('\t{ "%s", k_subj%d, %d, k_n%d, %d, k_e%d, %d, 0, 0 },' % (name.replace('"', '').replace('\\', ''), i, len(subj), i, len(n), i, len(e)))
print("static const Root k_roots[] = {")
print("\n".join(entries))
print("};")
