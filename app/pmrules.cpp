// PMRULES.CPP - (1.02) Tools > Accounts > Rules and Signatures
//
// Rules: what is done with new mail as it arrives in the Inbox (the engine
// applies them after each check of the Inbox: rules_run in
// engine/pmmain.c). Eight at most, one page: the Rule line chooses which
// one the other lines show. Kept in rules.txt in the account's folder
// beside its mail (A<n>\rules.txt), one a line:
//     what TAB words TAB action TAB folder
// what: F the sender, S the subject; action: M move to the folder (its
// IMAP name), F flag, R mark read, Q no new-mail alert. Not in TPmSettings.
//
// Signatures: the account's signature for new messages (as the account's
// Signature line), a second one for replies (A<n>\replysig.txt; empty: the
// same) and where it goes in a reply, above the quote (as before) or below
// it (TPmSettings::iSpare[0] bit 5, KPmSigBelowBit).

#include <eikchlst.h>
#include <eikedwin.h>
#include <eikenv.h>
#include <f32file.h>
#include "pmapp.h"

static void AcctFile(CPmView& aView, TInt aAcct, const TDesC& aName, TDes& aPath)
	{
	aView.StoreDirectory(aPath);
	aPath.AppendFormat(_L("A%d\\"), aAcct);
	aPath.Append(aName);
	}

// ----- the rules ---------------------------------------------------------------

void CPmRulesDialog::LoadL()
	{
	for (TInt i = 0; i < KPmRules; i++)
		{
		iRule[i].iWhat = 'S';
		iRule[i].iAct = 0;
		iRule[i].iWords.Zero();
		iRule[i].iFolder.Zero();
		}
	TFileName path;
	AcctFile(iView, iAcct, _L("rules.txt"), path);
	RFile f;
	if (f.Open(iEikonEnv->FsSession(), path, EFileRead | EFileShareReadersOnly) != KErrNone)
		return;
	TBuf8<2048> buf;
	f.Read(buf);
	f.Close();
	TInt n = 0;
	TPtrC8 rest(buf);
	while (rest.Length() && n < KPmRules)
		{
		TInt nl = rest.Locate('\n');
		TPtrC8 line = nl >= 0 ? rest.Left(nl) : rest;
		rest.Set(nl >= 0 ? rest.Mid(nl + 1) : TPtrC8());
		if (line.Length() && line[line.Length() - 1] == '\r')
			line.Set(line.Left(line.Length() - 1));
		TPtrC8 field[4];
		TInt k = 0;
		TPtrC8 l(line);
		while (k < 4)
			{
			TInt t = l.Locate('\t');
			field[k++].Set(t >= 0 ? l.Left(t) : l);
			if (t < 0) break;
			l.Set(l.Mid(t + 1));
			}
		if (k < 3 || field[0].Length() != 1 || field[2].Length() != 1)
			continue;
		TPmRule& r = iRule[n];
		r.iWhat = field[0][0] == 'F' ? 'F' : 'S';
		TInt a = TPtrC8((const TUint8*)"MFRQ").Locate(field[2][0]);
		r.iAct = a < 0 ? 0 : a + 1;
		r.iWords.Copy(field[1].Left(Min(field[1].Length(), r.iWords.MaxLength())));
		if (k > 3)
			r.iFolder = field[3].Left(Min(field[3].Length(), r.iFolder.MaxLength()));
		if (r.iAct) n++;
		}
	}

void CPmRulesDialog::SaveL()
	{
	TBuf8<2048> out;
	for (TInt i = 0; i < KPmRules; i++)
		{
		const TPmRule& r = iRule[i];
		if (!r.iAct || !r.iWords.Length())
			continue;
		TBuf8<64> w;
		w.Copy(r.iWords);
		out.Append(r.iWhat);
		out.Append('\t');
		out.Append(w);
		out.Append('\t');
		out.Append(TPtrC8((const TUint8*)"MFRQ").Mid(r.iAct - 1, 1));
		out.Append('\t');
		if (r.iAct == 1)
			out.Append(r.iFolder);
		out.Append('\n');
		}
	TFileName path;
	AcctFile(iView, iAcct, _L("rules.txt"), path);
	RFs& fs = iEikonEnv->FsSession();
	if (!out.Length())
		{
		fs.Delete(path);
		return;
		}
	RFile f;
	User::LeaveIfError(f.Replace(fs, path, EFileWrite));
	TInt err = f.Write(out);
	f.Close();
	User::LeaveIfError(err);
	}

