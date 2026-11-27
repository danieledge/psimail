// pmhelp.cpp - Tools > Help on PsiMail (Shift+Ctrl+H): the help topics, in
// an EIKON dialog with a topic list and a read-only text viewer.
//
// The built-in programs' help lives in .hlp files made by the SDK's aleppo
// tool and shown by the Help program. Aleppo's converter (almain.exe) is an
// EPOC WINS executable that doesn't run under wine, and a .hlp is a Data
// database with the Help program's own streams in it, so PsiMail keeps its
// help here, in the program, where it works on every machine. Each topic is
// a title and paragraphs; a line beginning "*" is a bullet.
#include "pmapp.h"
#include <eikchlst.h>
#include <eikrted.h>
#include <txtrich.h>
#include <frmtview.h>
#include <frmtlay.h>
#include <barsread.h>
#include "psihelp.h"            // (1.16) core's help text: whole lines, a bar only when needed

struct TPmHelpTopic { const char* iTitle; const char* iText; };

// The wording follows the EIKON style guide and the Coda sheet: the user's
// words, key names as the keyboard prints them (Ctrl, Shift, Fn, Tab, Enter,
// Esc), menu commands named as they appear (without their "..."), and no
// jargon that a glossary would not allow ("program", "folder", "the card",
// "Internal disk"). The machine is written "the Psion", "The Psion" or "this
// Psion" and nothing else: those three are put in as the machine's own name
// (the netBook, the Series 5mx, the Series 7) when the text is shown.
static const TPmHelpTopic KHelpTopics[] =
	{
	{ "Getting started",
	  "PsiMail reads and sends email through your provider's IMAP and SMTP servers, over a WiFi modem "
	  "or the Psion's own Internet connection.\n"
	  "\n"
	  "To add an account, use Tools > Accounts > Add. A new account starts with its servers empty and "
	  "the usual ports (993 and 465, both TLS): type your provider's servers (for Fastmail "
	  "imap.fastmail.com and smtp.fastmail.com), or leave them empty for imap. and smtp. and your "
	  "address's domain.\n"
	  "\n"
	  "Most providers want an app password rather than your normal one. On fastmail.com choose "
	  "Settings > Privacy & Security > Manage app passwords > New app password, with access to Mail "
	  "(and Calendars, for the calendar). Gmail and Outlook have the same under their security settings, "
	  "once two-step verification is on.\n"
	  "\n"
	  "Then press Shift+Ctrl+C (File > Check mail) to fetch your folders and the Inbox. The first time "
	  "PsiMail talks to a server it shows the server's key and asks whether to trust it.\n"
	  "\n"
	  "Passwords are kept on this Psion only (on its Internal disk, scrambled but not encrypted), "
	  "never on the card. The rest of the settings are copied to the card as well "
	  "(\\System\\Data\\Kept\\): if the Internal disk is ever cleared, PsiMail puts them back "
	  "and asks for each password again. Unsent mail is never removed when an account is set up "
	  "again in another order." },

	{ "Ways to connect",
	  "PsiMail needs only a way onto the Internet. Everything else is optional.\n"
	  "\n"
	  "A network card (netBook, Series 7): the fastest. Set the card up in the Control panel, then "
	  "choose Network card or dial-up (Psion TCP/IP) in Tools > Connection settings, and leave "
	  "Dial-up: first send empty. The Psion does the security (TLS) itself.\n"
	  "\n"
	  "The Atom modem: a small WiFi modem on the serial port. It can do the security itself "
	  "(Modem provides TLS), which makes connecting quicker. Its software is at "
	  "github.com/danieledge/psiconnect\n"
	  "\n"
	  "Another WiFi modem (a WiRSa, a WiFi232): works. The Psion does the security itself. About 5 KB "
	  "a second, so big attachments are slow.\n"
	  "\n"
	  "Connection settings > Set up (Ctrl+U) asks which of these you have and fills the settings in. "
	  "It is asked once on its own the first time PsiMail starts.\n"
	  "\n"
	  "On the netBook and Series 7 a box in the toolbar, above the clock, shows whether PsiMail is "
	  "online, the route (By modem, By network), whether it is secure, and when "
	  "mail was last checked. On a colour screen it has a light: green online, amber connecting, grey "
	  "offline, red not secure. Tap it, or use View > Status information (Shift+Ctrl+Q), for more." },

	{ "PsiProxy and other helpers",
	  "A helper is optional. Mail, calendar and contacts always go directly to your provider.\n"
	  "\n"
	  "PsiProxy is a free program for a computer on your network (Linux, a Mac, anything with Python "
	  "3.10 or later). Get it from github.com/danieledge/psiproxy - its page says how to install it "
	  "and run it all the time.\n"
	  "\n"
	  "With it PsiMail can show web pictures in formats the Psion cannot read (WebP, AVIF), made small "
	  "before they are sent, and can send its log for troubleshooting. PsiWeb gains much more from "
	  "it. The Atom modem has a helper of its own for web pictures (Atom modem helper).\n"
	  "\n"
	  "To use it: Tools > Connection settings > Network, Web helper: Atom modem or PsiProxy, the "
	  "computer's address and port 8081 (the Atom's own: psiproxy, port 8080). Then tick Tools > "
	  "Preferences > Screen > Use the web helper. The Web helper setting is shared "
	  "with PsiTerm and PsiWeb.\n"
	  "\n"
	  "The link to PsiProxy is not encrypted. Use it on your own network only, and set PSIPROXY_ALLOW "
	  "on the computer to the addresses allowed to use it.\n"
	  "\n"
	  "Without it: web pictures come direct, in JPEG, PNG and GIF. Nothing else changes." },

	{ "Reading messages",
	  "The folders are on the left and the messages of the open folder on the right; unread messages "
	  "are in bold. Move with the arrow keys, open with Enter, and go back with Esc. Tab moves between "
	  "the folder list and the messages. A tap selects a message and a second tap opens it.\n"
	  "\n"
	  "On the netBook and Series 7 the selected message is also shown under the list, as far as it "
	  "has been downloaded. Space pages through it (Shift+Space goes back) and Enter, or a tap on it, "
	  "opens it. View > Show > Preview turns this off.\n"
	  "\n"
	  "A message's text is downloaded when you open it (up to 64 KB - Message > Get whole message "
	  "fetches the rest). Left and Right step to the previous and next message. Tab moves between the "
	  "links and attachments in a message and Enter opens one; Message > Web > View as web page "
	  "shows the message in PsiWeb.\n"
	  "\n"
	  "Message > Attachments > Open opens an attached file in its own program (Word, Sketch and so on); "
	  "Message > Attachments > Save (Ctrl+S) saves it in the Attachments folder in Documents, on the "
	  "disk the mail is on. Delete moves a message to the Trash; Edit > Archive (Shift+Ctrl+X) puts it "
	  "in the Archive folder; Message > Mark > Unread and Flagged mark it (Shift+Ctrl+U, "
	  "Shift+Ctrl+F). With the pen, drag a message onto a folder on the left to move it there.\n"
	  "\n"
	  "Several messages at once: hold Shift and press Up or Down (or Pg Up, Pg Dn, Home, End) to choose "
	  "a run of them. Delete, Move, Archive, Unread and Flagged then act on all of them; Delete asks "
	  "first. An arrow key without Shift, or a tap on another message, lets them go.\n"
	  "\n"
	  "Conversations: View > Show > Conversations groups the messages that answer one another, as one "
	  "row with how many there are (bold if any is unread), where the newest of them would be. Right or "
	  "Enter unfolds it: the first message, then the answers, oldest first, set in a little. Left folds "
	  "it again. Delete or Move on a folded conversation acts on all of its messages. Conversations show "
	  "while the list is in date order (View > Sort).\n"
	  "\n"
	  "A tap on the sender's name in a message offers Write (a new message to them) and Add to "
	  "Contacts.\n"
	  "\n"
	  "Edit > Find (Ctrl+F) looks for words in the open folder: on the server, or (Find in: The mail "
	  "here) in the mail already downloaded, which needs no connection. Tap a column heading, or "
	  "use View > Sort, to change the order. View > Zoom in and Zoom out (Ctrl+M, Shift+Ctrl+M) go "
	  "round the three sizes." },

	{ "Pictures in messages",
	  "Pictures that come with a message - in the message's own HTML, or attached to it - are shown in "
	  "the message, shrunk to fit the screen. Pictures up to 300 KB are fetched with the message; a "
	  "bigger one is shown as its name and size, and a tap on it fetches it.\n"
	  "\n"
	  "Web pictures are fetched only if you ask, as they can tell the sender when you read a "
	  "message. A message with some says \"Pictures from the web are not shown - "
	  "Show them\" at the top; Show them (or Message > Web > Show web pictures) fetches them. "
	  "Tools > Preferences > Screen > Web pictures: Ask, Always or Never. On the modem each web site is a "
	  "call of its own.\n"
	  "\n"
	  "Tools > Preferences > Screen > Show pictures chooses what is shown: Yes, Only attached files (nothing "
	  "from inside the HTML), or No. Whatever the setting, an attached picture can still be saved or "
	  "opened with Message > Attachments.\n"
	  "\n"
	  "While a picture is being fetched or set out its frame says so; if it cannot be shown, the frame "
	  "says why, with Help on pictures under it. Working offline, pictures not yet fetched wait until "
	  "you go online.\n"
	  "\n"
	  "PsiMail reads JPEG (baseline and progressive), PNG and GIF. A picture in another format (WebP, "
	  "AVIF, HEIC, SVG) cannot be shown here; from the web, PsiProxy or the Atom modem's helper can turn "
	  "it into one PsiMail reads (see PsiProxy and other helpers). A picture too big for the memory, or "
	  "one that takes over 90 seconds to set out, is not shown either; it can still be saved with "
	  "Message > Attachments.\n"
	  "\n"
	  "On a colour screen (the netBook, the Series 7) pictures are set out in the screen's 256 colours; "
	  "pictures set out in greys before PsiMail 1.04 stay grey. On the Series 5mx they are set out in "
	  "its 16 greys with a fine dither, and Tools > Preferences > Screen greys shows a picture four "
	  "ways: choose the one that looks best with 1 to 4, make it lighter or darker with Up and Down, and "
	  "press Enter (A there opens the detailed settings, which can also choose the ordered pattern "
	  "instead). The same greys are used by PsiTerm and PsiWeb. A change applies to pictures set out "
	  "after it.\n"
	  "\n"
	  "Tools > Preferences > Screen > Reading mode: While reading a message raises the screen's contrast and "
	  "keeps the backlight on while a message is open and PsiMail is in front. Your own settings "
	  "come back in the list, in the background, on closing, or when the Psion is switched off." },

	{ "Writing messages",
	  "Message > Create new email (Ctrl+N) opens a new message; Message > Reply (Ctrl+R) replies, Reply "
	  "to all (Shift+Ctrl+R) replies to everyone and Forward (Ctrl+W) forwards. The Reply button on the "
	  "toolbar offers the same three. Type addresses separated by commas.\n"
	  "\n"
	  "In the text, Ctrl+B, Ctrl+I and Ctrl+U make bold, italic and underlined text, which is sent as "
	  "HTML with a plain copy for older programs. The Add file button adds files from the Psion (up "
	  "to 8). When you forward a message that has attachments, PsiMail asks whether to forward them too.\n"
	  "\n"
	  "Send puts the message in the Outbox and sends it straight away when you are online. Save as "
	  "draft keeps it in the Outbox to finish later: open the Outbox (Ctrl+B) and press Enter on it. "
	  "While you write, a copy is saved to the Outbox every minute. "
	  "The copy goes when you send, save or close the message. A "
	  "copy of each message you send is saved in the Sent folder unless the account settings say "
	  "otherwise (Copy to Sent folder).\n"
	  "\n"
	  "A reply starts in the text, with the message it answers folded to one line, \"[Frank wrote: 23 "
	  "lines - Ctrl+Q shows them]\": it is sent in full under your reply. Ctrl+Q puts it in the text to "
	  "edit; deleting the line leaves it out. Quotes of 3 lines or fewer are shown as they are.\n"
	  "\n"
	  "A signature goes at the end of every new message: set it in the account settings (Signature), "
	  "or in Tools > Accounts > Signatures, which can give replies a signature of their own. Type / for "
	  "a new line." },

	{ "Addresses from Contacts",
	  "PsiMail uses the addresses in the Psion's Contacts program.\n"
	  "\n"
	  "In an address line (To, CC or BCC), type the start of a name or an address and press Tab: one "
	  "match is filled in, and with several PsiMail shows them to choose from. The Contacts button "
	  "(Ctrl+L) opens the same list: type to narrow it, press Space or tap to mark several people, and "
	  "press Enter to add them.\n"
	  "\n"
	  "Edit > Add to Contacts > Sender adds the sender of the selected message to Contacts - as a new "
	  "entry, or as another address for the entry with the same name." },

	{ "Invitations and contact cards",
	  "An invitation to a meeting (a calendar file sent by Google Calendar, Outlook, Fastmail and the "
	  "like) is shown at the top of the message: what, when, where and who sent it, with Accept, "
	  "Tentative and Decline under it. Tab to one and press Enter, or tap it; Edit > Invitation has the "
	  "same commands.\n"
	  "\n"
	  "Accept and Tentative put the event in the Agenda (the file in Tools > Calendar settings), with "
	  "an alarm; with calendar sync on it then goes to your calendar on the server too. Each answer is "
	  "sent to the organiser, through the Outbox like any message, so their calendar shows it. Decline "
	  "takes an accepted event out of the Agenda again, if you say so. An event that repeats is put in "
	  "the Agenda for its first time only.\n"
	  "\n"
	  "When an event is cancelled, Remove from Agenda (in the message, or Edit > Invitation) takes it "
	  "out. An event sent only for your information has Add to Agenda instead.\n"
	  "\n"
	  "A contact card (a .vcf file) in a message is shown the same way, with Add to Contacts (also "
	  "Edit > Add to Contacts > Contact card): the name, company, job title, phone numbers, email "
	  "addresses, address and web page go into the Contacts program's own fields. A card whose email "
	  "address is in Contacts already is left as it is.\n"
	  "\n"
	  "To send your own card, press the Add file button when writing a message and choose Add my "
	  "contact card: your name and address go with the message as a .vcf file." },

	{ "Saving a message as a Word file",
	  "File > Save as Word file (Shift+Ctrl+S) saves the open message as a file the Psion's Word "
	  "program opens: the subject, From, To, Cc and Date, then the text with its bold, italic and "
	  "underlining, headings, lists and quotes as PsiMail shows them. It goes in the Documents folder "
	  "to begin with, named after the subject. Pictures are shown in the file as [Picture]." },

	{ "Folders",
	  "Every folder on the server is in the list on the left, with the number of unread messages after "
	  "its name. Standard folders - Inbox, Sent, Drafts, Trash, Junk and Archive - come first and have "
	  "their own pictures. The list holds up to 300 other folders; if the server has more, the "
	  "account's line shows how many more (+20), and so does View > Go to > Folder.\n"
	  "\n"
	  "File > Folder > Create new makes a folder, at the top level or inside another one. File > Folder "
	  "> Rename and Delete act on the highlighted folder (or the open one). The Inbox and the standard "
	  "folders cannot be renamed or deleted, and a folder that holds other folders must be emptied of "
	  "them first. Deleting a folder deletes the messages in it too, so PsiMail asks first.\n"
	  "\n"
	  "Edit > Move to folder (Ctrl+X) moves the selected message, and Edit > Archive (Shift+Ctrl+X) "
	  "moves it to the Archive folder. View > Go to > Folder (Ctrl+G) opens any folder, and Ctrl+I "
	  "goes straight to the Inbox.\n"
	  "\n"
	  "File > Folder > Check this folder (Shift+Ctrl+E) fetches new messages for the open folder alone; Get "
	  "older messages (Shift+Ctrl+G) brings in the next 50 older ones. Check mail fetches every folder "
	  "and sends what is waiting in the Outbox." },

	{ "Getting mail in advance, and offline",
	  "PsiMail keeps the newest messages of each folder (50, unless the account's Limits page says "
	  "otherwise) on the card or the Internal disk, so they can be read without a connection. After "
	  "Check mail it also gets the text of the newest messages in advance, so they open at once; the "
	  "number is set in Tools > Preferences (Get in advance), and 0 turns it off.\n"
	  "\n"
	  "File > Work offline (Shift+Ctrl+O) stops PsiMail connecting. Anything you do offline - deleting, "
	  "moving, flagging, sending - is kept and done the next time you check mail. Making, renaming and "
	  "deleting folders needs the server, so PsiMail asks to go online for those.\n"
	  "\n"
	  "What PsiMail is doing is shown at the bottom left of the screen. Esc stops a download that is "
	  "running; File > Disconnect (Ctrl+U) hangs up." },

	{ "Where the mail is kept",
	  "PsiMail keeps your mail in the System folder, out of the way of your own files: "
	  "\\System\\Data\\PsiMail on the card, or on the Internal disk if the card is not in or "
	  "Tools > Preferences > Keep mail on says so. View > Status information shows which.\n"
	  "\n"
	  "Before version 0.74 the mail was in a PsiMail folder at the top of the disk. The first time a "
	  "newer PsiMail opens, it moves that folder into the System folder - one quick step - and the "
	  "attachments you had saved go to the Attachments folder in Documents. If the move cannot be done "
	  "(the disk is full or a file is in use, say) PsiMail says so and goes on using the old folder, "
	  "and tries again next time.\n"
	  "\n"
	  "Removing PsiMail removes the program and its settings, but not your mail: that stays in "
	  "\\System\\Data\\PsiMail until you delete that folder yourself." },

	{ "The calendar",
	  "PsiMail can keep the Psion's Agenda in step with a CalDAV calendar such as Fastmail's. Turn it on "
	  "in Tools > Calendar settings (tick Sync with Agenda): the server (caldav.fastmail.com, or another "
	  "server's address), the password if it differs from the mail one, your time zone and the Agenda "
	  "file. New entries go to shows (none yet) until the first sync has found your calendars.\n"
	  "\n"
	  "Event > Sync calendar (Shift+Ctrl+Y, from anywhere) fetches changes and sends new Agenda entries. "
	  "With Sync with Agenda ticked, Check mail also does this. If Control panel > Sync keeps the "
	  "Agenda, PsiMail only fetches the calendar. View > Go to > Calendar (Shift+Ctrl+N) "
	  "shows the week; View > Switch view (Ctrl+Q) changes between the week and the month. Event > "
	  "Create new event (Ctrl+N) adds an entry to the Agenda, which is then sent to the server; Event > "
	  "Details shows the selected event.\n"
	  "\n"
	  "Calendar settings > Agenda > Existing entries: Keep here only leaves the entries already in the "
	  "Agenda where they are; Copy to the server sends them too. Repeating entries made on the Psion "
	  "stay on the Psion; to-do lists and anniversaries are not synced." },

	{ "Connections",
	  "Tools > Connection settings chooses how PsiMail reaches the Internet, the same way as PsiTerm.\n"
	  "\n"
	  "Test (Ctrl+T) tries the settings shown, before OK: whether the modem answers and at what "
	  "speed, and whether CTS allows RTS/CTS flow control; for Psion TCP/IP, whether the "
	  "connection is up and names can be looked up (it asks before dialling).\n"
	  "\n"
	  "Modem: a serial WiFi modem (such as a WiRSa or a WiFi232) on the Psion's serial port. PsiMail "
	  "dials the server with the modem's own dial command. Set Speed to the modem's speed; 115200 "
	  "with RTS/CTS flow control is fastest if the cable carries those lines.\n"
	  "\n"
	  "Network card or dial-up (Psion TCP/IP): the Psion's own TCP/IP - a network card, or a dial-up "
	  "connection set up in the Control panel. PsiMail starts it when it needs to and the Psion's "
	  "connection dialogs appear. Dial-up: first send is what is sent to a serial modem first; leave "
	  "it empty for a network card. Connection settings > Network > After use, Psion TCP/IP chooses "
	  "what happens next, for PsiTerm and PsiWeb too: Stays connected (the standard) keeps it up "
	  "until you disconnect with File > Disconnect (Ctrl+U); Disconnects when idle leaves it to "
	  "the Psion's own idle time, as before (kept for anyone updating from an older version). The "
	  "modem route always hangs up after use.\n"
	  "\n"
	  "Modem provides TLS is only for a modem whose own software does the TLS (the Atom modem's); "
	  "leave it at No for a WiRSa or a WiFi232. If it is Yes and the modem does not, PsiMail "
	  "notices on the first connection and does the TLS itself.\n"
	  "\n"
	  "Web helper (Connection settings > Network, shared with PsiTerm and PsiWeb) is optional: see "
	  "PsiProxy and other helpers. PsiMail uses it only for web pictures, when Tools > Preferences > "
	  "Screen > Use the web helper is ticked (if the helper does not answer, the "
	  "pictures come direct), and for Tools > Troubleshooting > Send log to helper, which sends "
	  "PsiMail's log to a PsiProxy on a computer, to help find a fault: the helper, or if that is the "
	  "Atom modem's own, the PsiProxy set in the modem's Web proxy settings (without one it says Needs "
	  "PsiProxy on a computer). The log goes as plain HTTP and can hold server and account names.\n"
	  "\n"
	  "PsiMail keeps no log unless Tools > Troubleshooting > Keep a log is ticked. Then the mail "
	  "engine writes what it does to its log (the last one is kept as well), for whoever is looking "
	  "into a problem; it takes effect at once, and unticking it stops it (the log kept so far stays). "
	  "Troubleshooting > Show log shows the end of it, and Send log to helper sends it; both are dimmed "
	  "while no log is kept. Passwords are never written to the log. Troubleshooting > Restart mail "
	  "engine starts the part of PsiMail that does the networking again, should it stop answering.\n"
	  "\n"
	  "Everything goes over TLS 1.3 (or STARTTLS, if the account says so); the server's certificate is "
	  "checked against the key you trusted the first time.\n"
	  "\n"
	  "Progress is shown at the bottom left, and the result at the top right. "
	  "To see every step (dialling, logging in, each message "
	  "fetched), which helps when a connection will not come up, tick Tools > Preferences > New mail > "
	  "Show detailed progress." },

	{ "The Email icon",
	  "On the Series 5mx the Email icon below the screen can open PsiMail instead of the built-in Email "
	  "program. PsiMail asks once, the first time it opens after it is installed; after that, Tools > "
	  "Preferences > Email icon opens chooses PsiMail or Built-in Email.\n"
	  "\n"
	  "With PsiMail chosen, a tap on the Email icon brings PsiMail to the front, or opens it if it is "
	  "closed. A small helper program waits in the background for the tap, taking very little memory "
	  "and doing nothing until then; the other icons work as they always do. After a reset, open "
	  "PsiMail once and the Email icon is PsiMail's again.\n"
	  "\n"
	  "Choose Built-in Email, or remove PsiMail, and the Email icon opens the built-in Email program "
	  "again. PsiMail's connections and the built-in Email program are not changed either way.\n"
	  "\n"
	  "In Coda for the netBook the Email icon opens PsiMail already, so PsiMail does not ask, and "
	  "Email icon opens shows PsiMail, dimmed." },

	{ "Updating PsiMail",
	  "Tools > Update PsiMail looks for a newer PsiMail: Released versions, Test versions, or A "
	  "computer on my network (a server of your own, with its address and port). It downloads the new "
	  "version in pieces to the card, checks its signature and offers to install it. PsiMail closes "
	  "while the installer runs; start it again afterwards.\n"
	  "\n"
	  "Tools > About PsiMail (Shift+Ctrl+A) shows the version you have." },

	{ "Keyboard shortcuts",
	  "Up/Down, Fn+Up/Fn+Down (Pg Up/Pg Dn), Fn+Left/Fn+Right (Home/End): move and scroll\n"
	  "Enter or Right: open; Esc or Left: back\n"
	  "Left/Right in a message: previous / next message\n"
	  "Tab: between the folders and the messages; in a message, the next link; in an address, Contacts\n"
	  "Del: delete (to the Trash)\n"
	  "Shift+Up/Down: choose several messages; Right/Left on a conversation: unfold / fold it\n"
	  "Ctrl+Q in a reply: show the quote\n"
	  "\n"
	  "Shift+Ctrl+C: check mail (send and receive)\n"
	  "Shift+Ctrl+E / Shift+Ctrl+G: check this folder / get older messages\n"
	  "Ctrl+U / Esc / Shift+Ctrl+O: disconnect / stop / work offline\n"
	  "Ctrl+N / Ctrl+R / Shift+Ctrl+R / Ctrl+W: create new / reply / reply to all / forward\n"
	  "Ctrl+D / Ctrl+X / Shift+Ctrl+X / Ctrl+Z: delete / move to folder / archive / undo\n"
	  "Shift+Ctrl+U / Shift+Ctrl+F: mark unread / flagged\n"
	  "Ctrl+S / Shift+Ctrl+S: save an attachment / save as Word file\n"
	  "Ctrl+P / Shift+Ctrl+P / Shift+Ctrl+V: print / print setup / print preview\n"
	  "Ctrl+I / Ctrl+G / Ctrl+B / Ctrl+F: Inbox / go to folder / Outbox / find\n"
	  "Shift+Ctrl+J: the next unread message\n"
	  "Ctrl+M / Shift+Ctrl+M: zoom in / out\n"
	  "Ctrl+T / Shift+Ctrl+T / Shift+Ctrl+L: toolbar / title bar / folder list\n"
	  "Shift+Ctrl+Q / Shift+Ctrl+B: status information / sort\n"
	  "Shift+Ctrl+N / Ctrl+Q: the calendar / switch view (week or month; a tall screen's day list too)\n"
	  "Shift+Ctrl+Y / Shift+Ctrl+D: sync the calendar / go to today\n"
	  "Ctrl+K / Shift+Ctrl+H / Shift+Ctrl+A / Ctrl+E: preferences / help / about / close" }
	};

