// PMAUTO.CPP - what PsiMail does by itself (0.74):
//
//  * New mail alert: when a check finds new mail in the Inbox, EIKON's beep
//    and an infoprint ("2 new messages"); from the background the infoprint
//    is the window server's (User::InfoPrint), which shows over whatever
//    program is in front. Preferences > New mail: Sound & message / Message
//    only / Off.
//  * A timed check of the Inbox (Off / every 10, 15, 30 minutes / every
//    hour), only while PsiMail runs. A CPeriodic every 5 minutes counts the
//    time - RTimer::After stops while the Psion is switched off, so it never
//    wakes the machine - and the check waits while the engine is busy
//    (another command, or downloading ahead), is never made while working
//    offline, and connects only if the preference says so (otherwise only
//    while a connection is already open). It is quiet: only new mail is
//    told, not "No new messages" nor a failure.
//  * Sending what waits: when a connection comes up for any reason, or
//    Work offline is turned off, messages waiting in the Outbox go (not
//    drafts, and not ones that failed: those wait for Check mail, so a bad
//    message doesn't go round at every connection). Never while working
//    offline. Preferences > New mail: Send waiting mail when connected.
//
// The settings are bits of TPmSettings::iView (KPmView* in pmapp.h), so
// the settings file keeps its size.

#include <eikdialg.h>
#include <eikchlst.h>
#include "pmapp.h"

const TInt KPmCheckTick = 5 * 60 * 1000000;      // 5 minutes (CPeriodic's interval is 32-bit microseconds)

static TInt CheckMinutes(const TPmSettings& aSettings)
	{
	switch ((aSettings.iView & KPmViewCheckMask) >> KPmViewCheckShift)
		{
	case 1: return 10;
	case 2: return 15;
	case 3: return 30;
	case 4: return 60;
	case 5: return 10;                   // (1.02) at once: IDLE where it can, else every 10 minutes
	default: return 0;
		}
	}

TBool CPmView::InBackground() const
	{
	return iCoeEnv->WsSession().GetFocusWindowGroup() != iCoeEnv->RootWin().Identifier();
	}

// ----- the new mail alert ----------------------------------------------------

void CPmView::NewMailAlertL(const PmCmd& aCmd, TBool aAuto)
	{
	TPtrC8 folder((const TUint8*)aCmd.folder);
	if (aCmd.op != PM_CMD_SENDRECV && aCmd.op != PM_CMD_IDLE && !(aCmd.op == PM_CMD_SYNC && folder.CompareF(_L8("INBOX")) == 0))
		return;
	TInt n = iShared->new_mail;
	TInt alert = (iSettings->iView & KPmViewAlertMask) >> KPmViewAlertShift;
	if (n <= 0 || alert == 2)
		return;
	if (alert == 0)
		CEikonEnv::Beep();
	TBuf<64> t;
	if (n == 1)
		t = _L("1 new message");
	else
		t.Format(_L("%d new messages"), n);
	if (InBackground())
		{
		// EIKON's infoprint is in PsiMail's window group, behind the program
		// in front: the window server's own shows over it
		t.Append(_L(" - PsiMail"));
		User::InfoPrint(t);
		}
	else if (aAuto)
		iEikonEnv->InfoMsg(t);
	// (in front, a check the user asked for has already said how many came)
	}

// ----- the timed check -------------------------------------------------------

TInt CPmView::CheckCallback(TAny* aSelf)
	{
	TRAPD(err, ((CPmView*)aSelf)->CheckDueL());
	(void)err;
	return 1;
	}

void CPmView::AutoSettingsChanged()
	{
	iIdleBroken = EFalse;                      // (1.02: tried again)
	if (!CheckMinutes(*iSettings))
		{
		delete iCheckTimer;
		iCheckTimer = NULL;
		iCheckTicks = 0;
		return;
		}
	if (iCheckTimer)
		return;                          // (the minutes are looked at each tick)
	TRAPD(err, iCheckTimer = CPeriodic::NewL(CActive::EPriorityIdle));
	if (err == KErrNone)
		iCheckTimer->Start(KPmCheckTick, KPmCheckTick, TCallBack(CheckCallback, this));
	}

void CPmView::CheckDueL()
	{
	TInt minutes = CheckMinutes(*iSettings);
	if (!minutes)
		return;
	if (++iCheckTicks * 5 < minutes)
		return;
	if (iMode == ENoAccount || (!iRunning && !iEngineAsleep) || iSettings->iOffline || iLoginHeld)
		{
		iCheckTicks = 0;                 // (working offline: never dial; a refused login: not until it is put right)
		return;
		}
	// the engine busy (a command, downloading ahead, the calendar): in 5 minutes
	if (Busy() || iAutoCmd || CalendarBusy())
		return;
	iCheckTicks = 0;
	if (!iShared->online && !(iSettings->iView & KPmViewCheckConnect))
		return;                          // only while connected
	iAutoCmd = iShared->cmd_head + 1;    // (done_seq when it is done)
	Cmd(PM_CMD_SYNC, _L8("INBOX"), 0, KNullDesC8);
	}

