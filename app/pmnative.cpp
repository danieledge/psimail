// pmnative.cpp - PsiMail's mailbox and reader built from EIKON's own
// controls, laid out like the Psion's built-in Email program: a title band
// (where you are, how many messages, the connection), column headings you
// can tap to sort, a folder tree with pictures, a message list with status
// pictures, a read-only rich text viewer, the standard toolbar on the right
// and zoom. (The calendar's pane, drawn beside the same folder tree, is
// pmcalview.cpp; writing a message and making an event are dialogs.)
#include "pmapp.h"
#include "pmicons.h"
#include "pgsetup.h"
#include "pmpict.h"
#include "psisafe.h"           // (1.12) the hidden-calendars list written whole (review R104)
#include <eiktxlbx.h>
#include <eiktxlbm.h>
#include <eikclb.h>
#include <eikclbd.h>
#include <eiklbi.h>
#include <eiklbv.h>
#include <eikrted.h>
#include <eiksbfrm.h>
#include <eikscrlb.h>
#include <eiktxtut.h>
#include <eiktbar.h>
#include <e32hal.h>
#include <txtrich.h>
#include <txtfrmat.h>
#include <frmtlay.h>
#include <frmtview.h>

static const TInt KIndent = 12;          // the folder tree: one level
static const TInt KIconCol = 28;         // the message list: status + attachment pictures
static const TInt KScrollBarW = 23;      // room beside the reader for its scroll bar (EIKON's width)
// the folder tree's per-row value: depth (3 bits), KTreeBold, icon << 4, lines << 16
static const TInt KTreeBold = 8;
// the message list's per-row value: icon, KMsgAttach, KMsgUnread
static const TInt KMsgAttach = 0x100;
static const TInt KMsgUnread = 0x200;

// ----- PsiMail.mbm, read once -------------------------------------------------------

// the multi-bitmap file: two UIDs, a checksum, the trailer's offset; the
// trailer is a count and the bitmaps' offsets; each bitmap has a 40-byte
// header (size, header length, width, height, twips, twips, bits a pixel,
// colour, palette, compression) before its rows, 4-byte aligned
CPmMbm* CPmMbm::NewL(RFs& aFs, const TDesC& aFile)
	{
	CPmMbm* m = new(ELeave) CPmMbm;
	CleanupStack::PushL(m);
	m->iFile = aFile;
	RFile f;
	User::LeaveIfError(f.Open(aFs, aFile, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(f);
	TInt size = 0;
	User::LeaveIfError(f.Size(size));
	if (size < 24 || size > 256 * 1024)
		User::Leave(KErrCorrupt);
	m->iData = HBufC8::NewL(size);
	TPtr8 p = m->iData->Des();
	User::LeaveIfError(f.Read(p, size));
	CleanupStack::PopAndDestroy();          // f
	const TUint32* w = (const TUint32*)p.Ptr();
	TUint32 trailer = w[4];
	if (w[0] != 0x10000037 || w[1] != 0x10000042 || trailer + 4 > (TUint32)size || (trailer & 3))
		User::Leave(KErrCorrupt);
	m->iCount = (TInt)w[trailer / 4];
	if (m->iCount < 0 || m->iCount > 1000 || trailer + 4 + m->iCount * 4 > (TUint32)size)
		User::Leave(KErrCorrupt);
	m->iOffsets = w + trailer / 4 + 1;
	CleanupStack::Pop();
	return m;
	}

CPmMbm::~CPmMbm()
	{
	delete iData;
	}

CFbsBitmap* CPmMbm::CreateBitmapL(TInt aId)
	{
	CFbsBitmap* b = new(ELeave) CFbsBitmap;
	CleanupStack::PushL(b);
	TBool done = EFalse;
	if (aId >= 0 && aId < iCount)
		{
		TInt size = iData->Length();
		TInt off = (TInt)iOffsets[aId];
		if (off >= 0 && (off & 3) == 0 && off + 40 <= size)
			{
			const TUint32* h = (const TUint32*)(iData->Ptr() + off);
			TInt hdr = (TInt)h[1], wd = (TInt)h[2], ht = (TInt)h[3], bpp = (TInt)h[6], colour = (TInt)h[7], comp = (TInt)h[9];
			// (1.00) the colour set's pictures are 8 bits a pixel in EPOC's 256 colours
			TDisplayMode mode = colour ? (bpp == 8 ? EColor256 : ENone)
				: bpp == 1 ? EGray2 : bpp == 2 ? EGray4 : bpp == 4 ? EGray16 : bpp == 8 ? EGray256 : ENone;
			TInt stride = ((wd * bpp + 31) / 32) * 4;
			if (mode != ENone && (colour == 0 || colour == 1) && !comp && hdr >= 40 && wd > 0 && ht > 0 && wd <= 640 && ht <= 240 &&
				stride > 0 && stride <= 640 && off + hdr >= off && off + hdr + stride * ht <= size)
				{
				User::LeaveIfError(b->Create(TSize(wd, ht), mode));
				TInt dst = CFbsBitmap::ScanLineLength(wd, mode);
				const TUint8* src = iData->Ptr() + off + hdr;
				for (TInt y = 0; y < ht; y++)
					{
					TPtrC8 row(src + y * stride, dst < stride ? dst : stride);
					TBuf8<640> line(row);          // (a row is at most 640 bytes: 640 pixels at 8 bits)
					b->SetScanLine(line, y);
					}
				done = ETrue;
				}
			}
		}
	if (!done)
		User::LeaveIfError(b->Load(iFile, aId));
	CleanupStack::Pop();
	return b;
	}

// (1.00) the colour set is loaded after the grey one on a colour screen
// (CPmView::LoadIconsL): a grey id is then drawn from its colour twin
TBool PmColourIcons(CArrayPtr<CFbsBitmap>* aIcons)
	{
	return aIcons && aIcons->Count() >= EMbmAllCount;
	}

CFbsBitmap* PmIconBitmap(CArrayPtr<CFbsBitmap>* aIcons, TInt aId)
	{
	if (!aIcons || aId < 0 || aId >= EMbmCount || aId >= aIcons->Count())
		return NULL;
	return aIcons->At(PmColourIcons(aIcons) ? aId + EMbmColourBase : aId);
	}

// (1.00) an attachment's picture, by the end of its name: a picture, a
// Word file, a Sheet file, a PDF, or anything else
TInt PmAttKindIcon(const TDesC& aName)
	{
	TInt dot = aName.LocateReverse('.');
	if (dot < 0 || aName.Length() - dot > 6)
		return EMbmAttOther;
	TBuf<6> e(aName.Mid(dot + 1));
	e.LowerCase();
	if (e == _L("jpg") || e == _L("jpeg") || e == _L("png") || e == _L("gif") || e == _L("bmp") || e == _L("mbm") || e == _L("webp") || e == _L("heic"))
		return EMbmAttPicture;
	if (e == _L("doc") || e == _L("docx") || e == _L("rtf") || e == _L("wrd") || e == _L("txt") || e == _L("odt"))
		return EMbmAttWord;
	if (e == _L("xls") || e == _L("xlsx") || e == _L("csv") || e == _L("shd") || e == _L("ods"))
		return EMbmAttSheet;
	if (e == _L("pdf"))
		return EMbmAttPdf;
	return EMbmAttOther;
	}

// a picture from PsiMail.mbm, drawn through its mask (black = drawn)
void PmDrawIcon(CWindowGc& aGc, CArrayPtr<CFbsBitmap>* aIcons, TInt aId, const TPoint& aPos)
	{
	if (!aIcons || aId < 0 || aId + 1 >= aIcons->Count())
		return;
	if (PmColourIcons(aIcons) && aId < EMbmCount)
		aId += EMbmColourBase;
	CFbsBitmap* bmp = aIcons->At(aId);
	CFbsBitmap* mask = aIcons->At(aId + 1);
	aGc.BitBltMasked(aPos, bmp, TRect(bmp->SizeInPixels()), mask, ETrue);
	}

// ----- (0.96) the screen and its colours ---------------------------------------------

// The 5mx: 16 greys and 240 lines, and PsiMail's own black, white and light
// grey as before. A colour screen (the netBook, the Series 7): the system's
// colour list, which the machine's colour scheme sets, so that what PsiMail
// draws itself matches EIKON's lists, buttons and dialogs beside it.
TBool PmColour()
	{
	CPmAppUi* ui = (CPmAppUi*)CEikonEnv::Static()->EikAppUi();
	return ui && ui->iColour;
	}

TBool PmTall()
	{
	CPmAppUi* ui = (CPmAppUi*)CEikonEnv::Static()->EikAppUi();
	return ui && ui->iTall;
	}

// (1.14) Coda: its netBook image says so in the ROM. There the Email icon
// opens PsiMail already (the image's Shell), and the toolbar has Coda's extras
TBool PmCoda()
	{
	CPmAppUi* ui = (CPmAppUi*)CEikonEnv::Static()->EikAppUi();
	return ui && ui->iCoda;
	}

// (1.14) the machine's name in the words its own manual uses: 240 lines is
// the Series 5mx; 480 is the netBook, unless the machine says it is a Series 7
TPtrC PmMachine()
	{
	if (!PmTall())
		return _L("Series 5mx");
	TMachineInfoV1Buf mi;
	if (UserHal::MachineInfo(mi) == KErrNone && mi().iMachineName.Find(_L("7")) >= 0)
		return _L("Series 7");
	return _L("netBook");
	}

// (1.14) a size in MB with one decimal ("56.4 MB"), as Coda's toolbar shows
// free memory, and "768 KB" below 1 MB
void PmSizeText(TDes& aOut, TInt64 aBytes)
	{
	aOut.Zero();
	TInt kb = (aBytes / TInt64(1024)).Low();
	if (aBytes < TInt64(0))
		kb = 0;
	if (kb < 1024)
		aOut.AppendFormat(_L("%d KB"), kb);
	else
		{
		TInt tenths = (kb * 10 + 512) / 1024;
		aOut.AppendFormat(_L("%d.%d MB"), tenths / 10, tenths % 10);
		}
	}

static TRgb SysColour(TInt aLogical, const TRgb& aGrey)
	{
	return PmColour() ? CEikonEnv::Static()->Color((TEikLogicalColor)aLogical) : aGrey;
	}

TRgb PmBack() { return SysColour(EEikColorControlBackground, KRgbWhite); }
TRgb PmText() { return SysColour(EEikColorControlText, KRgbBlack); }
TRgb PmFace() { return SysColour(EEikColorButtonFaceClear, TRgb(170, 170, 170)); }
TRgb PmFaceText() { return SysColour(EEikColorButtonText, KRgbBlack); }
TRgb PmHighlight() { return SysColour(EEikColorControlHighlightBackground, KRgbBlack); }
TRgb PmHighlightText() { return SysColour(EEikColorControlHighlightText, KRgbWhite); }

// the colours with a meaning (colour screens only; all from the 256's
// 6x6x6 cube, so none is dithered). Dark on a light workspace, light on a
// dark one (a dark colour scheme)
static TBool DarkWorkspace()
	{
	TRgb b = PmBack();
	return b.Red() + b.Green() + b.Blue() < 384;
	}

TRgb PmLinkColour()
	{
	if (!PmColour()) return KRgbBlack;
	return DarkWorkspace() ? TRgb(102, 204, 255) : TRgb(0, 0, 204);
	}

TRgb PmQuoteColour(TInt aLevel)
	{
	if (!PmColour()) return KRgbBlack;
	if (DarkWorkspace())
		return aLevel <= 1 ? TRgb(153, 204, 255) : aLevel == 2 ? TRgb(153, 255, 153) : TRgb(255, 204, 153);
	return aLevel <= 1 ? TRgb(0, 51, 153) : aLevel == 2 ? TRgb(0, 102, 51) : TRgb(153, 51, 0);
	}

TRgb PmFlagColour()
	{
	if (!PmColour()) return KRgbBlack;
	return DarkWorkspace() ? TRgb(255, 102, 102) : TRgb(204, 0, 0);
	}

// (1.00) the colour pass. Each is a colour of the 256's cube, with a
// light and a dark workspace's value; on the 5mx they are not drawn
TRgb PmUnreadBar()
	{
	return DarkWorkspace() ? TRgb(102, 204, 255) : TRgb(0, 0, 204);
	}

// a colour halfway between two, put on the cube (so it is not dithered)
TRgb PmMix(const TRgb& aA, const TRgb& aB)
	{
	TInt c[3] = { (aA.Red() + aB.Red()) / 2, (aA.Green() + aB.Green()) / 2, (aA.Blue() + aB.Blue()) / 2 };
	for (TInt i = 0; i < 3; i++)
		c[i] = (c[i] + 25) / 51 * 51;
	return TRgb(c[0], c[1], c[2]);
	}

// the preview's heading: the button face's tint, between it and the workspace
TRgb PmBandColour()
	{
	return PmMix(PmFace(), PmBack());
	}

// the calendar: the weekend's columns and today's, faintly tinted
TRgb PmWeekendTint()
	{
	return DarkWorkspace() ? PmMix(PmBack(), TRgb(102, 102, 102)) : TRgb(238, 238, 238);
	}

TRgb PmTodayTint()
	{
	return DarkWorkspace() ? PmMix(PmBack(), TRgb(102, 102, 0)) : TRgb(255, 255, 204);
	}

// the connection light: 0 grey (offline, not connected), 1 amber
// (connecting), 2 green (connected), 3 red (connected with no security)
TRgb PmLight(TInt aState)
	{
	switch (aState)
		{
	case 1: return TRgb(255, 153, 0);
	case 2: return TRgb(0, 204, 0);
	case 3: return TRgb(204, 0, 0);
	default: return TRgb(153, 153, 153);
		}
	}

TRgb PmCalColour(TInt aShade)
	{
	if (!PmColour()) return KRgbBlack;
	static const TUint8 KLight[4][3] = { { 0, 51, 153 }, { 0, 102, 51 }, { 153, 51, 0 }, { 102, 0, 153 } };
	static const TUint8 KDark[4][3] = { { 153, 204, 255 }, { 153, 255, 153 }, { 255, 204, 153 }, { 255, 153, 255 } };
	const TUint8* c = DarkWorkspace() ? KDark[aShade & 3] : KLight[aShade & 3];
	return TRgb(c[0], c[1], c[2]);
	}

// ----- the folder tree: a text list box that draws lines and pictures ---------------

class CPmTreeDrawer : public CTextListItemDrawer
	{
public:
	CPmTreeDrawer(MTextListBoxModel* aModel, const CFont* aFont)
		: CTextListItemDrawer(aModel, aFont) {}
	void SetFonts(const CFont* aFont, const CFont* aBold) { iFont = aFont; iBold = aBold; }
	void SetData(CArrayPtr<CFbsBitmap>* aIcons, CArrayFixFlat<TInt>* aTree) { iIcons = aIcons; iTree = aTree; }
protected:
	void DrawActualItem(TInt aItemIndex, const TRect& aRect, TBool aCurrent, TBool aEmphasized, TBool aDimmed) const;
private:
	CArrayPtr<CFbsBitmap>* iIcons;
	CArrayFixFlat<TInt>* iTree;
	const CFont* iBold;                    // folders with unread mail, as the built-in Email has them
	};

void CPmTreeDrawer::DrawActualItem(TInt aItemIndex, const TRect& aRect, TBool aCurrent, TBool aEmphasized, TBool /*aDimmed*/) const
	{
	CWindowGc& gc = *iGc;
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(PmBack());
	gc.DrawRect(aRect);
	TInt v = (iTree && aItemIndex < iTree->Count()) ? (*iTree)[aItemIndex] : 0;
	TInt depth = v & 7;
	const CFont* font = (v & KTreeBold) && iBold ? iBold : iFont;
	TInt icon = (v >> 4) & 0xfff;
	TInt lines = v >> 16;
	TInt x0 = aRect.iTl.iX + 3;
	TInt top = aRect.iTl.iY, bottom = aRect.iBr.iY;
	TInt mid = top + aRect.Height() / 2;
	// the tree's lines: down from the parent, across to the picture
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(PmText());
	for (TInt l = 1; l <= depth; l++)
		{
		TInt lx = x0 + (l - 1) * KIndent + 7;
		if (l < depth)
			{
			if (lines & (1 << l))
				gc.DrawLine(TPoint(lx, top), TPoint(lx, bottom));
			}
		else
			{
			gc.DrawLine(TPoint(lx, top), TPoint(lx, (lines & (1 << l)) ? bottom : mid + 1));
			gc.DrawLine(TPoint(lx, mid), TPoint(x0 + depth * KIndent, mid));
			}
		}
	TInt ix = x0 + depth * KIndent;
	if (iIcons && icon + 1 < iIcons->Count())
		{
		TSize s = iIcons->At(icon)->SizeInPixels();
		PmDrawIcon(gc, iIcons, icon, TPoint(ix, mid - s.iHeight / 2));
		}
	// the name: highlighted on its own, as the built-in programs do
	TBuf<80> text(Clip(iModel->ItemText(aItemIndex), 80));
	TInt tx = ix + 16 + 3;
	if (tx + font->TextWidthInPixels(text) + 5 > aRect.iBr.iX)
		TextUtils::ClipToFit(text, *font, aRect.iBr.iX - tx - 5);   // "..." where it won't fit
	TInt tw = font->TextWidthInPixels(text) + 5;
	if (tx + tw > aRect.iBr.iX) tw = aRect.iBr.iX - tx;
	TRect tr(tx, top + 1, tx + tw, bottom - 1);
	gc.UseFont(font);
	TInt base = (tr.Height() - font->HeightInPixels()) / 2 + font->AscentInPixels();
	if (aCurrent && aEmphasized)
		{
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushColor(PmHighlight());     // (0.96: the system's highlight on a colour screen)
		gc.SetPenColor(PmHighlightText());
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.DrawText(text, tr, base, CGraphicsContext::ELeft, 2);
		}
	else
		{
		gc.SetBrushColor(PmBack());
		gc.SetPenColor(PmText());
		gc.DrawText(text, tr, base, CGraphicsContext::ELeft, 2);
		if (aCurrent)
			{
			// where you are, when the message list has the keys
			gc.SetBrushStyle(CGraphicsContext::ENullBrush);
			gc.SetPenStyle(CGraphicsContext::EDottedPen);
			gc.DrawRect(tr);
			}
		}
	gc.DiscardFont();
	}

class CPmFolderListBox : public CEikTextListBox
	{
public:
	void SetFontL(const CFont* aFont, const CFont* aBold, TInt aRowHeight)
		{
		((CPmTreeDrawer*)iItemDrawer)->SetFonts(aFont, aBold);
		TInt h = aFont->HeightInPixels() + 4;
		if (h < aRowHeight) h = aRowHeight;
		SetItemHeightL(h);
		}
	void SetData(CArrayPtr<CFbsBitmap>* aIcons, CArrayFixFlat<TInt>* aTree)
		{
		((CPmTreeDrawer*)iItemDrawer)->SetData(aIcons, aTree);
		}
protected:
	void CreateItemDrawerL()
		{
		iItemDrawer = new(ELeave) CPmTreeDrawer(Model(), CEikonEnv::Static()->NormalFont());
		}
	};

// ----- the message list: EIKON's columns, with the status pictures drawn over -------

class CPmMsgItemDrawer : public CColumnListBoxItemDrawer
	{
public:
	CPmMsgItemDrawer(MTextListBoxModel* aModel, const CFont* aFont)
		: CColumnListBoxItemDrawer(aModel, aFont) {}
	void SetData(CArrayPtr<CFbsBitmap>* aIcons, CArrayFixFlat<TInt>* aMsgIcons) { iIcons = aIcons; iMsgIcons = aMsgIcons; }
	void SetFonts(CColumnListBoxData* aColumns, const CFont* aFont, const CFont* aBold)
		{ iColumns = aColumns; iNormal = aFont; iBoldFont = aBold; }
	// the columns' font is set per row: unread messages are in bold, as
	// the built-in Email program shows them
	void SetRowFont(const CFont* aFont) const
		{
		if (!iColumns)
			return;
		for (TInt c = 2; c <= 6; c++)
			{
			TRAPD(err, iColumns->SetColumnFontL(c, aFont));   // (the columns exist: no allocation)
			(void)err;
			}
		}
	void SetMark(const TInt* aLo, const TInt* aHi) { iMarkLo = aLo; iMarkHi = aHi; }
	void DrawItemText(TInt aItemIndex, const TRect& aRect, TBool aCurrent, TBool aEmphasized) const
		{
		// (1.05) a run chosen with Shift+arrows: each of its rows highlighted
		if (iMarkLo && iMarkHi && *iMarkLo >= 0 && *iMarkHi > *iMarkLo && aItemIndex >= *iMarkLo && aItemIndex <= *iMarkHi)
			{
			aCurrent = ETrue;
			aEmphasized = ETrue;
			}
		TInt v = (iMsgIcons && aItemIndex < iMsgIcons->Count()) ? (*iMsgIcons)[aItemIndex] : 0xff;
		TBool bold = (v & KMsgUnread) && iBoldFont && iNormal;
		if (bold)
			SetRowFont(iBoldFont);
		CColumnListBoxItemDrawer::DrawItemText(aItemIndex, aRect, aCurrent, aEmphasized);
		if (bold)
			SetRowFont(iNormal);
		if (!iMsgIcons || aItemIndex >= iMsgIcons->Count())
			return;
		TInt icon = v & 0xff;
		TInt y = aRect.iTl.iY + (aRect.Height() - 11) / 2;
		// a white tile under the pictures, so they read on the highlight too
		if (aCurrent)
			{
			iGc->SetPenStyle(CGraphicsContext::ENullPen);
			iGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
			iGc->SetBrushColor(PmBack());
			iGc->DrawRect(TRect(aRect.iTl.iX, aRect.iTl.iY, aRect.iTl.iX + KIconCol - 2, aRect.iBr.iY));
			}
		if (PmColourIcons(iIcons))
			{
			// (1.00) colour pictures (the red flag, the yellow envelope of an
			// unread message) and a blue bar at the left of an unread row
			TInt x = aRect.iTl.iX;
			if (v & KMsgUnread)
				{
				iGc->SetPenStyle(CGraphicsContext::ENullPen);
				iGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
				iGc->SetBrushColor(PmUnreadBar());
				iGc->DrawRect(TRect(x, aRect.iTl.iY + 1, x + 2, aRect.iBr.iY - 1));
				iGc->SetBrushStyle(CGraphicsContext::ENullBrush);
				}
			if (icon != 0xff)
				PmDrawIcon(*iGc, iIcons, icon, TPoint(x + 3, y));
			if (v & KMsgAttach)
				PmDrawIcon(*iGc, iIcons, EMbmMsgAttach, TPoint(x + 13, y));
			return;
			}
		if (icon == EMbmMsgFlag && PmColour())
			{
			// (0.96) on a colour screen the flag is red: drawn here, as the
			// pictures in PsiMail.mbm are greys
			TInt x = aRect.iTl.iX + 2;
			iGc->SetPenStyle(CGraphicsContext::ESolidPen);
			iGc->SetPenColor(PmText());
			iGc->DrawLine(TPoint(x, y), TPoint(x, y + 11));
			iGc->SetPenColor(PmFlagColour());
			iGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
			iGc->SetBrushColor(PmFlagColour());
			iGc->DrawRect(TRect(x + 1, y, x + 8, y + 6));
			iGc->SetBrushStyle(CGraphicsContext::ENullBrush);
			}
		else if (icon != 0xff)
			PmDrawIcon(*iGc, iIcons, icon, TPoint(aRect.iTl.iX + 1, y));
		if (v & KMsgAttach)
			PmDrawIcon(*iGc, iIcons, EMbmMsgAttach, TPoint(aRect.iTl.iX + 11, y));
		}
private:
	CArrayPtr<CFbsBitmap>* iIcons;
	CArrayFixFlat<TInt>* iMsgIcons;
	const TInt* iMarkLo;
	const TInt* iMarkHi;
	CColumnListBoxData* iColumns;
	const CFont* iNormal;
	const CFont* iBoldFont;
	};

class CPmMsgListBox : public CEikTextListBox
	{
public:
	void ConstructL(const CCoeControl* aParent)
		{
		CColumnListBoxModel* model = new(ELeave) CColumnListBoxModel;
		CleanupStack::PushL(model);
		model->ConstructL(new(ELeave) CDesCArrayFlat(32), ELbmOwnsItemArray);
		CPmMsgItemDrawer* drawer = new(ELeave) CPmMsgItemDrawer(model, CEikonEnv::Static()->NormalFont());
		CleanupStack::PushL(drawer);
		CEikListBox::ConstructL(model, drawer, aParent, 0);
		CleanupStack::Pop(2);
		}
	CColumnListBoxModel* Model() const { return (CColumnListBoxModel*)iModel; }
	void SetData(CArrayPtr<CFbsBitmap>* aIcons, CArrayFixFlat<TInt>* aMsgIcons)
		{
		((CPmMsgItemDrawer*)iItemDrawer)->SetData(aIcons, aMsgIcons);
		}
	void SetMark(const TInt* aLo, const TInt* aHi) { ((CPmMsgItemDrawer*)iItemDrawer)->SetMark(aLo, aHi); }
	void SetFonts(const CFont* aFont, const CFont* aBold)
		{
		((CPmMsgItemDrawer*)iItemDrawer)->SetFonts(Model()->ColumnData(), aFont, aBold);
		}
	// (1.02) a tap on the state picture of the message already chosen marks
	// it read or unread (one tap chooses, a second acts, as the guide has it;
	// the tap is on the picture, not the row). Elsewhere, the list's own
	void HandlePointerEventL(const TPointerEvent& aEvent)
		{
		if (aEvent.iType == TPointerEvent::EButton1Down && aEvent.iPosition.iX < Rect().iTl.iX + KIconCol - 2)
			{
			TInt idx = -1;
			if (View() && View()->XYPosToItemIndex(aEvent.iPosition, idx) && idx >= 0 && idx == CurrentItemIndex())
				{
				iPictureTap = ETrue;
				return;
				}
			}
		if (iPictureTap)
			{
			if (aEvent.iType == TPointerEvent::EButton1Up)
				{
				iPictureTap = EFalse;
				((CPmAppUi*)CEikonEnv::Static()->EikAppUi())->CommandL(EPmCmdUnread);
				}
			return;
			}
		// (1.05) the pen goes down on a message and is dragged out of the
		// list to the left: a drag to the folders, not the list's own (it
		// would scroll); let go on a folder, the message is moved there
		if (aEvent.iType == TPointerEvent::EButton1Down)
			{
			iDragging = EFalse;
			iDownRow = -1;
			if (View()) View()->XYPosToItemIndex(aEvent.iPosition, iDownRow);
			}
		else if (aEvent.iType == TPointerEvent::EDrag && !iDragging && aEvent.iPosition.iX < Rect().iTl.iX - 4)
			{
			iDragging = ETrue;
			CEikonEnv::Static()->BusyMsgL(_L("Moving to a folder\x85"));
			}
		if (iDragging)
			{
			if (aEvent.iType == TPointerEvent::EButton1Up)
				{
				iDragging = EFalse;
				CEikonEnv::Static()->BusyMsgCancel();
				CPmView* v = ((CPmAppUi*)CEikonEnv::Static()->EikAppUi())->View();
				if (v && !v->DropOnFolderL(aEvent.iPosition, iDownRow))
					CEikonEnv::Static()->InfoMsg(_L("Not moved - let go on a folder"));
				}
			return;
			}
		CEikTextListBox::HandlePointerEventL(aEvent);
		}
private:
	TBool iPictureTap;
	TBool iDragging;
	TInt iDownRow;
	};

// ----- zoom -----------------------------------------------------------------------

// three sizes, as the built-in Email program has: small, medium (the
// default) and large, and zooming goes round from one to the next
static const TInt KZoomLevels = 3;
static const TInt KZoomFactors[KZoomLevels] = { 800, 1000, 1250 };
// the lists' and headings' Arial at each size (twips), and their rows (pixels)
static const TInt KListTwips[KZoomLevels] = { 150, 200, 250 };
static const TInt KRowHeight[KZoomLevels] = { 18, 24, 29 };

static TInt ZoomLevel(const TPmSettings& aSettings)
	{
	TInt z = aSettings.iZoom;                  // 0 = the default
	if (z < 1) z = 2;
	if (z > KZoomLevels) z = KZoomLevels;
	return z - 1;                              // 0..2
	}

TBool CPmView::NativeMode() const
	{
	return iMode == EList || iMode == EOutbox || iMode == EMessage || iMode == ENoAccount || iMode == ECalendar;
	}

TInt CPmView::RowHeight() const
	{
	TInt z = ZoomLevel(*iSettings);
	TInt h = iListFont ? iListFont->HeightInPixels() + 4 : 0;
	return h < KRowHeight[z] ? KRowHeight[z] : h;
	}

void CPmView::LoadIconsL()
	{
	iIcons = new(ELeave) CArrayPtrFlat<CFbsBitmap>(EMbmCount);
	CPmAppUi* ui = (CPmAppUi*)iEikonEnv->EikAppUi();
	TFileName mbm = ui->Application()->BitmapStoreName();
	// (1.00) a colour screen takes the colour set too; the 5mx only the greys
	TInt n = PmColour() ? EMbmAllCount : EMbmCount;
	for (TInt i = 0; i < n; i++)
		{
		CFbsBitmap* b = NULL;
		TInt err;
		if (ui->Mbm())
			{
			TRAP(err, b = ui->Mbm()->CreateBitmapL(i));
			}
		else
			{
			TRAP(err, b = iEikonEnv->CreateBitmapL(mbm, i));
			}
		if (err != KErrNone && i >= EMbmCount)
			{
			// (an older PsiMail.mbm, with no colour set: the greys)
			while (iIcons->Count() > EMbmCount)
				{
				delete iIcons->At(iIcons->Count() - 1);
				iIcons->Delete(iIcons->Count() - 1);
				}
			return;
			}
		if (err != KErrNone)
			{
			iIcons->ResetAndDestroy();           // no PsiMail.mbm: no pictures
			return;
			}
		CleanupStack::PushL(b);
		iIcons->AppendL(b);
		CleanupStack::Pop();
		}
	}

void CPmView::CreateNativeL()
	{
	iLinks = new(ELeave) CArrayFixFlat<TPmLinkRange>(16);
	iLinkSel = -1;
	iPenHead = -1;
	iZoomFactor = new(ELeave) TZoomFactor(iCoeEnv->ScreenDevice());
	iTree = new(ELeave) CArrayFixFlat<TInt>(16);
	iMsgIcons = new(ELeave) CArrayFixFlat<TInt>(32);
	LoadIconsL();

	iFolderList = new(ELeave) CPmFolderListBox;
	iFolderList->ConstructL(this, 0);
	iFolderList->Model()->SetItemTextArray(new(ELeave) CDesCArrayFlat(16));
	iFolderList->Model()->SetOwnershipType(ELbmOwnsItemArray);
	iFolderList->CreateScrollBarFrameL();
	iFolderList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);
	iFolderList->SetListBoxObserver(this);
	iFolderList->SetData(iIcons, iTree);

	iMsgList = new(ELeave) CPmMsgListBox;
	iMsgList->ConstructL(this);
	iMsgList->CreateScrollBarFrameL();
	iMsgList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);
	iMsgList->SetListBoxObserver(this);
	iMsgList->SetData(iIcons, iMsgIcons);
	iMsgList->SetMark(&iMarkLo, &iMarkHi);

	iReader = new(ELeave) CEikRichTextEditor(TEikBorder(TEikBorder::ENone));
	iReader->SetContainerWindowL(*this);
	iReader->ConstructL(this, 0, 0, CEikEdwin::EReadOnly | CEikEdwin::ENoAutoSelection | CEikEdwin::EAlwaysShowSelection);
	// (the reader's scroll bar is drawn here: see DrawReaderBar)
	if (PmColour())
		iReader->SetBackgroundColorL(PmBack());   // (0.96) the workspace's colour, whatever the colour scheme
	CreatePreviewL();                          // (0.96) the preview under the list (pmpreview.cpp)
	iPictures = CPmPictures::NewL();

	ApplyZoomL();
	ShowNative(EFalse);
	}