static const TInt KHelpCount = sizeof(KHelpTopics) / sizeof(KHelpTopics[0]);

// ----- the text: core's (ssh/psihelp.h) -------------------------------------
//
// (1.16) The help's text is psion-core's CPsiHelpText, as PsiTerm's: whole
// lines (7 on 240, 12 on 480), scrolled a whole line at a time, so the top
// line is never cut, and a scroll bar only when the topic is longer than the
// box. Until 1.15 it was PsiMail's own CPmHelpText, which scrolled by pixels
// and always drew its bar. Up at the top of the text goes back to the Topic
// line (HelpTopLine).

SEikControlInfo CPmHelpDialog::CreateCustomControlL(TInt aControlType)
	{
	SEikControlInfo info;
	info.iControl = NULL;
	info.iTrailerTextId = 0;
	info.iFlags = 0;
	if (aControlType == EPsiCtHelpText)
		{
		CPsiHelpText* t = new(ELeave) CPsiHelpText;
		t->SetTop(TCallBack(HelpTopLine, this));
		info.iControl = t;
		}
	return info;
	}

TInt CPmHelpDialog::HelpTopLine(TAny* aSelf)
	{
	TRAPD(err, ((CPmHelpDialog*)aSelf)->TopicLineL());
	return err;
	}

