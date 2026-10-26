// PMLOGIN.CPP - when the server refuses the login (issue 1)
//
// A login the server refuses (or an account with no password saved) is
// asked about once, in one dialog: "Log in to <account>", with what the
// server said in its own words ('no such user', 'invalid credentials'), the
// user name and the password, and Account... for the servers and the rest
// of the account's settings. A wrong user name can be put right there, not
// only the password.
//
// PsiMail used to ask for the password alone, after an information
// window, and asked again for every command that failed the same way: the
// folders and the Inbox are fetched together, so there were always two;
// they came on top of each other, because the tick that hands the engine's
// results over ran inside the dialogs; each OK tried again and failed again
// with the same user name; and opening a folder or the timed check brought
// the same questions back. Someone with the user name wrong could not get
// out of the dialogs to Tools > Accounts.
//
// Now:
//  * TickL hands results over one at a time (iInResults): a dialog the
//    result brings is answered before the next result is looked at.
//  * The commands that were already queued when the question was answered
//    (the Inbox after the folders, say) fail quietly: they are the same
//    failure, and the answer covers them (iLoginQuietTo). One that is sent
//    after the answer (the retry, or the next thing the user does) is asked
//    about again: once for each thing the user does.
//  * Cancel leaves PsiMail usable with the mail it has, and says how to put
//    the account right ("Not logged in - ..."). Until the user does (Account
//    settings, Check mail, Refresh, or another account), PsiMail does not
//    try by itself: no timed check, no sending what waits, no look at a
//    folder as it opens (iLoginHeld). What the user asks for in the meantime
//    fails with the same infoprint, without the dialog.
//  * A login failure while another dialog or a menu is up (the first-run
//    "Use the Email icon for PsiMail?", say) is not asked about on top of
//    it: the question waits until that has gone (LoginDeferredL).
// The same goes for "Certificate not trusted": the commands queued with the
// first are tried again after Yes (they go after the TRUST command), and
// are not asked about again (UntrustedL).

#include "pmapp.h"

_LIT(KNotLoggedIn, "Not logged in - change the account in Tools > Accounts > Settings");

static void LoginFromC(TDes& aDst, const char* aSrc)
	{
	TPtrC8 p((const TUint8*)aSrc);
	aDst.Copy(p.Left(p.Length() < aDst.MaxLength() ? p.Length() : aDst.MaxLength()));
	}

static void LoginToC(char* aDst, TInt aMax, const TDesC& aSrc)
	{
	TInt n = aSrc.Length() < aMax - 1 ? aSrc.Length() : aMax - 1;
	TPtr8 d((TUint8*)aDst, aMax);
	d.Copy(aSrc.Left(n));
	aDst[n] = 0;
	}

// text that may not fit: cut, with "..." (the dialog's lines are a fixed width)
static void LoginClip(TDes& aDst, const TDesC& aSrc, TInt aMax)
	{
	if (aMax > aDst.MaxLength())
		aMax = aDst.MaxLength();
	if (aSrc.Length() <= aMax)
		{
		aDst = aSrc;
		return;
		}
	aDst = aSrc.Left(aMax - 3);
	aDst.Append(_L("..."));
	}

// is done number aSeq (iDoneSeen's count) at or before aUpto? (they wrap)
static TBool SeqUpTo(TUint aSeq, TUint aUpto)
	{
	return (TInt)(aSeq - aUpto) <= 0;
	}

// ----- the dialog --------------------------------------------------------------

void CPmLoginDialog::PreLayoutDynInitL()
	{
	SetTitleL(iTitle);
	SetLabelL(EPmDlgInfo1, iWhat);
	SetLabelL(EPmDlgInfo2, iWhy);
	SetLabelL(EPmDlgInfo3, iNeedPass ? _L("The password is kept on this Psion")
		: _L("Leave the password blank to keep the saved one"));
	SetEdwinTextL(EPmDlgUser, &iUser);
	TryChangeFocusToL(iNeedPass ? EPmDlgPass : EPmDlgUser);
	}

TBool CPmLoginDialog::OkToExitL(TInt aButtonId)
	{
	if (aButtonId == EPmBidAccount)
		return ETrue;                    // the account dialog, after this one
	GetEdwinText(iUser, EPmDlgUser);
	iUser.Trim();
	if (iUser.Length() == 0)
		{
		iEikonEnv->InfoMsg(_L("No user name entered"));
		TryChangeFocusToL(EPmDlgUser);
		return EFalse;
		}
	GetSecretEditorText(iPass, EPmDlgPass);
	if (iNeedPass && iPass.Length() == 0)
		{
		iEikonEnv->InfoMsg(_L("No password entered"));
		TryChangeFocusToL(EPmDlgPass);
		return EFalse;
		}
	return ETrue;
	}