void CPmView::DestroyNative()
	{
	delete iFolderList;
	iFolderList = NULL;
	delete iMsgList;
	iMsgList = NULL;
	delete iReader;                              // (its pictures go with it, before their bitmaps)
	iReader = NULL;
	delete iPreview;
	iPreview = NULL;
	delete iPrevPictures;                        // (after the preview: its pictures point in)
	iPrevPictures = NULL;
	delete iPictures;
	iPictures = NULL;
	CWsScreenDevice* dev = iCoeEnv->ScreenDevice();
	if (iListFont) dev->ReleaseFont(iListFont);
	iListFont = NULL;
	if (iSmallFont) dev->ReleaseFont(iSmallFont);
	iSmallFont = NULL;
	if (iBoldFont) dev->ReleaseFont(iBoldFont);
	iBoldFont = NULL;
	if (iTitleFont) dev->ReleaseFont(iTitleFont);
	iTitleFont = NULL;
	delete iZoomFactor;
	iZoomFactor = NULL;
	delete iLinks;
	iLinks = NULL;
	delete iTree;
	iTree = NULL;
	delete iMsgIcons;
	iMsgIcons = NULL;
	if (iIcons)
		iIcons->ResetAndDestroy();
	delete iIcons;
	iIcons = NULL;
	}

TInt CPmView::CountComponentControls() const
	{
	return iFolderList ? (iPreview ? 4 : 3) : 0;
	}

CCoeControl* CPmView::ComponentControl(TInt aIndex) const
	{
	switch (aIndex)
		{
	case 0: return iFolderList;
	case 1: return iMsgList;
	case 2: return iReader;
	default: return iPreview;
		}
	}

void CPmView::SizeChanged()
	{
	if (iFolderList)
		LayoutNative();
	}

static CFont* GetFontL(CWsScreenDevice* aDev, TInt aTwips, TBool aBold, CFont*& aOld)
	{
	TFontSpec spec(_L("Arial"), aTwips);
	if (aBold)
		spec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
	CFont* font = NULL;
	User::LeaveIfError(aDev->GetNearestFontInTwips(font, spec));
	if (aOld)
		aDev->ReleaseFont(aOld);
	aOld = font;
	return font;
	}

// fonts and the reader's zoom from the zoom level
void CPmView::ApplyZoomL()
	{
	TInt z = ZoomLevel(*iSettings);
	CWsScreenDevice* dev = iCoeEnv->ScreenDevice();
	GetFontL(dev, KListTwips[z], EFalse, iListFont);
	GetFontL(dev, 150, EFalse, iSmallFont);
	GetFontL(dev, KListTwips[z], ETrue, iBoldFont);
	GetFontL(dev, 160, ETrue, iTitleFont);     // (the title band doesn't zoom)
	iStatusH = 0;                              // (the title band says it all now)

	iFolderList->SetFontL(iListFont, iBoldFont, KRowHeight[z]);
	CColumnListBoxData* cd = iMsgList->Model()->ColumnData();
	for (TInt c = 0; c < 7; c++)
		cd->SetColumnFontL(c, iListFont);
	iMsgList->SetFonts(iListFont, iBoldFont);
	cd->SetColumnAlignmentL(6, CGraphicsContext::ERight);
	TInt h = iListFont->HeightInPixels() + 4;
	if (h < KRowHeight[z]) h = KRowHeight[z];
	iMsgList->SetItemHeightL(h);

	iZoomFactor->SetZoomFactor(KZoomFactors[z]);
	iReader->SetZoomFactorL(iZoomFactor);
	if (iPreview)
		{
		iPreview->SetZoomFactorL(iZoomFactor);
		iPrevUid = 0;                              // (set out again at this size)
		}
	LayoutNative();
	}

void CPmView::ZoomL(TInt aStep)
	{
	TInt z = (ZoomLevel(*iSettings) + aStep + KZoomLevels) % KZoomLevels;
	iSettings->iZoom = z + 1;
	((CPmAppUi*)iEikonEnv->EikAppUi())->SaveSettings();
	if (iFolderList)
		{
		ApplyZoomL();
		iReaderUid = 0;                          // lay the message out again
		iNativeMode = (TMode)-1;
		iMsgListSum = 0;
		if (NativeMode())
			{
			UpdateNativeL();
			DrawNow();
			}
		}
	}

void CPmView::LayoutNative()
	{
	TRect r = Rect();
	TBool folders = !(iSettings->iView & 4);
	iTitleH = (iSettings->iView & 2) ? 0 : iTitleFont->HeightInPixels() + 6;
	iHeadH = iBoldFont->HeightInPixels() + 6;
	if (iHeadH < KRowHeight[ZoomLevel(*iSettings)]) iHeadH = KRowHeight[ZoomLevel(*iSettings)];
	TInt top = r.iTl.iY + iTitleH;
	TInt listTop = top + iHeadH;
	TInt w = r.Width();
	// the folder list takes about 30% (at least room for "Archive (123)")
	iSplitX = 0;
	if (folders)
		{
		iSplitX = w * 3 / 10;
		if (iSplitX < 130) iSplitX = 130;
		}
	TInt msgX = r.iTl.iX + (folders ? iSplitX + 1 : 0);
	// (1.00) a tall screen showing the calendar: its calendars under the
	// folders, with their colours (CalLegend*), when there is room
	TInt foldersH = r.iBr.iY - listTop;
	iLegendRect = TRect();
	// ... and in the mail, the rest of today's events ("Today", CalToday*)
	if (folders && PmTall() && iMode == EList && iCal && iCal->iCal.enabled && !iCalLoaded)
		{
		TRAPD(ce, LoadCalendarL());
		(void)ce;
		}
	TInt panelRows = iMode == ECalendar ? iCalModel.ncal : (iMode == EList && iCal && iCal->iCal.enabled && iCalLoaded) ? 3 : 0;
	TInt folderRows = iFolderList->Model()->NumberOfItems();
	iLegendFor = folderRows;
	if (folders && PmTall() && panelRows > 0 && folderRows > 0)
		{
		TInt ih = iFolderList->ItemHeight();
		TInt need = (folderRows + 1) * ih;      // (the list's own margins: a row more, so the last shows)
		TInt legend = (panelRows + 1) * ih + 10;
		if (listTop + need + legend <= r.iBr.iY)
			{
			foldersH = need;
			iLegendRect = TRect(r.iTl.iX, listTop + need, r.iTl.iX + iSplitX, r.iBr.iY);
			}
		}
	TRAPD(e1, iFolderList->SetRectL(TRect(TPoint(r.iTl.iX, listTop), TSize(folders ? iSplitX : 1, foldersH))));
	// (0.96) on a tall screen the list has the top of its pane, and the
	// selected message is shown under it (pmpreview.cpp): whole rows, a
	// little under half the height
	TRect listRect(TPoint(msgX, listTop), r.iBr);
	iPrevRect = TRect();
	iPrevStrip = TRect();
	if (PreviewOn())
		{
		TInt rh = RowHeight();
		TInt rows = listRect.Height() * 45 / 100 / rh;
		if (rows < 3) rows = 3;
		if (rows * rh + 80 < listRect.Height())
			{
			listRect.iBr.iY = listTop + rows * rh;
			LayoutPreview(TRect(msgX, listRect.iBr.iY, r.iBr.iX, r.iBr.iY));
			}
		}
	TRAPD(e2a, iMsgList->SetRectL(listRect));
	TRAPD(e3, iReader->SetRectL(TRect(TPoint(r.iTl.iX, top), TPoint(r.iBr.iX - KScrollBarW, r.iBr.iY))));
	iBarRect = TRect(r.iBr.iX - KScrollBarW, top, r.iBr.iX, r.iBr.iY);
	(void)e1; (void)e2a; (void)e3;
	// columns: pictures, from, subject, date (dd/mm/yyyy, as the locale has it)
	TInt lw = r.iBr.iX - msgX - 30;            // (room for the scroll bar)
	TBuf<32> sample;
	TTime t(TDateTime(2026, EDecember, 27, 23, 58, 0, 0));
	TRAPD(e2, t.FormatL(sample, _L("%D%M%Y%/0%1%/1%2%/2%3%/3")));
	if (e2) sample = _L("28/12/2026");
	TInt date = iListFont->TextWidthInPixels(sample);
	TRAP(e2, t.FormatL(sample, _L("%-B%:0%J%:1%T%+B")));
	if (!e2 && iListFont->TextWidthInPixels(sample) > date)
		date = iListFont->TextWidthInPixels(sample);
	date += 10;
	TInt gap = 6;
	TInt rest = lw - KIconCol - 2 - date - gap * 2;
	TInt from = rest * 2 / 5;
	TInt subj = rest - from;
	if (subj < 20) subj = 20;
	if (from != iColFromW || subj != iColSubjW)
		{
		iColFromW = from;
		iColSubjW = subj;
		iMsgListSum = 0;                       // (the rows are clipped to the columns: build them again)
		}
	CColumnListBoxData* cd = iMsgList->Model()->ColumnData();
	TRAPD(err,
		cd->SetColumnWidthPixelL(0, KIconCol);
		cd->SetColumnWidthPixelL(1, 2);
		cd->SetColumnWidthPixelL(2, from);
		cd->SetColumnWidthPixelL(3, gap);
		cd->SetColumnWidthPixelL(4, subj);
		cd->SetColumnWidthPixelL(5, gap);
		cd->SetColumnWidthPixelL(6, date);
		);
	(void)err;
	// the headings line up with the columns
	iHeadX[0] = r.iTl.iX;
	iHeadX[1] = msgX;
	iHeadX[2] = msgX + KIconCol + 2 - 4;
	iHeadX[3] = iHeadX[2] + from + gap;
	iHeadX[4] = iHeadX[3] + subj + gap;
	}