// ----- (1.02) new mail at once ---------------------------------------------
//
// Preferences > New mail > Check for new mail: At once. On a route that
// stays connected (Psion TCP/IP: a network card, or a dial-up kept up) and
// with a connection open, the engine waits on the Inbox (PM_CMD_IDLE, IMAP
// IDLE, 25 minutes at a time) and brings new mail as it comes; any other
// command ends the wait within a second. On the modem route the line is
// dropped after use, so it is a check every 10 minutes instead. A server
// without IDLE: the same, until the preference is set again.

// ----- (1.15) short of memory (MEM-55) ----------------------------------------
//
// The engine (psimail.exe, about 0.8 MB of code and heap) need not run
// while PsiMail is behind other programs with nothing to do. When the
// memory watcher (PsiHub, core psimemlvl.h) says memory is low, the engine
// ends by itself at an idle moment (engine/pmmain.c, pm_loop) unless the
// app says it must stay (keep_awake); on "sleep" the app ends it
// (MemWordL). Either way it is woken quietly by the next command, or when
// PsiMail comes to the front (WakeEngineL).

// what needs the engine even behind other programs
TBool CPmView::KeepEngine() const
	{
	if (iMode == ENoAccount)
		return EFalse;
	if (!iSettings->iOffline && CheckMinutes(*iSettings))
		return ETrue;                    // a timed check, or new mail at once
	return iIdleCmd || CalendarBusy() || iUpdDlg;
	}

// a transfer running or queued, or an update: not to be put to sleep
TBool CPmView::MemBusy() const
	{
	return (iRunning && iShared && Busy()) || iUpdDlg || CalendarBusy();
	}

void CPmView::MemTick(TBool aFront)
	{
	if (iShared && iRunning)
		iShared->keep_awake = aFront || KeepEngine() || iUpdDlg ? 1 : 0;
	((CPmAppUi*)iEikonEnv->EikAppUi())->SetMemBusy(MemBusy());
	}

TBool CPmView::IdleWanted() const
	{
	return ((iSettings->iView & KPmViewCheckMask) >> KPmViewCheckShift) == 5 && !iIdleBroken &&
		iSettings->iNetMode == 1 && !iSettings->iOffline && iRunning && iMode != ENoAccount &&
		!iLoginHeld && iShared && iShared->online;
	}

void CPmView::IdleTickL()
	{
	if (iIdleCmd || !IdleWanted() || Busy() || iAutoCmd || CalendarBusy())
		return;
	iIdleCmd = iShared->cmd_head + 1;          // (done_seq when it is done)
	Cmd(PM_CMD_IDLE, _L8("INBOX"), 0, _L8("1500"));
	}

// a timed check's result: quiet, except for new mail
TBool CPmView::AutoResultL(const PmCmd& aCmd)
	{
	if (aCmd.op == PM_CMD_IDLE)
		{
		iIdleCmd = 0;
		TInt res = iShared->last_res;
		if (res == PM_RES_FAILED)
			iIdleBroken = ETrue;               // (no IDLE at the server: the 10 minutes)
		if (res == PM_RES_OK && iShared->new_mail > 0)
			{
			ReloadL();
			NewMailAlertL(aCmd, ETrue);
			}
		return ETrue;                          // (quiet: nothing to say when nothing came)
		}
	if (!iAutoCmd || iDoneSeen != iAutoCmd || aCmd.op != PM_CMD_SYNC)
		return EFalse;
	iAutoCmd = 0;
	TInt res = iShared->last_res;
	if (res == PM_RES_OK || res == PM_RES_OFFLINE || res == PM_RES_FAILED)
		ReloadL();
	if (res == PM_RES_OK)
		NewMailAlertL(aCmd, ETrue);
	return ETrue;
	}

// ----- sending what waits ----------------------------------------------------

