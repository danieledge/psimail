// PMPREVIEW.CPP - (0.96) the preview: the selected message under the list
//
// On a tall screen (the netBook's and Series 7's 480 lines) the message list
// takes the top of its pane and the selected message is shown under it: a
// heading (subject, sender, date) and the start of its text, with quotes
// and links styled as the reader has them. The 5mx's 240 lines have no room
// for it, and there the list is as it always was.
//
// It is a second read-only editor, apart from the reader: it shows what is
// on the disk already and asks the engine for nothing, and it does not mark
// a message read. Enter, or a tap on it, opens the message in the reader as
// before. The keys stay with the list; Space pages the preview (Shift+Space
// back).
//
// View > Show > Preview turns it off (TPmSettings::iView bit 21,
// KPmViewNoPreview: zero, the standard, is on).

#include "pmapp.h"
#include "pmicons.h"
#include "pmpict.h"
#include <eikrted.h>
#include <eiktxtut.h>
#include <txtrich.h>
#include <txtfrmat.h>
#include <frmtlay.h>
#include <frmtview.h>

static const TInt KPreviewFile = 12 * 1024;    // of the message's file: the start is enough
static const TInt KPreviewText = 6000;         // characters shown
enum { EPvBold, EPvItalic, EPvUnder, EPvColour, EPvIndent, EPvMono, EPvBig };

TBool CPmView::PreviewWanted() const
	{
	return PmTall() && !(iSettings->iView & KPmViewNoPreview);
	}

TBool CPmView::PreviewOn() const
	{
	return iPreview && PreviewWanted() && iMode == EList;
	}

void CPmView::CreatePreviewL()
	{
	iPrevUid = 0;
	iPrevPen = EFalse;
	iPrevStripDown = -1;
	if (!PmTall())
		return;                                // (the 5mx: none, and nothing of it is made)
	iPrevPictures = CPmPictures::NewL();     // (1.00) its pictures, apart from the reader's
	iPreview = new(ELeave) CEikRichTextEditor(TEikBorder(TEikBorder::ENone));
	iPreview->SetContainerWindowL(*this);
	iPreview->ConstructL(this, 0, 0, CEikEdwin::EReadOnly | CEikEdwin::ENoAutoSelection);
	if (PmColour())
		iPreview->SetBackgroundColorL(PmBack());
	}

void CPmView::TogglePreviewL()
	{
	if (!PmTall())
		{
		iEikonEnv->InfoMsg(_L("Not available - this screen is too small"));
		return;
		}
	iSettings->iView ^= KPmViewNoPreview;
	((CPmAppUi*)iEikonEnv->EikAppUi())->SaveSettings();
	if (!iFolderList)
		return;
	LayoutNative();
	iPrevUid = 0;
	if (NativeMode() && iNativeShown)
		{
		iNativeMode = (TMode)-1;
		UpdateNativeL();
		DrawNow();
		}
	}

// aArea: the lower part of the list's pane. A rule, the heading's two
// lines, a rule, the text.
void CPmView::LayoutPreview(const TRect& aArea)
	{
	iPrevRect = aArea;
	TInt hh = iBoldFont->HeightInPixels() + iListFont->HeightInPixels() + 9;
	iPrevHead = TRect(aArea.iTl.iX, aArea.iTl.iY + 2, aArea.iBr.iX, aArea.iTl.iY + 2 + hh);
	// (1.00) the actions under it: Reply, Reply to all, Forward, Archive, Flag
	TInt sh = iSmallFont->HeightInPixels() + 10;
	if (sh < 22) sh = 22;
	iPrevStrip = TRect(aArea.iTl.iX, aArea.iBr.iY - sh, aArea.iBr.iX, aArea.iBr.iY);
	TRect text(aArea.iTl.iX + 5, iPrevHead.iBr.iY + 3, aArea.iBr.iX - 4, iPrevStrip.iTl.iY - 1);
	if (text.Height() < 20)
		text.iBr.iY = text.iTl.iY + 20;
	TRAPD(err, iPreview->SetRectL(text));
	(void)err;
	}

