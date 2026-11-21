/* psimail.h - memory shared between PsiMail.app and psimail.exe
 *
 * PsiMail.app (EIKON: screen, menus, dialogs) creates a global chunk with
 * this layout and starts psimail.exe, the mail engine. The engine does all
 * the network work - IMAP, SMTP, TLS 1.3 - and keeps the local mail store on
 * disk (see engine/store.c for the files). The app reads those files to show
 * folders, message lists and messages, and asks the engine for everything
 * else through a small command queue.
 *
 * 'net' comes first so PsiTerm's network glue (ssh/psiglue.cpp, which
 * expects a PsiShared at the start of the chunk) works unchanged.
 *
 * Every field has one writer, so no locking is needed on the single-CPU
 * Psion: the app writes the settings, the commands and cmd_head; the engine
 * writes everything marked (engine).
 */
#ifndef PSIMAIL_H
#define PSIMAIL_H

#ifndef PSI_SHARED_NAME
#define PSI_SHARED_NAME "PsiMailShared"
#endif
#include "psishared.h"     /* ssh/ - PsiTerm's shared-memory layout */

#define PM_MAGIC         0x4c49414du    /* 'MAIL' */
#define PM_MAX_ACCOUNTS  4
#define PM_CMDQ          16
#define PM_ARG_MAX       256

/* tls: how to talk to a server */
enum { PM_TLS_NONE = 0, PM_TLS_ON = 1, PM_TLS_STARTTLS = 2 };

/* PmAccount (a mail account) and PmCalendar (the CalDAV calendar) are
   psion-core's core/mail/engine/cal/calacct.h, so that PsiSync reads
   PsiMail's settings files with the same layout */
#include "calacct.h"

/* commands: the app fills in a PmCmd at cmd[cmd_head % PM_CMDQ], then
   increments cmd_head. The engine takes them in order. */
enum
	{
	PM_CMD_NONE = 0,
	PM_CMD_FOLDERS,          /* refresh the folder list (and unread counts) */
	PM_CMD_SYNC,             /* folder: new mail, flag changes, deletions */
	PM_CMD_OLDER,            /* folder: fetch older messages than we have */
	PM_CMD_BODY,             /* folder, uid: download the message text */
	PM_CMD_FULLBODY,         /* folder, uid: the whole text, past max_body_kb */
	PM_CMD_ATTACH,           /* folder, uid, arg = part id: save to attach_dir ("part\tdir": to dir) */
	PM_CMD_FLAG,             /* folder, uid, arg = "+S" "-S" "+F" "-F" (seen / flagged) */
	PM_CMD_MOVE,             /* folder, uid, arg = destination folder ("" = Trash) */
	PM_CMD_SEARCH,           /* folder, arg = words: results in search.idx */
	PM_CMD_SEND,             /* send everything in the outbox */
	PM_CMD_SENDRECV,         /* send the outbox, then sync INBOX and folder */
	PM_CMD_HANGUP,           /* close connections and free the serial port */
	PM_CMD_TRUST,            /* arg = "host:port": accept its certificate (pin) */
	PM_CMD_EXPUNGE,          /* folder: remove messages marked deleted */
	PM_CMD_CALSYNC,          /* calendar: send push.txt, fetch changes (arg "list": calendars only) */
	PM_CMD_UPDATE,           /* arg = "host:port" or "github", folder = where to save PsiMail.sis */
	PM_CMD_QUIT,
	/* folders (online only): the new IMAP name comes back in last_file */
	PM_CMD_MKFOLDER,         /* folder = parent ("" = top level), arg = the name to show */
	PM_CMD_RENFOLDER,        /* folder, arg = its new name */
	PM_CMD_DELFOLDER,        /* folder (not the Inbox or a standard folder) */
	PM_CMD_PICTURES,         /* folder, uid, arg = part ids to fetch and decode ("1.2 1.3"; "!2" = however big): see engine/pictures.c */
	PM_CMD_UNDO,             /* folder, uid: put back the message moved from there (Edit > Undo: engine/undo.c); last_file = its uid now */
	PM_CMD_WEBPICS,          /* folder, uid: fetch and set out the message's pictures from the web (engine/webpics.c) */
	PM_CMD_PUTLOG,           /* POST the engine's logs to the web proxy (0.86: Tools > Send log to server; webpics.c pm_send_log) */
	PM_CMD_IDLE              /* (1.01) wait on the Inbox for new mail (IMAP IDLE), arg = the most seconds; then sync it if any came */
	};

/* pictures in a message (engine/pictures.c, app/pmpict.cpp): a part this
   big (its size in the message, base64 and all) or smaller is fetched
   without asking when the message is read; bigger ones wait for a tap */
#define PM_PIC_AUTO_KB     300
#define PM_PIC_AUTO_TOTAL_KB 1024    /* all of a message's automatic pictures together */
#define PM_PIC_MAX_KB      4096      /* never fetched: too big for the line */
#define PM_PIC_MAX_W       544       /* a decoded picture is at most this wide (the reader beside the toolbar) */
#define PM_PIC_MAX_H       960
#define PM_PIC_BUDGET_KB   600       /* decoded pictures kept for one message, all together */