static void ShowScrollBar(CEikScrollBarFrame* aFrame, TBool aShow)
	{
	if (!aFrame)
		return;
	CEikScrollBar* sb = aFrame->GetScrollBarHandle(CEikScrollBar::EVertical);
	if (sb && sb->IsVisible() != aShow)
		sb->MakeVisible(aShow);
	}

// a list's scroll bar: there when its rows don't all fit
static void ListScrollBar(CEikListBox* aList, TBool aShown)
	{
	TBool need = aShown && aList->Model()->NumberOfItems() * aList->ItemHeight() > aList->Rect().Height();
	ShowScrollBar(aList->ScrollBarFrame(), need);
	if (need)
		{
		TRAPD(err, aList->UpdateScrollBarsL());
		(void)err;
		}
	}

void CPmView::ShowNative(TBool aShow)
	{
	iNativeShown = aShow;
	if (iSettings->iView & 4)
		iSidebar = EFalse;                       // (no folder list to be in)
	TBool list = aShow && (iMode == EList || iMode == EOutbox);
	TBool reader = aShow && iMode == EMessage;
	TBool folders = (list || (aShow && (iMode == ENoAccount || iMode == ECalendar))) && !(iSettings->iView & 4);
	TBool msgs = list && iRows->Count() > 0;
	iFolderList->MakeVisible(folders);
	iMsgList->MakeVisible(msgs);
	iReader->MakeVisible(reader);
	if (iPreview)
		iPreview->MakeVisible(msgs && PreviewOn() && !iPrevRect.IsEmpty());
	// (the scroll bars are controls of their own: hide them with their owner)
	ListScrollBar(iFolderList, folders);
	ListScrollBar(iMsgList, msgs);

	iFolderList->SetFocus(folders && iSidebar, ENoDrawNow);
	iMsgList->SetFocus(list && !iSidebar, ENoDrawNow);
	iReader->SetFocus(reader, ENoDrawNow);
	if (reader)
		{
		// reading, not writing: no cursor; and the scroll bar up to date
		TRAPD(err, iReader->TextView()->SetCursorVisibilityL(TCursor::EFCursorInvisible, TCursor::EFCursorInvisible));
		(void)err;
		UpdateReaderBar();
		}
	}

// ----- filling the controls ---------------------------------------------------------

static TUint Checksum(TUint aSum, const TDesC& aText)
	{
	for (TInt i = 0; i < aText.Length(); i++)
		aSum = aSum * 31 + aText[i];
	return aSum;
	}

static TInt FolderIcon(TUint aKind)
	{
	switch (aKind)
		{
	case 'I': return EMbmInbox;
	case 'S': return EMbmSent;
	case 'D': return EMbmDrafts;
	case 'T': return EMbmTrash;
	case 'J': return EMbmJunk;
	case 'A': return EMbmArchive;
	default: return EMbmFolder;
		}
	}

// the tree: row 0 is the account, then its folders (and the Outbox) under
// it, then the Calendar. The list's row is the sidebar's entry + 1.
void CPmView::UpdateFolderListL()
	{
	CDesCArray* items = (CDesCArray*)iFolderList->Model()->ItemTextArray();
	CDesCArrayFlat* fresh = new(ELeave) CDesCArrayFlat(16);
	CleanupStack::PushL(fresh);
	CArrayFixFlat<TInt>* depth = new(ELeave) CArrayFixFlat<TInt>(16);
	CleanupStack::PushL(depth);
	CArrayFixFlat<TInt>* icon = new(ELeave) CArrayFixFlat<TInt>(16);
	CleanupStack::PushL(icon);
	TBuf<100> line;
	const PmAccount& acct = iSettings->iAccounts[iSettings->iAcct];
	if (acct.used && acct.name[0])
		{
		TPtrC8 n((const TUint8*)acct.name);
		line.Copy(Clip(n, 40));
		}
	else
		line = _L("Mail");
	// (1.18) more folders on the server than the list keeps (the standard
	// ones are always in it): said on the account's line
	if (iFoldersMore > 0)
		{
		if (line.Length() > 40) line.SetLength(40);
		line.AppendFormat(_L(" (%d not shown)"), iFoldersMore);
		}
	fresh->AppendL(line);
	depth->AppendL(0);
	icon->AppendL(EMbmFolderOpen);
	for (TInt i = 0; i < iFolders->Count(); i++)
		{
		const TPmFolder& f = (*iFolders)[i];
		// "Work/Projects": "Projects", a level down
		TInt d = 0, last = -1;
		if (f.iKind == '-' || f.iKind == 'N')
			for (TInt j = 0; j < f.iName.Length(); j++)
				if (f.iName[j] == '/' || f.iName[j] == '.') { d++; last = j; }
		line = Clip(f.iName.Mid(last + 1), 60);
		TBool counts = !(f.iKind == 'S' || f.iKind == 'D' || f.iKind == 'T' || f.iKind == 'J');
		if (counts && f.iUnread > 0)
			line.AppendFormat(_L(" (%d)"), f.iUnread);
		fresh->AppendL(line);
		depth->AppendL(1 + (d < 5 ? d : 5) + (counts && f.iUnread > 0 ? KTreeBold : 0));
		icon->AppendL(FolderIcon(f.iKind));
		}
	TInt ob = OutboxCount();
	line = _L("Outbox");
	if (ob) line.AppendFormat(_L(" (%d)"), ob);
	fresh->AppendL(line);
	depth->AppendL(1);
	icon->AppendL(EMbmOutbox);
	fresh->AppendL(_L("Calendar"));
	depth->AppendL(0);
	icon->AppendL(EMbmCalendar);
	// the lines: level l carries on below a row while a later row sits at
	// level l before anything shallower
	iTree->Reset();
	TInt n = depth->Count();
	for (TInt r = 0; r < n; r++)
		{
		TInt dr = (*depth)[r] & 7;
		TInt mask = 0;
		for (TInt l = 1; l <= dr; l++)
			for (TInt k = r + 1; k < n; k++)
				{
				if (((*depth)[k] & 7) < l) break;
				if (((*depth)[k] & 7) == l) { mask |= 1 << l; break; }
				}
		iTree->AppendL((*depth)[r] | ((*icon)[r] << 4) | (mask << 16));
		}
	CleanupStack::PopAndDestroy(2);         // icon, depth
	TBool same = items->Count() == fresh->Count();
	for (TInt k = 0; same && k < fresh->Count(); k++)
		same = (*items)[k] == (*fresh)[k];
	// (a changed count changes the text too, so bold follows the text)
	if (!same)
		{
		items->Reset();
		for (TInt k = 0; k < fresh->Count(); k++)
			items->AppendL((*fresh)[k]);
		iFolderList->HandleItemAdditionL();
		// (1.01) the panel under the folders follows their number
		if (PmTall() && iLegendFor != items->Count())
			{
			LayoutNative();
			DrawNow();
			}
		ListScrollBar(iFolderList, iNativeShown && iFolderList->IsVisible());
		}
	CleanupStack::PopAndDestroy();          // fresh
	TInt sel = (iSidebar ? iFolderSel : CurrentSidebarItem()) + 1;
	if (sel >= items->Count()) sel = items->Count() - 1;
	TBool moved = sel >= 0 && sel != iFolderList->CurrentItemIndex();
	if (moved)
		iFolderList->SetCurrentItemIndex(sel);
	if ((!same || moved) && iNativeShown && iFolderList->IsVisible())
		iFolderList->DrawNow();
	}

// dates as the built-in programs show them: today's as a time, others as
// the locale's date
void CPmView::FormatNativeDate(TInt aDate, TDes& aOut) const
	{
	aOut.Zero();
	if (aDate <= 0)
		return;
	TTime t(TDateTime(1970, EJanuary, 0, 0, 0, 0, 0));
	t += TTimeIntervalSeconds(aDate);
	t += TLocale().UniversalTimeOffset();
	TTime now;
	now.HomeTime();
	TDateTime d = t.DateTime();
	TDateTime n = now.DateTime();
	TBool today = d.Year() == n.Year() && d.Month() == n.Month() && d.Day() == n.Day();
	TRAPD(err, t.FormatL(aOut, today ? _L("%-B%:0%J%:1%T%+B") : _L("%D%M%Y%/0%1%/1%2%/2%3%/3")));
	if (err)
		FormatDate(aDate, aOut);
	}

static TInt MsgIcon(const TPmRow& aRow, TBool aOutbox)
	{
	TInt v;
	if (aOutbox)
		v = aRow.iFlags.Locate('E') >= 0 ? EMbmMsgError : aRow.iFlags.Locate('D') >= 0 ? EMbmMsgDraft : EMbmMsgOutbox;
	else if (aRow.iFlags.Locate('F') >= 0)
		v = EMbmMsgFlag;
	else if (aRow.iFlags.Locate('S') < 0)
		v = EMbmMsgUnread;
	else if (aRow.iFlags.Locate('A') >= 0)
		v = EMbmMsgReplied;
	else
		v = EMbmMsgRead;
	if (aRow.iFlags.Locate('T') >= 0)
		v |= KMsgAttach;
	if (!aOutbox && aRow.iFlags.Locate('S') < 0)
		v |= KMsgUnread;
	return v;
	}

void CPmView::UpdateMessageListL()
	{
	CDesCArray* items = (CDesCArray*)iMsgList->Model()->ItemTextArray();
	// only when something changed: a rebuild moves the list about
	TUint sum = iMode == EOutbox ? 7 : 3;
	for (TInt i = 0; i < iRows->Count(); i++)
		{
		const TPmRow& r = (*iRows)[i];
		sum = sum * 131 + r.iUid;
		sum = sum * 7 + r.iThread * 3 + r.iDepth;
		sum = Checksum(sum, r.iFlags);
		}
	sum = Checksum(sum, iFolder);
	TBool changed = items->Count() != iRows->Count() || iMsgListSum != sum;
	if (changed)
		{
		iMsgListSum = sum;
		items->Reset();
		iMsgIcons->Reset();
		TBuf<240> line;
		TBuf<32> date;
		for (TInt i = 0; i < iRows->Count(); i++)
			{
			const TPmRow& r = (*iRows)[i];
			if (iMode == EOutbox)
				date = r.iFlags.Locate('E') >= 0 ? _L("not sent") : r.iFlags.Locate('D') >= 0 ? _L("draft") : _L("to send");
			else
				FormatNativeDate(r.iDate, date);
			iMsgIcons->AppendL(MsgIcon(r, iMode == EOutbox));
			// what won't fit its column ends in "...", as the style guide has
			// it (and EIKON's own lists), in the row's font (bold when unread)
			const CFont* f = (!(iMode == EOutbox) && r.iFlags.Locate('S') < 0 && iBoldFont) ? iBoldFont : iListFont;
			TBuf<64> from;
			if (r.iDepth)
				from = _L("   ");                  // (1.05) a reply in an unfolded conversation
			from.Append(Clip(r.iFrom, 60 - from.Length()));
			if (f && iColFromW > 20) TextUtils::ClipToFit(from, *f, iColFromW - 6);
			TBuf<160> subj(r.iSubject.Length() ? Clip(r.iSubject, 140) : TPtrC(_L("(no subject)")));
			TBuf<12> count;
			if (r.iThread > 1)
				count.Format(_L(" (%d)"), r.iThread);   // (1.05) a folded conversation
			if (f && iColSubjW > 20 && count.Length())
				{
				TInt cw = f->TextWidthInPixels(count);
				if (iColSubjW - 6 - cw > 20) TextUtils::ClipToFit(subj, *f, iColSubjW - 6 - cw);
				}
			else if (f && iColSubjW > 20) TextUtils::ClipToFit(subj, *f, iColSubjW - 6);
			subj.Append(count);
			line = _L("\t\t");
			line.Append(from);
			line.Append(_L("\t\t"));
			line.Append(subj);
			line.Append(_L("\t\t"));
			line.Append(date);
			items->AppendL(line);
			}
		iMsgList->HandleItemAdditionL();
		ListScrollBar(iMsgList, iNativeShown && iMsgList->IsVisible());
		}
	if (iRows->Count())
		{
		TInt sel = iSel;
		if (sel >= iRows->Count()) sel = iRows->Count() - 1;
		if (sel < 0) sel = 0;
		if (sel != iMsgList->CurrentItemIndex())
			{
			iMsgList->SetCurrentItemIndex(sel);
			changed = ETrue;
			}
		}
	if (changed && iNativeShown && iMsgList->IsVisible())
		iMsgList->DrawNow();
	}

// ----- the order of the message list -------------------------------------------------

// 0 newest first (the store's order, reversed), 1 oldest first, 2/3 from
// A-Z/Z-A, 4/5 subject A-Z/Z-A, 6 unread first
void CPmView::SortRows()
	{
	TInt mode = iSettings->iSort;
	TInt n = iRows->Count();
	if (mode <= 0 || mode > 6 || n < 2)
		return;
	if (mode >= 2 && mode <= 5)
		{
		TKeyArrayFix key(mode <= 3 ? _FOFF(TPmRow, iFrom) : _FOFF(TPmRow, iSubject), ECmpFolded);
		iRows->Sort(key);
		}
	if (mode == 6)
		{
		// unread first, each part newest first (a stable split)
		CArrayFixFlat<TPmRow>* tmp = new CArrayFixFlat<TPmRow>(n);
		if (!tmp)
			return;
		TRAPD(err,
			for (TInt pass = 0; pass < 2; pass++)
				for (TInt i = 0; i < n; i++)
					if (((*iRows)[i].iFlags.Locate('S') < 0) == (pass == 0))
						tmp->AppendL((*iRows)[i]);
			);
		if (!err)
			for (TInt i = 0; i < n; i++)
				(*iRows)[i] = (*tmp)[i];
		delete tmp;
		return;
		}
	if (mode == 1 || mode == 3 || mode == 5)
		for (TInt i = 0; i < n / 2; i++)
			{
			TPmRow t = (*iRows)[i];
			(*iRows)[i] = (*iRows)[n - 1 - i];
			(*iRows)[n - 1 - i] = t;
			}
	}

void CPmView::SortL(TInt aMode)
	{
	if (aMode < 0 || aMode > 6)
		aMode = 0;
	iSettings->iSort = aMode;
	((CPmAppUi*)iEikonEnv->EikAppUi())->SaveSettings();
	if (iMode != EList)
		return;
	LoadListL();                               // (sorts, and keeps the selection)
	iMsgListSum = 0;
	Render();
	if (iNativeShown)
		{
		ActivateGc();
		DrawHeaders(SystemGc());
		DeactivateGc();
		}
	}

// ----- (1.05) conversations ------------------------------------------------------------

// View > Show > Conversations, in a folder's list (not Find's results, not
// the Outbox) while it is in date order
TBool CPmView::ThreadsOn() const
	{
	return (iSettings->iView & KPmViewThreads) && iMode != EOutbox && !iSearch &&
		(iSettings->iSort == 0 || iSettings->iSort == 1);
	}

// how many messages the list holds, and how many unread, a folded
// conversation's hidden ones too
void CPmView::CountRows(TInt& aTotal, TInt& aUnread) const
	{
	aTotal = aUnread = 0;
	for (TInt i = 0; i < iRows->Count(); i++)
		{
		const TPmRow& r = (*iRows)[i];
		if (r.iThread > 1)
			{
			aTotal += r.iThread;
			aUnread += r.iThreadUnread;
			}
		else
			{
			aTotal++;
			if (r.iFlags.Locate('S') < 0) aUnread++;
			}
		}
	}

static void ShellSortHash(TUint* aKey, TInt* aIdx, TInt aN)
	{
	for (TInt gap = aN / 2; gap > 0; gap /= 2)
		for (TInt i = gap; i < aN; i++)
			{
			TUint k = aKey[i];
			TInt x = aIdx[i];
			TInt j = i;
			for (; j >= gap && aKey[j - gap] > k; j -= gap)
				{
				aKey[j] = aKey[j - gap];
				aIdx[j] = aIdx[j - gap];
				}
			aKey[j] = k;
			aIdx[j] = x;
			}
	}

