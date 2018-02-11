/* hdrs/queue.h */
/* definitions for queueing commands */


#define Q_COMMAND	0
#define Q_WAIT		1

#define NUM_QUEUES	2

extern int immediate_mode;

extern int queue_size[NUM_QUEUES];
extern int queue_overrun;

struct textlist {
  struct textlist *next;
  char text[0];
};


/* Command Queue */
struct cmdque
{
  struct cmdque *next;
  dbref player;		// Player who will do command (%!)
  dbref cause;		// Player causing command (%#)
  char *id;		// ID string for use with @cancel
  char *env[10];	// Environment (%0 through %9)
  char cmd[0];		// Command text (non-@wait)

  long wait;		// Time of execution on a @wait
  char waitcmd[0];	// Command text (@wait-only)
};
