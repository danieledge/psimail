# PsiMail

[![Buy me a coffee](https://img.shields.io/badge/Buy%20me%20a%20coffee-support-FFDD00?logo=buymeacoffee&logoColor=000)](https://buymeacoffee.com/danedge)

Email and a calendar for the **Psion Series 5mx**, **netBook** and **Series 7**,
made to replace the
built-in Email program. PsiMail reads and sends mail through any IMAP/SMTP
server with TLS - it is made for Fastmail - with the server's certificate
checked, and keeps the Psion's own Agenda in step with Fastmail's calendars
(any CalDAV server). It uses PsiTerm's networking - a Wi-Fi modem such as the
WiRSa (`ATDT host:port`) or the Psion's own dial-up TCP/IP - and PsiTerm's
TLS 1.3 client.

**Status: 1.03, alpha.** The engine has been run against Dovecot, Radicale
and Fastmail both as PC code and as the exact ARM code in an emulator, and the
EIKON app is driven screen by screen in a 5mx emulator. Expect bugs, and
please report them via [GitHub issues](https://github.com/danieledge/psimail/issues).

[![Download PsiMail.sis](https://img.shields.io/badge/Download-PsiMail.sis-2ea44f?logo=github)](https://github.com/danieledge/psiterm/raw/main/dist/PsiMail.sis)

<p align="center"><img src="docs/screenshots/netbook-psimail-inbox.png" alt="PsiMail's Inbox on the netBook: the folder tree, the message list with unread messages in bold, and the selected message under it" width="640"></p>

## Contents

- [What's new in 1.03](#whats-new-in-103) (and [1.02](#102), [1.01](#101), [1.00](#100), [0.96](#096))
- [Screenshots](#screenshots)
- [What it does](#what-it-does)
- [Setting up Fastmail](#setting-up-fastmail)
- [Settings](#settings)
- [Connections](#connections)
- [The calendar](#the-calendar)
- [Privacy](#privacy)
- [Keys](#keys) and [writing a message](#writing-a-message)
- [Updating from the Psion](#updating-from-the-psion)
- [Troubleshooting](#troubleshooting)
- [How it works](#how-it-works), [Security](#security), [Building](#building), [Releases](#releases), [Files on the card](#files-on-the-card)
- [The suite](#the-suite)
- [Not yet](#not-yet)

## What's new in 1.03

Knowing your route, on every machine:

- **Set up PsiMail**: the first time PsiMail starts on a Psion with no
  settings of any of the suite's programs, it asks how the Psion gets
  online (a network card or dial-up, the Atom modem, another WiFi modem)
  and where web pictures come from (direct, the Atom modem helper,
  PsiProxy on a computer), and fills in Connection settings - nothing
  else, and Cancel changes nothing. Later: Connection settings > Set up
  (Ctrl+U). It is the same assistant as PsiWeb's (psion-core).
- **The route box** at the right of the title bar says the route and who
  does the security: "Modem - Atom does TLS", "TCP/IP - TLS here",
  "Modem - ready", "Connecting...", "Offline". On a colour screen it has a
  light: green online, amber connecting, grey not connected, red when the
  account has no security. A tap opens Status information.
- **Status information** has eight short lines: the account, the route,
  who does the security, where web pictures come from, this folder, the
  Outbox, the last thing done, and the disk the mail is on with its room.
  About says the route, and a baud rate only for the modem.
- **The words** are the suite's own now: "Network card or dial-up (Psion
  TCP/IP)" for Psion Internet, "Dial-up: first send", and Web helper
  (None - direct, or the Atom modem or PsiProxy) for "Use proxy".
- **Help**: Ways to connect, and PsiProxy and other helpers, with where to
  get PsiProxy and the Atom modem's software.
- Mail says which machine wrote it (User-Agent), and on a colour screen web
  picture requests say the screen (X-Psi-Screen), so PsiProxy and the Atom
  make the pictures for it.
- TLS 1.3 can use AES-128-GCM as well as ChaCha20-Poly1305, for servers
  that prefer it (psion-core 4aec0ab).

### 1.02

On every machine:

- **Find on this Psion.** Edit > Find while working offline looks through
  the mail on the Psion: the sender, the subject and the text of every
  message downloaded, every word you type. It needs no connection and
  shows the newest 50 it finds, as the server's search does.
- **Next unread** (View > Go to > Next unread, Shift+Ctrl+J): the next
  unread message in the folder, then the next folder with unread mail. In a
  message, the next unread one is opened.
- **Rules** (Tools > Accounts > Rules...): up to eight. When the sender or
  the subject contains some words, new mail in the Inbox is moved to a
  folder, flagged, marked as read, or comes with no new mail alert. They
  act as each check brings the mail in, and only on new mail.
- **Signatures** (Tools > Accounts > Signatures...): a second signature for
  replies, and whether it goes above the quote (as before) or below it.
- **A tap on the envelope** of the message already chosen marks it read or
  unread.
- **New mail at once**: Preferences > New mail > Check for new mail: At
  once. On a connection that stays up (Psion TCP/IP: a network card, or a
  dial-up kept connected) PsiMail waits on the Inbox (IMAP IDLE) and brings
  new mail as it arrives; anything you do comes first. On the modem route,
  or with a server that cannot do it, it checks every 10 minutes.

### 1.01

The netBook and Series 7's 480 lines put to use. The Series 5mx looks and
works as in 0.99 (its screens compared pixel for pixel again).

- The preview shows the message's pictures that are on the Psion already
  (inline ones in their places, attached ones after the text). It still
  fetches nothing: open the message for the rest.
- Under the preview, Reply, Reply to all, Forward, Archive and Flag act on
  the message shown. They are the same commands as in the menus, with the
  same shortcuts.
- "Today" under the folders: what is left of today in the calendar, three
  events at most, each with its calendar's colour. A tap goes to the
  calendar.
- The calendar's week is a time grid: the hours down the side, a column a
  day, each event a block in its calendar's colour (side by side where they
  overlap), all-day events in a bar at the top, today's column tinted and a
  red line at the time now. One tap chooses an event, a second opens it.
  View > Switch view (Ctrl+Q goes round) has Week, Month and Day list; the
  day list is the week as the 5mx shows it.

### 1.00

The colour pass, for the netBook and Series 7's colour screens. The Series
5mx looks and works exactly as in 0.99: its 78 screens in the emulator are
compared pixel for pixel with 0.99's (`test/emu/golden.py`).

- Every picture PsiMail draws has a colour twin: manila folders, a blue
  Inbox tray, a yellow envelope for an unread message and an open white one
  for a read message, a red flag, a blue paperclip, and the four toolbar
  pictures. They use only EPOC's 256 colours, so nothing is dithered.
- An unread message has a blue bar at the left of its row as well as bold
  text. The "?" over the pictures' column is a small envelope.
- The preview under the list has a heading band in the button face's tint,
  and the message's first attachment in a box: what kind of file it is
  (a picture, a Word file, a Sheet file, a PDF), its name, its size and
  "+2" for any more. The reader's attachment boxes show the kind of file too.
- The calendar's month: the weekend's columns and today's cell are tinted,
  and today's date is red. On a tall screen the calendars are listed under
  the folders with their colours; a tap on one hides or shows its events
  (kept in `cal\hidden.txt` beside the mail, not in the settings).
- A panel in the toolbar's empty space, above the clock: a light (grey
  offline or not connected, amber connecting, green online, red when the
  account has no security), the route (Modem or Psion TCP/IP), who does the
  TLS (here, or the Atom modem), whether web pictures come direct or by a
  helper, and when the mail was last checked ("Last 09:41"). A tap opens
  Status information.
- Server certificates: ECDSA ones on P-256 and P-384 are checked too (Gmail's
  IMAP has one), and five more roots (psion-core 33c3eeb).

### 0.96

On the netBook and Series 7:

- The selected message is shown under the list of messages. Space pages through it, Shift+Space goes back, and Enter or a tap on it opens it. It shows what has been downloaded already and does not mark the message read. View > Show > Preview turns it off.
- What PsiMail draws itself now takes the system's colours: the highlight in the folder list and the calendar, the column headings, the scroll bars. It follows the machine's colour scheme.
- Links are blue. Quoted text has a colour for each level. A flagged message has a red flag.
- The calendar's month view lists each day's events, as many as fit, each in its calendar's colour. The day's list has a bar in the calendar's colour beside each event. Today has a red frame.
- Writing a message: the text takes the whole height of the screen. It was four lines high.
- Help opens in a taller window.
- Download ahead fetches 25 messages unless you set another number. This goes by memory: 24 MB or more. It is still 10 on a 16 MB 5mx.
- Preferences > Screen greys is dimmed. The greys are the 5mx's screen's.

On every machine:

- View > Show toolbar, Show title bar and Show list of folders are now in a View > Show menu, with Preview. The shortcuts are the same.
- About PsiMail names the three machines.
- The Series 5mx looks and works as it did in 0.95.

The review behind this, with what was left for later, is in [docs/netbook-review.md](docs/netbook-review.md).

### 0.95

Since 0.93, the last signed release.

- Runs on the netBook and Series 7 as well as the 5mx, on the whole 640x480 screen, with the program icon in colour. Secure connections work there: the engine no longer uses the BX instruction, which the netBook's StrongARM does not have.
- When the server refuses the login, PsiMail asks once, in one Log in dialog with the server's reply, the user name and the password. It used to ask for the password again for every command that failed.
- Cancel in that dialog leaves PsiMail usable. It stops checking and sending until you change the account, check mail or refresh.
- Correcting the user name keeps the Outbox and anything waiting to be sent.
- "Certificate not trusted" is asked once, not once per command.
- Send log goes through the Atom modem's own proxy to PsiProxy, and only says "Log sent" if PsiProxy got it.
- Fixed: closing PsiMail from the System screen with a dialog open could leave it running. A message being written is now kept as a draft.
- Fixed: the mail engine kept running after PsiMail was killed, so the installer found its files in use.

## Screenshots

PsiMail on the netBook (1.03: the inbox and the calendar) and the Series 5mx (0.96, which looks the same), with the same demo account, mail and calendar (made up). Taken in emulators. Tap or click one to see it full size.

<table>
<tr><th>netBook</th><th>Series 5mx</th></tr>
<tr><td colspan="2"><b>The Inbox. On the netBook the selected message is shown under the list</b></td></tr>
<tr>
<td><a href="docs/screenshots/netbook-psimail-inbox.png"><img src="docs/screenshots/netbook-psimail-inbox.png" alt="netBook: the Inbox with the folder list, the messages and the preview" width="400"></a></td>
<td><a href="docs/screenshots/5mx-psimail-inbox.png"><img src="docs/screenshots/5mx-psimail-inbox.png" alt="Series 5mx: the Inbox with the folder list and the messages" width="400"></a></td>
</tr>
<tr><td colspan="2"><b>A message. Links are blue and each level of quoted text has a colour on the netBook</b></td></tr>
<tr>
<td><a href="docs/screenshots/netbook-psimail-reader-quotes.png"><img src="docs/screenshots/netbook-psimail-reader-quotes.png" alt="netBook: a reply with a link and two levels of quoted text" width="400"></a></td>
<td><a href="docs/screenshots/5mx-psimail-reader-quotes.png"><img src="docs/screenshots/5mx-psimail-reader-quotes.png" alt="Series 5mx: a reply with a link and two levels of quoted text" width="400"></a></td>
</tr>
<tr><td colspan="2"><b>A message with a picture in the text</b></td></tr>
<tr>
<td><a href="docs/screenshots/netbook-psimail-reader-logo.png"><img src="docs/screenshots/netbook-psimail-reader-logo.png" alt="netBook: the start of a message, with a PNG logo" width="400"></a></td>
<td><a href="docs/screenshots/5mx-psimail-reader-logo.png"><img src="docs/screenshots/5mx-psimail-reader-logo.png" alt="Series 5mx: the start of a message, with a PNG logo" width="400"></a></td>
</tr>
<tr><td colspan="2"><b>Further down: a photo, dithered on the Psion</b></td></tr>
<tr>
<td><a href="docs/screenshots/netbook-psimail-reader.png"><img src="docs/screenshots/netbook-psimail-reader.png" alt="netBook: a JPEG photo in a message" width="400"></a></td>
<td><a href="docs/screenshots/5mx-psimail-reader.png"><img src="docs/screenshots/5mx-psimail-reader.png" alt="Series 5mx: a JPEG photo in a message" width="400"></a></td>
</tr>
<tr><td colspan="2"><b>The calendar's week: a time grid on the netBook, the chosen day's list on the 5mx</b></td></tr>
<tr>
<td><a href="docs/screenshots/netbook-psimail-calendar-week.png"><img src="docs/screenshots/netbook-psimail-calendar-week.png" alt="netBook: the week as a time grid, each event in its calendar's colour" width="400"></a></td>
<td><a href="docs/screenshots/5mx-psimail-calendar-week.png"><img src="docs/screenshots/5mx-psimail-calendar-week.png" alt="Series 5mx: the week and the day's events" width="400"></a></td>
</tr>
<tr><td colspan="2"><b>The calendar: a month</b></td></tr>
<tr>
<td><a href="docs/screenshots/netbook-psimail-calendar-month.png"><img src="docs/screenshots/netbook-psimail-calendar-month.png" alt="netBook: the month, with each day's events" width="400"></a></td>
<td><a href="docs/screenshots/5mx-psimail-calendar-month.png"><img src="docs/screenshots/5mx-psimail-calendar-month.png" alt="Series 5mx: the month, with each day's first event" width="400"></a></td>
</tr>
<tr><td colspan="2"><b>Writing a message</b></td></tr>
<tr>
<td><a href="docs/screenshots/netbook-psimail-compose.png"><img src="docs/screenshots/netbook-psimail-compose.png" alt="netBook: the text takes the height of the screen" width="400"></a></td>
<td><a href="docs/screenshots/5mx-psimail-compose.png"><img src="docs/screenshots/5mx-psimail-compose.png" alt="Series 5mx: the same on 240 lines" width="400"></a></td>
</tr>
</table>

## What it does

**Laid out like the built-in Email program**, from EIKON's own parts:

* A title band (account\\folder, how many messages or what's happening, the
  connection), column headings (? / From / Subject / Date) you tap to sort,
  a folder tree with pictures, status pictures in the message list, a
  read-only rich text reader with a scroll bar, the standard toolbar (New and
  Reply/f'ward pop up a choice, Check mail, Delete - or Close when reading),
  and View > Show > Toolbar / Title bar / List of folders. On the netBook
  and Series 7 the selected message is shown under the list (View > Show >
  Preview).
* Menus, shortcuts, wording and messages follow Symbian's EIKON Application
  Style Guide: standard shortcuts, busy messages bottom left, infoprints, a
  second tap opens, and zoom goes round three sizes like the built-in Email
  program. The menus are laid out as the built-in Email program's: File,
  Edit, Message (Event in the calendar), View, Tools. The pictures are
  original pixel art, made by `tools/mkicons.py` into PsiMail.mbm.
* Quiet progress: one steady busy message per job ("Checking mail...",
  "Sending...", "Getting message...") and one outcome ("2 new messages",
  "Sent", "No new mail"). Nothing for work PsiMail does by itself. Tools >
  Preferences > New mail > Show detailed progress shows every step again.
* Tools > Help on PsiMail (Shift+Ctrl+H): the help topics in a dialog with a
  scroll bar (see `app/pmhelp.cpp` for why not a .hlp file).

**Folders and messages**

* Folders with unread counts (bold while there is unread mail, as are the
  unread messages themselves); the newest 50 messages per folder (more with
  File > Folder > Get older messages), kept on the CF card to read offline.
* File > Folder > Create new / Rename / Delete (IMAP CREATE, RENAME and
  DELETE, with SUBSCRIBE; names go as modified UTF-7, the local files follow
  a rename). The Inbox and the standard folders stay as they are. These need
  the server, so offline they ask to go online.
* Delete (to Trash), archive, move to a folder, read/unread, flag.
  Edit > Undo (Ctrl+Z) puts back the last few deleted, moved or archived
  messages, from the server too (moved back from the Trash or the folder;
  a move still waiting offline is simply taken out of the queue): see
  `engine/undo.c`.
* Search a folder on the server (Edit > Find).
* Work offline: changes and new messages are kept and sent next time.
* Up to 4 accounts.

**Reading**

* The reader's header is part of the message, so it scrolls, zooms and
  prints with it: the subject bold and larger, the flag and unread pictures,
  the sender in bold with the address in grey and the date on the right, To
  and Cc on one smaller line ("To: me, Ann, +3 others" - a tap shows them
  all), and the attachments as chips (paperclip, name, size) that open on a
  tap or with Tab and Enter (`app/pmheader.cpp`).
* Message text downloaded when opened, up to 64 KB (the rest on request,
  Message > Get whole message), and the newest messages downloaded ahead
  after Check mail so they open at once.
* HTML mail is shown as rich text - headings, bold and italic, lists,
  quotes, links you can move to with Tab and open with Enter - and
  Message > Web > View as web page opens the original in PsiWeb (NetSurf),
  as do links. Plain text gets its quotes and links shown the same way.
* HTML newsletters without the empty space: hidden preheaders, spacer
  cells and pictures, empty paragraphs and cells are left out, and blank
  lines come one at a time (`engine/html.c`).
* Pictures in messages, JPEG (baseline and progressive), PNG and GIF,
  decoded on the Psion to 16 greys in the background, so the screen stays
  live while a big one is set out. Pictures that come with the message up to
  300 KB are fetched with it; a bigger one shows its name and size, and a tap
  fetches it. Pictures on the web are fetched only when asked for - see
  [Privacy](#privacy).
* Attachments: Message > Attachments > Open opens one in its own program
  (Word, Sketch and so on); Save (Ctrl+S) puts it in
  `D:\Documents\Attachments\` (your files, so in your Documents).
* File > Printing > Save as Word file (Shift+Ctrl+S): the open message as a Psion Word
  file (header, text, bold/italic/underline, headings, lists), through the
  Word engine itself (CWordModel), so the built-in Word opens it.
* File > Printing: Page setup, Print setup, Print preview and Print
  (Ctrl+P), with EIKON's own dialogs, so any installed printer driver works
  (PsiWin's printing via the PC, serial, parallel, infrared). It prints the
  message as the reader shows it: the header lines, the text and its
  pictures (`app/pmprint.cpp`).

**Invitations and contact cards**

* Invitations (`text/calendar` or an `.ics` file, iTIP `METHOD:REQUEST`):
  the reader shows what, when (on the Psion's clock, whatever time zone the
  invitation was written in), where and who from at the top, with Accept,
  Tentative and Decline as links (Tab, Enter or a tap; also Edit >
  Invitation). Accept and Tentative put the event in the Agenda (and so on
  the CalDAV server, with calendar sync on); each answer goes to the
  organiser as an iTIP `METHOD:REPLY` through the Outbox. A cancellation
  (`METHOD:CANCEL`) offers Remove from Agenda.
* Contact cards (`text/vcard` / `.vcf`, vCard 2.1, 3.0 and 4.0): Add to
  Contacts puts the name, company, job title, phone numbers, email
  addresses, address and web page into the Contacts program's own fields
  (as its template labels them). Attach > Add my contact card sends yours.

**Writing**

* New message, reply, reply to all, forward (with or without the original's
  attachments: PsiMail asks); files from the Psion attached (up to 8). Sent
  as UTF-8; a copy saved to Sent.
* Bold, italic and underline in the text (sent as HTML with a plain text
  copy); a signature from the account settings.
* Addresses from the Psion's Contacts: type the start of a name and press
  Tab, or the Contacts button (Ctrl+L) to pick several. Edit > Add to
  Contacts > Sender adds the sender of a message.
* Drafts are kept in the Outbox to finish later.

**New mail and the Email icon**

* New mail: a beep and an infoprint ("2 new messages", over any program
  when PsiMail is in the background); an optional timed check of the Inbox
  every 10, 15, 30 or 60 minutes while PsiMail runs (only while connected,
  or connecting); messages waiting in the Outbox go when a connection comes
  up or Work offline is turned off. Tools > Preferences > New mail
  (`app/pmauto.cpp`).
* The Email icon below the screen can open PsiMail instead of the built-in
  Email program: PsiMail asks once after it is installed, and Tools >
  Preferences > Email icon opens (PsiMail / Built-in Email) changes it.
  Built-in Email, or removing PsiMail, gives the icon back to the built-in
  program (see `button/` below).

**Calendar**: two-way sync between Fastmail's calendars and the Psion's
Agenda, and a week and month view of its own (see [below](#the-calendar)).

## Setting up Fastmail

1. On fastmail.com: Settings > Privacy & Security > Manage app passwords >
   New app password, with access to "Mail, Contacts & Calendars" (or
   IMAP/SMTP only if you don't want the calendar). Gmail and Outlook have
   app passwords too, under their security settings, once two-step
   verification is on.
2. Install `dist/PsiMail.sis`, start PsiMail. The account dialog starts
   with Fastmail's servers filled in (imap.fastmail.com:993 and
   smtp.fastmail.com:465, both TLS): fill in your address and the app
   password. For another provider, type its servers and ports.
3. Tools > Connection settings: as for PsiTerm (modem or Psion TCP/IP, baud
   rate, flow control) - see [Connections](#connections).
4. Shift+Ctrl+C (Check mail) sends and receives. The first time PsiMail
   talks to a server it shows the server's key and asks whether to trust it.

If Fastmail already saves messages sent by SMTP in your Sent folder, set
"Copy to Sent folder" to No in the account settings to avoid two copies (and
save the upload time).

## Settings

**Tools > Accounts > Settings** (Add, Switch, Remove for more accounts):

| Page | Settings |
|---|---|
| Account | Account name, your name, email address, signature (`/` starts a new line) |
| Incoming | IMAP server, port, security (None, TLS (SSL), STARTTLS), user name, password |
| Outgoing | SMTP server, port, security, copy to Sent folder |
| Limits | messages per folder (50), text per message in KB (64) |

**Tools > Preferences** (Ctrl+K):

| Page | Setting | Choices |
|---|---|---|
| General | Order of messages | newest or oldest first, by sender, by subject, unread first |
| | Download ahead | how many of the newest messages' text to fetch after Check mail (0 = off) |
| | Keep mail on | the memory disk (CF card) if there is one, or the internal disk |
| | Email icon opens | PsiMail or Built-in Email |
| New mail | Alert for new mail | Sound & message, Message only, Off |
| | Check for new mail | Off, every 10, 15 or 30 minutes, every hour |
| | If not connected | Wait for a connection, or Connect |
| | Send waiting mail when connected | No, Yes |
| | Show detailed progress | every engine and link step, for finding connection problems |
| Screen | Show pictures | Yes, Only attached files, No |
| | Web pictures | Ask, Always, Never (see [Privacy](#privacy)) |
| | Web pictures via proxy | No (the standard) or Yes: fetch them through the shared proxy (Connection settings > Network), which converts them to 16 greys on the computer; if it does not answer, they come direct |

**Tools > Calendar settings**: see [The calendar](#the-calendar).
**Tools > Connection settings**: see [Connections](#connections).

## Connections

Tools > Connection settings chooses how PsiMail reaches the Internet; the
settings are shared with PsiTerm and PsiWeb.

* **Modem**: a serial Wi-Fi modem (such as a WiRSa, a WiFi232 or the
  [Atom modem](https://github.com/danieledge/psiconnect)) on the Psion's serial port.
  PsiMail dials the server with `ATDT host:port`. Set the baud rate to the
  modem's; 115200 with RTS/CTS flow control is fastest if the cable carries
  those lines. The line is only taken while PsiMail needs it, and File >
  Disconnect (Ctrl+U) hangs up so PsiTerm or PsiWeb can have it.
* **Network card or dial-up (Psion TCP/IP)** (called Psion Internet before
  1.03): the Psion's own TCP/IP - a PC Card network card on a netBook or
  Series 7 (set it up in the Control panel; leave *Dial-up: first send*
  empty), or a dial-up connection set up in the Control panel's Internet and
  Modems settings. PsiMail starts it when it needs to and the Psion's
  connection dialogs appear; PsiWeb shares it. Nobody has run PsiMail over
  a network card on a real netBook yet: reports welcome.
* **Set up** (Ctrl+U) asks which of these you have, and the web helper, and
  fills the lines in (the assistant a new Psion is shown once).
* **Test** (Ctrl+T) tries the settings shown, before OK: whether the modem
  answers and at what speed, CTS and DCD, the modem's name, and whether
  RTS/CTS will work; for Psion TCP/IP, whether the connection is up and
  names can be looked up (it asks before dialling).

* **After use, Psion TCP/IP** (Network page): *Stays connected* (the
  standard on a new Psion) keeps the dial-up connection up after use, for
  PsiTerm and PsiWeb too, until File > Disconnect (Ctrl+U); *Disconnects when
  idle* leaves it to the Psion's own idle time, as before. Anyone updating
  from an older version keeps *Disconnects when idle* until they change it.
  The modem route always hangs up after use.
* **Web helper** (Network page; *Use proxy* before 1.03), shared with PsiTerm and PsiWeb, is optional
  and off as standard; no address is built in. **PsiMail does not need it**:
  mail, the calendar and contacts always go direct to your servers, over
  PsiMail's own TLS 1.3 with the certificate checked. PsiMail uses the proxy
  for only two things:
  * **Web pictures** in HTML mail, if Preferences > Screen > *Web pictures
    via helper* is Yes. The computer fetches each picture (doing the TLS) and
    turns it into a small 16-grey GIF, so there is less to download and
    decode, and formats the Psion cannot decode (WebP, AVIF) can be shown. If
    the proxy does not answer, the pictures come direct, for the rest of
    that run.
  * **Send log to server** (Tools > Troubleshooting), which needs PsiProxy
    and Tools > Troubleshooting > Keep a log ticked.

  [PsiProxy](https://github.com/danieledge/psiproxy) is the proxy made for
  these apps (port 8081 by default); its README says how to set it up.
  [Choosing your setup](https://github.com/danieledge/psion-core/blob/main/docs/SETUP.md) says what each set-up (the Atom modem,
  PsiProxy, or both) gives across the suite.
* **Modem provides TLS** (Link page) is only for a modem whose firmware does
  the TLS itself (the Atom modem's); leave it at No for a WiRSa or a WiFi232.
  If it is Yes and the modem does not, PsiMail notices on the first IMAP or
  SMTP connection (no plain greeting within 12 seconds), does the TLS itself
  on that same connection, and says so in `psimail.log`.

The serial port can be used by one program at a time, and the Remote link
must be off. Everything goes over TLS 1.3 (or STARTTLS, if the account says
so), with the certificate checked - see [Security](#security).

## The calendar

Tools > Calendar settings: turn "Sync with Agenda" on. The server is
caldav.fastmail.com (for another server give its address, with a path if
it needs one: `dav.example.com/cal/`). The mail password is used unless
you give another. Pick your time zone (PsiMail guesses from the Psion's
home city) and the Agenda file (normally `C:\Documents\Agenda`).

After that, Check mail (Shift+Ctrl+C) also syncs the calendar, or Event >
Sync calendar (Shift+Ctrl+Y, from anywhere) does just that. The Agenda can
stay open: PsiMail goes through the Agenda server as PsiWin does.

* Every event on your calendars from 30 days ago to 180 days ahead (both
  can be changed) becomes an Agenda entry: timed events as appointments,
  all-day ones as day entries, with their location and alarm.
* Events that repeat arrive as one entry per time. Change or delete one on
  the Psion and just that one changes on the server.
* Entries you add on the Psion go to the calendar chosen in the settings
  ("New entries go to", once the first sync has found your calendars).
  Entries already on the Psion stay there unless you choose
  "Copy to the server" before the first sync.
* Changes both ways; if an event changed on both sides, the server's
  version wins. Deleting on either side deletes on the other. Events that
  fall out of the window stay in the Agenda.
* The server keeps what the Psion doesn't show (descriptions, guests,
  other alarms): PsiMail changes only the fields it knows.

PsiMail has its own calendar too (Shift+Ctrl+N, View > Go to > Calendar,
or Calendar in the folder tree), drawn beside the folder tree with the
mail list's fonts, rows, zoom and colours: the week's seven days and the
chosen day's events (times, name, place; marks for an alarm, one of a
series, waiting to be sent; a line where "now" is), or the month with each
day's first event (Ctrl+Q switches). Today has a frame; the chosen day or
event is shown inverted, as EIKON's lists do. Enter (or a second tap) shows
an event in full; Event > Create new event (Ctrl+N) adds one through a
standard dialog (to the Agenda, and from there to the server). It shows what
has been synced, and new Psion entries waiting to be sent.

Not yet: repeating entries made on the Psion (they stay on the Psion), to-do
lists, anniversaries, more than one calendar account.

## Privacy

* **Pictures from the web are not fetched unless you ask.** A newsletter's
  pictures would tell the sender when, and where, you read it. A message
  with some says "Pictures from the web are not shown - Show them" at the
  top; Show them, or Message > Web > Show web pictures, fetches them for that
  message. Tools > Preferences > Screen > Web pictures: Ask (the default), Always or
  Never (`engine/webpics.c`, `app/pmwebpic.cpp`).
* Even when asked, PsiMail never fetches spacers or tracking pixels, sends
  no password with the pictures, and keeps to at most 16 pictures, 300 KB
  each. Working offline fetches nothing, and on the modem PsiMail says first
  how many calls it will take (each web site is a call of its own).
* Pictures sent with the message come from your own mail server with the
  message, not from the sender's web site (Tools > Preferences > Show
  pictures: Yes, Only attached files, or No).
* Passwords are kept scrambled, not encrypted, in
  `C:\System\Apps\PsiMail\PsiMail.ini` (and `Calendar.ini`): anyone with your
  Psion can use them.

## Keys

| | |
|---|---|
| Up/Down, Fn+Up/Fn+Down (Pg Up/Pg Dn), Fn+Left/Fn+Right (Home/End) | move / scroll |
| Enter or Right | open |
| Esc or Left | back (Esc stops a download while one is running) |
| Left/Right in a message | previous / next message |
| Tab / Shift+Tab in a message | next / previous link or attachment (Enter opens it) |
| Tab or Left in the message list | the folder column |
| Del (and Backspace in a list) | delete |
| Shift+Ctrl+C / Ctrl+U / Esc | check mail (send & receive) / disconnect / stop |
| Shift+Ctrl+W | work offline |
| Ctrl+N / Ctrl+R / Shift+Ctrl+R / Ctrl+W | new / reply / reply to all / forward |
| Ctrl+D / Ctrl+X / Shift+Ctrl+E | delete / move to folder / archive |
| Ctrl+Z | undo the last delete, move or archive |
| Ctrl+P / Shift+Ctrl+P / Shift+Ctrl+V | print / print setup / print preview |
| Shift+Ctrl+U / Shift+Ctrl+F | unread / flagged |
| Ctrl+S | save an attachment (Message > Attachments > Open / Save) |
| Shift+Ctrl+S | save the open message as a Word file (File > Printing > Save as Word file) |
| Ctrl+I / Ctrl+G / Ctrl+B / Ctrl+F | inbox / go to folder / outbox / find (offline: on this Psion) |
| Shift+Ctrl+J | the next unread message (1.02) |
| Shift+Ctrl+X / Shift+Ctrl+G | check this folder / get older messages (File > Folder, with Create new / Rename / Delete) |
| Ctrl+M / Shift+Ctrl+M | zoom in / out (three sizes, going round) |
| Ctrl+T / Shift+Ctrl+T / Shift+Ctrl+L | show the toolbar / title bar / folder list |
| Shift+Ctrl+Q / Shift+Ctrl+B | status information / sort |
| Ctrl+K | preferences |
| Shift+Ctrl+H / Shift+Ctrl+A / Ctrl+E | help / about PsiMail / close |
| Shift+Ctrl+N / Ctrl+Q | the calendar / switch its view (week or month, and on a tall screen the day list; View > Switch view lists them) |
| Shift+Ctrl+Y / Shift+Ctrl+D | sync the calendar / go to today |
| In the calendar: Left/Right, Pg Up/Pg Dn, Home | day, week (month), today |
| In the calendar: Ctrl+N / Enter | new event / the event in full |

### Writing a message

Writing a message fills the screen, as in the built-in Email program: To,
CC, BCC, Attachments and Subject, then the text, with the buttons on the
right. Ctrl+B, Ctrl+I and Ctrl+U make the text bold, italic or underlined
(it then goes as HTML email, with a plain text copy for mail programs that
want one). Ctrl+S sends, Ctrl+D saves it as a draft (in the Outbox), Ctrl+A
adds or removes attachments, Ctrl+L adds addresses from Contacts (Tab in an
address line fills in a match), Esc closes (asking before it throws anything
away). A new event (Ctrl+N in the calendar) is a standard dialog: the
event, where, the date, all day or start and end, and an alarm.

## Updating from the Psion

Tools > Update PsiMail asks where to look: GitHub for published releases
(raw.githubusercontent.com/danieledge/psiterm/main/dist/, over TLS 1.3), its
test builds, or `host:port` of PsiTerm's local update server
(core/server/psion-update.sh, port 8686). It fetches PsiMail-version.txt; if that
is newer, it downloads PsiMail.sis in 64 KB pieces (each checked, and fetched
again if damaged) to D:\PsiMail-update.sis (C: without a card), checks the
Ed25519 signature in PsiMail.sis.sig against the release key, and offers to
install it.

To put a build on the local server:

    core/tools/release/publish-local.sh PsiMail 0.3.1

(signs with ~/.psiterm-signing/release.key; core/tools/release/sign.py works
without the `cryptography` package too). The version must go up each time.

## Troubleshooting

* **Nothing connects.** Use Test in Tools > Connection settings first. Then
  set Tools > Preferences > New mail > Show detailed progress to Yes and
  try again: every step (dialling, the modem's answer, logging in, each
  message) is shown at the bottom left. Check that the Remote link is off
  and that PsiTerm or PsiWeb isn't holding the serial port.
* **`psimail.log`**: PsiMail keeps no log unless Tools > Troubleshooting >
  Keep a log is ticked (it is off at first, and after an update). Then the
  mail engine writes what it does to `psimail.log`, next to the program in
  `\System\Apps\PsiMail\` (on the disk PsiMail is installed on), each line
  timed, at most 32 KB; after a crash or a restart the previous engine's log
  is kept as `psimail.old`, with the reason it ended at the top of the new
  log. The tick takes effect at once; untick it to stop (the log kept so far
  stays). They are the files to send with a bug report. Tools >
  Troubleshooting > Send log to server sends them to your PsiProxy: the
  shared proxy, or, if that is the Atom modem's own, the PsiProxy set in the
  modem's Web proxy settings (AT$PU; without one it says *Needs PsiProxy on
  a computer*). It is dimmed until a proxy is set, and while no log is kept. They go as plain HTTP and hold server, account and folder names
  and the addresses of web pictures (never passwords), so send them only to
  your own PsiProxy.
* **"The mail engine is not running"**: the engine is a separate program;
  PsiMail starts it again by itself if it stops (not after three stops in
  10 minutes). Tools > Restart mail engine starts it by hand.
* **Where is my mail?** View > Status information shows where the mail is
  kept. If the move to `\System\Data\PsiMail` failed, PsiMail says so,
  keeps using the old `\PsiMail\` folder, and writes why to `Store.log`
  beside PsiMail.app (with Keep a log ticked).
* **A server's certificate is refused**: PsiMail checks the chain, the name
  and the dates (when the Psion's clock looks set). Check the Psion's date,
  or trust the server's key when asked - see [Security](#security).

## How it works

Like PsiWeb, two programs share a chunk of memory (`psimail.h`):

* `psimail.exe` (`engine/`, C): IMAP (`imap.c`, `imapparse.c`), SMTP
  (`smtp.c`), MIME (`mime.c`, `compose.c`), character sets (`charset.c`:
  the Psion's UI is Windows-1252), HTML to text (`html.c`), the files on the
  card (`store.c`), certificate checks (psion-core's
  `core/mail/engine/tlsv/`), on `ssh/psiglue.cpp` and `ssh/tls13.c` (built with
  `TLS_VERIFY`). `pmepoc.cpp` is its EPOC side, with the heartbeat that
  ends the engine only once the app has really gone.
  The calendar engine is psion-core's `core/mail/engine/cal/`, shared
  with PsiSync: `caldav.c` (CalDAV over `http.c`, with `xmlscan.c`),
  `ics.c` (iCalendar) and `caltz.c` (time zones: servers send UTC, the
  Agenda wants wall-clock times); `pmcalport.c` gives it PsiMail's settings. Invitations and contact cards in
  messages are `invite.c` (parsing, the iTIP reply) and `invmsg.c`
  (fetching the parts when a message is read). Pictures are `pictures.c`
  over `img/` (picojpeg for baseline JPEG, `pmjprog.c` for progressive),
  decoded at a lower priority than the app; web pictures are `webpics.c`.
  Undo is `undo.c`.
* `PsiMail.app` (`app/`, EIKON C++): keys, menus, dialogs, and the Agenda
  side of the calendar (`pmcal.cpp`). It reads the store's text files
  directly and sends the engine commands. The reader's header is
  `pmheader.cpp`, the quiet status `pmstatus.cpp`, restarting the engine
  `pmrecover.cpp`.
* `ui/` (C++ without EIKON, so it also builds on a PC): `pmcalmodel.cpp`
  (the calendar's events by day, from the engine's files) and `pmquote.cpp`
  (a message's plain text, for quoting in a reply). PsiMail's earlier
  hand-drawn screens and their built-in fonts are gone; everything is
  drawn with EIKON's own controls and fonts.
* `button/`: the Email icon. The EIKON server turns a tap on the icons
  below the screen into keys (the Email icon is `EEikAppbarApp5Key`), which
  the System screen captures to start the built-in programs; the newest
  capture of a key wins. `pmbutton.exe` captures that one key (again
  whenever a window group comes or goes, so it stays the newest) and brings
  PsiMail to the front or opens it; it has no window and sleeps on the
  window server. It runs only while `C:\System\Apps\PsiMail\Button.ini`
  exists, and stops when that file goes (Preferences, or the uninstaller's
  FN line), when PsiMail.app is removed or replaced, or when PsiMail tells
  it to; with it gone the System screen's capture is the one that counts
  again. PsiMail starts it each time PsiMail opens with the preference on,
  so after a reset the icon is PsiMail's once PsiMail has been opened.
  `button/pmbtnrec.cpp` is a boot hook (a recogniser that recognises
  nothing and starts `pmbutton.exe` 30 s after boot); it is built but NOT
  installed: in the emulator a program started while the Psion is still
  starting stopped the System screen from finishing, and a recogniser on
  C: that broke booting could only be cleared by a cold reset.

Everything the engine downloads is decoded as it arrives and written to a
file, so no message is held whole in memory (peak heap in the emulator:
~70 KB).

### Security

The Psion has no certificate store and PsiTerm's TLS doesn't check
certificates (its downloads are signed instead). PsiMail sends a password,
so it checks: the server's CertificateVerify signature (RSA-PSS, or ECDSA
on P-256 and P-384) against its certificate, the chain up to a root built
into PsiMail (ISRG Root X1 - Fastmail's - and the other common RSA and
ECDSA roots in `tools/roots.pem`, `tools/mkroots.py`), the name, and the
dates (when the Psion's clock looks set). A server that fails can be
trusted by the user, which pins its key (`pins.txt`). RSA is offered first,
as it costs the Psion less; a server with only an ECDSA certificate (Gmail's
IMAP, for one) takes about 1 s more on a 5mx for a P-256 check and 4 s for
P-384, and is remembered after the first time (`certs.txt`).

Passwords are kept scrambled, not encrypted, in `C:\System\Apps\PsiMail\PsiMail.ini`
(and `Calendar.ini`).

## Building

PsiMail's own code is in this repository; the code it shares with the other
programs (PsiTerm's networking and TLS, the picture decoders, the build
container and the emulator tools) is [psion-core](https://github.com/danieledge/psion-core),
carried here as the git submodule `core/`:

    git clone --recurse-submodules https://github.com/danieledge/psimail.git
    cd psimail
    core/tools/docker/psibuild setup               # once: the build image and the SDK
    core/tools/docker/psibuild "bash build.sh host"  # dist/PsiMail.sis

builds `psimail.exe`, `PsiMail.app`, `pmbutton.exe`, `psihold.exe` and
`dist/PsiMail.sis` (and with `host` the PC version of the engine,
`build/mail-host/psimail-host`, for the tests). Without the container, set
`PSION_SDK` to the [psion_cpp_sdk_linux](https://github.com/static-void/psion_cpp_sdk_linux)
folder (needs wine) and run `bash build.sh [host]`. The details are in
core's [BUILDING.md](https://github.com/danieledge/psion-core/blob/main/docs/BUILDING.md).
Tests: see [test/README.md](test/README.md).

## Releases

Every installed PsiMail's updater (Tools > Update PsiMail) reads
`dist/PsiMail-version.txt`, `dist/PsiMail.sis` and `dist/PsiMail.sis.sig`
from the [psiterm](https://github.com/danieledge/psiterm) repository, as it
did before PsiMail had a repository of its own; nothing in the program
changed. So a release is still committed there:

    release.sh 0.84

signs `dist/PsiMail.sis` with the release key (kept outside every
repository), writes `dist/PsiMail-version.txt`, and copies the three files
together into the psiterm checkout beside this one (`../psiterm/dist/`, or
`$PSITERM`), to be committed and pushed there (`dev` for the test builds,
`main` for releases). The version goes in `app/psimail.cpp` (`KVersion`) and
`pkg/psimail.pkg`, two digits (0.84 is `0,84,0`), because the EPOC installer
compares major.minor only.

## Files on the card

See the top of `engine/store.c`. In short `D:\System\Data\PsiMail\A0\`
holds the first account: `folders.txt`, a folder per mail folder with
`index.txt` (one line per message), `<uid>.txt` for each downloaded message
(and `<uid>.htm`, the HTML original; `<uid>.inv` / `.vcd`, an invitation or
contact card summed up - see `engine/invite.h`), and `outbox\`. The
calendar's files are in `D:\System\Data\PsiMail\cal\` (see
`core/mail/engine/cal/caldav.c` and `core/mail/agenda/pmcal.h`). Without a card (or with
Tools > Preferences > Keep mail on set to the internal disk) it is the same
on C:.

The store is the program's working data, so it is under `\System\` as the
style guide asks (10.2.1), in `\System\Data\` - EPOC's folder for programs'
data - rather than `\System\Mail\`, which belongs to the Message Server (it
rebuilds its index there, and can move the whole folder to another disk when
the built-in Email's messages are moved). Before 0.74 it was `\PsiMail\` at the
top of the disk: the first start of a newer PsiMail renames that folder into
place (one rename on the same disk; saved attachments go on to
`\Documents\Attachments\`). If the rename fails PsiMail keeps using
`\PsiMail\`, says so, and writes why to `Store.log` beside PsiMail.app
(`app/pmstore.cpp`). View > Status information shows where the mail is.

Removing PsiMail removes the program and its settings but leaves the mail:
it is the user's.

## Not yet

* IMAP IDLE (push mail); the timed check covers the Inbox while PsiMail runs.
* Server-side drafts (drafts stay in the Outbox).
* Repeating entries made on the Psion, to-do lists and anniversaries in the
  calendar sync.

## The suite

PsiMail is one of a set of programs that bring the Series 5mx into 2026:
[PsiTerm](https://github.com/danieledge/psiterm) (SSH and a terminal),
[PsiWeb](https://github.com/danieledge/psiweb) (a web browser),
[PsiCast](https://github.com/danieledge/psicast) (screen sharing),
[PsiLinux](https://github.com/danieledge/psilinux),
[PsiConnect](https://github.com/danieledge/psiconnect) (the Atom modem
firmware) and [PsiProxy](https://github.com/danieledge/psiproxy), on the
shared [psion-core](https://github.com/danieledge/psion-core). The suite's
home page is [danieledge.github.io/psion](https://danieledge.github.io/psion/).

## Support

If PsiMail brings email back to your Psion and you'd like to say thanks,
you can [buy me a coffee](https://buymeacoffee.com/danedge).
