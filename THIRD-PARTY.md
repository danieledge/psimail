# Third-party components

PsiMail's own code is MIT (`LICENSE`). What it builds on:

| Component | Where | Licence |
|---|---|---|
| The shared layer of the suite: PsiTerm's networking (`psiglue.cpp`), the TLS 1.3 client (`tls13.c`), the picture decoders, and with them [zlib](https://zlib.net/), LibTomCrypt, LibTomMath, Dropbear's X25519 ([TweetNaCl](https://tweetnacl.cr.yp.to/)) and [picojpeg](https://github.com/richgel999/picojpeg) | the `core/` submodule ([psion-core](https://github.com/danieledge/psion-core)) | see core's [THIRD-PARTY.md](https://github.com/danieledge/psion-core/blob/main/THIRD-PARTY.md): MIT, zlib, public domain |
| EPOC C Standard Library (`stdlib.sis`, ESTLIB.DLL) from the Psion EPOC R5 C++ SDK | embedded in `dist/PsiMail.sis` (not in this repository) | Psion's redistributable runtime, shipped with programs that use it as the SDK directs |

The PNG and GIF decoders, the progressive JPEG decoder and the 16-grey
dithering (`core/mail/engine/img/`) are PsiMail's own code (MIT), kept in
core because the Atom modem firmware uses them too; `pmjprog.c`'s integer
IDCT is the one in [stb_image](https://github.com/nothings/stb) by Sean
Barrett (public domain / MIT, either at your choice), adapted.

The invitation and contact card readers (`engine/invite.c`, iCalendar
RFC 5545 / iTIP RFC 5546 / vCard 2.1, 3.0 and 4.0) are PsiMail's own code
(MIT). The root certificates in `engine/roots.h` (`tools/roots.pem`) are the
public roots of their certificate authorities.
