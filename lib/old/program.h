/* The MARE $program code is shown here in full. It is extremely buggy and is
   urged to be used, if at all, under restricted POW_SECURITY permissions.
   -- Copyright 1995, by Byron Stanoszek */


/* hdrs/program.h */

#define PQUEUE_LIMIT 10
#define MAX_PROCESSES 65535
#define MAX_DEPTH 20

#define PERM_OX		0x1
#define PERM_OW		0x2
#define PERM_OR		0x4
#define PERM_UX		0x10
#define PERM_UW		0x20
#define PERM_UR		0x40
#define PERM_INH	0x1000
#define PERM_STK	0x2000
#define PERM_TMP	0x4000

#define ERR_OK		0
#define ERR_FORK	1
#define ERR_NOPROG	2
#define ERR_SYNTAX	3
#define ERR_FENDSWI	4
#define ERR_SIGDEPTH	5
#define ERR_ILLRANGE	6
#define ERR_NOENDSIG	7
#define ERR_FENDSIG	8
#define ERR_NOENDFUNC	9
#define ERR_FENDFUNC	10
#define ERR_MAXDEPTH	11
#define ERR_NODEPTH	12
#define ERR_FRETURN	13
#define ERR_NOFUNCTION	14
#define ERR_NOARG	15
#define ERR_FDONE	16
#define ERR_NODONE	17
#define ERR_FBREAK	18
#define ERR_FENDFOR	19
#define ERR_UNDEF	20
#define ERR_NOENDSWI	21
#define ERR_NOENDIF	22
#define ERR_NOMARKER	23
#define ERR_FCONT	24
#define ERR_NOMEM	25

#define TOP_ERRORMSG	25

/* program runtime signals */
#define PSIG_CHILD	1
#define PSIG_ALARM	2
#define PSIG_PARENT	3

#define TOP_SIGNAL	3

extern int current_process_id;
extern int prog_exec;


struct all_prog_list {
  struct all_prog_list *next;
  struct progcon *type;
  dbref owner;
  dbref thing;
  unsigned int line:16;
};

struct progdata {
  struct progdata *next;
  char *line;
};

struct progcon {
  struct progcon *next;
  char *name;
  char *cmdstr;
  char *header;
  char *lock;
  unsigned int perms:16;
  unsigned int maxline:16;
  struct progdata *line;
};

struct progltype {
  unsigned int line:16; /* actual program line number */
  char *str;            /* command contents */
};

typedef struct varlist VLIST;
struct varlist {
  VLIST *next;
  char *vname;        /* variable name */
  char *str;          /* variable contents */
};

typedef struct progque PROGQUE;

struct progque {
  PROGQUE *next;
  unsigned int pid:16;  	/* Process-Id */
  unsigned int ppid:16; 	/* Parent Process-Id */
  dbref uid;            	/* Player who owns the PID */
  dbref oid;            	/* Player whom enacts upon the UID; Cause */
  char *program;        	/* Program name */
  unsigned int pmode:16;	/* Permission Modes for the Program */
  unsigned int lnum:16;         /* Current Line Number */
  unsigned int mlin:16;         /* Last/Maximum Line Number */
  char status[20];		/* PID Status */
  unsigned int priority:8;	/* Priority; Number of Commands Executed */
  unsigned int alarm_ticks:16;	/* Ticks till next alarm */
  int cpu_time;			/* Accumlative CPU Time used */
  unsigned int depth:5;         /* Depth of Program */
  unsigned int current_sig:8;   /* Current Signal in-pending */
  unsigned int bkgnd:1;         /* Background Suppress-Halt Program Parent Flag */
  int console;			/* Attached Console Connection-Id */
  struct progltype *proglin;    /* Program Line Structure */
  struct varlist *vlist;	/* Variable Structure */
  int prog_return[MAX_DEPTH];	/* What line to return to */
  char *qargs[MAX_DEPTH][10];	/* Environment */
  int sig_pending[MAX_DEPTH];	/* Additional Signals Pending */
  char *result_arg[MAX_DEPTH];	/* Result Argument from Functions */
  int state[MAX_DEPTH];		/* Specifies type of control loop */
  int loop_count[MAX_DEPTH];	/* Counter for control loop */
  char *loop_pure[MAX_DEPTH];	/* Contents of 'for' */
};

extern PROGQUE *qprog;