// The rows in date order become conversations: a message whose
// In-Reply-To is another's Message-ID goes under that one, and each goes
// with the first message of its chain. A conversation stands where its
// newest message (oldest, in oldest-first order) would be. Folded (the
// standard), it is one row - its newest message's, with how many there are
// and bold if any is unread; unfolded (Right or Enter), the first message
// and then the replies, oldest first, indented. aKeep: the message to keep
// selected (its conversation's row, if that is folded).
void CPmView::ThreadRowsL(TUint aKeep)
	{
	TInt n = iRows->Count();
	if (!ThreadsOn() || n < 2)
		return;
	TInt* parent = (TInt*)User::AllocLC(n * 5 * sizeof(TInt));
	TInt* root = parent + n;
	TInt* size = root + n;
	TInt* idx = size + n;
	TUint* key = (TUint*)(idx + n);
	TInt i;
	for (i = 0; i < n; i++)
		{
		key[i] = (*iRows)[i].iIdHash;
		idx[i] = i;
		parent[i] = -1;
		size[i] = 0;
		}
	ShellSortHash(key, idx, n);
	for (i = 0; i < n; i++)
		{
		TUint h = (*iRows)[i].iReplyHash;
		if (!h)
			continue;
		TInt lo = 0, hi = n - 1;
		while (lo < hi)
			{
			TInt mid = (lo + hi) / 2;
			if (key[mid] < h) lo = mid + 1; else hi = mid;
			}
		if (key[lo] == h && idx[lo] != i)
			parent[i] = idx[lo];
		}
	TBool any = EFalse;
	for (i = 0; i < n; i++)
		{
		TInt r = i;
		for (TInt step = 0; step < 64 && parent[r] >= 0; step++)
			r = parent[r];                     // (a loop of replies stops after 64)
		root[i] = r;
		size[r]++;
		if (r != i) any = ETrue;
		}
	if (!any)
		{
		CleanupStack::PopAndDestroy();          // parent
		return;
		}
	CArrayFixFlat<TPmRow>* out = new(ELeave) CArrayFixFlat<TPmRow>(n);
	CleanupStack::PushL(out);
	TInt keepAt = -1;
	for (i = 0; i < n; i++)
		{
		TInt r = root[i];
		if (size[r] < 0)
			continue;                           // that conversation is out already
		if (size[r] == 1)
			{
			out->AppendL((*iRows)[i]);
			if ((*iRows)[i].iUid == aKeep) keepAt = out->Count() - 1;
			continue;
			}
		TUint rootUid = (*iRows)[r].iUid;
		TBool open = EFalse;
		for (TInt o = 0; o < iOpenThreads; o++)
			if (iOpenThread[o] == rootUid) open = ETrue;
		if (!open)
			{
			// one row: this conversation's first in the list's order (its
			// newest, newest first), with the count and the others' marks
			TPmRow row = (*iRows)[i];
			TInt unread = 0;
			TBool flag = EFalse, att = EFalse, hasKeep = EFalse;
			for (TInt k = 0; k < n; k++)
				if (root[k] == r)
					{
					const TPmRow& m = (*iRows)[k];
					if (m.iFlags.Locate('S') < 0) unread++;
					if (m.iFlags.Locate('F') >= 0) flag = ETrue;
					if (m.iFlags.Locate('T') >= 0) att = ETrue;
					if (m.iUid == aKeep) hasKeep = ETrue;
					iThreadUids->AppendL(rootUid);   // (what the row stands for: Delete, Move...)
					iThreadUids->AppendL(m.iUid);
					}
			TInt s = row.iFlags.Locate('S');
			if (unread && s >= 0) row.iFlags.Delete(s, 1);
			if (!unread && s < 0 && row.iFlags.Length() < row.iFlags.MaxLength()) row.iFlags.Append('S');
			if (flag && row.iFlags.Locate('F') < 0 && row.iFlags.Length() < row.iFlags.MaxLength()) row.iFlags.Append('F');
			if (att && row.iFlags.Locate('T') < 0 && row.iFlags.Length() < row.iFlags.MaxLength()) row.iFlags.Append('T');
			row.iRoot = rootUid;
			row.iDepth = 0;
			row.iThread = size[r];
			row.iThreadUnread = unread;
			out->AppendL(row);
			if (hasKeep) keepAt = out->Count() - 1;
			}
		else
			{
			// the first message, then the replies oldest first (the list
			// is newest first or oldest first: walk it the right way)
			TPmRow first = (*iRows)[r];
			first.iRoot = rootUid;
			first.iDepth = 0;
			out->AppendL(first);
			if (first.iUid == aKeep) keepAt = out->Count() - 1;
			TBool newestFirst = iSettings->iSort == 0;
			for (TInt k = 0; k < n; k++)
				{
				TInt kk = newestFirst ? n - 1 - k : k;
				if (root[kk] != r || kk == r)
					continue;
				TPmRow m = (*iRows)[kk];
				m.iRoot = rootUid;
				m.iDepth = 1;
				out->AppendL(m);
				if (m.iUid == aKeep) keepAt = out->Count() - 1;
				}
			}
		size[r] = -1;
		}
	iRows->Reset();
	for (i = 0; i < out->Count(); i++)
		iRows->AppendL((*out)[i]);
	CleanupStack::PopAndDestroy(2);            // out, parent
	iSel = keepAt >= 0 ? keepAt : 0;
	}

// Right (or Enter, or a second tap) on a folded conversation unfolds it;
// Left on one of an unfolded conversation's messages folds it. EFalse:
// not a conversation, the key does what it did
TBool CPmView::ThreadKeyL(TBool aOpen)
	{
	if (!ThreadsOn() || iSidebar || iSel < 0 || iSel >= iRows->Count())
		return EFalse;
	const TPmRow& r = (*iRows)[iSel];
	if (!r.iRoot)
		return EFalse;
	TUint root = r.iRoot;
	TInt at = -1;
	for (TInt o = 0; o < iOpenThreads; o++)
		if (iOpenThread[o] == root) at = o;
	if (aOpen)
		{
		if (r.iThread <= 1)
			return EFalse;                     // unfolded already: Right opens the message
		if (iOpenThreads == 16)
			{
			for (TInt o = 1; o < 16; o++) iOpenThread[o - 1] = iOpenThread[o];
			iOpenThreads--;
			}
		iOpenThread[iOpenThreads++] = root;
		}
	else
		{
		if (at < 0)
			return EFalse;                     // folded: Left goes to the folders
		for (TInt o = at + 1; o < iOpenThreads; o++) iOpenThread[o - 1] = iOpenThread[o];
		iOpenThreads--;
		}
	// unfolded: the first message selected; folded: the conversation's row
	TUint keep = aOpen ? root : r.iUid;
	if (aOpen)
		(*iRows)[iSel].iUid = root;            // (so LoadListL keeps the first one)
	else
		(*iRows)[iSel].iUid = keep;
	LoadListL();
	iMsgListSum = 0;
	Render();
	PreviewSyncL();
	return ETrue;
	}

void CPmView::ToggleThreadsL()
	{
	iSettings->iView ^= KPmViewThreads;
	((CPmAppUi*)iEikonEnv->EikAppUi())->SaveSettings();
	if (iMode != EList)
		return;
	iOpenThreads = 0;
	LoadListL();
	iMsgListSum = 0;
	Render();
	if (iNativeShown)
		{
		ActivateGc();
		DrawHeaders(SystemGc());
		DeactivateGc();
		}
	if ((iSettings->iView & KPmViewThreads) && !ThreadsOn())
		Toast(_L("Conversations show when the list is in date order (View > Sort)"));
	}

// ----- (1.05) several messages at once ----------------------------------------------

TInt CPmView::MarkedCount() const
	{
	return iMarkLo >= 0 && iMarkHi > iMarkLo ? iMarkHi - iMarkLo + 1 : 1;
	}

void CPmView::ClearMark()
	{
	TBool was = iMarkLo >= 0;
	iMarkFrom = iMarkLo = iMarkHi = -1;
	if (was && iMsgList && iNativeShown && iMsgList->IsVisible())
		iMsgList->DrawDeferred();
	}

// the messages chosen (the selected one if no run), a folded
// conversation's row as all of its messages
void CPmView::MarkedUidsL(CArrayFixFlat<TUint>& aUids) const
	{
	TInt lo = iSel, hi = iSel;
	if (iMarkLo >= 0 && iMarkHi > iMarkLo) { lo = iMarkLo; hi = iMarkHi; }
	for (TInt i = lo; i <= hi && i >= 0 && i < iRows->Count(); i++)
		{
		const TPmRow& r = (*iRows)[i];
		if (r.iThread > 1)
			{
			for (TInt k = 0; k + 1 < iThreadUids->Count(); k += 2)
				if ((*iThreadUids)[k] == r.iRoot)
					aUids.AppendL((*iThreadUids)[k + 1]);
			}
		else
			aUids.AppendL(r.iUid);
		}
	}

// one command for each of aUids (PM_CMD_MOVE or PM_CMD_FLAG), in the open
// folder; sent as the engine's queue has room
TBool CPmView::BatchL(TInt aOp, const TDesC8& aArg, CArrayFixFlat<TUint>& aUids)
	{
	if (iBatch->Count())
		{
		Toast(_L("Not available at this time"));   // (the last lot is still going)
		return EFalse;
		}
	iBatchOp = aOp;
	iBatchFolder = iFolder;
	iBatchArg = aArg.Left(Min(aArg.Length(), iBatchArg.MaxLength()));
	for (TInt i = 0; i < aUids.Count(); i++)
		iBatch->AppendL(aUids[i]);
	PumpBatchL();
	return ETrue;
	}

void CPmView::PumpBatchL()
	{
	while (iBatch->Count() && iShared && iShared->cmd_head - iShared->cmd_tail < PM_CMDQ - 2)
		{
		TUint uid = (*iBatch)[0];
		iBatch->Delete(0);
		iBatchQuiet++;
		Cmd(iBatchOp, iBatchFolder, uid, iBatchArg);
		}
	}

// Delete (aDelete: to the Trash, or for good from the Trash) or Move of
// the messages chosen
TBool CPmView::BatchMoveL(const TDesC8& aDest, TBool aDelete)
	{
	CArrayFixFlat<TUint>* uids = new(ELeave) CArrayFixFlat<TUint>(16);
	CleanupStack::PushL(uids);
	MarkedUidsL(*uids);
	TInt n = uids->Count();
	if (!n)
		{
		CleanupStack::PopAndDestroy();
		return EFalse;
		}
	const TPmFolder* f = CurrentFolder();
	TBool forGood = aDelete && f && f->iKind == 'T';
	TBuf<40> fact;
	fact.Format(_L("%d messages chosen"), n);
	if (aDelete && !iEikonEnv->QueryWinL(fact, forGood ? _L("Delete them for good?") : _L("Move them to the Trash?")))
		{
		CleanupStack::PopAndDestroy();
		return EFalse;
		}
	if (!BatchL(PM_CMD_MOVE, aDest, *uids))
		{
		CleanupStack::PopAndDestroy();
		return EFalse;
		}
	if (!forGood)
		NoteUndoL((*uids)[n - 1]);              // (Edit > Undo puts back the last of them)
	CleanupStack::PopAndDestroy();             // uids
	// the rows go now (the engine's index follows)
	TInt lo = iSel, hi = iSel;
	if (iMarkLo >= 0 && iMarkHi > iMarkLo) { lo = iMarkLo; hi = iMarkHi; }
	if (lo >= 0 && hi < iRows->Count())
		iRows->Delete(lo, hi - lo + 1);
	ClearMark();
	iSel = lo;
	if (iSel >= iRows->Count()) iSel = iRows->Count() - 1;
	if (iSel < 0) iSel = 0;
	TBuf<60> t;
	if (aDelete) t.Format(forGood ? _L("%d deleted") : _L("%d moved to the Trash"), n);
	else t.Format(_L("%d moved"), n);
	Toast(t);
	EnsureVisible();
	iMsgListSum = 0;
	Render();
	return ETrue;
	}

// (1.05) a message (or the ones chosen) dragged from the list and let go
// on a folder in the tree: moved there. EFalse: not over a folder
TBool CPmView::DropOnFolderL(const TPoint& aPos, TInt aRow)
	{
	if (iMode != EList || !iFolderList || !iFolderList->IsVisible() || !iFolderList->Rect().Contains(aPos))
		return EFalse;
	// the message the pen went down on (the run chosen, if it is one of them)
	if (aRow >= 0 && aRow < iRows->Count() && !(iMarkLo >= 0 && aRow >= iMarkLo && aRow <= iMarkHi))
		{
		ClearMark();
		iSel = aRow;
		iMsgList->SetCurrentItemIndex(aRow);
		}
	TInt idx = -1;
	if (!iFolderList->View() || !iFolderList->View()->XYPosToItemIndex(aPos, idx) || idx <= 0)
		return EFalse;
	TInt fi = idx - 1;                         // (row 0 is the account)
	if (fi >= iFolders->Count())
		{
		Toast(_L("Messages can be moved to a folder only"));
		return ETrue;
		}
	const TPmFolder& f = (*iFolders)[fi];
	if (f.iKind == 'N')
		{
		Toast(_L("That folder holds only other folders"));
		return ETrue;
		}
	TBuf<64> name(f.iName);
	TBuf8<128> dest(f.iImap);
	TBool several = MarkedCount() > 1 || (iSel >= 0 && iSel < iRows->Count() && (*iRows)[iSel].iThread > 1);
	if (MoveCurrentL(dest) && !several)
		{
		TInt last = -1;
		for (TInt j = 0; j < name.Length(); j++)
			if (name[j] == '/') last = j;
		TBuf<80> t(_L("Moved to "));
		t.Append(Clip(name.Mid(last + 1), 60));
		Toast(t);
		}
	return ETrue;
	}

// ----- the reader -------------------------------------------------------------------

// character and paragraph styles collected while the text is put together
struct TPmSpan { TInt iPos; TInt iLen; TInt iKind; TInt iArg; };
enum { ESpanBold, ESpanItalic, ESpanUnder, ESpanBig, ESpanMono, ESpanIndent, ESpanSpaceBefore, ESpanRuleBelow,
	ESpanColour };      // (0.96) iArg: a TRgb's value (colour screens: links, quoted text)

static void AddSpan(CArrayFixFlat<TPmSpan>& aSpans, TInt aPos, TInt aLen, TInt aKind, TInt aArg = 0)
	{
	if (aLen <= 0)
		return;
	TPmSpan s;
	s.iPos = aPos; s.iLen = aLen; s.iKind = aKind; s.iArg = aArg;
	TRAPD(err, aSpans.AppendL(s));
	(void)err;
	}

// (1.05) under a picture that can't be shown (its frame says why): a line
// "Help on pictures", a link to the Help topic that says what PsiMail
// reads and what can show the rest
void CPmView::PictureHelpLinkL(TInt aEntry, TDes& aText, CArrayFixFlat<TPmSpan>& aSpans)
	{
	const TPmPicEntry& pe = iPictures->At(aEntry);
	if (pe.iState != TPmPicEntry::EFailed || pe.iWhy == _L("spacer") || aText.Length() > aText.MaxLength() - 40)
		return;
	TPmLinkRange lr;
	lr.iPos = aText.Length();
	aText.Append(_L("Help on pictures"));
	lr.iLen = aText.Length() - lr.iPos;
	lr.iLink = KPmLinkPictureHelp;
	iLinks->AppendL(lr);
	AddSpan(aSpans, lr.iPos, lr.iLen, ESpanUnder);
	AddSpan(aSpans, lr.iPos, lr.iLen, ESpanBig, 850);
	aText.Append(TChar(CEditableText::EParagraphDelimiter));
	}