// (1.00) the actions under the preview: in-view controls, as the
// calendar's Week and Month buttons are, each also a menu command with a
// shortcut. A tap acts on the message shown
static const TInt KStripCmds[5] = { EPmCmdReply, EPmCmdReplyAll, EPmCmdForward, EPmCmdArchive, EPmCmdFlag };
static const TText* const KStripText[5] = { _S("Reply"), _S("Reply to all"), _S("Forward"), _S("Archive"), _S("Flag") };

TInt CPmView::PreviewStripButton(TInt aIndex, TRect& aRect) const
	{
	if (iPrevStrip.IsEmpty() || aIndex < 0 || aIndex >= 5)
		return EFalse;
	TInt x = iPrevStrip.iTl.iX + 5;
	for (TInt i = 0; i <= aIndex; i++)
		{
		TPtrC t(KStripText[i]);
		TInt w = iSmallFont->TextWidthInPixels(t) + 16;
		if (i == aIndex)
			{
			aRect = TRect(x, iPrevStrip.iTl.iY + 3, x + w, iPrevStrip.iBr.iY - 3);
			return aRect.iBr.iX <= iPrevStrip.iBr.iX - 2;
			}
		x += w + 6;
		}
	return EFalse;
	}

void CPmView::DrawPreviewStrip(CWindowGc& gc) const
	{
	if (iPrevStrip.IsEmpty())
		return;
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(PmColour() ? PmBandColour() : PmFace());
	gc.DrawRect(iPrevStrip);
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(PmColour() ? TRgb(153, 153, 153) : PmText());
	gc.DrawLine(iPrevStrip.iTl, TPoint(iPrevStrip.iBr.iX, iPrevStrip.iTl.iY));
	TBool any = iRows->Count() && iSel >= 0 && iSel < iRows->Count();
	gc.UseFont(iSmallFont);
	for (TInt i = 0; i < 5; i++)
		{
		TRect b;
		if (!PreviewStripButton(i, b))
			break;
		PmDrawButtonFace(gc, b, iPrevStripDown == i);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.SetPenColor(any ? PmFaceText() : KPmDarkGrey);
		TPtrC t(KStripText[i]);
		gc.DrawText(t, TPoint(b.iTl.iX + 8, b.iTl.iY + (b.Height() - iSmallFont->HeightInPixels()) / 2 + iSmallFont->AscentInPixels()));
		}
	gc.DiscardFont();
	}

