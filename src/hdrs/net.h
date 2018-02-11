/* hdrs/net.h */

/* Connection Flags */
#define C_CONNECTED	BV0	// Player is connected to the main game
#define C_LINEMODE	BV1	// Client supports Telnet EOR protocol
#define C_REMOTE	BV2	// Remote connection under concentrator
#define C_OUTBOUND	BV3	// This is an outbound connection
#define C_LOGIN		BV4	// User login initialization
#define C_WAITCONNECT	BV5	// Connect to game when database finishes load
#define C_RECONNECT	BV6	// Game will reconnect player at next chance
#define C_LOGOFF	BV7	// Game will log player off at next chance
#define C_DESTROY	BV8	// Game will destroy player character at logout
#define C_PARENT	BV9	// Concentrator parent, used with Remote
#define C_OUTPUTOFF	BV10	// Output temporarily saved in buffers
#define C_INPUTOFF	BV11	// Input not received or processed
#define C_SBAROFF	BV12	// No status bar updates
#define C_RESOLVER	BV13	// Awaiting response from DNS query
#define C_IDENT		BV14	// Awaiting response from IDENTD query
#define C_SOCKET	BV15	// FD is a socket listening for connections
#define C_DNS		BV16	// FD is a socket receiving DNS messages
#define C_SERVICE	BV17	// Currently supporting an I/O service
#define C_SUPPRESS	BV18	// Disallow broadcast messages, @cpage, etc
#define C_OUTPUTFLUSHED	BV19	// Output was truncated to fit MAX_OUTPUT
#define C_NONBLOCK	BV20	// Waiting on a nonblocking connect() call
#define C_IDENTSOCK	BV21	// FD is an outbound socket connecting to ident

#define C_NOMESSAGE  (C_OUTBOUND|C_SOCKET|C_DNS|C_PARENT|C_SERVICE|C_SUPPRESS)

/* flush_io() parameters */
#define INPUT_STREAM	0	// Input buffer containing commands from client
#define OUTPUT_STREAM	1	// Output on queue to be sent to fd
#define SERVER_STREAM	2	// High-priority server output; can't turn off

#define ALL_IO		-1	// Flushes all I/O streams
#define PENDING_INPUT	-2	// Flushes only unread input from socket

/* Netstat macros */
#define NS_FILES	0	// Number of file descriptors in use
#define NS_OUTPUT	1	// Server output
#define NS_INPUT	2	// Server input
#define NS_EXEC		3	// Reboots (program executions)
#define NS_CRASHES	4	// Number of segmentation faults produced
#define NS_DBSAVE	5	// Number of database saves
#define NS_LOGINS	6	// Top connection-id established
#define NS_CONNECTS	7	// Successful player connects
#define NS_FAILED	8	// Failed connects
#define NS_NEWCR	9	// Newly created players
#define NS_GUESTS	10	// Number of guest players created
#define NS_NCMDS	11	// Number of commands
#define NS_NPCMDS	12	// Player commands executed
#define NS_NQCMDS	13	// Queue commands executed
#define NS_NFCMDS	14	// Failed commands
#define NS_QUEUE	15	// Queue process calls
#define NS_OVERRUNS	16	// Queue time-slice overruns
#define NS_MAIL		17	// Mail messages sent
#define NS_EMAIL	18	// Internet Email sent
#define NS_ERECV	19	// Email received (SMTP Mode)
#define NS_QUERY	20	// Hostname Lookup queries sent
#define NS_RESOLV	21	// DNS hostname queries successfully resolved

# define NET_STATS	22
/* Note: When changing netstats, please change the corresponding value in
   function(netstat) in prog/eval.c */

extern int nstat[NET_STATS];


/* +term entries */
#define TERM_ANSI	BV0	// 0=strip all ansi escape codes from output
#define TERM_HIGHFONT	BV1	// 0=strip 8-bit font down to 7-bit
#define TERM_GRAPHICS	BV2	// 1=use special graphic icons in display
#define TERM_BEEPS	BV3	// 1=beeps are enabled (default)
#define TERM_MUSIC	BV4	// 1=send ansi sound effects/music codes
#define TERM_WORDWRAP	BV6	// 1=perfect wordwrap mode for telnet clients
#define TERM_ICONS	BV7	// 1=terminal supports 32-bit color icons