// ----- the view ----------------------------------------------------------------

// Not to be asked about (again): quiet, with the infoprint. ETrue if so.
TBool CPmView::LoginQuietL(const PmCmd& aCmd)
	{
	TBool sibling = SeqUpTo(iDoneSeen, iLoginQuietTo) && iLoginQuietTo != 0;
	TBool other = aCmd.acct != iSettings->iAcct;     // (switched account since: not this one's question)
	TBool covered = iEikonEnv->EikAppUi()->IsDisplayingMenuOrDialog();
	if (!sibling && !iLoginHeld && !other && !iLoginDeferred && !covered)
		return EFalse;
	if (iLoginDeferred && !other)
		return ETrue;                    // (the question is waiting already: it covers this one)
	if (covered && !other && !sibling && !iLoginHeld)
		{
		// asked once the other dialog has gone (TickL)
		iLoginDeferred = ETrue;
		iLoginCmd = aCmd;
		iLoginRes = iShared->last_res;
		LoginFromC(iLoginMsg, iShared->last_msg);
		return ETrue;
		}
	if ((aCmd.op == PM_CMD_BODY || aCmd.op == PM_CMD_FULLBODY) && aCmd.uid == iMsgUid)
		iBodyError = _L("Not downloaded - not logged in");
	if (aCmd.op == PM_CMD_ATTACH)
		iFwdPending = 0;
	if (!other)
		Toast(KNotLoggedIn);
	return ETrue;
	}

void CPmView::LoginHoldL()
	{
	iLoginHeld = ETrue;
	Toast(KNotLoggedIn);
	}

// the same command again, now the account has changed
void CPmView::LoginRetryL(const PmCmd& aCmd)
	{
	if (iClosing || iSettings->iOffline)
		return;
	if (aCmd.op == PM_CMD_SENDRECV && !OpInFlight(PM_CMD_SENDRECV))
		{
		SendRecvL();                     // (the calendar after it, as from the menu)
		return;
		}
	if (OpInFlight(aCmd.op) && aCmd.op != PM_CMD_BODY && aCmd.op != PM_CMD_ATTACH)
		return;                          // (queued again already: the account's first fetch)
	Cmd(aCmd.op, TPtrC8((const TUint8*)aCmd.folder), aCmd.uid, TPtrC8((const TUint8*)aCmd.arg));
	}

// PM_RES_LOGIN_FAILED or PM_RES_NEED_PASS from a mail command
void CPmView::LoginFailedL(const PmCmd& aCmd, TInt aRes, const TDesC& aMsg)
	{
	if (iClosing || LoginQuietL(aCmd))
		return;
	LoginAskL(aCmd, aRes, aMsg);
	}

void CPmView::LoginDeferredL()
	{
	if (!iLoginDeferred || iInResults || iClosing || iEikonEnv->EikAppUi()->IsDisplayingMenuOrDialog())
		return;
	iLoginDeferred = EFalse;
	if (iLoginCmd.acct != iSettings->iAcct || iLoginHeld)
		return;                          // (another account now, or Cancel said since)
	PmCmd cmd = iLoginCmd;
	TBuf<160> msg(iLoginMsg);
	iInResults = ETrue;                  // (its dialog is answered before the next result is looked at)
	TRAPD(err, LoginAskL(cmd, iLoginRes, msg));
	iInResults = EFalse;
	User::LeaveIfError(err);
	}