CPmRulesDialog::~CPmRulesDialog()
	{
	delete iImap;
	delete iNames;
	}

void CPmRulesDialog::PreLayoutDynInitL()
	{
	LoadL();
	// the folders a message can go to: every one but the Inbox and those
	// that hold only other folders
	iNames = new(ELeave) CDesCArrayFlat(16);
	iImap = new(ELeave) CArrayFixFlat<TBuf8<128> >(16);
	for (TInt i = 0; i < iView.FolderCount(); i++)
		{
		const TPmFolder& f = iView.FolderAt(i);
		if (f.iKind == 'I' || f.iKind == 'N')
			continue;
		iNames->AppendL(f.iName);
		iImap->AppendL(f.iImap);
		}
	if (!iNames->Count())
		iNames->AppendL(_L("(no folders yet)"));
	CEikChoiceList* cl = (CEikChoiceList*)Control(EPmDlgRuleFolder);
	PmFitChoiceListL(*cl, *iNames);
	cl->SetArrayL(iNames);
	cl->SetArrayExternalOwnership(ETrue);    // (deleted here, after the dialog)
	iCurrent = 0;
	ShowRuleL(0);
	}

// the lines from a rule
void CPmRulesDialog::ShowRuleL(TInt aIndex)
	{
	const TPmRule& r = iRule[aIndex];
	((CEikChoiceList*)Control(EPmDlgRuleWhat))->SetCurrentItem(r.iWhat == 'F' ? 0 : 1);
	TBuf<60> w(r.iWords);
	SetEdwinTextL(EPmDlgRuleWords, &w);
	((CEikChoiceList*)Control(EPmDlgRuleAct))->SetCurrentItem(r.iAct);
	TInt fi = 0;
	for (TInt i = 0; i < iImap->Count(); i++)
		if ((*iImap)[i] == r.iFolder) fi = i;
	((CEikChoiceList*)Control(EPmDlgRuleFolder))->SetCurrentItem(fi);
	DimLines();
	DrawNow();
	}

// a rule from the lines
void CPmRulesDialog::TakeRule(TInt aIndex)
	{
	TPmRule& r = iRule[aIndex];
	r.iWhat = ((CEikChoiceList*)Control(EPmDlgRuleWhat))->CurrentItem() == 0 ? 'F' : 'S';
	GetEdwinText(r.iWords, EPmDlgRuleWords);
	r.iWords.Trim();
	r.iAct = ((CEikChoiceList*)Control(EPmDlgRuleAct))->CurrentItem();
	TInt fi = ((CEikChoiceList*)Control(EPmDlgRuleFolder))->CurrentItem();
	r.iFolder.Zero();
	if (fi >= 0 && fi < iImap->Count())
		r.iFolder = (*iImap)[fi];
	}

void CPmRulesDialog::DimLines()
	{
	TInt act = ((CEikChoiceList*)Control(EPmDlgRuleAct))->CurrentItem();
	SetLineDimmedNow(EPmDlgRuleWhat, act == 0);
	SetLineDimmedNow(EPmDlgRuleWords, act == 0);
	SetLineDimmedNow(EPmDlgRuleFolder, act != 1 || !iImap->Count());
	}

void CPmRulesDialog::HandleControlStateChangeL(TInt aControlId)
	{
	if (aControlId == EPmDlgRuleNo)
		{
		TakeRule(iCurrent);
		iCurrent = ((CEikChoiceList*)Control(EPmDlgRuleNo))->CurrentItem();
		if (iCurrent < 0 || iCurrent >= KPmRules) iCurrent = 0;
		ShowRuleL(iCurrent);
		}
	else if (aControlId == EPmDlgRuleAct)
		DimLines();
	}