void CPmView::DrawPreviewHead(CWindowGc& gc) const
	{
	if (!PreviewOn() || iPrevRect.IsEmpty())
		return;
	// the rule between the list and the preview: two lines, as the edge of
	// the folder list is one
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(PmText());
	gc.DrawRect(TRect(iPrevRect.iTl.iX, iPrevRect.iTl.iY, iPrevRect.iBr.iX, iPrevRect.iTl.iY + 1));
	gc.SetBrushColor(PmBack());
	gc.DrawRect(TRect(iPrevRect.iTl.iX, iPrevRect.iTl.iY + 1, iPrevRect.iBr.iX, iPrevHead.iBr.iY + 3));
	// (1.00) a colour screen: the heading on a band of the button face's
	// tint, so it reads as a message and not as more of the list
	TBool band = PmColour();
	if (band)
		{
		gc.SetBrushColor(PmBandColour());
		gc.DrawRect(TRect(iPrevRect.iTl.iX, iPrevRect.iTl.iY + 1, iPrevRect.iBr.iX, iPrevHead.iBr.iY + 1));
		}
	if (!iRows->Count() || iSel < 0 || iSel >= iRows->Count())
		return;
	const TPmRow& row = (*iRows)[iSel];
	TBool flagged = row.iFlags.Locate('F') >= 0;
	TInt x = iPrevHead.iTl.iX + 5, right = iPrevHead.iBr.iX - 6;
	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	// the date on the right of the first line, the subject in bold beside it
	TBuf<32> date;
	FormatNativeDate(row.iDate, date);
	TInt y1 = iPrevHead.iTl.iY + 3;
	gc.UseFont(iSmallFont);
	gc.SetPenColor(PmText());
	TInt dw = iSmallFont->TextWidthInPixels(date);
	gc.DrawText(date, TPoint(right - dw, y1 + (iBoldFont->HeightInPixels() - iSmallFont->HeightInPixels()) / 2 + iSmallFont->AscentInPixels()));
	gc.DiscardFont();
	TBuf<160> subj(row.iSubject.Length() ? Clip(row.iSubject, 150) : TPtrC(_L("(no subject)")));
	TextUtils::ClipToFit(subj, *iBoldFont, right - dw - 12 - x);
	gc.UseFont(iBoldFont);
	gc.SetPenColor(flagged ? PmFlagColour() : PmText());
	gc.DrawText(subj, TPoint(x, y1 + iBoldFont->AscentInPixels()));
	gc.DiscardFont();
	// who it is from (or to, in Sent), and a paperclip if it carries files
	TInt y2 = y1 + iBoldFont->HeightInPixels() + 2;
	TInt fr = right;
	if (band && iPrevAttN > 0 && row.iUid == iPrevUid)
		{
		// (1.00) the first attachment in a box, as the reader shows them:
		// its kind of file, its name and size, and "+2" for any more
		TBuf<80> att(iPrevAttName);
		TBuf<24> tail(iPrevAttSize);
		if (iPrevAttN > 1)
			tail.AppendFormat(_L(" +%d"), iPrevAttN - 1);
		TInt tw = iSmallFont->TextWidthInPixels(tail);
		TInt room = (right - x) * 2 / 5 - tw - 30;
		if (room < 30) room = 30;
		TextUtils::ClipToFit(att, *iSmallFont, room);
		TInt bw = 11 + 4 + iSmallFont->TextWidthInPixels(att) + 6 + tw + 8;
		TInt bh = iListFont->HeightInPixels();
		TRect box(right - bw, y2, right, y2 + bh);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(TRgb(102, 102, 102));
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(KRgbWhite);
		gc.DrawRect(box);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		PmDrawIcon(gc, iIcons, PmAttKindIcon(iPrevAttName), TPoint(box.iTl.iX + 4, box.iTl.iY + (bh - 11) / 2));
		gc.UseFont(iSmallFont);
		gc.SetPenColor(KRgbBlack);
		TInt sb = box.iTl.iY + (bh - iSmallFont->HeightInPixels()) / 2 + iSmallFont->AscentInPixels();
		gc.DrawText(att, TPoint(box.iTl.iX + 19, sb));
		gc.DrawText(tail, TPoint(box.iBr.iX - 4 - tw, sb));
		gc.DiscardFont();
		fr = box.iTl.iX - 6;
		}
	else if (row.iFlags.Locate('T') >= 0 && iIcons && iIcons->Count())
		{
		fr -= 12;
		PmDrawIcon(gc, iIcons, EMbmMsgAttach, TPoint(fr, y2 + (iListFont->HeightInPixels() - 11) / 2));
		fr -= 6;
		}
	TBuf<64> from(Clip(row.iFrom, 60));
	TextUtils::ClipToFit(from, *iListFont, fr - x);
	gc.UseFont(iListFont);
	gc.SetPenColor(PmText());
	gc.DrawText(from, TPoint(x, y2 + iListFont->AscentInPixels()));
	gc.DiscardFont();
	// a rule under the heading (the band's edge, on a colour screen)
	gc.SetPenColor(PmColour() ? TRgb(153, 153, 153) : KRgbBlack);
	if (band)
		gc.DrawLine(TPoint(iPrevRect.iTl.iX, iPrevHead.iBr.iY + 1), TPoint(iPrevRect.iBr.iX, iPrevHead.iBr.iY + 1));
	else
		gc.DrawLine(TPoint(x, iPrevHead.iBr.iY), TPoint(right, iPrevHead.iBr.iY));
	DrawPreviewStrip(gc);
	}

static void PvSpanL(CArrayFixFlat<TInt>& aSpans, TInt aPos, TInt aLen, TInt aKind, TInt aArg = 0)
	{
	if (aLen <= 0)
		return;
	aSpans.AppendL(aPos);
	aSpans.AppendL(aLen);
	aSpans.AppendL(aKind);
	aSpans.AppendL(aArg);
	}