void CPmView::UpdateReaderL()
	{
	const TPmRow* row = CurrentRow();
	TBool waiting = iWaitingBody || !iText;
	if (iReaderUid == iMsgUid && iReaderWaiting == waiting && iReaderError == iBodyError && iNativeMode == EMessage)
		return;
	iReaderError = iBodyError;
	iReaderUid = iMsgUid;
	iReaderWaiting = waiting;
	iLinks->Reset();
	iLinkSel = -1;
	// the message's pictures: what the engine listed, and what it has decoded
	TInt picPref = PicturesPref();
	if (picPref < 0 || picPref > 2) picPref = 0;
	{
	TFileName dir;
	FolderDir(iFolder, dir);
	iPictures->LoadIndexL(iCoeEnv->FsSession(), dir, iMsgUid);
	for (TInt k = 0; k < iPictures->Count(); k++)
		{
		TRAPD(le, iPictures->LoadReadyL(iCoeEnv->FsSession(), k));
		(void)le;
		iPictures->At(k).iInline = EFalse;
		}
	}

	HBufC* buf = HBufC::NewLC((iText ? iText->Length() : 0) + 4096);   // (headers, and a line per picture)
	TPtr t = buf->Des();
	CArrayFixFlat<TPmSpan>* spans = new(ELeave) CArrayFixFlat<TPmSpan>(64);
	CleanupStack::PushL(spans);
	const TChar KPara(CEditableText::EParagraphDelimiter);
	TInt p0;

	// the header: subject, sender and date, recipients, attachments, and a
	// rule (pmheader.cpp). Lines that belong under it go in here, after it
	// (HeaderEnd() is where it stops)
	HeaderTextL(t, row);

	// an invitation or a contact card: what it is, and the choices as links
	// (pminvite.cpp): styles come back as (position, length, kind) triples
	{
	CArrayFixFlat<TInt>* st = new(ELeave) CArrayFixFlat<TInt>(12);
	CleanupStack::PushL(st);
	InviteBannerL(t, *st);
	static const TInt KKinds[5] = { ESpanBold, ESpanUnder, ESpanBig, ESpanItalic, ESpanRuleBelow };
	for (TInt b = 0; b + 2 < st->Count(); b += 3)
		{
		TInt kind = (*st)[b + 2];
		if (kind >= 0 && kind < 5)
			AddSpan(*spans, (*st)[b], (*st)[b + 1], KKinds[kind], kind == 2 ? 1150 : 0);
		}
	CleanupStack::PopAndDestroy();           // st
	}

	// (0.75) pictures from the web: not fetched unless asked for (pmwebpic.cpp)
	if (!waiting)
		{
		TInt wl, wk;
		WebPicturesLineL(t, *iLinks, wl, wk);
		if (wl >= 0)
			{
			AddSpan(*spans, wl, t.Length() - 1 - wl, ESpanItalic);
			AddSpan(*spans, t.Length() - 1 - wk, wk, ESpanUnder);
			}
		}

	if (waiting)
		{
		if (iBodyError.Length())
			{
			t.Append(iBodyError);
			t.Append(KPara);
			t.Append(_L("Press Enter on it in the list to try again"));
			}
		else
			t.Append(iSettings->iOffline ? _L("Not downloaded - you are working offline")
				: _L("Downloading the message\x85"));
		t.Append(KPara);
		}
	else
		{
		// the body: after the header lines and the blank line
		TPtrC rest = iText->Mid(iBodyOff);
		while (rest.Length())
			{
			TInt nl = rest.Locate('\n');
			TPtrC line = nl >= 0 ? rest.Left(nl) : rest;
			rest.Set(nl >= 0 ? rest.Mid(nl + 1) : TPtrC());
			if (!line.Length())
				break;
			}
		TInt bold = -1, ital = -1, link = -1, linkNo = 0;
		TBool lastEmpty = ETrue;                  // (0.75) one empty line at a time, none at the start
		while (rest.Length() && t.Length() < t.MaxLength() - 400)
			{
			TInt nl = rest.Locate('\n');
			TPtrC line = nl >= 0 ? rest.Left(nl) : rest;
			rest.Set(nl >= 0 ? rest.Mid(nl + 1) : TPtrC());
			if (line.Length() && line[line.Length() - 1] == '\r')
				line.Set(line.Left(line.Length() - 1));
			TInt paraStart = t.Length();
			TInt kind = 0, arg = 0;
			if (line.Length() >= 2 && line[0] == 0x01)
				{
				TChar k = line[1];
				TPtrC body = line.Mid(2);
				switch (k)
					{
				case 'u':
					continue;                     // a link's address: not shown
				case 'r':
					for (TInt d = 0; d < 24; d++) t.Append(TChar(0x97));
					t.Append(KPara);
					continue;
				case 'h':
					kind = 'h';
					arg = body.Length() ? body[0] - '0' : 2;
					body.Set(body.Length() ? body.Mid(1) : body);
					break;
				case 'l':
					{
					kind = 'l';
					arg = body.Length() ? body[0] - '0' : 1;
					TInt m = body.Locate(0x02);
					TPtrC marker = m > 1 ? body.Mid(1, m - 1) : TPtrC(_L("\x95"));
					body.Set(m >= 0 ? body.Mid(m + 1) : body.Mid(body.Length() ? 1 : 0));
					t.Append(Clip(marker, 16));
					t.Append(' ');
					break;
					}
				case 'q':
					kind = 'q';
					arg = body.Length() ? body[0] - '0' : 1;
					body.Set(body.Length() ? body.Mid(1) : body);
					break;
				case 'c': kind = 'c'; break;
				case 'i':
					{
					// alt text, then the address: a "cid:" part of the
					// message is shown as itself, anything else as words
					// (nothing is fetched from the web: see pmpict.h)
					kind = 'i';
					TInt sep = body.Locate(0x02);
					TPtrC alt = sep >= 0 ? body.Left(sep) : body;
					TPtrC src = sep >= 0 ? body.Mid(sep + 1) : TPtrC();
					while (alt.Length() && alt[0] == ' ') alt.Set(alt.Mid(1));
					// (0.75) the HTML's width and height: "src \x02 WxH"
					TInt hintW = 0, hintH = 0;
					TInt sep2 = src.Locate(0x02);
					if (sep2 >= 0)
						{
						ParsePictureSize(src.Mid(sep2 + 1), hintW, hintH);
						src.Set(src.Left(sep2));
						}
					TInt e = -1;
					if (picPref == 0 && src.Length() > 4 && src.Left(4).CompareF(_L("cid:")) == 0)
						e = iPictures->FindCid(src.Mid(4));
					else if (picPref == 0 && src.Length() > 7 && src.Left(4).CompareF(_L("http")) == 0)
						{
						// from the web: shown once asked for (pmwebpic.cpp); till
						// then the line at the top says so, and a picture with no
						// words of its own takes no room
						if (WebPicturesOn())
							{
							e = iPictures->AddWebL(src, hintW, hintH);
							if (e >= 0 && iPictures->At(e).iState == TPmPicEntry::EUnknown)
								{
								TRAPD(le, iPictures->LoadReadyL(iCoeEnv->FsSession(), e));
								(void)le;
								if (iPictures->At(e).iState == TPmPicEntry::EUnknown && OpInFlight(PM_CMD_WEBPICS))
									iPictures->At(e).iState = TPmPicEntry::EWaiting;
								}
							if (e >= 0 && iPictures->At(e).iState == TPmPicEntry::EFailed && iPictures->At(e).iWhy == _L("spacer"))
								continue;          // (a spacer after all: nothing)
							}
						else if (!alt.Length())
							continue;
						}
					if (e >= 0 && hintW + hintH > 0 && !iPictures->At(e).iHintW && !iPictures->At(e).iHintH)
						{
						iPictures->At(e).iHintW = hintW;
						iPictures->At(e).iHintH = hintH;
						}
					if (e >= 0 && t.Length() < t.MaxLength() - 4)
						{
						// the picture on a line of its own (a space stands
						// in for it until the text is in the editor)
						iPictures->At(e).iInline = ETrue;
						iPictures->AddPlaceL(e, t.Length());
						t.Append(' ');
						t.Append(KPara);
						PictureHelpLinkL(e, t, *spans);
						continue;
						}
					t.Append(_L("[Picture"));
					if (alt.Length()) t.Append(_L(": "));
					body.Set(alt);
					break;
					}
				case 's': kind = 's'; break;
				default: break;                   // 'p': a paragraph
					}
				while (body.Length() && body[0] == ' ' && kind != 'c') body.Set(body.Mid(1));
				line.Set(body);
				}
			// inline styles and links
			for (TInt i = 0; i < line.Length(); i++)
				{
				TText c = line[i];
				switch (c)
					{
				case 0x11: bold = t.Length(); break;
				case 0x12: if (bold >= 0) AddSpan(*spans, bold, t.Length() - bold, ESpanBold); bold = -1; break;
				case 0x13: ital = t.Length(); break;
				case 0x14: if (ital >= 0) AddSpan(*spans, ital, t.Length() - ital, ESpanItalic); ital = -1; break;
				case 0x15:
					{
					linkNo = 0;
					while (i + 1 < line.Length() && line[i + 1] >= '0' && line[i + 1] <= '9')
						linkNo = linkNo * 10 + (line[++i] - '0');
					if (i + 1 < line.Length() && line[i + 1] == 0x16) i++;
					link = t.Length();
					break;
					}
				case 0x17:
					if (link >= 0 && linkNo > 0 && t.Length() > link)
						{
						TPmLinkRange lr;
						lr.iPos = link; lr.iLen = t.Length() - link; lr.iLink = linkNo;
						iLinks->AppendL(lr);
						AddSpan(*spans, link, lr.iLen, ESpanUnder);
						if (PmColour())
							AddSpan(*spans, link, lr.iLen, ESpanColour, PmLinkColour().Value());
						}
					link = -1;
					break;
				default:
					// (never past the end: the buffer is the text and 4096
					// more, and a message of many rules - 24 characters for
					// 3 - then one long line used to overrun it: USER 11)
					if ((c >= 0x20 || c == '\t') && t.Length() < t.MaxLength() - 8)
						t.Append(c);
					break;
					}
				}
			if (kind == 'i') t.Append(']');
			// close styles still open at the end of the line
			if (bold >= 0) { AddSpan(*spans, bold, t.Length() - bold, ESpanBold); bold = t.Length() + 1; }
			if (ital >= 0) { AddSpan(*spans, ital, t.Length() - ital, ESpanItalic); ital = t.Length() + 1; }
			TInt plen = t.Length() - paraStart;
			switch (kind)
				{
			case 'h':
				AddSpan(*spans, paraStart, plen, ESpanBold);
				AddSpan(*spans, paraStart, plen, ESpanBig, arg == 1 ? 1500 : arg == 2 ? 1300 : 1150);
				AddSpan(*spans, paraStart, plen, ESpanSpaceBefore, 1);
				break;
			case 'l': AddSpan(*spans, paraStart, plen, ESpanIndent, arg * 2); break;
			case 'q':
				AddSpan(*spans, paraStart, plen, ESpanIndent, arg * 2);
				AddSpan(*spans, paraStart, plen, ESpanItalic);
				if (PmColour())
					AddSpan(*spans, paraStart, plen, ESpanColour, PmQuoteColour(arg).Value());
				break;
			case 'c': AddSpan(*spans, paraStart, plen, ESpanMono); break;
			case 'i':
			case 's':
				AddSpan(*spans, paraStart, plen, ESpanItalic);
				break;
			default: break;
				}
			if (plen == 0 && kind == 0)
				{
				if (lastEmpty)
					continue;
				lastEmpty = ETrue;
				}
			else
				lastEmpty = EFalse;
			t.Append(KPara);
			}
		if (lastEmpty && t.Length() >= 2 && t[t.Length() - 1] == KPara && t[t.Length() - 2] == KPara)
			t.SetLength(t.Length() - 1);          // (no empty line at the end)
		// pictures that came as files (not placed by the HTML): after the
		// text, each with its name; a big one waits for a tap
		for (TInt e = 0; picPref != 2 && e < iPictures->Count() && t.Length() < t.MaxLength() - 300; e++)
			{
			TPmPicEntry& pe = iPictures->At(e);
			if (pe.iInline)
				continue;
			TBuf<16> z;
			CPmPictures::SizeText(pe.iSize, z);
			if (pe.iState == TPmPicEntry::EReady || pe.iState == TPmPicEntry::EFailed ||
				pe.iState == TPmPicEntry::EWaiting || pe.iSize <= PM_PIC_AUTO_KB * 1024)
				{
				iPictures->AddPlaceL(e, t.Length());
				t.Append(' ');
				t.Append(KPara);
				p0 = t.Length();
				t.Append(pe.iName.Length() ? Clip(pe.iName, 60) : TPtrC(_L("picture")));
				t.Append(_L(" ("));
				t.Append(z);
				t.Append(')');
				AddSpan(*spans, p0, t.Length() - p0, ESpanItalic);
				AddSpan(*spans, p0, t.Length() - p0, ESpanBig, 850);
				t.Append(KPara);
				PictureHelpLinkL(e, t, *spans);
				}
			else
				{
				pe.iState = TPmPicEntry::ETooBig;
				TPmLinkRange lr;
				lr.iPos = t.Length();
				t.Append(_L("[Picture: "));
				t.Append(pe.iName.Length() ? Clip(pe.iName, 60) : TPtrC(_L("picture")));
				t.Append(_L(", "));
				t.Append(z);
				if (pe.iSize > PM_PIC_MAX_KB * 1024) t.Append(_L(" - too big to download]"));
				else t.Append(_L(" - not downloaded, tap to get it]"));
				lr.iLen = t.Length() - lr.iPos;
				lr.iLink = -2000 - e;
				if (pe.iSize <= PM_PIC_MAX_KB * 1024)
					{
					iLinks->AppendL(lr);
					AddSpan(*spans, lr.iPos, lr.iLen, ESpanUnder);
					}
				AddSpan(*spans, lr.iPos, lr.iLen, ESpanItalic);
				t.Append(KPara);
				}
			}
		if (iTruncated > 0)
			{
			TBuf<120> m;
			m.Format(_L("[%d KB more not downloaded - Message > Get whole message]"), (iTruncated + 1023) / 1024);
			p0 = t.Length();
			t.Append(m);
			AddSpan(*spans, p0, t.Length() - p0, ESpanItalic);
			t.Append(KPara);
			}
		}

	// the pictures not decoded yet: asked for now, so their frames say so
	// (the engine's answer comes through TickL and RefreshPicturesL)
	if (!waiting)
		{
		AskForPicturesL();
		AskForWebPicturesL();                 // (Web pictures: Always - pmwebpic.cpp)
		}

	// into the editor, then the styles
	CRichText* rt = iReader->RichText();
	rt->Reset();
	rt->InsertL(0, t);
	// the built-in programs' text: Arial, 10 point (the zoom scales it)
	const TInt baseTwips = 200;
	{
	TCharFormat cf(_L("Arial"), baseTwips);
	TCharFormatMask cm;
	cm.SetAttrib(EAttFontTypeface);
	cm.SetAttrib(EAttFontHeight);
	if (PmColour())
		{
		// (0.96) the workspace's colours, as the lists beside it
		cf.iFontPresentation.iTextColor = PmText();
		cm.SetAttrib(EAttColor);
		}
	rt->ApplyCharFormatL(cf, cm, 0, rt->DocumentLength());
	}
	for (TInt s = 0; s < spans->Count(); s++)
		{
		const TPmSpan& sp = (*spans)[s];
		if (sp.iPos + sp.iLen > rt->DocumentLength())
			continue;
		TCharFormat cf;
		TCharFormatMask cm;
		switch (sp.iKind)
			{
		case ESpanBold:
			cf.iFontSpec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
			cm.SetAttrib(EAttFontStrokeWeight);
			break;
		case ESpanItalic:
			cf.iFontSpec.iFontStyle.SetPosture(EPostureItalic);
			cm.SetAttrib(EAttFontPosture);
			break;
		case ESpanUnder:
			cf.iFontPresentation.iUnderline = EUnderlineOn;
			cm.SetAttrib(EAttFontUnderline);
			break;
		case ESpanBig:
			cf.iFontSpec.iHeight = baseTwips * sp.iArg / 1000;
			cm.SetAttrib(EAttFontHeight);
			break;
		case ESpanColour:
			cf.iFontPresentation.iTextColor = TRgb((TUint32)sp.iArg);
			cm.SetAttrib(EAttColor);
			break;
		case ESpanMono:
			cf.iFontSpec.iTypeface.iName = _L("Courier");
			cf.iFontSpec.iTypeface.SetIsProportional(EFalse);
			cm.SetAttrib(EAttFontTypeface);
			break;
		default:
			{
			CParaFormat* pf = CParaFormat::NewLC();
			TParaFormatMask pm;
			if (sp.iKind == ESpanIndent)
				{
				pf->iLeftMarginInTwips = sp.iArg * 120;
				pm.SetAttrib(EAttLeftMargin);
				}
			else if (sp.iKind == ESpanRuleBelow)
				{
				TParaBorder b;
				b.iLineStyle = TParaBorder::ESolid;
				b.iThickness = 1;
				pf->SetParaBorderL(CParaFormat::EParaBorderBottom, b);
				pf->iSpaceAfterInTwips = 120;
				pm.SetAttrib(EAttBottomBorder);
				pm.SetAttrib(EAttSpaceAfter);
				}
			else
				{
				pf->iSpaceBeforeInTwips = 80;
				pm.SetAttrib(EAttSpaceBefore);
				}
			rt->ApplyParaFormatL(pf, pm, sp.iPos, sp.iLen);
			CleanupStack::PopAndDestroy();   // pf
			continue;
			}
			}
		rt->ApplyCharFormatL(cf, cm, sp.iPos, sp.iLen);
		}
	HeaderFormatL(*rt);                     // the header's fonts, tabs and chips (pmheader.cpp)
	CleanupStack::PopAndDestroy(2);         // spans, buf
	// the pictures, in place of their stand-in spaces
	for (TInt pl = 0; pl < iPictures->PlaceCount(); pl++)
		PlacePictureL(pl);
	iReader->HandleTextChangedL();
	iReader->SetCursorPosL(0, EFalse);
	// reading, not writing: no cursor
	iReader->TextView()->SetCursorVisibilityL(TCursor::EFCursorInvisible, TCursor::EFCursorInvisible);
	iReaderAbove = 0;
	UpdateReaderBar();
	}

// ----- the pictures in the reader ------------------------------------------------------

// what a picture shows: its bitmap, shrunk to fit the room across and down
// the reader (a line taller than the reader cannot be scrolled into view),
// or a frame with a word on the way there
void PmPictureLook(CPmPicture& aPic, const TPmPicEntry& aEntry, const CFont* aFont, const TSize& aRoom,
	TBool aOffline, MGraphicsDeviceMap* aMap)
	{
	if (aEntry.iState == TPmPicEntry::EReady && aEntry.iBitmap)
		{
		TSize bs = aEntry.iBitmap->SizeInPixels();
		TSize shown = bs;
		// (0.75) no bigger than the HTML says (a 600-pixel picture shown at 300)
		if (aEntry.iHintW > 0 && aEntry.iHintW < shown.iWidth && bs.iWidth > 0)
			{
			shown.iHeight = bs.iHeight * aEntry.iHintW / bs.iWidth;
			shown.iWidth = aEntry.iHintW;
			}
		else if (aEntry.iHintW <= 0 && aEntry.iHintH > 0 && aEntry.iHintH < shown.iHeight && bs.iHeight > 0)
			{
			shown.iWidth = bs.iWidth * aEntry.iHintH / bs.iHeight;
			shown.iHeight = aEntry.iHintH;
			}
		if (shown.iWidth > aRoom.iWidth && aRoom.iWidth > 16)
			{
			shown.iWidth = aRoom.iWidth;
			shown.iHeight = bs.iHeight * aRoom.iWidth / bs.iWidth;
			}
		if (shown.iHeight > aRoom.iHeight && aRoom.iHeight > 16)
			{
			shown.iWidth = shown.iWidth * aRoom.iHeight / shown.iHeight;
			shown.iHeight = aRoom.iHeight;
			}
		if (shown.iWidth < 1) shown.iWidth = 1;
		if (shown.iHeight < 1) shown.iHeight = 1;
		aPic.Set(aEntry.iBitmap, shown, KNullDesC, EFalse, aMap);
		return;
		}
	TBuf<90> text;
	TBool failed = EFalse;
	if (aEntry.iState == TPmPicEntry::EFailed)
		{
		text = _L("Picture not shown - ");
		text.Append(Clip(aEntry.iWhy, 60));
		failed = ETrue;
		}
	else if (aEntry.iState == TPmPicEntry::EWaiting)
		text = _L("Getting the picture\x85");
	else if (aOffline)
		text = _L("Picture not downloaded - you are working offline");
	else
		text = _L("Picture not downloaded");
	TSize frame = CPmPicture::FrameSize(aFont, text, aRoom.iWidth);
	if (aEntry.iState == TPmPicEntry::EWaiting && aEntry.iHintW > 0 && aEntry.iHintH > 0)
		{
		// (0.75) on its way: the room the HTML says it takes, so the text
		// doesn't jump when it comes
		TSize hs(aEntry.iHintW, aEntry.iHintH);
		if (hs.iWidth > aRoom.iWidth && aRoom.iWidth > 16) { hs.iHeight = hs.iHeight * aRoom.iWidth / hs.iWidth; hs.iWidth = aRoom.iWidth; }
		if (hs.iHeight > aRoom.iHeight && aRoom.iHeight > 16) { hs.iWidth = hs.iWidth * aRoom.iHeight / hs.iHeight; hs.iHeight = aRoom.iHeight; }
		if (hs.iWidth > frame.iWidth) frame.iWidth = hs.iWidth;
		if (hs.iHeight > frame.iHeight) frame.iHeight = hs.iHeight;
		}
	aPic.Set(NULL, frame, text, failed, aMap);
	}

// the room a picture has in the reader: across it, and down it less a line
// or so, so the text before and after can still be seen
static TSize PictureRoom(CEikRichTextEditor* aReader)
	{
	TInt w = aReader->Rect().Width() - 8;
	TInt h = aReader->Rect().Height() - 24;
	if (w < 100) w = 500;                      // (not laid out yet)
	if (h < 60) h = 160;
	if (w > PM_PIC_MAX_W) w = PM_PIC_MAX_W;
	return TSize(w, h);
	}

void CPmView::PlacePictureL(TInt aPlace)
	{
	TPmPicPlace& pl = iPictures->Place(aPlace);
	TPmPicEntry& e = iPictures->At(pl.iEntry);
	CRichText* rt = iReader->RichText();
	if (pl.iDocPos < 0 || pl.iDocPos >= rt->DocumentLength())
		return;
	CPmPicture* pic = new(ELeave) CPmPicture(*iPictures, iSmallFont);
	CleanupStack::PushL(pic);
	PmPictureLook(*pic, e, iSmallFont, PictureRoom(iReader), iSettings->iOffline, iZoomFactor);
	TPictureHeader h;
	h.iPicture = pic;
	h.iPictureType = KUidPmPicture;
	pic->GetOriginalSizeInTwips(h.iSize);
	rt->DeleteL(pl.iDocPos, 1);                // the stand-in space
	rt->InsertL(pl.iDocPos, h);                // (the text owns the picture now)
	CleanupStack::Pop();                       // pic
	pl.iPicture = pic;
	}

// asks the engine for the pictures placed in the text that are not decoded
// yet: the ones small enough to fetch unasked (a bigger one waits for a
// tap), once per message. Called before the pictures are placed, so that
// their frames say "Getting the picture..."
void CPmView::AskForPicturesL()
	{
	// (offline too: a part downloaded but not yet decoded is still turned
	// into a picture; the engine fetches nothing then)
	if (iPictures->Asked() || (!iRunning && !iEngineAsleep))
		return;
	TBuf8<PM_ARG_MAX> arg;
	TInt total = 0;
	for (TInt pl = 0; pl < iPictures->PlaceCount(); pl++)
		{
		TInt ei = iPictures->Place(pl).iEntry;
		TPmPicEntry& e = iPictures->At(ei);
		if (e.iWeb || e.iState != TPmPicEntry::EUnknown || e.iSize > PM_PIC_AUTO_KB * 1024)
			continue;
		if (total + e.iSize > PM_PIC_AUTO_TOTAL_KB * 1024)
			continue;
		if (arg.Length() + e.iPart.Length() + 1 > arg.MaxLength())
			break;
		total += e.iSize;
		if (arg.Length()) arg.Append(' ');
		arg.Append(e.iPart);
		if (!iSettings->iOffline)
			e.iState = TPmPicEntry::EWaiting;
		}
	iPictures->SetAsked(ETrue);
	if (!arg.Length())
		return;
	Cmd(PM_CMD_PICTURES, iFolder, iMsgUid, arg);
	}

// pictures the engine has decoded since the text was laid out go into it:
// the text is laid out again with them (as Word does when a picture
// changes size), and the view put back where it was
void CPmView::RefreshPicturesL()
	{
	if (iMode != EMessage || !iPictures || !iPictures->Count() || iReaderUid != iMsgUid)
		return;
	TBool any = EFalse;
	for (TInt i = 0; i < iPictures->Count(); i++)
		{
		TPmPicEntry& e = iPictures->At(i);
		if (e.iState != TPmPicEntry::EUnknown && e.iState != TPmPicEntry::EWaiting)
			continue;
		if (iPictures->LoadReadyL(iCoeEnv->FsSession(), i))
			any = ETrue;
		}
	// (1.15, MEM-59) each picture laid the whole message out again (about
	// three times its text in passing): now at most every 2 s while the
	// engine is still setting pictures out, and once when it has done
	if (any)
		iPicRelayoutDue = ETrue;
	if (!iPicRelayoutDue)
		return;
	TUint now = User::TickCount();
	if ((OpInFlight(PM_CMD_PICTURES) || OpInFlight(PM_CMD_WEBPICS)) && iPicRelayoutAt && now - iPicRelayoutAt < 128)
		return;                                    // (TickL comes back to it)
	iPicRelayoutDue = EFalse;
	iPicRelayoutAt = now ? now : 1;
	RelayoutReaderL();
	}

// the reader's text laid out afresh (a picture changed), with what was at
// the top of it still there
void CPmView::RelayoutReaderL()
	{
	if (!iNativeShown || !iReader->IsVisible())
		{
		iReaderUid = 0;                            // laid out afresh when next shown
		return;
		}
	TInt topPos = -1;
	TPoint top(iReader->Rect().iTl.iX + 4, iReader->Rect().iTl.iY + 2);
	if (iReaderAbove > 0)
		{
		TRAPD(pe, topPos = iReader->TextView()->XyPosToDocPosL(top));
		if (pe) topPos = -1;
		}
	iReaderUid = 0;
	UpdateReaderL();
	if (topPos > 0 && topPos < iReader->TextLength())
		{
		TInt y = top.iY;
		TRAPD(ve, iReader->TextView()->SetViewL(topPos, y));
		(void)ve;
		CTextLayout* lay = iReader->TextLayout();
		iReaderAbove = lay ? lay->PixelsAboveBand() : 0;
		if (iReaderAbove < 0) iReaderAbove = 0;
		UpdateReaderBar();
		}
	}