TBool CPmRulesDialog::OkToExitL(TInt /*aButtonId*/)
	{
	TakeRule(iCurrent);
	// one missing thing at a time, on the rule it belongs to
	for (TInt i = 0; i < KPmRules; i++)
		{
		const TPmRule& r = iRule[i];
		if (!r.iAct)
			continue;
		if (!r.iWords.Length() || (r.iAct == 1 && !r.iFolder.Length()))
			{
			if (i != iCurrent)
				{
				iCurrent = i;
				((CEikChoiceList*)Control(EPmDlgRuleNo))->SetCurrentItem(i);
				ShowRuleL(i);
				}
			TryChangeFocusToL(r.iWords.Length() ? EPmDlgRuleFolder : EPmDlgRuleWords);
			iEikonEnv->InfoMsg(r.iWords.Length() ? _L("No folder chosen") : _L("No words entered"));
			return EFalse;
			}
		}
	SaveL();
	return ETrue;
	}

// ----- the signatures ------------------------------------------------------------

// as the account dialog shows a signature: "\n" in the settings is " / " on the line
static void SigToLine(TDes& aSig)
	{
	TInt at;
	while ((at = aSig.Find(_L("\\n"))) >= 0)
		aSig.Replace(at, 2, _L(" / "));
	}

static void LineToSig(TDes& aSig)
	{
	aSig.Trim();
	TInt at;
	while ((at = aSig.Find(_L(" / "))) >= 0)
		aSig.Replace(at, 3, _L("\\n"));
	}

void CPmSigDialog::PreLayoutDynInitL()
	{
	PmAccount& a = iSettings.iAccounts[iAcct];
	TBuf<160> sig;
	TPtrC8 s8((const TUint8*)a.signature);
	sig.Copy(s8.Left(s8.Locate(0) >= 0 ? s8.Locate(0) : s8.Length()));
	SigToLine(sig);
	SetEdwinTextL(EPmDlgSigNew, &sig);
	TBuf<160> reply;
	PmReplySignature(iView, iAcct, reply);
	SigToLine(reply);
	SetEdwinTextL(EPmDlgSigReply, &reply);
	((CEikChoiceList*)Control(EPmDlgSigWhere))->SetCurrentItem((iSettings.iSpare[0] & KPmSigBelowBit) ? 1 : 0);
	}

TBool CPmSigDialog::OkToExitL(TInt /*aButtonId*/)
	{
	PmAccount& a = iSettings.iAccounts[iAcct];
	TBuf<200> sig;
	GetEdwinText(sig, EPmDlgSigNew);
	LineToSig(sig);
	TPtr8 s8((TUint8*)a.signature, sizeof(a.signature) - 1);
	s8.Copy(sig.Left(Min(sig.Length(), (TInt)sizeof(a.signature) - 1)));
	a.signature[s8.Length()] = 0;
	TBuf<200> reply;
	GetEdwinText(reply, EPmDlgSigReply);
	LineToSig(reply);
	TFileName path;
	AcctFile(iView, iAcct, _L("replysig.txt"), path);
	RFs& fs = iEikonEnv->FsSession();
	if (!reply.Length())
		fs.Delete(path);
	else
		{
		RFile f;
		User::LeaveIfError(f.Replace(fs, path, EFileWrite));
		TBuf8<200> r8;
		r8.Copy(reply);
		TInt err = f.Write(r8);
		f.Close();
		User::LeaveIfError(err);
		}
	if (((CEikChoiceList*)Control(EPmDlgSigWhere))->CurrentItem() == 1)
		iSettings.iSpare[0] |= KPmSigBelowBit;
	else
		iSettings.iSpare[0] &= ~KPmSigBelowBit;
	return ETrue;
	}

// the account's signature for replies ("" when it has none of its own)
void PmReplySignature(CPmView& aView, TInt aAcct, TDes& aOut)
	{
	aOut.Zero();
	TFileName path;
	AcctFile(aView, aAcct, _L("replysig.txt"), path);
	RFile f;
	if (f.Open(CEikonEnv::Static()->FsSession(), path, EFileRead | EFileShareReadersOnly) != KErrNone)
		return;
	TBuf8<160> b;
	f.Read(b);
	f.Close();
	aOut.Copy(b.Left(Min(b.Length(), aOut.MaxLength())));
	}