// The message's text from its file (see ui/pmui.h for the format), as the
// reader sets it out but without pictures: block by block, with the inline
// styles. aSpans gets (position, length, kind, value) for each style.
void CPmView::PreviewTextL(TDes& aText, CArrayFixFlat<TInt>& aSpans, const TDesC& aFile)
	{
	const TChar KPara(CEditableText::EParagraphDelimiter);
	TPtrC rest(aFile);
	// past the header lines, to the blank line
	while (rest.Length())
		{
		TInt nl = rest.Locate('\n');
		TPtrC line = nl >= 0 ? rest.Left(nl) : rest;
		rest.Set(nl >= 0 ? rest.Mid(nl + 1) : TPtrC());
		if (line.Length() && line[line.Length() - 1] == '\r')
			line.Set(line.Left(line.Length() - 1));
		if (!line.Length())
			break;
		}
	TBool lastEmpty = ETrue;
	TInt bold = -1, ital = -1, link = -1;
	while (rest.Length() && aText.Length() < KPreviewText)
		{
		TInt nl = rest.Locate('\n');
		TPtrC line = nl >= 0 ? rest.Left(nl) : rest;
		rest.Set(nl >= 0 ? rest.Mid(nl + 1) : TPtrC());
		if (line.Length() && line[line.Length() - 1] == '\r')
			line.Set(line.Left(line.Length() - 1));
		TInt start = aText.Length();
		TInt kind = 0, arg = 0;
		if (line.Length() >= 2 && line[0] == 0x01)
			{
			kind = line[1];
			TPtrC body = line.Mid(2);
			if (kind == 'u')
				continue;                          // a link's address
			if (kind == 'r')
				{
				for (TInt d = 0; d < 24; d++) aText.Append(TChar(0x97));
				aText.Append(KPara);
				lastEmpty = EFalse;
				continue;
				}
			if (kind == 'h' || kind == 'q')
				{
				arg = body.Length() ? body[0] - '0' : 1;
				body.Set(body.Length() ? body.Mid(1) : body);
				}
			else if (kind == 'l')
				{
				arg = body.Length() ? body[0] - '0' : 1;
				TInt m = body.Locate(0x02);
				TPtrC marker = m > 1 ? body.Mid(1, m - 1) : TPtrC(_L("\x95"));
				body.Set(m >= 0 ? body.Mid(m + 1) : body.Mid(body.Length() ? 1 : 0));
				aText.Append(Clip(marker, 8));
				aText.Append(' ');
				}
			else if (kind == 'i')
				{
				// a picture: shown when the engine has decoded it already
				// (1.00; the preview asks for nothing), else its words
				TInt sep = body.Locate(0x02);
				TPtrC alt = sep >= 0 ? body.Left(sep) : body;
				while (alt.Length() && alt[0] == ' ') alt.Set(alt.Mid(1));
				TPtrC src = sep >= 0 ? body.Mid(sep + 1) : TPtrC();
				TInt sep2 = src.Locate(0x02);
				if (sep2 >= 0) src.Set(src.Left(sep2));
				TInt e = -1;
				if (iPrevPictures && PicturesPref() == 0 && src.Length() > 4 && src.Left(4).CompareF(_L("cid:")) == 0)
					e = iPrevPictures->FindCid(src.Mid(4));
				if (e >= 0 && aText.Length() < aText.MaxLength() - 4)
					{
					// (1.15, MEM-59) read only when it is placed
					TRAPD(le, iPrevPictures->LoadReadyL(iCoeEnv->FsSession(), e));
					(void)le;
					}
				if (e >= 0 && iPrevPictures->At(e).iState == TPmPicEntry::EReady && iPrevPictures->At(e).iBitmap &&
					aText.Length() < aText.MaxLength() - 4)
					{
					iPrevPictures->AddPlaceL(e, aText.Length());
					aText.Append(' ');
					aText.Append(KPara);
					lastEmpty = EFalse;
					continue;
					}
				aText.Append(_L("[Picture"));
				if (alt.Length()) aText.Append(_L(": "));
				body.Set(alt);
				}
			while (body.Length() && body[0] == ' ' && kind != 'c') body.Set(body.Mid(1));
			line.Set(body);
			}
		if (line.Length() > aText.MaxLength() - aText.Length() - 40)
			line.Set(line.Left(aText.MaxLength() - aText.Length() - 40));
		for (TInt i = 0; i < line.Length(); i++)
			{
			TText c = line[i];
			switch (c)
				{
			case 0x11: bold = aText.Length(); break;
			case 0x12: if (bold >= 0) PvSpanL(aSpans, bold, aText.Length() - bold, EPvBold); bold = -1; break;
			case 0x13: ital = aText.Length(); break;
			case 0x14: if (ital >= 0) PvSpanL(aSpans, ital, aText.Length() - ital, EPvItalic); ital = -1; break;
			case 0x15:
				while (i + 1 < line.Length() && line[i + 1] >= '0' && line[i + 1] <= '9') i++;
				if (i + 1 < line.Length() && line[i + 1] == 0x16) i++;
				link = aText.Length();
				break;
			case 0x17:
				if (link >= 0)
					{
					PvSpanL(aSpans, link, aText.Length() - link, EPvUnder);
					if (PmColour())
						PvSpanL(aSpans, link, aText.Length() - link, EPvColour, PmLinkColour().Value());
					}
				link = -1;
				break;
			default:
				if (c >= 0x20 || c == '\t')
					aText.Append(c);
				break;
				}
			}
		if (kind == 'i') aText.Append(']');
		if (bold >= 0) { PvSpanL(aSpans, bold, aText.Length() - bold, EPvBold); bold = aText.Length() + 1; }
		if (ital >= 0) { PvSpanL(aSpans, ital, aText.Length() - ital, EPvItalic); ital = aText.Length() + 1; }
		link = -1;
		TInt len = aText.Length() - start;
		switch (kind)
			{
		case 'h':
			PvSpanL(aSpans, start, len, EPvBold);
			PvSpanL(aSpans, start, len, EPvBig, arg == 1 ? 1300 : 1150);
			break;
		case 'l': PvSpanL(aSpans, start, len, EPvIndent, arg * 2); break;
		case 'q':
			PvSpanL(aSpans, start, len, EPvIndent, arg * 2);
			PvSpanL(aSpans, start, len, EPvItalic);
			if (PmColour())
				PvSpanL(aSpans, start, len, EPvColour, PmQuoteColour(arg).Value());
			break;
		case 'c': PvSpanL(aSpans, start, len, EPvMono); break;
		case 'i':
		case 's':
			PvSpanL(aSpans, start, len, EPvItalic);
			break;
		default:
			// a plain-text message's quotes: "> " at the start of the line
			if (kind == 0 && len > 0 && aText[start] == '>')
				{
				TInt depth = 0;
				for (TInt q = start; q < aText.Length() && (aText[q] == '>' || aText[q] == ' '); q++)
					if (aText[q] == '>') depth++;
				PvSpanL(aSpans, start, len, EPvItalic);
				if (PmColour())
					PvSpanL(aSpans, start, len, EPvColour, PmQuoteColour(depth).Value());
				}
			break;
			}
		if (len == 0 && kind == 0)
			{
			if (lastEmpty)
				continue;                          // one empty line at a time, none at the start
			lastEmpty = ETrue;
			}
		else
			lastEmpty = EFalse;
		aText.Append(KPara);
		}
	if (rest.Length())
		{
		TInt p0 = aText.Length();
		aText.Append(_L("More in the message - press Enter to open it"));
		PvSpanL(aSpans, p0, aText.Length() - p0, EPvItalic);
		aText.Append(KPara);
		}
	}