// the reader's scroll bar, drawn here as EIKON draws its own: a shaft with
// a thumb, and the up and down buttons at the bottom. In pixels: the text's
// height, the window's, and how far down it has been scrolled.
void CPmView::ReaderBarModel(TInt& aTotal, TInt& aShown, TInt& aAbove) const
	{
	CTextLayout* lay = iReader->TextLayout();
	aShown = iReader->Rect().Height();
	aTotal = lay ? lay->FormattedHeightInPixels() : aShown;
	if (aTotal < aShown) aTotal = aShown;
	aAbove = iReaderAbove;
	if (aAbove > aTotal - aShown) aAbove = aTotal - aShown;
	if (aAbove < 0) aAbove = 0;
	}

// the parts: the shaft, the thumb within it, the two buttons
void PmScrollBarParts(const TRect& aRect, TInt aTotal, TInt aShown, TInt aAbove, TRect& aShaft, TRect& aThumb, TRect& aUp, TRect& aDown)
	{
	const TRect& r = aRect;
	TInt bh = 20;
	aDown = TRect(r.iTl.iX, r.iBr.iY - bh, r.iBr.iX, r.iBr.iY);
	aUp = TRect(r.iTl.iX, r.iBr.iY - 2 * bh, r.iBr.iX, r.iBr.iY - bh);
	aShaft = TRect(r.iTl.iX, r.iTl.iY, r.iBr.iX, aUp.iTl.iY);
	TInt sh = aShaft.Height();
	TInt th = aTotal > 0 ? sh * aShown / aTotal : sh;
	if (th < 16) th = 16;
	if (th > sh) th = sh;
	TInt room = aTotal - aShown;
	TInt ty = aShaft.iTl.iY + (room > 0 ? (sh - th) * aAbove / room : 0);
	aThumb = TRect(aShaft.iTl.iX, ty, aShaft.iBr.iX, ty + th);
	}

void CPmView::ReaderBarParts(TRect& aShaft, TRect& aThumb, TRect& aUp, TRect& aDown) const
	{
	TInt total, shown, above;
	ReaderBarModel(total, shown, above);
	PmScrollBarParts(iBarRect, total, shown, above, aShaft, aThumb, aUp, aDown);
	}

static void DrawBarButton(CWindowGc& aGc, const TRect& aRect, TBool aUp, TBool aDown);

void PmDrawScrollBar(CWindowGc& gc, const TRect& aRect, TInt aTotal, TInt aShown, TInt aAbove, TInt aPress)
	{
	if (aRect.Width() <= 0)
		return;
	TRect shaft, thumb, up, down;
	PmScrollBarParts(aRect, aTotal, aShown, aAbove, shaft, thumb, up, down);
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(KRgbBlack);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(SysColour(EEikColorScrollBarShaft, TRgb(170, 170, 170)));
	gc.DrawRect(shaft);
	// the thumb: a raised button with grip lines
	TRect t(thumb.iTl.iX + 1, thumb.iTl.iY, thumb.iBr.iX - 1, thumb.iBr.iY);
	PmDrawButtonFace(gc, t, EFalse, ETrue);
	gc.SetPenColor(KRgbBlack);
	TInt cy = t.iTl.iY + t.Height() / 2;
	for (TInt k = -4; k <= 4; k += 2)
		if (cy + k > t.iTl.iY + 2 && cy + k < t.iBr.iY - 2)
			gc.DrawLine(TPoint(t.iTl.iX + 5, cy + k), TPoint(t.iBr.iX - 5, cy + k));
	DrawBarButton(gc, up, ETrue, aPress == 1);
	DrawBarButton(gc, down, EFalse, aPress == 2);
	}

void CPmView::DrawReaderBar(CWindowGc& gc) const
	{
	if (iMode != EMessage || iBarRect.Width() <= 0)
		return;
	TInt total, shown, above;
	ReaderBarModel(total, shown, above);
	PmDrawScrollBar(gc, iBarRect, total, shown, above, iBarPress);
	}

void CPmView::UpdateReaderBar()
	{
	if (iMode != EMessage || !iNativeShown || !IsReadyToDraw())
		return;
	ActivateGc();
	DrawReaderBar(SystemGc());
	DeactivateGc();
	}

// scrolls the reader, keeping count of how far down it is (for the scroll bar)
void CPmView::ReaderScrollL(TInt aMovement)
	{
	TCursorPosition::TMovementType m = (TCursorPosition::TMovementType)aMovement;
	TInt px = iReader->TextView()->ScrollDisplayL(m);
	if (px < 0) px = -px;
	TBool down = m == TCursorPosition::EFLineDown || m == TCursorPosition::EFPageDown;
	iReaderAbove += down ? px : -px;
	if (iReaderAbove < 0) iReaderAbove = 0;
	UpdateReaderBar();
	}

// Home and End. The end is reached a page at a time: putting the cursor on
// the last (empty) paragraph would show a blank page with it at the top.
void CPmView::ReaderToL(TBool aEnd)
	{
	if (!aEnd)
		{
		iReader->SetCursorPosL(0, EFalse);
		iReaderAbove = 0;
		}
	else
		for (TInt guard = 0; guard < 400; guard++)
			{
			TInt px = iReader->TextView()->ScrollDisplayL(TCursorPosition::EFPageDown);
			if (px < 0) px = -px;
			if (px == 0)
				break;
			iReaderAbove += px;
			}
	iReader->TextView()->SetCursorVisibilityL(TCursor::EFCursorInvisible, TCursor::EFCursorInvisible);
	UpdateReaderBar();
	}

// the pen on the reader's scroll bar: ETrue if it was there
TBool CPmView::ReaderBarPointerL(const TPointerEvent& aEvent)
	{
	if (iMode != EMessage)
		return EFalse;
	TPoint p = aEvent.iPosition;
	TRect shaft, thumb, up, down;
	ReaderBarParts(shaft, thumb, up, down);
	if (aEvent.iType == TPointerEvent::EButton1Down)
		{
		if (!iBarRect.Contains(p))
			return EFalse;
		iBarPress = 0;
		if (up.Contains(p)) { iBarPress = 1; ReaderScrollL(TCursorPosition::EFLineUp); }
		else if (down.Contains(p)) { iBarPress = 2; ReaderScrollL(TCursorPosition::EFLineDown); }
		else if (thumb.Contains(p)) { iBarPress = 3; iBarGrab = p.iY - thumb.iTl.iY; }
		else if (p.iY < thumb.iTl.iY) ReaderScrollL(TCursorPosition::EFPageUp);
		else ReaderScrollL(TCursorPosition::EFPageDown);
		UpdateReaderBar();
		return ETrue;
		}
	if (iBarPress == 0)
		return EFalse;
	if (aEvent.iType == TPointerEvent::EDrag && iBarPress == 3)
		{
		// the thumb follows the pen: scroll a line at a time to match
		TInt total, shown, above;
		ReaderBarModel(total, shown, above);
		TInt room = shaft.Height() - thumb.Height();
		if (room > 0 && total > shown)
			{
			TInt want = (p.iY - iBarGrab - shaft.iTl.iY) * (total - shown) / room;
			TInt line = iListFont->HeightInPixels();
			for (TInt guard = 0; guard < 200; guard++)
				{
				TInt at = iReaderAbove;
				if (at + line < want) ReaderScrollL(TCursorPosition::EFLineDown);
				else if (at - line > want && at > 0) ReaderScrollL(TCursorPosition::EFLineUp);
				else break;
				if (iReaderAbove == at) break;
				}
			}
		return ETrue;
		}
	if (aEvent.iType == TPointerEvent::EButton1Up)
		{
		iBarPress = 0;
		UpdateReaderBar();
		}
	return ETrue;
	}


// the address of link n (from the "\x01u<n> url" lines), 0 if none
TInt CPmView::NativeLinkUrl(TInt aLink, TDes& aUrl) const
	{
	aUrl.Zero();
	if (!iText || aLink <= 0)
		return 0;
	TBuf<8> tag;
	tag.Format(_L("\x01u%d "), aLink);
	TInt at = iText->Find(tag);
	if (at < 0)
		return 0;
	TPtrC rest = iText->Mid(at + tag.Length());
	TInt nl = rest.Locate('\n');
	TPtrC url = nl >= 0 ? rest.Left(nl) : rest;
	if (url.Length() && url[url.Length() - 1] == '\r')
		url.Set(url.Left(url.Length() - 1));
	aUrl.Copy(Clip(url, aUrl.MaxLength()));
	return aUrl.Length();
	}

// Tab / Shift+Tab: the next or previous link (or attachment), selected
void CPmView::NativeLinkStepL(TInt aDir)
	{
	TInt n = iLinks->Count();
	if (!n)
		{
		Toast(_L("No links in this message"));
		return;
		}
	iLinkSel += aDir;
	if (iLinkSel >= n) iLinkSel = 0;
	if (iLinkSel < 0) iLinkSel = n - 1;
	const TPmLinkRange& lr = (*iLinks)[iLinkSel];
	iReader->SetSelectionL(lr.iPos + lr.iLen, lr.iPos);
	}

void CPmView::NativeActivateLinkL()
	{
	if (iLinkSel < 0 || iLinkSel >= iLinks->Count())
		return;
	TInt link = (*iLinks)[iLinkSel].iLink;
	if (link == KPmLinkWebPictures)
		{
		ShowWebPicturesL();                   // "Show them" (pmwebpic.cpp)
		return;
		}
	if (link == KPmLinkPictureHelp)
		{
		((CPmAppUi*)iEikonEnv->EikAppUi())->HelpTopicL(KPmHelpPictures);
		return;
		}
	if (link <= -3000)
		{
		InviteLinkL(link);                    // the invitation's or card's box (pminvite.cpp)
		return;
		}
	if (link <= -2000)
		{
		// a big picture: fetched only when asked for
		TInt e = -link - 2000;
		if (e < iPictures->Count())
			{
			if (iSettings->iOffline)
				{
				Toast(_L("Not downloaded - you are working offline"));
				return;
				}
			TBuf8<20> arg;
			arg.Append('!');
			arg.Append(iPictures->At(e).iPart);
			iPictures->At(e).iState = TPmPicEntry::EWaiting;
			Cmd(PM_CMD_PICTURES, iFolder, iMsgUid, arg);
			RelayoutReaderL();                     // its line becomes a frame saying so
			}
		return;
		}
	if (link <= -1000)
		{
		OpenAttachmentL(-link - 1000);        // in its own program (Save is on the menu)
		return;
		}
	if (link < 0)
		{
		HeaderLinkL(link);                    // the header's "+2 others" (pmheader.cpp)
		return;
		}
	TBuf<256> url;
	if (!NativeLinkUrl(link, url))
		return;
	if (Clip(url, 7).CompareF(_L("mailto:")) == 0)
		{
		((CPmAppUi*)iEikonEnv->EikAppUi())->MailtoL(url, EFalse);   // write to them (never with files a message names)
		return;
		}
	OpenWebL(url);
	}

// ----- the title band and the column headings (drawn here, around the controls) ----

// an EIKON button face: light grey, lit from the top left
void PmDrawButtonFace(CWindowGc& aGc, const TRect& aRect, TBool aDown, TBool aScrollBar)
	{
	aGc.SetPenStyle(CGraphicsContext::ENullPen);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	// (0.96) a scroll bar's parts in the system's scroll bar colour on a
	// colour screen, as EIKON's own scroll bars beside it
	TRgb face = aScrollBar ? SysColour(EEikColorScrollButtonThumbBackground, TRgb(170, 170, 170)) : KPmLightGrey;
	aGc.SetBrushColor(aDown ? KPmDarkGrey : face);
	aGc.DrawRect(aRect);
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	TPoint tl = aRect.iTl, br(aRect.iBr.iX - 1, aRect.iBr.iY - 1);
	aGc.SetPenColor(aDown ? KRgbBlack : KRgbWhite);
	aGc.DrawLine(tl, TPoint(br.iX, tl.iY));
	aGc.DrawLine(tl, TPoint(tl.iX, br.iY));
	aGc.SetPenColor(aDown ? KRgbWhite : KRgbBlack);
	aGc.DrawLine(TPoint(tl.iX, br.iY), TPoint(br.iX + 1, br.iY));
	aGc.DrawLine(TPoint(br.iX, tl.iY), TPoint(br.iX, br.iY));
	if (!aDown)
		{
		aGc.SetPenColor(KPmDarkGrey);
		aGc.DrawLine(TPoint(tl.iX + 1, br.iY - 1), TPoint(br.iX, br.iY - 1));
		aGc.DrawLine(TPoint(br.iX - 1, tl.iY + 1), TPoint(br.iX - 1, br.iY - 1));
		}
	}

// the scroll bar's buttons: a face with a solid triangle
static void DrawBarButton(CWindowGc& aGc, const TRect& aRect, TBool aUp, TBool aDown)
	{
	PmDrawButtonFace(aGc, aRect, aDown, ETrue);
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetPenColor(SysColour(EEikColorScrollButtonIcon, KRgbBlack));
	TInt cx = aRect.iTl.iX + aRect.Width() / 2, cy = aRect.iTl.iY + aRect.Height() / 2;
	for (TInt k = 0; k < 5; k++)
		{
		TInt y = aUp ? cy - 2 + k : cy + 2 - k;
		aGc.DrawLine(TPoint(cx - k, y), TPoint(cx + k + 1, y));
		}
	}

// a small triangle: the order a column is sorted in
static void DrawArrow(CWindowGc& aGc, TInt aX, TInt aY, TBool aDownwards)
	{
	for (TInt k = 0; k < 4; k++)
		{
		TInt y = aDownwards ? aY + k : aY + 3 - k;
		aGc.DrawLine(TPoint(aX + k, y), TPoint(aX + 7 - k, y));
		}
	}

// the box on the right of the title band: only when there's something to
// say (the middle shows what the engine is doing)
// (1.03) the route box at the right of the title band: the route and who
// does the TLS ("Modem - Atom does TLS", "TCP/IP - TLS here"), or
// Offline / ready / Connecting...; aLight for a colour screen's light
// (PmLight: 0 grey, 1 amber, 2 green, 3 red). A tap opens Status information.
static void ConnectionText(const TPmSettings& aSettings, const PmShared* aShared, TDes& aOut, TInt* aLight = NULL)
	{
	aOut.Zero();
	TInt light = 0;
	TBool modem = aSettings.iNetMode == 0;
	TBool atomTls = modem && (aSettings.iSpare[0] & KPmModemTlsBit);
	const PmAccount& a = aSettings.iAccounts[aSettings.iAcct >= 0 && aSettings.iAcct < PM_MAX_ACCOUNTS ? aSettings.iAcct : 0];
	TBool insecure = a.used && a.imap_tls == PM_TLS_NONE && !atomTls;
	TPtrC route(modem ? _L("Modem") : _L("TCP/IP"));
	if (aSettings.iOffline)
		aOut = _L("Offline");
	else if (aShared && aShared->online)
		{
		aOut = route;
		if (insecure) { aOut.Append(_L(" - not secure")); light = 3; }
		else { aOut.Append(atomTls ? _L(" - Atom does TLS") : _L(" - TLS here")); light = 2; }
		}
	else if (aShared && aShared->busy && aShared->cur_op != PM_CMD_IDLE)
		{
		aOut = _L("Connecting\x85");
		light = 1;
		}
	else
		{
		aOut = route;
		aOut.Append(_L(" - ready"));
		}
	if (aLight)
		*aLight = light;
	}

// the box's width: the words, and the light on a colour screen
static TInt ConnBoxWidth(const CFont* aFont, const TDesC& aText)
	{
	return aFont->TextWidthInPixels(aText) + 12 + (PmColour() ? 12 : 0);
	}

void CPmView::UpdateStatusLine()
	{
	if (!iNativeShown || !IsReadyToDraw() || !iTitleH)
		return;
	ActivateGc();
	DrawTitle(SystemGc());
	DeactivateGc();
	}

void CPmView::DrawStatus(CWindowGc& aGc) const
	{
	DrawTitle(aGc);
	}

void CPmView::DrawTitle(CWindowGc& gc) const
	{
	if (!iTitleH)
		return;
	TRect r = Rect();
	TRect band(r.iTl, TSize(r.Width(), iTitleH));
	// (0.82) white with black text and a black rule under it, as Psion's own
	// screens: a band of dark grey (the old look) is made on the 5mx's
	// passive screen by flickering the pixels, and over a whole band that
	// shows as streaks; black, white and small greys stay steady
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(PmBack());
	gc.DrawRect(band);
	gc.SetBrushColor(PmText());
	gc.DrawRect(TRect(band.iTl.iX, band.iBr.iY - 1, band.iBr.iX, band.iBr.iY));
	// the folder-list button (it shows or hides the folders)
	TRect btn(band.iTl.iX + 1, band.iTl.iY + 1, band.iTl.iX + iTitleH + 3, band.iBr.iY - 1);
	PmDrawButtonFace(gc, btn, iPenHead == 10);
	PmDrawIcon(gc, iIcons, (iSettings->iView & 4) ? EMbmFolder : EMbmFolderOpen,
		TPoint(btn.iTl.iX + (btn.Width() - 16) / 2, btn.iTl.iY + (btn.Height() - 13) / 2));
	gc.UseFont(iTitleFont);
	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	TInt base = band.iTl.iY + (iTitleH - iTitleFont->HeightInPixels()) / 2 + iTitleFont->AscentInPixels();
	// the connection, in a box on the right (when there's one to show)
	TBuf<40> conn;
	TInt light = 0;
	ConnectionText(*iSettings, iShared, conn, &light);
	if (iMode == ENoAccount && !iSettings->iOffline)
		conn.Zero();                             // (no account yet: nothing to say of the route)
	TRect box(band.iBr.iX, band.iTl.iY, band.iBr.iX, band.iBr.iY);
	if (conn.Length())
		{
		TInt cw = ConnBoxWidth(iTitleFont, conn);
		box = TRect(band.iBr.iX - cw - 2, band.iTl.iY + 2, band.iBr.iX - 2, band.iBr.iY - 2);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(PmFaceText());
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(iPenHead == 11 ? KPmDarkGrey : KPmLightGrey);
		gc.DrawRect(box);
		TInt tx = box.iTl.iX + 6;
		if (PmColour())
			{
			// (1.03) the light: grey, amber, green or red
			TInt d = box.Height() - 6;
			if (d > 9) d = 9;
			TRect dot(tx, box.iTl.iY + (box.Height() - d) / 2, tx + d, box.iTl.iY + (box.Height() - d) / 2 + d);
			gc.SetBrushColor(PmLight(light));
			gc.DrawEllipse(dot);
			tx += 12;
			}
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.DrawText(conn, TPoint(tx, base));
		}
	// where you are: the folder
	TBuf<120> where;
	if (iMode == EOutbox)
		where = _L("Outbox");
	else if (iMode == ECalendar)
		where = _L("Calendar");
	else if (iMode == ENoAccount)
		where = _L("PsiMail");
	else if (iSearch)
		where = _L("Find results");
	else
		{
		const TPmFolder* f = CurrentFolder();
		TPtrC name = f ? TPtrC(f->iName) : TPtrC(_L("Inbox"));
		TInt last = -1;
		for (TInt j = 0; j < name.Length(); j++)
			if (name[j] == '/') last = j;
		where = Clip(name.Mid(last + 1), 40);
		}
	TInt x = btn.iBr.iX + 6;
	TInt maxW = (box.iTl.iX - x) / 2;
	if (maxW > 10 && iTitleFont->TextWidthInPixels(where) > maxW)
		TextUtils::ClipToFit(where, *iTitleFont, maxW);
	gc.SetPenColor(PmText());
	gc.DrawText(where, TPoint(x, base));
	TInt ww = iTitleFont->TextWidthInPixels(where);
	// the middle: what's happening, or how many messages
	TBuf<160> mid;
	if (!iSplashDone && iRunning && iShared && iShared->state == PM_STATE_STARTING)
		mid = _L("Starting\x85");
	else if (iMode == EMessage)
		{
		if (iRows->Count())
			mid.Format(_L("Message %d of %d"), iSel + 1, iRows->Count());
		}
	else if (iMode == ECalendar)
		CalendarText(mid);
	else if (iMode == EOutbox)
		{
		if (iRows->Count() == 1) mid = _L("1 message");
		else if (iRows->Count()) mid.Format(_L("%d messages"), iRows->Count());
		else mid = _L("No messages");
		}
	else if (iMode == EList)
		{
		TInt unread = 0, n = 0;
		CountRows(n, unread);                  // (1.05: folded conversations' messages too)
		if (iSearch) mid.Format(_L("%d found"), n);
		else if (n == 0) mid = _L("No messages");
		else if (n == 1) mid = unread ? _L("1 message (new)") : _L("1 message");
		else if (unread) mid.Format(_L("%d messages (%d new)"), n, unread);
		else mid.Format(_L("%d messages"), n);
		}
	TInt ml = x + ww + 10, mr = box.iTl.iX - 8;
	gc.UseFont(iSmallFont);
	if (mr - ml > 10 && iSmallFont->TextWidthInPixels(mid) > mr - ml)
		TextUtils::ClipToFit(mid, *iSmallFont, mr - ml);
	else if (mr - ml <= 10)
		mid.Zero();
	TInt mw = iSmallFont->TextWidthInPixels(mid);
	TInt mx = ml + (mr - ml - mw) / 2;
	TInt mbase = band.iTl.iY + (iTitleH - iSmallFont->HeightInPixels()) / 2 + iSmallFont->AscentInPixels();
	gc.SetPenColor(PmText());                // (small, but black: small grey text is hard to read on the 5mx)
	gc.DrawText(mid, TPoint(mx, mbase));
	gc.DiscardFont();
	}