void CPmView::LoginAskL(const PmCmd& aCmd, TInt aRes, const TDesC& aMsg)
	{
	PmAccount& a = iSettings->iAccounts[aCmd.acct];
	TBool needPass = aRes == PM_RES_NEED_PASS;
	TBuf<40> name;
	LoginFromC(name, a.name);
	TBuf<60> title(_L("Log in to "));
	title.Append(name.Left(name.Length() < 40 ? name.Length() : 40));
	// which server, and its own words: "Login: 535 ..." is the SMTP server's
	// (engine/smtp.c), anything else the IMAP server's (engine/imap.c)
	TBool smtp = aMsg.Length() >= 7 && aMsg.Left(7).CompareF(_L("Login: ")) == 0;
	TBuf<64> host;
	LoginFromC(host, smtp ? a.smtp_host : a.imap_host);
	TBuf<120> what;
	TBuf<120> why;
	if (needPass)
		{
		what = _L("No password saved for this account");
		why = _L("Enter it to log in to ");
		why.Append(host.Left(host.Length() < 50 ? host.Length() : 50));
		}
	else
		{
		TBuf<120> h;
		h = host;
		h.Append(_L(" refused the login"));
		LoginClip(what, h, 46);
		TPtrC said = smtp ? aMsg.Mid(7) : TPtrC(aMsg);
		TBuf<160> w(_L("The server said '"));
		w.Append(said.Left(said.Length() < 120 ? said.Length() : 120));
		w.Append('\'');
		LoginClip(why, w, 62);
		}
	TBuf<96> user;
	LoginFromC(user, a.user);
	TBuf<32> pass;
	CPmLoginDialog* dlg = new(ELeave) CPmLoginDialog(title, what, why, user, pass, needPass);
	TInt button = dlg->ExecuteLD(R_PM_LOGIN_DIALOG);
	// everything queued before this answer is covered by it (LoginQuietL)
	iLoginQuietTo = iShared->cmd_head;
	if (iClosing)
		return;
	if (button == EPmBidAccount)
		{
		CPmAppUi* ui = (CPmAppUi*)iEikonEnv->EikAppUi();
		if (!ui->EditCurrentAccountL())
			{
			LoginHoldL();
			return;
			}
		iLoginHeld = EFalse;
		LoginRetryL(aCmd);
		return;
		}
	if (!button)
		{
		LoginHoldL();                    // Cancel: PsiMail stays usable with the mail it has
		return;
		}
	LoginToC(a.user, sizeof(a.user), user);
	if (pass.Length())
		LoginToC(a.pass, sizeof(a.pass), pass);
	((CPmAppUi*)iEikonEnv->EikAppUi())->SaveSettings();
	CopySettingsToShared();
	iLoginHeld = EFalse;
	LoginRetryL(aCmd);
	}

// PM_RES_UNTRUSTED: the certificate dialog, once for the commands queued
// together; after Yes they are tried again behind the TRUST command
void CPmView::UntrustedL(const PmCmd& aCmd)
	{
	if (iClosing)
		return;
	TBuf<80> host;
	LoginFromC(host, iShared->trust_host);
	if (iTrustQuietTo && SeqUpTo(iDoneSeen, iTrustQuietTo) && host == iTrustHost)
		{
		if (iTrustYes)
			{
			if (aCmd.op == PM_CMD_CALSYNC) iCalPending = ETrue;
			Cmd(aCmd.op, TPtrC8((const TUint8*)aCmd.folder), aCmd.uid, TPtrC8((const TUint8*)aCmd.arg));
			}
		return;
		}
	TBuf<100> why;
	LoginFromC(why, iShared->trust_why);
	TBuf<100> fp;
	LoginFromC(fp, iShared->trust_fp);
	TBuf<200> lines[4];
	lines[0] = _L("PsiMail can't be sure it is talking to");
	lines[1] = host;
	lines[1].Append(_L(": "));
	lines[1].Append(why.Left(why.Length() < 110 ? why.Length() : 110));
	lines[2] = _L("Key: ");
	lines[2].Append(fp.Left(fp.Length() < 95 ? fp.Length() : 95));
	lines[3] = _L("Trust it only if you expected this (e.g. your own server)");
	TPtrC ptrs[4];
	for (TInt k = 0; k < 4; k++) ptrs[k].Set(lines[k]);
	CPmInfoDialog* dlg = new(ELeave) CPmInfoDialog(_L("Certificate not trusted"), ptrs, 4);
	dlg->ExecuteLD(R_PM_INFO_DIALOG);
	TBool yes = !iClosing && iEikonEnv->QueryWinL(host, _L("Trust this server's key from now on?"));
	iTrustQuietTo = iShared->cmd_head;
	iTrustHost = host;
	iTrustYes = yes;
	if (!yes || iClosing)
		return;
	TBuf8<80> hp;
	hp.Copy(host);
	Cmd(PM_CMD_TRUST, KNullDesC8, 0, hp);
	// and try again
	if (aCmd.op == PM_CMD_CALSYNC) iCalPending = ETrue;
	Cmd(aCmd.op, TPtrC8((const TUint8*)aCmd.folder), aCmd.uid, TPtrC8((const TUint8*)aCmd.arg));
	}

// ----- the app UI ----------------------------------------------------------------

TBool CPmAppUi::EditCurrentAccountL()
	{
	if (!EditAccountL(iSettings.iAcct, EFalse))
		return EFalse;
	iView->AccountChangedL();
	return ETrue;
	}