// The selected message into the preview, unless it is there already. Called
// whenever the selection may have moved (the list's keys and taps, and
// UpdateNativeL), so it must be cheap when nothing has changed.
// a tab-separated field (as psimail.cpp reads the store's files)
static TPtrC PvField(TPtrC& aLine)
	{
	TInt t = aLine.Locate('\t');
	TPtrC f = t < 0 ? aLine : aLine.Left(t);
	aLine.Set(t < 0 ? TPtrC() : aLine.Mid(t + 1));
	return f;
	}

// (1.00) the message's attachments, for the box in the heading: how many,
// and the first one's name and size (the .att file: part, size, name)
void CPmView::PreviewAttachmentsL(TUint aUid)
	{
	iPrevAttN = 0;
	iPrevAttName.Zero();
	iPrevAttSize.Zero();
	if (!PmColour())
		return;
	TBuf<150> path;
	MsgPath(aUid, _L("att"), path);
	HBufC* att = NULL;
	ReadFileL(path, att, 16 * 1024);
	if (!att)
		return;
	CleanupStack::PushL(att);
	TPtrC rest = *att;
	while (rest.Length())
		{
		TInt nl = rest.Locate('\n');
		TPtrC line = nl >= 0 ? rest.Left(nl) : rest;
		rest.Set(nl >= 0 ? rest.Mid(nl + 1) : TPtrC());
		if (!line.Length())
			continue;
		if (!iPrevAttN++)
			{
			TPtrC l = line;
			PvField(l);
			TLex lex(PvField(l));
			TInt size = 0;
			if (lex.Val(size) != KErrNone || size < 0) size = 0;
			TPtrC name = PvField(l);
			iPrevAttName = name.Left(name.Length() < iPrevAttName.MaxLength() ? name.Length() : iPrevAttName.MaxLength());
			TInt kb = (size * 3 / 4 + 1023) / 1024;   // (base64 is 4/3 of the file)
			if (kb >= 1024) iPrevAttSize.Format(_L("%d.%d MB"), kb / 1024, (kb % 1024) * 10 / 1024);
			else iPrevAttSize.Format(_L("%d KB"), kb);
			}
		}
	CleanupStack::PopAndDestroy();          // att
	}

