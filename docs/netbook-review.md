# PsiMail on the netBook and Series 7: a review

15 November 2026, PsiMail 0.95. Written before any of the 0.96 work.

PsiMail 0.95 was run in the netBook emulator (v1.05(450) image, 640 x 480,
256 colours) and the Series 5mx emulator (640 x 240, 16 greys) with the same
demo account. Every view, menu and dialog was opened on both. This is what
was found, what the netBook could do better, and the order to do it in.

The rule for all of it: the 5mx keeps its layout and its greys. Anything new
is chosen at run time from the screen (its height, and whether it is a colour
screen), as PsiTerm and PsiWeb do
(`ScreenDevice()->DisplayMode() >= EColor16`, and the screen's height).

## What 0.95 does on the netBook

It runs, and nothing is broken. The view fills the 640 x 480 screen, the
lists show 12 more rows, the reader shows twice as much of a message, EIKON's
own parts (menus, dialogs, the list highlight, the toolbar) come in the
netBook's colours.

But it is a 5mx program stretched to twice the height:

1. **Half the Inbox is empty.** Twelve demo messages fill the top half of
   the list. The bottom half of the screen is white. Opening a message still
   replaces the list, as it must on 240 lines. The netBook's own Email, and
   every desktop mail program, would show the message under the list.
2. **Hard-coded greys and black.** The list's highlight is the system's navy
   (EIKON draws it), but the folder tree's highlight, the calendar's chosen
   day and event, the month's chosen cell and the "Week / Month" buttons are
   drawn by PsiMail in black. The column headings, the title band's boxes and
   PsiMail's own scroll bars are a fixed grey (170, 170, 170), which is not
   the netBook's button face, and not a Coda theme's either.
3. **Nothing uses colour.** Links are underlined black. Quoted text is
   italic black at every level. A flagged message differs from a read one
   only by a small picture. The calendar does not show which calendar an
   event is in at all.
4. **The month view has room it does not use.** A cell is 55 x 55 pixels on
   the netBook and shows the date, some dots, and one event title. Three or
   four titles would fit.
5. **Dialogs made for 240 lines.** Writing a message fills the screen, but
   the text box is still four lines high: the bottom half of the screen is
   empty grey. Help shows five lines of text in a small box in the middle of
   a 480-line screen. Status information and About are fine.
6. **5mx words.** About says "for the Psion Series 5mx". Preferences has a
   "Screen greys..." button, which opens the 5mx's grey calibration: it has
   no meaning on a colour TFT.
7. **Pictures are 16 greys.** The engine's decoders (psion-core,
   `mail/engine/img`) are grey from end to end: the JPEG decoder keeps only
   luma, the row sink is 8-bit grey, the `.pmi` cache is 4 bits a pixel.
   A colour photo is shown in dithered greys on a colour screen.
8. **5mx limits.** Download ahead is 10 messages, a message's decoded
   pictures are kept within 600 KB, and pictures are at most 544 wide. These
   were set for 16 MB and a 36 MHz ARM710T.
9. **The toolbar** is right: title, four buttons, the netBook's own clock.
   It leaves a gap between Delete and the clock, as the built-in programs'
   toolbars do.