// Outbox messages to send: not drafts, and not ones that have failed
TInt CPmView::WaitingToSend()
	{
	RFs& fs = iCoeEnv->FsSession();
	TBuf<120> dir;
	OutboxDir(dir);
	TBuf<130> spec(dir);
	spec.Append(_L("*.txt"));
	CDir* list = NULL;
	if (fs.GetDir(spec, KEntryAttNormal, ESortByName, list) != KErrNone || !list)
		return 0;
	TInt n = 0;
	for (TInt i = 0; i < list->Count(); i++)
		{
		const TDesC& name = (*list)[i].iName;
		TFileName p(dir);
		p.Append(name.Left(name.Length() - 4));
		p.Append(_L(".err"));
		TEntry e;
		if (fs.Entry(p, e) == KErrNone)
			continue;                    // failed: it waits for Check mail
		p = dir;
		p.Append(name);
		RFile f;
		if (f.Open(fs, p, EFileRead | EFileShareReadersOnly) != KErrNone)
			continue;
		TBuf8<512> head;
		f.Read(head);
		f.Close();
		TInt end = head.Find(_L8("\n\n"));
		TPtrC8 h = end >= 0 ? head.Left(end + 1) : TPtrC8(head);
		if (h.Find(_L8("\nDraft: 1")) >= 0 || (h.Length() >= 8 && h.Left(8) == _L8("Draft: 1")))
			continue;                    // a draft: not for sending
		n++;
		}
	delete list;
	return n;
	}

void CPmView::SendWaitingL()
	{
	TInt n = WaitingToSend();
	if (!n)
		return;
	Cmd(PM_CMD_SEND, KNullDesC8, 0, KNullDesC8);
	TBuf<60> t;
	if (n == 1)
		t = _L("Sending 1 waiting message\x85");
	else
		t.Format(_L("Sending %d waiting messages\x85"), n);
	Working(t);
	}

// from TickL: a connection has come up
void CPmView::AutoTickL()
	{
	PmShared* s = iShared;
	if (iAutoCmd && (TInt)(s->done_seq - iAutoCmd) >= 0)
		iAutoCmd = 0;                    // (its result was passed over in a busy tick)
	TBool up = s->online && !iAutoOnlineWas;
	iAutoOnlineWas = s->online;
	if (!up || iSettings->iOffline || (iSettings->iView & KPmViewNoAutoSend) || iMode == ENoAccount || iLoginHeld)
		return;
	if (OpInFlight(PM_CMD_SEND) || OpInFlight(PM_CMD_SENDRECV))
		return;                          // (sending already)
	SendWaitingL();
	}

// Work offline has been turned off
void CPmView::LeftOfflineL()
	{
	if (iSettings->iOffline || (iSettings->iView & KPmViewNoAutoSend) || iMode == ENoAccount || iLoginHeld)
		return;
	if (OpInFlight(PM_CMD_SEND) || OpInFlight(PM_CMD_SENDRECV))
		return;
	SendWaitingL();
	}

// ----- Preferences > New mail ------------------------------------------------

void CPmPrefsDialog::NewMailInitL()
	{
	TInt v = iSettings.iView;
	TInt a = (v & KPmViewAlertMask) >> KPmViewAlertShift;
	SetChoiceListCurrentItem(EPmDlgAlert, a <= 2 ? a : 0);
	TInt c = (v & KPmViewCheckMask) >> KPmViewCheckShift;
	if (c > 5) c = 0;                    // (5: at once, 1.02)
	SetChoiceListCurrentItem(EPmDlgCheckEvery, c);
	SetChoiceListCurrentItem(EPmDlgCheckConnect, (v & KPmViewCheckConnect) ? 1 : 0);
	// (1.14 tick boxes; the stored bits keep their meaning)
	SetCheckBoxState(EPmDlgAutoSend, (v & KPmViewNoAutoSend) ? CEikButtonBase::EClear : CEikButtonBase::ESet);
	SetCheckBoxState(EPmDlgDetailed, (v & KPmViewDetailedProgress) ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	SetLineDimmedNow(EPmDlgCheckConnect, c == 0);
	}

void CPmPrefsDialog::NewMailSave()
	{
	TInt v = iSettings.iView & ~(KPmViewAlertMask | KPmViewCheckMask | KPmViewCheckConnect | KPmViewNoAutoSend | KPmViewDetailedProgress);
	v |= (ChoiceListCurrentItem(EPmDlgAlert) << KPmViewAlertShift) & KPmViewAlertMask;
	v |= (ChoiceListCurrentItem(EPmDlgCheckEvery) << KPmViewCheckShift) & KPmViewCheckMask;
	if (ChoiceListCurrentItem(EPmDlgCheckConnect) == 1)
		v |= KPmViewCheckConnect;
	if (CheckBoxState(EPmDlgAutoSend) != CEikButtonBase::ESet)   // not ticked: don't send by itself
		v |= KPmViewNoAutoSend;
	if (CheckBoxState(EPmDlgDetailed) == CEikButtonBase::ESet)
		v |= KPmViewDetailedProgress;
	iSettings.iView = v;
	}

// "If not connected" belongs to the timed check: dimmed while it is off
void CPmPrefsDialog::HandleControlStateChangeL(TInt aControlId)
	{
	if (aControlId == EPmDlgCheckEvery)
		SetLineDimmedNow(EPmDlgCheckConnect, ChoiceListCurrentItem(EPmDlgCheckEvery) == 0);
	}