void CPmView::PreviewSyncL()
	{
	if (!iPreview)
		return;
	if (!PreviewOn() || !iNativeShown || !iRows->Count() || iSel < 0 || iSel >= iRows->Count())
		{
		iPrevUid = 0;
		return;
		}
	const TPmRow& row = (*iRows)[iSel];
	TBuf<150> path;
	MsgPath(row.iUid, _L("txt"), path);
	TEntry entry;
	TInt have = iCoeEnv->FsSession().Entry(path, entry) == KErrNone ? 1 : 0;
	if (row.iUid == iPrevUid && iFolder == iPrevFolder && have == iPrevHave)
		return;
	iPrevUid = row.iUid;
	iPrevFolder = iFolder;
	iPrevHave = have;
	PreviewAttachmentsL(row.iUid);

	// (1.00) the last message's pictures go with its text, before their bitmaps
	iPreview->RichText()->Reset();
	iPrevPictures->ClearPlaces();
	{
	TFileName dir;
	FolderDir(iFolder, dir);
	iPrevPictures->LoadIndexL(iCoeEnv->FsSession(), dir, row.iUid);
	// (1.15, MEM-59) the pictures are read as they are placed (in the
	// text, then attached ones after it), not every one at full size first
	}
	HBufC* file = NULL;
	if (have)
		ReadFileL(path, file, KPreviewFile);
	CleanupStack::PushL(file);
	HBufC* buf = HBufC::NewLC(KPreviewText + 2048);
	TPtr t = buf->Des();
	CArrayFixFlat<TInt>* spans = new(ELeave) CArrayFixFlat<TInt>(64);
	CleanupStack::PushL(spans);
	if (file)
		{
		PreviewTextL(t, *spans, *file);
		if (entry.iSize > KPreviewFile && t.Find(_L("More in the message")) < 0)
			{
			TInt p0 = t.Length();
			t.Append(_L("More in the message - press Enter to open it"));
			PvSpanL(*spans, p0, t.Length() - p0, EPvItalic);
			t.Append(TChar(CEditableText::EParagraphDelimiter));
			}
		}
	else
		{
		t.Append(iSettings->iOffline ? _L("Not downloaded - you are working offline")
			: _L("Not downloaded yet - press Enter to get it"));
		PvSpanL(*spans, 0, t.Length(), EPvItalic);
		t.Append(TChar(CEditableText::EParagraphDelimiter));
		}
	// (1.01) attached pictures the engine has decoded already go after the
	// text, as the reader puts them
	for (TInt pe = 0; file && pe < iPrevPictures->Count() && t.Length() < t.MaxLength() - 4; pe++)
		{
		TPmPicEntry& en = iPrevPictures->At(pe);
		TBool placed = EFalse;
		for (TInt pl = 0; pl < iPrevPictures->PlaceCount(); pl++)
			if (iPrevPictures->Place(pl).iEntry == pe) placed = ETrue;
		if (placed || en.iWeb || PicturesPref() == 2)
			continue;
		TRAPD(le, iPrevPictures->LoadReadyL(iCoeEnv->FsSession(), pe));
		(void)le;
		if (en.iState != TPmPicEntry::EReady || !en.iBitmap)
			continue;
		iPrevPictures->AddPlaceL(pe, t.Length());
		t.Append(' ');
		t.Append(TChar(CEditableText::EParagraphDelimiter));
		}

	CRichText* rt = iPreview->RichText();
	rt->Reset();
	rt->InsertL(0, t);
	const TInt baseTwips = 200;                // the reader's text: Arial, 10 point
	{
	TCharFormat cf(_L("Arial"), baseTwips);
	TCharFormatMask cm;
	cm.SetAttrib(EAttFontTypeface);
	cm.SetAttrib(EAttFontHeight);
	if (PmColour())
		{
		cf.iFontPresentation.iTextColor = PmText();
		cm.SetAttrib(EAttColor);
		}
	rt->ApplyCharFormatL(cf, cm, 0, rt->DocumentLength());
	}
	for (TInt s = 0; s + 3 < spans->Count(); s += 4)
		{
		TInt pos = (*spans)[s], len = (*spans)[s + 1], kind = (*spans)[s + 2], arg = (*spans)[s + 3];
		if (pos < 0 || pos + len > rt->DocumentLength())
			continue;
		TCharFormat cf;
		TCharFormatMask cm;
		switch (kind)
			{
		case EPvBold:
			cf.iFontSpec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
			cm.SetAttrib(EAttFontStrokeWeight);
			break;
		case EPvItalic:
			cf.iFontSpec.iFontStyle.SetPosture(EPostureItalic);
			cm.SetAttrib(EAttFontPosture);
			break;
		case EPvUnder:
			cf.iFontPresentation.iUnderline = EUnderlineOn;
			cm.SetAttrib(EAttFontUnderline);
			break;
		case EPvColour:
			cf.iFontPresentation.iTextColor = TRgb((TUint32)arg);
			cm.SetAttrib(EAttColor);
			break;
		case EPvBig:
			cf.iFontSpec.iHeight = baseTwips * arg / 1000;
			cm.SetAttrib(EAttFontHeight);
			break;
		case EPvMono:
			cf.iFontSpec.iTypeface.iName = _L("Courier");
			cf.iFontSpec.iTypeface.SetIsProportional(EFalse);
			cm.SetAttrib(EAttFontTypeface);
			break;
		default:
			{
			CParaFormat* pf = CParaFormat::NewLC();
			TParaFormatMask pm;
			pf->iLeftMarginInTwips = arg * 120;
			pm.SetAttrib(EAttLeftMargin);
			rt->ApplyParaFormatL(pf, pm, pos, len);
			CleanupStack::PopAndDestroy();     // pf
			continue;
			}
			}
		rt->ApplyCharFormatL(cf, cm, pos, len);
		}
	CleanupStack::PopAndDestroy(3);            // spans, buf, file
	// (1.00) the pictures in their places: as wide as the preview, and no
	// more than two thirds of its height, so the text round them shows
	TSize room(iPreview->Rect().Width() - 8, iPreview->Rect().Height() * 2 / 3);
	if (room.iWidth > PM_PIC_MAX_W) room.iWidth = PM_PIC_MAX_W;
	for (TInt pl = 0; pl < iPrevPictures->PlaceCount(); pl++)
		{
		TPmPicPlace& place = iPrevPictures->Place(pl);
		if (place.iDocPos < 0 || place.iDocPos >= rt->DocumentLength())
			continue;
		CPmPicture* pic = new(ELeave) CPmPicture(*iPrevPictures, iSmallFont);
		CleanupStack::PushL(pic);
		PmPictureLook(*pic, iPrevPictures->At(place.iEntry), iSmallFont, room, iSettings->iOffline, iCoeEnv->ScreenDevice());
		TPictureHeader h;
		h.iPicture = pic;
		h.iPictureType = KUidPmPicture;
		pic->GetOriginalSizeInTwips(h.iSize);
		rt->DeleteL(place.iDocPos, 1);
		rt->InsertL(place.iDocPos, h);         // (the text owns it now)
		CleanupStack::Pop();                   // pic
		place.iPicture = pic;
		}
	iPreview->HandleTextChangedL();
	iPreview->SetCursorPosL(0, EFalse);
	if (iPreview->TextView())
		iPreview->TextView()->SetCursorVisibilityL(TCursor::EFCursorInvisible, TCursor::EFCursorInvisible);
	if (IsReadyToDraw() && iPreview->IsVisible())
		{
		ActivateGc();
		DrawPreviewHead(SystemGc());
		DeactivateGc();
		iPreview->DrawNow();
		}
	}