/* +config entries */
#define CONF_REGINFO	BV5	// 1=display email & rlname on +finger
#define CONF_PAGER	BV8	// 1=use pager in help, +mail, and server pages
#define CONF_SAFE	BV9	// 1=@destroy has 60s timer when in direct mode

/* Sitelock classes */
#define LOCKOUT_GUEST	1	// Visitors cannot log in from this host/ip
#define LOCKOUT_CREATE	2	// Players can't create characters from host
#define LOCKOUT_LOGIN	3	// Players can't log in from specified host

extern struct sitelock {
  struct sitelock *next;
  unsigned char class;
  char host[0];
} *sitelock_list;


/* Socket types */
#define SOCKET_GAME	0	// Everyone can log into this port
#define SOCKET_ADMIN	1	// Only admin can log in here
#define SOCKET_HTTP	2	// Web server access only
#define SOCKET_MESG	3	// Display message and disconnect user

/* Time format displays */
#define TMA		0	// All time values: 17d, 16h, 45m
#define TML		1	// Online time, Long format: 17d 16:45m
#define TMS		2	// Idle time, Short format: 2w
#define TMF		3	// Full format: 2 weeks
#define TMF2		4	// Full format, no 'a' or 'an'

/* Special connection states */
#define ENV_SIZE	13	// Environment variables for connection state


#define IO_CHUNK_SIZE	(8192-sizeof(struct io_queue))

struct io_queue {
  struct io_queue *next;
  int len;			// Number of bytes already written to buffer
  int offset;			// Number of bytes already read from buffer
  unsigned char data[0];	// Buffer (size IO_CHUNK_SIZE or greater)
};

struct conc_input {
  int offset;			// Current byte offset for reading next data
  int concid;			// Connection ID/Command
  unsigned short len;		// Total length of packet to be read
  unsigned char *data;		// Packet data
};

/* Network Connection Structure */

typedef struct descriptor_data {
  struct descriptor_data *next;		// Next in linked list
  struct descriptor_data *parent;	// Controlling session descriptor

  /* Main descriptor data */
  int fd;				// File descriptor
  int flags;				// Connection flags
  int concid;				// Connection ID#
  dbref player;				// Connected Player dbref#

  /* Session data */
  long login_time;			// Time of game login
  long last_time;			// Time last command was entered
  long connected_at;			// Time the game session begins

  /* Login server position */
  char *env[ENV_SIZE];			// Environment variables
  int timer;				// Login initialization timer
  int state;				// Connection state
  unsigned int socket:16;		// Port to which connection originated
  unsigned int type:8;			// Type of port (SOCKET_GAME, etc..)

  /* Input/output management */
  struct io_queue *io[3], *io_last[3];	// IO queues: in/out/server
  struct conc_input conc_io;		// Current concentrator input
  char *raw_input;			// Current command input
  int output_size;			// Amount of pending output

  /* Cumulative totals */
  int cmds;				// Total commands typed in
  int input;				// Total input to game driver
  int output;				// Total output to player

  /* Internet address */
  char *addr;				// Source hostname
  int ipaddr;				// Source IP Address
  unsigned int port:16;			// Port on remote server

  /* terminal settings */
  unsigned int rows:16;			// Number of rows on console
  unsigned int cols:16;			// Number of columns on console
  unsigned int term:8;			// Terminal type (numeric)

  unsigned int *iconlist;		// List of icons already sent to concid
  int icon_size;			// Number of elements in iconlist array

} DESC;

extern DESC *descriptor_list;


/* Server Command Structure */

struct srvcmd {
  int state;
  char *title;
  int (*fun)(struct descriptor_data *, char *);
};

extern struct srvcmd server_command[];

/* Macros to send Telnet IACs */

#define PROMPT(d) ((d->flags & C_LINEMODE)?"\377\357":"")
#define ECHO_OFF "\377\373\001"
#define ECHO_ON "\377\374\001"

extern long mud_up_time;
extern long last_reboot_time;
extern long open_files;

extern int halt_dump;
extern long dump_interval;
extern int epoch_fail;
extern int cpu_percent;
extern int cmdav[2];
extern double cmdfp[2];

extern int shutdown_flag;
extern int disable_startups;
extern int game_online;
extern int mare_watchdog;

extern long now;
extern quad time_slice;

extern int resolv;