static TBool ListMode(TInt aMode)
	{
	return aMode == CPmView::EList || aMode == CPmView::EOutbox || aMode == CPmView::ENoAccount;
	}

void CPmView::DrawHeaders(CWindowGc& gc) const
	{
	if (!ListMode(iMode) && iMode != ECalendar)
		return;
	TRect r = Rect();
	TInt y = r.iTl.iY + iTitleH;
	TInt sort = iSettings->iSort;
	TBool sentLike = iMode == EOutbox;
	const TPmFolder* f = iMode == EList ? CurrentFolder() : NULL;
	if (f && (f->iKind == 'S' || f->iKind == 'D'))
		sentLike = ETrue;
	const TText* KNames[5] = { _S("Folders"), _S("?"), sentLike ? _S("To") : _S("From"), _S("Subject"), _S("Date") };
	gc.UseFont(iBoldFont);
	TInt last = iMode == ECalendar ? 1 : 5;          // (the calendar's own buttons follow)
	for (TInt i = (iSettings->iView & 4) ? 1 : 0; i < last; i++)
		{
		TInt x0 = iHeadX[i];
		TInt x1 = i < 4 ? iHeadX[i + 1] : r.iBr.iX;
		if (i == 0) x1 = iHeadX[1];
		TRect b(x0, y, x1, y + iHeadH);
		PmDrawButtonFace(gc, b, iPenHead == i);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.SetPenColor(PmFaceText());
		TPtrC name(KNames[i]);
		TInt base = y + (iHeadH - iBoldFont->HeightInPixels()) / 2 + iBoldFont->AscentInPixels();
		// which column sorts the list, and which way
		TInt arrow = -1;
		if (iMode == EList)
			{
			if (i == 4 && (sort == 0 || sort == 1)) arrow = sort == 0;
			if (i == 2 && (sort == 2 || sort == 3)) arrow = sort == 2;
			if (i == 3 && (sort == 4 || sort == 5)) arrow = sort == 4;
			if (i == 1 && sort == 6) arrow = 1;
			}
		TInt room = b.Width() - 8 - (arrow >= 0 ? 12 : 0);
		TBuf<16> t(name);
		if (room > 8 && iBoldFont->TextWidthInPixels(t) > room)
			TextUtils::ClipToFit(t, *iBoldFont, room);
		TInt tx = i == 1 ? x0 + (b.Width() - iBoldFont->TextWidthInPixels(t)) / 2 : x0 + 4;
		if (i == 1 && PmColourIcons(iIcons))
			// (1.00) the state column: an envelope in place of "?"
			PmDrawIcon(gc, iIcons, EMbmMsgRead, TPoint(x0 + (b.Width() - 14) / 2, y + (iHeadH - 11) / 2));
		else
			gc.DrawText(t, TPoint(tx, base));
		if (arrow >= 0)
			DrawArrow(gc, x0 + 4 + iBoldFont->TextWidthInPixels(t) + 4, y + iHeadH / 2 - 2, arrow);
		}
	gc.DiscardFont();
	}

// the pen on the title band or a heading: 10 the folder-list button, 11 the
// connection box, 0..4 a heading, -1 neither
TInt CPmView::HeaderHit(const TPoint& aPos) const
	{
	TRect r = Rect();
	if (iTitleH && aPos.iY < r.iTl.iY + iTitleH)
		{
		if (aPos.iX < r.iTl.iX + iTitleH + 4)
			return 10;
		TBuf<40> conn;
		ConnectionText(*iSettings, iShared, conn);
		if (iMode == ENoAccount && !iSettings->iOffline)
			conn.Zero();
		if (conn.Length() && aPos.iX > r.iBr.iX - ConnBoxWidth(iTitleFont, conn) - 2)
			return 11;
		return -1;
		}
	TInt y = r.iTl.iY + iTitleH;
	if (!(ListMode(iMode) || iMode == ECalendar) || aPos.iY < y || aPos.iY >= y + iHeadH)
		return -1;
	for (TInt i = 4; i >= 0; i--)
		if (aPos.iX >= iHeadX[i])
			{
			if (i == 0 && (iSettings->iView & 4)) return -1;
			if (i > 0 && iMode == ECalendar) return -1;   // (the pane's buttons: CalendarPointerL)
			return i;
			}
	return -1;
	}

void CPmView::HeaderActionL(TInt aHit)
	{
	TInt s = iSettings->iSort;
	switch (aHit)
		{
	case 10:
		// the folder button: back to the list from a message, else the folders
		if (iMode == EMessage) BackL();
		else ToggleViewL(4);
		break;
	case 11: StatusInfoL(); break;
	case 0: if (iMode == EList || iMode == EOutbox || iMode == ECalendar) FocusFoldersL(); break;
	case 1: if (iMode == EList) SortL(s == 6 ? 0 : 6); break;
	case 2: if (iMode == EList) SortL(s == 2 ? 3 : 2); break;
	case 3: if (iMode == EList) SortL(s == 4 ? 5 : 4); break;
	case 4: if (iMode == EList) SortL(s == 0 ? 1 : 0); break;
	default: break;
		}
	}

void CPmView::DrawNative(const TRect& /*aRect*/) const
	{
	CWindowGc& gc = SystemGc();
	TRect r = Rect();
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(PmBack());
	gc.DrawRect(TRect(r.iTl.iX, r.iTl.iY + iTitleH, r.iBr.iX, r.iBr.iY));
	DrawTitle(gc);
	DrawHeaders(gc);
	DrawReaderBar(gc);
	if (iMode == ECalendar)
		{
		DrawCalendar(gc);
		DrawCalLegend(gc);
		}
	else if (iMode == EList)
		DrawCalToday(gc);
	if (PreviewOn())
		DrawPreviewHead(gc);
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(PmText());
	TInt top = r.iTl.iY + iTitleH + iHeadH;
	if ((ListMode(iMode) || iMode == ECalendar) && iSplitX)
		gc.DrawLine(TPoint(r.iTl.iX + iSplitX, top), TPoint(r.iTl.iX + iSplitX, r.iBr.iY));
	// an empty folder, or no account yet: say so where the list would be
	TBool emptyList = (iMode == EList || iMode == EOutbox) && iRows->Count() == 0;
	if (emptyList || iMode == ENoAccount)
		{
		// (1.14) Coda's empty state: the first line bold, the second how to
		// go on, in the dialogs' fonts (not the list's zoom)
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		TRect area(iHeadX[1], top, r.iBr.iX, r.iBr.iY);
		TBuf<120> line[3];
		if (iMode == ENoAccount)
			{
			line[0] = _L("No account yet");
			line[1] = _L("Tools > Accounts > Add sets one up");
			}
		else if (iMode == EOutbox) line[0] = _L("Nothing waiting to be sent");
		else if (iSearch) line[0] = _L("Nothing found");
		else line[0] = Busy() ? _L("Looking for messages\x85") : _L("No messages");
		TInt n = 0;
		while (n < 3 && line[n].Length()) n++;
		const CFont* plain = iEikonEnv->NormalFont();
		// (the first line: the normal font in bold, as PsiView's empty state)
		CFont* bold = NULL;
		TFontSpec spec = plain->FontSpecInTwips();
		spec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
		if (iCoeEnv->ScreenDevice()->GetNearestFontInTwips(bold, spec) != KErrNone)
			bold = NULL;
		if (!bold)
			bold = (CFont*)iEikonEnv->LegendFont();
		TInt lh = bold->HeightInPixels() + 4;
		TInt y = area.iTl.iY + (area.Height() - n * lh) / 2;
		for (TInt i = 0; i < n; i++)
			{
			const CFont* f = i == 0 ? bold : plain;
			gc.UseFont(f);
			TextUtils::ClipToFit(line[i], *f, area.Width() - 8);   // "..." if it still won't fit
			TInt w = f->TextWidthInPixels(line[i]);
			gc.DrawText(line[i], TPoint(area.iTl.iX + (area.Width() - w) / 2, y + i * lh + f->AscentInPixels()));
			gc.DiscardFont();
			}
		if (bold != iEikonEnv->LegendFont())
			iCoeEnv->ScreenDevice()->ReleaseFont(bold);
		}
	}

// brings the controls up to date with the view's state (Render() calls this)
void CPmView::UpdateNativeL()
	{
	TBool modeChanged = iNativeMode != iMode;
	if (modeChanged && iPreview)
		LayoutNative();                          // (0.96: the list makes room for the preview, or takes it back)
	if (iMode == EList || iMode == EOutbox)
		{
		UpdateFolderListL();
		UpdateMessageListL();
		}
	else if (iMode == EMessage)
		UpdateReaderL();
	else if (iMode == ENoAccount || iMode == ECalendar)
		UpdateFolderListL();
	TBool wantMsgList = (iMode == EList || iMode == EOutbox) && iRows->Count() > 0;
	if (modeChanged)
		{
		TRAPD(e, ((CPmAppUi*)iEikonEnv->EikAppUi())->SetTool4L(iMode == EMessage));
		(void)e;
		}
	if (modeChanged || wantMsgList != iMsgList->IsVisible() ||
		iFolderList->IsFocused() != ((iMode == EList || iMode == EOutbox || iMode == ECalendar) && iSidebar))
		{
		ShowNative(ETrue);
		iNativeMode = iMode;
		DrawNow();
		}
	else if (iMode == ECalendar && IsReadyToDraw())
		{
		// the pane and the title band (the folder tree redraws itself)
		ActivateGc();
		DrawTitle(SystemGc());
		DrawCalendar(SystemGc());
		DeactivateGc();
		}
	else
		UpdateStatusLine();                     // (the lists redraw themselves when they change)
	PreviewSyncL();                             // (0.96) the selected message under the list
	}

void CPmView::SyncSelectionFromLists()
	{
	if (iMode == EList || iMode == EOutbox || iMode == ECalendar)
		{
		if (iSidebar)
			{
			TInt i = iFolderList->CurrentItemIndex() - 1;   // (row 0: the account)
			if (i >= 0)
				iFolderSel = i;
			}
		else if (iRows->Count() && iMode != ECalendar)
			iSel = iMsgList->CurrentItemIndex();
		}
	}

// the account at the top of the tree: switch accounts, if there are others
static void AccountRowL(CPmView& aView, const TPmSettings& aSettings)
	{
	TInt used = 0;
	for (TInt i = 0; i < PM_MAX_ACCOUNTS; i++)
		if (aSettings.iAccounts[i].used) used++;
	if (used > 1)
		CEikonEnv::Static()->EikAppUi()->HandleCommandL(EPmCmdSwitchAccount);
	else
		{
		TBuf<100> m;
		TPtrC8 e((const TUint8*)aSettings.iAccounts[aSettings.iAcct].email);
		m.Copy(e.Left(e.Length() < 90 ? e.Length() : 90));
		CEikonEnv::Static()->InfoMsg(m);
		}
	(void)aView;
	}

// ----- the toolbar's pop-ups, the View menu, Status information --------------------

void CPmView::ToolbarPopupL(TInt aCommand)
	{
	CPmAppUi* ui = (CPmAppUi*)iEikonEnv->EikAppUi();
	if (aCommand == EPmCmdReplyPopup && !((iMode == EList || iMode == EMessage) && CurrentRow()))
		{
		iEikonEnv->InfoMsg(_L("No message selected"));
		return;
		}
	CCoeControl* b = ui->ToolBarButton(aCommand);
	TPoint pos = b ? b->PositionRelativeToScreen() : TPoint(PositionRelativeToScreen().iX + Rect().Width(), 20);
	ui->LaunchPopupMenuL(aCommand == EPmCmdNewPopup ? R_PM_NEW_POPUP : R_PM_REPLY_POPUP,
		pos, EPopupTargetTopRight);
	}

void CPmView::ToggleViewL(TInt aFlag)
	{
	iSettings->iView ^= aFlag;
	CPmAppUi* ui = (CPmAppUi*)iEikonEnv->EikAppUi();
	ui->SaveSettings();
	if (!iFolderList)
		return;
	if ((iSettings->iView & 4) && iSidebar)
		iSidebar = EFalse;
	if (aFlag == 1)
		ui->ShowToolBar(NativeMode() && iNativeShown);     // (sets our rect)
	LayoutNative();
	if (NativeMode() && iNativeShown)
		{
		iNativeMode = (TMode)-1;
		UpdateNativeL();
		DrawNow();
		}
	}

// ----- (1.00) the toolbar's connection panel (tall screens) ---------------------------

void CPmView::ConnPanelText(TInt& aLight, TDes& aText) const
	{
	aText.Zero();
	TBool modem = iSettings->iNetMode == 0;
	const PmAccount& a = iSettings->iAccounts[iSettings->iAcct];
	TBool atomTls = modem && (iSettings->iSpare[0] & KPmModemTlsBit);
	// (1.18) "Mail on" or "Mail off": on Coda the toolbar's own Internet
	// line sits just below, and "Offline" here beside its "Online" read as
	// a contradiction (this is the mail server's connection, that the
	// Internet link). "Mail online" did not fit the box
	// (1.14) in a word or two that fit the box: Online or Offline (amber
	// while it connects; working offline is ticked on the File menu), the
	// route, whether it is secure, and when the mail was last checked - the
	// details (the web pictures' way too) are in Status information, a tap away
	if (iShared && iShared->online && !iSettings->iOffline)
		{
		aLight = (a.used && a.imap_tls == PM_TLS_NONE && !atomTls) ? 3 : 2;
		aText = _L("Mail on");
		}
	else
		{
		aLight = !iSettings->iOffline && iShared && iShared->busy ? 1 : 0;
		aText = _L("Mail off");
		}
	aText.Append('\n');
	aText.Append(modem ? _L("By modem") : _L("By network"));
	aText.Append('\n');
	if (a.used && a.imap_tls == PM_TLS_NONE && !atomTls)
		aText.Append(_L("Not secure"));
	else
		aText.Append(_L("Secure"));
	aText.Append('\n');
	aText.Append(_L("Checked"));            // (when the mail was last checked, under it)
	aText.Append('\n');
	if (iLastChecked.Int64() == TInt64(0))
		aText.Append(_L("not yet"));
	else
		{
		TBuf<16> t;
		TRAPD(err, iLastChecked.FormatL(t, _L("%-B%:0%J%:1%T%+B")));
		if (!err)
			aText.Append(t);
		}
	}

CPmConnPanel* CPmConnPanel::NewL(CPmView& aView)
	{
	CPmConnPanel* p = new(ELeave) CPmConnPanel(aView);
	CleanupStack::PushL(p);
	p->CreateWindowL();
	p->MakeVisible(EFalse);
	p->ActivateL();
	CleanupStack::Pop();
	return p;
	}

void CPmConnPanel::Place(const TRect& aRect)
	{
	if (aRect.IsEmpty())
		{
		MakeVisible(EFalse);
		return;
		}
	TRAPD(err, SetRectL(aRect));
	(void)err;
	iShown.Zero();
	MakeVisible(ETrue);
	Refresh();
	}

void CPmConnPanel::Refresh()
	{
	if (!IsVisible())
		return;
	TBuf<160> t;
	TInt light = 0;
	iView.ConnPanelText(light, t);
	if (t == iShown && light == iLight)
		return;
	iShown = t;
	iLight = light;
	DrawNow();
	}

void CPmConnPanel::Draw(const TRect& /*aRect*/) const
	{
	CWindowGc& gc = SystemGc();
	TRect r = Rect();
	// the toolbar's face round a white box, as the clock sits in it
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(iEikonEnv->Color(EEikColorToolbarBackground));
	gc.DrawRect(r);
	TRect box(r);
	box.Shrink(1, 1);
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(TRgb(102, 102, 102));
	gc.SetBrushColor(iDown ? PmFace() : PmBack());
	gc.DrawRect(box);
	const CFont* f = iEikonEnv->DenseFont();
	gc.UseFont(f);
	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	TInt lh = f->HeightInPixels() + 2;
	TInt x = box.iTl.iX + 4, y = box.iTl.iY + 3;
	TPtrC rest(iShown);
	for (TInt i = 0; rest.Length() && i < 6; i++)
		{
		TInt nl = rest.Locate('\n');
		TPtrC piece(nl >= 0 ? rest.Left(nl) : rest);
		TBuf<40> line(piece.Left(Min(piece.Length(), 40)));
		rest.Set(nl >= 0 ? rest.Mid(nl + 1) : TPtrC());
		TInt lx = x;
		if (i == 0)
			{
			// the light, then the state
			TInt d = 8;
			TRect dot(x, y + (lh - d) / 2, x + d, y + (lh - d) / 2 + d);
			gc.SetPenColor(KRgbBlack);
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(PmLight(iLight));
			gc.DrawEllipse(dot);
			gc.SetBrushStyle(CGraphicsContext::ENullBrush);
			lx += d + 4;
			}
		TextUtils::ClipToFit(line, *f, box.iBr.iX - 3 - lx);
		gc.SetPenColor(PmText());
		gc.DrawText(line, TPoint(lx, y + (lh - f->HeightInPixels()) / 2 + f->AscentInPixels()));
		y += lh;
		if (y + lh > box.iBr.iY)
			break;
		}
	gc.DiscardFont();
	}

// a tap: Status information (as the title band's box does)
void CPmConnPanel::HandlePointerEventL(const TPointerEvent& aEvent)
	{
	if (aEvent.iType == TPointerEvent::EButton1Down)
		{
		iDown = ETrue;
		DrawNow();
		}
	else if (aEvent.iType == TPointerEvent::EButton1Up)
		{
		TBool was = iDown;
		iDown = EFalse;
		DrawNow();
		if (was && Rect().Contains(aEvent.iPosition))
			iView.StatusInfoL();
		}
	}