void CPmAppUi::HelpL(TInt aTopic)
	{
	CPmHelpDialog* dlg = new(ELeave) CPmHelpDialog(aTopic);
	dlg->ExecuteLD(PmTall() ? R_PM_HELP_DIALOG_TALL : R_PM_HELP_DIALOG);   // (0.96: more of it on 480 lines)
	}

void CPmHelpDialog::PreLayoutDynInitL()
	{
	CDesCArrayFlat* titles = new(ELeave) CDesCArrayFlat(KHelpCount);
	CleanupStack::PushL(titles);
	TBuf<40> t;
	for (TInt i = 0; i < KHelpCount; i++)
		{
		t.Copy(TPtrC8((const TUint8*)KHelpTopics[i].iTitle));
		titles->AppendL(t);
		}
	CleanupStack::Pop();
	CEikChoiceList* cl = (CEikChoiceList*)Control(EPmDlgHelpTopic);
	cl->SetArrayL(titles);                  // it owns the array now
	if (iTopic < 0 || iTopic >= KHelpCount)
		iTopic = 0;
	cl->SetCurrentItem(iTopic);
	ShowTopicL(iTopic);
	}

// the topic's text: its title in bold, then the paragraphs
void CPmHelpDialog::ShowTopicL(TInt aTopic)
	{
	const TPmHelpTopic& topic = KHelpTopics[aTopic];
	TPtrC8 title((const TUint8*)topic.iTitle);
	TPtrC8 text((const TUint8*)topic.iText);
	HBufC* b = HBufC::NewLC(title.Length() + text.Length() + 404);   // (room for the machine's name, below)
	TPtr p = b->Des();
	p.Copy(title);
	p.Append(CEditableText::EParagraphDelimiter);
	for (TInt i = 0; i < text.Length(); i++)
		{
		TUint8 c = text[i];
		if (c == '\n')
			p.Append(CEditableText::EParagraphDelimiter);
		else
			p.Append((TText)c);
		}
	// (1.14) the machine by its own name: "the Psion" in the text is "the
	// netBook", "the Series 5mx" or "the Series 7" on the screen
	TPtrC machine = PmMachine();
	const TText* const KWords[3] = { _S("the Psion"), _S("The Psion"), _S("this Psion") };
	for (TInt w = 0; w < 3; w++)
		{
		TPtrC word(KWords[w]);
		TInt keep = word.Length() - 5;          // ("the ", "The ", "this ")
		TInt at;
		while ((at = p.Find(word)) >= 0 && p.Length() + machine.Length() - 5 <= p.MaxLength())
			{
			p.Replace(at + keep, 5, machine);
			}
		}
	CPsiHelpText* box = (CPsiHelpText*)Control(EPmDlgHelpText);
	CEikRichTextEditor* ed = box->Editor();
	ed->SetTextL(b);
	TCharFormat cf;
	TCharFormatMask cm;
	cf.iFontSpec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
	cm.SetAttrib(EAttFontStrokeWeight);
	ed->RichText()->ApplyCharFormatL(cf, cm, 0, title.Length());
	ed->HandleTextChangedL();
	ed->SetCursorPosL(0, EFalse);
	box->TextChangedL();                    // (at its top; a bar only if it is longer than the box)
	CleanupStack::PopAndDestroy();          // b
	}

// the keys start on the topic list (Left and Right change the topic; Down
// moves into the text, where Up and Down scroll it)
void CPmHelpDialog::PostLayoutDynInitL()
	{
	((CPsiHelpText*)Control(EPmDlgHelpText))->TextChangedL();
	TryChangeFocusToL(EPmDlgHelpTopic);
	}

void CPmHelpDialog::HandleControlStateChangeL(TInt aControlId)
	{
	if (aControlId != EPmDlgHelpTopic)
		return;
	TInt t = ((CEikChoiceList*)Control(EPmDlgHelpTopic))->CurrentItem();
	if (t != iTopic && t >= 0 && t < KHelpCount)
		{
		iTopic = t;
		ShowTopicL(t);
		}
	}

TBool CPmHelpDialog::OkToExitL(TInt /*aButtonId*/)
	{
	return ETrue;
	}