Things that are fine and should be left: the menus (they follow the style
guide and fit), the shortcuts, the zoom sizes (Arial at 150, 200 and 250
twips reads well on the netBook's screen; its pixels are about the same size
as the 5mx's), the reader's header, the connection and account dialogs.

## What the netBook allows

- 480 lines: a list and a message at once.
- 256 colours through the window server, and the system's colour list
  (`CEikonEnv::Color()`), which the Coda themes change. Using it makes
  PsiMail follow the theme.
- A 190 MHz StrongARM: formatting a message for a preview each time the
  selection moves costs nothing. On the 5mx it would lag.
- About 48 MB free: more messages downloaded ahead, a bigger picture budget.
- Coda's Office converters: a .docx or .xlsx attachment already opens in Word
  or Sheet from PsiMail, because Message > Attachments > Open hands the file
  to the system (`RApaLsSession::StartDocument`), and the converters are
  found by the system's recognisers. Nothing to change in PsiMail.

## Enhancements, in order

### To do in 0.96

1. **A preview pane** (three panes: folders, list, message). On a screen of
   400 lines or more the selected message is shown under the list: subject,
   sender, date, and the text, with links and quotes styled. Arrow keys still
   move through the list; Space pages the preview; Enter or a second tap
   opens the full reader as before. It only shows messages already on the
   disk, and does not mark them read, so it never touches the network.
   View > Show > Preview turns it off. Not offered on the 5mx.
2. **The system's colours everywhere PsiMail draws.** Highlights in the
   folder tree and the calendar from the system's highlight colours; button
   faces, headings and scroll bars from the system's button colours. On the
   5mx these are the same black, white and greys as now.
3. **Colour that means something**, on colour screens only: links in blue,
   quoted text in a colour per level, flagged messages in red in the list,
   the calendar's calendars each in a colour, today in the month marked in
   colour.
4. **The month view with several events a day** where the cell has room
   (the netBook: three or four lines; the 5mx: one, as now).
5. **Writing a message**: the text box takes the rest of the screen's
   height. **Help** in a taller dialog.
6. **Words**: About names the three machines; "Screen greys..." is dimmed on
   a colour screen.
7. **Download ahead**: 25 messages as the standard on the netBook (10 on the
   5mx). The setting is still in Preferences.
8. **The View menu**: "Show toolbar", "Show title bar" and "Show list of
   folders" become a "Show" cascade (Toolbar, Title bar, List of folders,
   Preview) so that the menu stays within eight commands. The shortcuts do
   not change.

### Deferred, and why

- **Colour pictures.** The right fix is in psion-core: colour through the
  three decoders and the shrinking sink, a new `.pmi` layout (8 bits a pixel
  in the netBook's palette), and the app's bitmaps. It touches every picture
  path, the fuzz tests and the Atom modem's transcoder, which shares the
  code. It needs its own piece of work and its own release. Until then
  pictures stay 16 greys, which the netBook shows cleanly.
- **Conversation view (threading).** The index has no References or
  In-Reply-To column; the engine would have to fetch and store them, and the
  store's format would change. Worth doing, not small.
- **A local search.** Edit > Find asks the server. Searching the store on
  the netBook is quick enough to do locally; it needs an engine command.
- **A week view with a time grid** (hours down the side). The week strip and
  day list work, and gain rows from the height. A grid is a new view.
- **Inline pictures in the preview pane.** The preview is text; the reader
  shows the pictures. Putting them in the preview means sharing the reader's
  picture machinery between two editors.
- **More toolbar buttons.** The style guide says four. The netBook's own
  programs keep to it.
- **Signatures per account, bigger pictures.** No change needed for the
  netBook: the signature is already per account, and 544 pixels is the
  reader's width beside the toolbar on both machines.

## What 0.96 did

Items 1 to 8 above are in 0.96. Each was checked in both emulators after it
went in.

- The preview is `app/pmpreview.cpp`: a second read-only editor, made only
  on a tall screen. Its switch is bit 21 of `TPmSettings::iView`
  (`KPmViewNoPreview`); the struct's size is unchanged.
- The colours are a handful of functions at the top of `app/pmnative.cpp`
  (`PmBack`, `PmText`, `PmFace`, `PmHighlight` and so on). On the 5mx they
  return the black, white and grey that were written into the code before.
  On a colour screen they return the system's colours. Checked with the
  Coda and the Dark colour schemes.
- A flagged message has a red flag in the list, and its subject is red in
  the preview. The whole row in red was tried: EIKON's column list draws its
  text in its own colour, so the flag is drawn instead.
- The 5mx: the same layout, greys and dialogs as 0.95. The only change it
  sees is the View > Show menu and the wording of About.

One thing seen and not changed: in the netBook emulator Esc sometimes acts
twice (it closes a dialog, and the list behind then moves the highlight to
the folder list). 0.95 does the same there, and the 5mx emulator does not.
It may be the emulator's keyboard. Worth trying on a real netBook.