void CPmView::StatusInfoL()
	{
	// (1.03) eight short lines, none cut: the account, the route, who does
	// the security, the web pictures' helper, this folder, the Outbox, the
	// last thing done, and where the mail is kept
	TBuf<80> lines[8];
	const PmAccount& a = iSettings->iAccounts[iSettings->iAcct];
	lines[0] = _L("Account: ");
	if (a.used)
		{
		TBuf<24> n;
		n.Copy(Clip(TPtrC8((const TUint8*)a.name), 20));
		lines[0].Append(n);
		TBuf<40> e;
		e.Copy(Clip(TPtrC8((const TUint8*)a.email), 34));
		lines[0].AppendFormat(_L(" (%S)"), &e);
		}
	else
		lines[0].Append(_L("none yet - Tools > Accounts > Add"));
	const TInt KBauds[5] = { 9600, 19200, 38400, 57600, 115200 };
	TInt bi = iSettings->iBaudIndex;
	if (bi < 0 || bi > 4) bi = 4;
	TBool modem = iSettings->iNetMode == 0;
	// (1.14) the route in the words PsiMail, PsiWeb and PsiTerm share
	if (modem)
		lines[1].Format(_L("By modem, %d baud"), KBauds[bi]);
	else
		lines[1] = _L("By network card or dial-up");
	lines[1].Append(iSettings->iOffline ? _L(" - working offline") : iShared && iShared->online ? _L(" - online") : _L(" - offline"));
	TBool atomTls = modem && (iSettings->iSpare[0] & KPmModemTlsBit);
	if (a.used && a.imap_tls == PM_TLS_NONE && !atomTls)
		lines[2] = _L("Security: none - the account's Security is None");
	else if (atomTls)
		lines[2] = _L("Security: TLS, by the modem");
	else
		lines[2] = _L("Security: TLS 1.3, certificate checked");
	{
	TBuf8<64> host;
	TInt useProxy = 0;
	if (iShared)
		{
		useProxy = iShared->use_proxy;
		host = TPtrC8((const TUint8*)iShared->proxy_host);
		}
	host.ZeroTerminate();
	TPtrC8 route((const TUint8*)pg_su_route_name(useProxy, (const char*)host.Ptr()));
	TBuf<40> r;
	r.Copy(route);
	lines[3].Format(_L("Web pictures: %S"), &r);
	}
	if (iMode == EList)
		{
		TInt unread = 0, total = 0;
		CountRows(total, unread);
		lines[4].Format(total == 1 ? _L("This folder: %d message here, %d unread") : _L("This folder: %d messages here, %d unread"), total, unread);
		}
	else
		lines[4] = _L("No folder open");
	TInt waiting = OutboxCount();
	if (waiting == 0)
		lines[5] = _L("Outbox: nothing waiting to be sent");
	else if (waiting == 1)
		lines[5] = _L("Outbox: 1 message waiting to be sent");
	else
		lines[5].Format(_L("Outbox: %d messages waiting to be sent"), waiting);
	if (Busy() && iLastProgress.Length())
		{
		lines[6] = _L("Now: ");
		lines[6].Append(Clip(iLastProgress, 60));
		}
	else if (iStatus.Length())
		{
		lines[6] = _L("Last: ");
		lines[6].Append(Clip(iStatus, 60));
		}
	else
		lines[6] = iRunning ? _L("The mail engine is ready") : iEngineAsleep ? _L("The mail engine is resting, to save memory") : _L("The mail engine is not running");
	TBool memLine = !(Busy() && iLastProgress.Length()) && !iStatus.Length();
	{
	// where the mail is kept (pmstore.cpp), the disk's room, and memory
	TBuf<100> dir;
	StoreDir(dir);
	TInt drive = EDriveC;
	if (dir.Length() && RFs::CharToDrive(dir[0], drive) != KErrNone) drive = EDriveC;
	TVolumeInfo vol;
	TBool room = iCoeEnv->FsSession().Volume(vol, drive) == KErrNone;
	TMemoryInfoV1Buf mem;
	UserHal::MemoryInfo(mem);
	// (1.14) the disk by its name and sizes in MB, as Coda's toolbar has them
	TBuf<16> diskFree, memFree;
	PmSizeText(diskFree, room ? vol.iFree : TInt64(0));
	PmSizeText(memFree, TInt64(mem().iFreeRamInBytes));
	lines[7] = drive == EDriveC ? _L("Mail on the Internal disk") : _L("Mail on the card");
	if (room)
		lines[7].AppendFormat(_L(", %S free"), &diskFree);
	if (memLine)
		lines[6].AppendFormat(_L(" - memory %S free"), &memFree);   // (where it fits)
	}
	TPtrC ptrs[8];
	for (TInt k = 0; k < 8; k++) ptrs[k].Set(lines[k]);
	CPmInfoDialog* dlg = new(ELeave) CPmInfoDialog(_L("Status information"), ptrs, 8);
	dlg->ExecuteLD(R_PM_STATUS_DIALOG);
	}

// ----- keys and the pen -------------------------------------------------------------

static TBool IsListKey(TUint aCode)
	{
	return aCode == EKeyUpArrow || aCode == EKeyDownArrow || aCode == EKeyPageUp ||
		aCode == EKeyPageDown || aCode == EKeyHome || aCode == EKeyEnd;
	}

TKeyResponse CPmView::NativeKeyL(const TKeyEvent& aKeyEvent, TEventCode aType)
	{
	TUint code = aKeyEvent.iCode;
	TBool canFolders = !(iSettings->iView & 4);
	if (iMode == ECalendar)
		{
		if (iSidebar && canFolders)
			{
			// the folder tree works as it does beside the mail
			if (IsListKey(code))
				{
				iFolderList->OfferKeyEventL(aKeyEvent, aType);
				SyncSelectionFromLists();
				return EKeyWasConsumed;
				}
			switch (code)
				{
			case EKeyEnter:
			case EKeyRightArrow:
			case EKeyTab:
				if (iFolderList->CurrentItemIndex() == 0)
					AccountRowL(*this, *iSettings);
				else
					{
					SyncSelectionFromLists();
					OpenCurrentL();
					}
				return EKeyWasConsumed;
			case EKeyEscape:
				iSidebar = EFalse;
				Render();
				return EKeyWasConsumed;
			default:
				return EKeyWasNotConsumed;
				}
			}
		return CalendarKeyL(code);
		}
	if (iMode == EList || iMode == EOutbox)
		{
		if (IsListKey(code))
			{
			// (1.05) Shift with Up, Down, Page up/down, Home or End chooses
			// a run of messages; without it the run is let go
			TBool shift = (aKeyEvent.iModifiers & EModifierShift) && !iSidebar && iMode == EList && iRows->Count();
			if (shift && iMarkFrom < 0)
				iMarkFrom = iSel;
			else if (!shift && iMarkFrom >= 0)
				ClearMark();
			TKeyEvent k = aKeyEvent;
			k.iModifiers &= ~EModifierShift;
			if (iSidebar)
				iFolderList->OfferKeyEventL(k, aType);
			else if (iRows->Count())
				iMsgList->OfferKeyEventL(k, aType);
			SyncSelectionFromLists();
			if (shift)
				{
				iMarkLo = Min(iMarkFrom, iSel);
				iMarkHi = Max(iMarkFrom, iSel);
				if (iMsgList->IsVisible()) iMsgList->DrawNow();
				}
			UpdateStatusLine();
			PreviewSyncL();
			return EKeyWasConsumed;
			}
		if (code == ' ' && PreviewOn() && !iSidebar)
			{
			// (0.96) Space pages the preview, Shift+Space back
			PreviewPageL(!(aKeyEvent.iModifiers & EModifierShift));
			return EKeyWasConsumed;
			}
		switch (code)
			{
		case EKeyEnter:
		case EKeyRightArrow:
			if (iSidebar && iFolderList->CurrentItemIndex() == 0)
				{
				AccountRowL(*this, *iSettings);
				return EKeyWasConsumed;
				}
			SyncSelectionFromLists();
			if (iMode == EList && ThreadKeyL(ETrue))
				return EKeyWasConsumed;          // (1.05) a folded conversation: unfolded
			OpenCurrentL();
			return EKeyWasConsumed;
		case EKeyTab:
			SyncSelectionFromLists();
			if (iSidebar) OpenCurrentL(); else if (canFolders) FocusFoldersL();
			return EKeyWasConsumed;
		case EKeyLeftArrow:
			SyncSelectionFromLists();
			if (iMode == EList && ThreadKeyL(EFalse))
				return EKeyWasConsumed;          // (1.05) an unfolded conversation: folded
			if (!iSidebar && canFolders) FocusFoldersL();
			return EKeyWasConsumed;
		case EKeyEscape:
			BackL();
			return EKeyWasConsumed;
		case EKeyDelete:
		case EKeyBackspace:
			if (!iSidebar)
				{
				SyncSelectionFromLists();
				DeleteCurrentL();
				}
			return EKeyWasConsumed;
		default:
			return EKeyWasNotConsumed;
			}
		}
	if (iMode == EMessage)
		{
		switch (code)
			{
		case EKeyUpArrow: ReaderScrollL(TCursorPosition::EFLineUp); break;
		case EKeyDownArrow: ReaderScrollL(TCursorPosition::EFLineDown); break;
		case EKeyPageUp: ReaderScrollL(TCursorPosition::EFPageUp); break;
		case EKeyPageDown:
		case ' ':
			ReaderScrollL(TCursorPosition::EFPageDown);
			break;
		case EKeyHome: ReaderToL(EFalse); break;
		case EKeyEnd: ReaderToL(ETrue); break;
		case EKeyLeftArrow: StepMessageL(-1); return EKeyWasConsumed;
		case EKeyRightArrow: StepMessageL(1); return EKeyWasConsumed;
		case EKeyTab: NativeLinkStepL((aKeyEvent.iModifiers & EModifierShift) ? -1 : 1); break;
		case EKeyEnter: NativeActivateLinkL(); return EKeyWasConsumed;
		case EKeyEscape:
			if (iLinkSel >= 0)
				{
				iLinkSel = -1;
				iReader->SetCursorPosL(iReader->CursorPos(), EFalse);
				}
			else
				BackL();
			return EKeyWasConsumed;
		case EKeyDelete:
			DeleteCurrentL();
			return EKeyWasConsumed;
		default:
			return EKeyWasNotConsumed;
			}
		UpdateReaderBar();
		return EKeyWasConsumed;
		}
	return EKeyWasNotConsumed;
	}

// the pen on the title band and headings (the controls take the rest):
// returns ETrue if it was ours
// (1.00) the calendars under the folders (tall screens): each with its
// colour; a tap shows or hides that calendar's events (cal\hidden.txt)
TInt CPmView::CalLegendRow(TInt aRow, TRect& aRect) const
	{
	if (iLegendRect.IsEmpty() || aRow < 0 || aRow >= (iMode == ECalendar ? iCalModel.ncal : 3))
		return EFalse;
	TInt ih = iFolderList ? iFolderList->ItemHeight() : 20;
	TInt y = iLegendRect.iTl.iY + 6 + (aRow + 1) * ih;
	aRect = TRect(iLegendRect.iTl.iX, y, iLegendRect.iBr.iX, y + ih);
	return ETrue;
	}

void CPmView::DrawCalLegend(CWindowGc& gc) const
	{
	if (iLegendRect.IsEmpty() || iMode != ECalendar)
		return;
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(PmBack());
	gc.DrawRect(iLegendRect);
	TInt ih = iFolderList ? iFolderList->ItemHeight() : 20;
	TInt x = iLegendRect.iTl.iX + 6;
	// a rule, and the heading
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(TRgb(153, 153, 153));
	gc.DrawLine(TPoint(x, iLegendRect.iTl.iY + 2), TPoint(iLegendRect.iBr.iX - 6, iLegendRect.iTl.iY + 2));
	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	gc.UseFont(iBoldFont);
	gc.SetPenColor(PmText());
	TInt hy = iLegendRect.iTl.iY + 6;
	gc.DrawText(_L("Calendars"), TPoint(x, hy + (ih - iBoldFont->HeightInPixels()) / 2 + iBoldFont->AscentInPixels()));
	gc.DiscardFont();
	gc.UseFont(iListFont);
	for (TInt i = 0; i < iCalModel.ncal; i++)
		{
		TRect row;
		if (!CalLegendRow(i, row))
			break;
		TBool shown = !(iCalModel.hidden & (1u << i));
		TInt sy = row.iTl.iY + (ih - 10) / 2;
		TRect sw(x, sy, x + 12, sy + 10);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(PmCalColour(i & 3));
		gc.SetBrushStyle(shown ? CGraphicsContext::ESolidBrush : CGraphicsContext::ENullBrush);
		gc.SetBrushColor(PmCalColour(i & 3));
		gc.DrawRect(sw);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		TBuf<60> name;
		const char* n = iCalModel.calName[i];
		for (TInt c = 0; c < iCalModel.calLen[i] && c < 58; c++) name.Append((TUint8)n[c]);
		if (!name.Length()) name = _L("(no name)");
		TextUtils::ClipToFit(name, *iListFont, row.iBr.iX - x - 20 - 4);
		gc.SetPenColor(shown ? PmText() : KPmDarkGrey);
		gc.DrawText(name, TPoint(x + 18, row.iTl.iY + (ih - iListFont->HeightInPixels()) / 2 + iListFont->AscentInPixels()));
		}
	gc.DiscardFont();
	}

// (1.00) under the folders in the mail (tall screens, the calendar on):
// what is left of today, three events at most; a tap goes to the calendar
void CPmView::DrawCalToday(CWindowGc& gc) const
	{
	if (iLegendRect.IsEmpty() || iMode != EList)
		return;
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(PmBack());
	gc.DrawRect(iLegendRect);
	TInt ih = iFolderList ? iFolderList->ItemHeight() : 20;
	TInt x = iLegendRect.iTl.iX + 6;
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(PmColour() ? TRgb(153, 153, 153) : PmText());
	gc.DrawLine(TPoint(x, iLegendRect.iTl.iY + 2), TPoint(iLegendRect.iBr.iX - 6, iLegendRect.iTl.iY + 2));
	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	gc.UseFont(iBoldFont);
	gc.SetPenColor(PmText());
	TInt hy = iLegendRect.iTl.iY + 6;
	gc.DrawText(_L("Today"), TPoint(x, hy + (ih - iBoldFont->HeightInPixels()) / 2 + iBoldFont->AscentInPixels()));
	gc.DiscardFont();
	CPmView* self = (CPmView*)this;
	self->CalendarToday();
	PmUiEvent evs[12];
	TInt got = calm_day(&iCalModel, iCalToday, evs, 12);
	TInt shown = 0;
	gc.UseFont(iSmallFont);
	for (TInt e = 0; e < got && shown < 3; e++)
		{
		if (!evs[e].allday && evs[e].end <= iCalNow && evs[e].start < 1440)
			continue;                              // (over already)
		TRect row;
		if (!CalLegendRow(shown, row))
			break;
		shown++;
		TBuf<80> t;
		if (evs[e].allday || evs[e].start < 0)
			t = _L("All day ");
		else
			t.Format(_L("%02d:%02d "), evs[e].start / 60, evs[e].start % 60);
		TBuf<60> title;
		for (TInt c = 0; c < evs[e].tlen && c < 58; c++) title.Append((TUint8)evs[e].title[c]);
		if (!title.Length()) title = _L("(no name)");
		t.Append(title);
		// the calendar's colour as a bar, then the time and the title
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(PmColour() ? PmCalColour(evs[e].shade) : PmText());
		gc.DrawRect(TRect(x, row.iTl.iY + 3, x + 3, row.iBr.iY - 3));
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		TextUtils::ClipToFit(t, *iSmallFont, row.iBr.iX - x - 10 - 4);
		gc.SetPenColor(PmText());
		gc.DrawText(t, TPoint(x + 8, row.iTl.iY + (ih - iSmallFont->HeightInPixels()) / 2 + iSmallFont->AscentInPixels()));
		}
	if (!shown)
		{
		TRect row;
		if (CalLegendRow(0, row))
			{
			gc.SetPenColor(KPmDarkGrey);
			gc.DrawText(_L("Nothing more today"), TPoint(x + 8, row.iTl.iY + (ih - iSmallFont->HeightInPixels()) / 2 + iSmallFont->AscentInPixels()));
			}
		}
	gc.DiscardFont();
	}

TBool CPmView::CalLegendPointerL(const TPointerEvent& aEvent)
	{
	if (!iLegendRect.IsEmpty() && iMode == EList && iLegendRect.Contains(aEvent.iPosition))
		{
		if (aEvent.iType == TPointerEvent::EButton1Up)
			ShowCalendarL();                       // ("Today": to the calendar)
		return ETrue;
		}
	if (iLegendRect.IsEmpty() || iMode != ECalendar || !iLegendRect.Contains(aEvent.iPosition))
		return EFalse;
	if (aEvent.iType != TPointerEvent::EButton1Up)
		return ETrue;
	for (TInt i = 0; i < iCalModel.ncal; i++)
		{
		TRect row;
		if (CalLegendRow(i, row) && row.Contains(aEvent.iPosition))
			{
			ToggleCalendarL(i);
			break;
			}
		}
	return ETrue;
	}

// shows or hides one calendar: cal\hidden.txt holds the hidden ones' ids
void CPmView::ToggleCalendarL(TInt aIndex)
	{
	if (aIndex < 0 || aIndex >= iCalModel.ncal)
		return;
	TUint hidden = iCalModel.hidden ^ (1u << aIndex);
	TBuf8<600> out;
	for (TInt i = 0; i < iCalModel.ncal; i++)
		if (hidden & (1u << i))
			{
			const char* id = iCalModel.calId[i];
			TInt n = 0;
			while (id[n] && id[n] != '\t' && n < 64) n++;
			if (out.Length() + n + 1 > out.MaxLength())
				break;
			out.Append(TPtrC8((const TUint8*)id, n));
			out.Append('\n');
			}
	TBuf<120> path;
	StoreDir(path);
	path.Append(_L("cal\\hidden.txt"));
	RFs& fs = iCoeEnv->FsSession();
	if (!out.Length())
		fs.Delete(path);
	else
		PsiSafeWrite(fs, path, out);         // (1.12, review R104: the old list stays if it fails)
	LoadCalendarL();
	DrawNow();
	}

TBool CPmView::NativePointerL(const TPointerEvent& aEvent)
	{
	if (ReaderBarPointerL(aEvent))
		return ETrue;
	if (CalLegendPointerL(aEvent))
		return ETrue;
	TInt hit = HeaderHit(aEvent.iPosition);
	if (aEvent.iType == TPointerEvent::EButton1Down)
		{
		iPenHead = hit;
		if (hit < 0)
			return EFalse;
		}
	else if (iPenHead < 0)
		return EFalse;
	else if (aEvent.iType == TPointerEvent::EButton1Up)
		{
		TInt was = iPenHead;
		iPenHead = -1;
		DrawNow();
		if (hit == was)
			HeaderActionL(was);
		return ETrue;
		}
	else
		return ETrue;                            // (a drag: wait for the pen to lift)
	// pressed: show it
	ActivateGc();
	DrawTitle(SystemGc());
	DrawHeaders(SystemGc());
	DeactivateGc();
	return ETrue;
	}

void CPmView::HandleListBoxEventL(CEikListBox* aListBox, TListBoxEvent aEventType)
	{
	if (aListBox == iFolderList)
		{
		// a tap on a folder opens it (row 0: the account)
		TInt row = iFolderList->CurrentItemIndex();
		if (row <= 0)
			{
			if (aEventType == EEventItemDoubleClicked || aEventType == EEventEnterKeyPressed || aEventType == EEventItemClicked)
				AccountRowL(*this, *iSettings);
			return;
			}
		iSidebar = ETrue;
		iFolderSel = row - 1;
		OpenCurrentL();
		return;
		}
	if (aListBox == iMsgList)
		{
		// a tap selects; a second tap on the selected message opens it (the
		// style guide: no double taps)
		TBool wasSide = iSidebar;
		TInt was = iSel;
		iSidebar = EFalse;
		iSel = iMsgList->CurrentItemIndex();
		if (iMarkLo >= 0 && (iSel < iMarkLo || iSel > iMarkHi))
			ClearMark();                       // (1.05) a tap outside the run lets it go
		if (iUnfoldTick && User::TickCount() - iUnfoldTick < 32)
			{
			iUnfoldTick = 0;                       // (1.05) the same tap's second event: not an open
			return;
			}
		iUnfoldTick = 0;
		if (aEventType == EEventEnterKeyPressed || aEventType == EEventItemDoubleClicked ||
			(aEventType == EEventItemClicked && !wasSide && iSel == was))
			{
			if (iMode == EList && ThreadKeyL(ETrue))
				{
				iUnfoldTick = User::TickCount();    // (1.05) a folded conversation: unfolded
				if (!iUnfoldTick) iUnfoldTick = 1;
				return;
				}
			OpenCurrentL();
			return;
			}
		if (wasSide)
			Render();
		else
			UpdateStatusLine();
		PreviewSyncL();
		}
	}