void CPmView::DrawPreviewStripNow()
	{
	if (!IsReadyToDraw() || iPrevStrip.IsEmpty())
		return;
	ActivateGc();
	DrawPreviewStrip(SystemGc());
	DeactivateGc();
	}

void CPmView::PreviewPageL(TBool aDown)
	{
	if (!PreviewOn() || !iPreview->IsVisible() || !iPreview->TextView())
		return;
	iPreview->TextView()->ScrollDisplayL(aDown ? TCursorPosition::EFPageDown : TCursorPosition::EFPageUp);
	}

// the pen on the preview: a tap opens the message (the text itself takes
// no pen: there is nothing in it to select). ETrue if the event was ours
TBool CPmView::PreviewPointerL(const TPointerEvent& aEvent)
	{
	if (!PreviewOn() || iPrevRect.IsEmpty())
		{
		iPrevPen = EFalse;
		return EFalse;
		}
	TBool inside = iPrevRect.Contains(aEvent.iPosition);
	// (1.00) the action strip: the button pressed, then its command when
	// the pen lifts on it
	if (aEvent.iType == TPointerEvent::EButton1Down && iPrevStrip.Contains(aEvent.iPosition))
		{
		iPrevStripDown = -1;
		for (TInt i = 0; i < 5; i++)
			{
			TRect b;
			if (PreviewStripButton(i, b) && b.Contains(aEvent.iPosition))
				iPrevStripDown = i;
			}
		iPrevPen = EFalse;
		DrawPreviewStripNow();
		return ETrue;
		}
	if (iPrevStripDown >= 0)
		{
		if (aEvent.iType != TPointerEvent::EButton1Up)
			return ETrue;
		TInt was = iPrevStripDown;
		iPrevStripDown = -1;
		DrawPreviewStripNow();
		TRect b;
		if (PreviewStripButton(was, b) && b.Contains(aEvent.iPosition))
			{
			if (!iRows->Count())
				iEikonEnv->InfoMsg(_L("No message selected"));
			else
				((CPmAppUi*)iEikonEnv->EikAppUi())->CommandL(KStripCmds[was]);
			}
		return ETrue;
		}
	if (iPrevStrip.Contains(aEvent.iPosition))
		return ETrue;
	if (aEvent.iType == TPointerEvent::EButton1Down)
		{
		iPrevPen = inside;
		return inside;
		}
	if (!iPrevPen)
		return EFalse;
	if (aEvent.iType == TPointerEvent::EButton1Up)
		{
		iPrevPen = EFalse;
		if (inside && iRows->Count())
			{
			iSidebar = EFalse;
			OpenCurrentL();
			}
		}
	return ETrue;
	}