typedef struct
	{
	int op;
	int acct;
	unsigned int uid;
	char folder[128];        /* IMAP name (modified UTF-7), as in folders.txt */
	char arg[PM_ARG_MAX];
	} PmCmd;

/* a command's outcome (engine): what last_res, last_msg and last_file say
   for the last one, kept per command */
typedef struct
	{
	int op;
	int res;                 /* PM_RES_* */
	unsigned int uid;
	char msg[160];
	char file[128];
	int update_ready;        /* (PM_CMD_UPDATE) */
	} PmDone;

/* engine state */
enum { PM_STATE_STARTING = 0, PM_STATE_READY, PM_STATE_EXITED };

/* result of the last command */
enum
	{
	PM_RES_OK = 0,
	PM_RES_FAILED,           /* last_msg says why */
	PM_RES_OFFLINE,          /* queued: will happen at the next connection */
	PM_RES_CANCELLED,
	PM_RES_UNTRUSTED,        /* certificate not trusted: see trust_* */
	PM_RES_NEED_PASS,        /* the account has no password: ask for one */
	PM_RES_LOGIN_FAILED
	};

typedef struct
	{
	PsiShared net;                  /* MUST stay first - see above */

	unsigned int magic;
	volatile int state;             /* PM_STATE_* (engine) */
	volatile int quitting;          /* app: exit now */
	volatile unsigned int app_beat; /* app: +1 every tick; engine quits if it stops */
	char exit_msg[128];             /* (engine) why it stopped */

	/* settings (app) */
	char store_dir[96];             /* e.g. "D:\\System\\Data\\PsiMail\\" (ends with \) */
	char attach_dir[96];            /* where attachments are saved */
	int offline;                    /* 1 = never dial: queue changes */
	int prefetch;                   /* after a sync, download the text of the newest N (0 = off) */
	int web_pic_proxy;              /* 1 = fetch web pictures via the shared HTTP proxy (webpics.c) */
	int use_proxy;                  /* (0.87) the shared proxy (PsiLink.ini): 1 = on, and it has a host */
	char proxy_host[64];            /*   its host */
	int proxy_port;                 /*   its port */
	PmAccount acct[PM_MAX_ACCOUNTS];
	volatile unsigned int acct_seq; /* app bumps after changing acct[] */
	PmCalendar cal;

	/* command queue */
	volatile unsigned int cmd_head; /* app */
	volatile unsigned int cmd_tail; /* engine */
	PmCmd cmd[PM_CMDQ];

	/* progress (engine) */
	volatile int busy;              /* a command is running */
	volatile int online;            /* a connection is open */
	volatile int cur_op;
	char progress[128];             /* "Fetching 12 of 50..." */

	/* results (engine) */
	volatile unsigned int done_seq; /* +1 after each command */
	volatile int last_op;
	volatile int last_acct;
	volatile unsigned int last_uid;
	volatile int last_res;          /* PM_RES_* */
	char last_msg[160];
	char last_folder[128];
	char last_file[128];            /* e.g. the attachment saved */
	volatile unsigned int changed_seq; /* +1 whenever store files change */
	volatile int new_mail;          /* INBOX messages that arrived in the last sync */

	/* PM_RES_UNTRUSTED: what to show the user (engine) */
	char trust_host[80];            /* "imap.example.com:993" */
	char trust_why[96];
	char trust_fp[100];             /* SHA-256 of the key, hex with colons */

	volatile unsigned int heap_used;

	/* Tools > Update PsiMail (the version running is in net.version) */
	volatile int update_ready;      /* 1: last_file is a checked, newer PsiMail.sis */
	char update_version[16];        /* what the server has */

	/* (0.75) the heartbeat's other half, and the log (app) */
	volatile unsigned int app_pid;  /* PsiMail.app's process id: the engine quits on a silent heartbeat only once that has gone */
	char app_note[96];              /* why the last engine ended ("panic KERN-EXEC 3"), for psimail.log */

	/* every command's outcome, by sequence (engine): done[(seq - 1) % PM_CMDQ]
	   is the command whose completion took done_seq to seq. The app walks
	   them in order, so two commands finishing between its ticks are both
	   acted on (last_* alone held only the later one). */
	PmDone done[PM_CMDQ];

	/* (1.03, app) the machine, for User-Agent ("Psion Series 5mx", the
	   machine's own name), and the screen for X-Psi-Screen on web picture
	   requests ("640x480x256": PsiProxy and the Atom then size and colour
	   pictures for it); "" before the app has said */
	char machine[32];
	char screen[16];

	/* (1.11, engine) news of the store the app shows once, in a dialog:
	   "title TAB line" ("Unsent mail kept", "3 messages of dan@...: set
	   that account up again to send them"); +1 on store_note_seq each time */
	volatile unsigned int store_note_seq;
	char store_note[200];

	/* (1.15) short of memory (the memory watcher's level, psi_mem_level):
	   trim_req (app) gives the heap's free end back at the next idle
	   moment (the engine clears it); keep_awake (app) is set while the
	   engine must stay - PsiMail in front, a dialog up, a timed check or
	   new mail at once wanted; slept (engine) says it ended because the
	   memory was low with nothing to do, so the app starts it again on
	   the next command rather than calling it a crash */
	volatile int trim_req;
	volatile int keep_awake;
	volatile int slept;
	} PmShared;

#endif
