/* io/net.c */
/* Main server module, handles all network connections. */

#include "externs.h"
#include <errno.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <arpa/telnet.h>
#include <netdb.h>

char *db_file = "db/mdb";
char **mainarg;

int nstat[NET_STATS];
int shutdown_flag;
int game_online;
int alarm_count;
int sig_caught;
int save_flag;
int badforksave;
int resolv=-1;
int xcpu;
int dberrcheck=1;
int disable_startups;
int boot_flags;
int mare_test_db;
int db_forceload;
int db_readonly;
int mare_watchdog;

long open_files=-1;
long mud_up_time;
long last_reboot_time;
long now;
quad time_slice;

char system_info[64];
char system_version[64];
char system_model[16];
int  system_ncpu;
int  system_cpu_mhz;

char global_buff[16384];
char env[10][8192];
char log_text[8192];
dbref log_dbref=NOTHING;

struct sitelock *sitelock_list;

DESC *descriptor_list;
static int top_desc;

int tinyport=DEFAULT_PORT;

/* Local functions */
static void init_args(int argc, char *argv[]);
static void init_io(void);
static void init_game(void);
static void open_ports(void);
static void open_game(void);
static void close_game(void);
static void close_sockets(void);
static int  open_sockets(void);

static void alarm_handler(int sig);
static void sig_handler(int sig);
static void reaper(int sig);
static void error_handler(int sig);

static void serverport(void);
static DESC *make_socket(int port, int type);
static void new_connection(DESC *w);
static void shutdownsock(DESC *d);
static void free_io(DESC *d);
static void process_commands(void);

static int do_command(DESC *d, char *command);
static int telnet_iac(DESC *d, unsigned char *s, int len);


/* startup & initialize everything */
int main(int argc, char *argv[])
{
  /* process command line flags and arguments */
  init_args(argc, argv);

  /* initialize input & output */
  init_io();

  /* initialize game structure */
  init_game();
  
  /* start game driver */
  serverport();

  return 0;
}

#ifdef linux

// Gather CPU information from /proc/cpuinfo (Linux only).
// Returns the number of CPUs detected plus additional information in
// system_ncpu, system_model, and system_cpu_mhz.
//
static void get_cpuinfo()
{
  FILE *f;
  char buf[256], *s, *t;
  int mhz=0;

  if(!(f=fopen("/proc/cpuinfo", "r")))
    return;

  while(ftext(buf, 256, f)) {
    if(!(s=strchr(buf, ':')))
      continue;
    for(t=s-1;t > buf && isspace(*t);t--);
    *(t+1)='\0';
    for(s++;isspace(*s);s++);
    if(!*s)
      continue;

#ifdef __alpha__
    if(!strcasecmp(buf, "cpus detected"))
      system_ncpu=atoi(s);
    else if(!strcasecmp(buf, "cycle frequency [Hz]"))
      mhz=atoi(s)/1000000;
    else if(!strcasecmp(buf, "cpu model")) {
      strcpy(system_model, "alpha-");
      strmaxcpy(system_model+6, s, 10);
    }
#elif defined(__mips__)
    if(!strcasecmp(buf, "system type") && !strncmp(s, "EE ", 3)) {
      system_ncpu++;
      mhz=295;  /* Spec: 294.912 MHz */
      strcpy(system_model, "mips-ps2");
      break;
    }
#else
    if(!strcasecmp(buf, "processor"))
      system_ncpu++;
    else if(!strcasecmp(buf, "cpu MHz") || !strcasecmp(buf, "Clocking") ||
            !strcasecmp(buf, "clock"))
      mhz=atoi(s);
#endif
  }
  fclose(f);

  /* Round MHz to the nearest multipler */
  if((mhz+69) % 100 < 7)
    mhz=mhz/100*100+33;
  else if((mhz+37) % 100 < 6)
    mhz=mhz/100*100+66;
  else if(mhz % 5 < 3)
    mhz-=mhz % 5;
  else
    mhz+=5-(mhz % 5);
  if(mhz == 666)
    mhz++;
  system_cpu_mhz=mhz;

  /* Make architecture model name lowercase */
  for(s=system_model;*s;s++)
    *s=tolower(*s);

  if(!system_ncpu)
    system_ncpu=1;
}

#endif  /* linux */

static void init_args(int argc, char *argv[])
{
  struct utsname sys;
  char option, *cmd=argv[0], getinput=0;
  int a;

  mainarg=argv;

  while(argc > 1) {
    --argc; ++argv;
    if(getinput) {
      db_file=*argv;
      getinput=0;
      continue;
    }

    if(**argv == '-') {
      for(a=1;(*argv)[a];a++) {
        switch((option=(*argv)[a])) {
        case 'd': /* set default database input/output file */
          getinput=option;
          break;
        case 'f': /* force loading the database without combat structures */
          db_forceload=1;
          break;
        case 'r': /* make database file read-only */
          db_readonly=1;
          break;
        case 's': /* disable startups */
          disable_startups=1;
          break;
        case 't': /* test load database and dump data to stdout */
          mare_test_db=1;
          getinput=option;
          break;
        case 'v': /* display version and exit */
          fprintf(stdout, "%s: Version Reference: %s\n", cmd, mud_version);
          exit(0);
        default:
          fprintf(stderr, "%s: illegal option -- %c\n", cmd, option);
          fprintf(stderr, "Usage: %s [-frsv] [-d|-t file] [port]\n", cmd);
          exit(1);
        }
      }
      continue;
    }

    /* change port number? */
    tinyport=atoi(*argv);
    if(tinyport < 1 || tinyport > 65535) {
      fprintf(stderr, "%s: Invalid port number '%s'\n", cmd, *argv);
      exit(1);
    } if(tinyport < 1024 && geteuid()) {
      fprintf(stderr, "%s: Only root can run on port %d\n", cmd, tinyport);
      exit(1);
    }
    break;
  }

  /* validate multiword options */
  if(getinput) {
    fprintf(stderr, "%s: Option '%c' takes an argument.\n", cmd, getinput);
    exit(1);
  }

  /* extract system info */
  uname(&sys);
  sprintf(system_info, "%s %s %s (%s)", sys.machine, sys.sysname,
          sys.release, sys.nodename);
  sprintf(system_version, "%s %s", sys.sysname, sys.release);
  strmaxcpy(system_model, sys.machine, 16);

#ifdef linux
  get_cpuinfo();  /* Determine CPU Model and MHz */
#endif

  /* determine max number of open files allowed */
#ifdef __CYGWIN__
  open_files=-1;
#elif defined(RLIMIT_NOFILE)
  {
    struct rlimit rlim;
    getrlimit(RLIMIT_NOFILE, &rlim);
    open_files=rlim.rlim_cur;
  }
#else
  open_files=sysconf(_SC_OPEN_MAX);
#endif

  if((unsigned int)open_files > 0x7ffffffe)
    open_files=-1;

  /* See if we're running from 'wd' */
  if(getenv("WATCHDOG"))
    mare_watchdog=1;
}

static void init_io()
{
  now=time(NULL);

  /* Test database only? (-t option) */
  if(mare_test_db) {
    test_database();
    exit(0);
  }

  /* Ignore all pipe and floating point exception signals */
  signal(SIGPIPE, SIG_IGN);
  signal(SIGUSR2, SIG_IGN);
  signal(SIGFPE, error_handler);  // does not ignore properly!

  /* Alarm routines */
  signal(SIGALRM, alarm_handler);

  /* Database save signal */
  signal(SIGCHLD, reaper);

  /* Trap normal signals */
  signal(SIGQUIT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGUSR1, sig_handler);
  signal(SIGHUP, sig_handler);
  signal(SIGINT, sig_handler);

  /* Trap error signals */
  signal(SIGILL, error_handler);
  signal(SIGBUS, error_handler);
  signal(SIGSEGV, error_handler);

  if(open_sockets())  /* reboot mode */
    return;

  /* All error output is logged to logs/error instead, and TinyMare's
     Main Output is logged to logs/main, with time prefixing each line */

  fclose(stdin);
  fclose(stdout);
  fclose(stderr);

  /* Disassociate from controlling terminal */
  if(!mare_watchdog) {
    if(fork())
      exit(0);
    setsid();
  }

  mud_up_time=now;
  nstat[NS_EXEC]++;
}

static void init_game()
{
  FILE *f;

  init_compress();

  /* Initialize command counters */
  cmdav[0]=nstat[NS_NPCMDS];
  cmdav[1]=nstat[NS_NQCMDS];

  /* Initialize random number generator */
  mt_srand(now);

  last_reboot_time=now;

  log_main("---");
  log_main("Running %s - %s", mud_version, system_info);
  log_main("Starting Program Execution #%d", nstat[NS_EXEC]);
  log_main("%s", "");

  /* Attempt to open all login sockets */
  open_ports();

  /* Open database file for reading */
  if((f=fopen(db_file, "rb")) == NULL) {
    if(mare_test_db || db_readonly || errno != ENOENT) {
      log_main("netmare: Unable to open database file '%s': %s", db_file,
               strerror(errno));
      exit(1);
    }
    log_main("Creating new database from scratch.");
    create_db();
    init_config();
    open_game();
  } else {
    db_read_file=f;
    log_main("Reading file '%s'...", db_file);

    log_main(" Date: %s", strtime(fdate(db_file)));
    log_main(" Size: %ld bytes", fsize(db_file));
  }
}

/* Read file run/etc/portmap and open all listed ports */
static void open_ports()
{
  static char *ports[]={"game", "admin", "http", "mesg"};

  DESC *d;
  FILE *f;
  char buf[256], *r, *s;
  int a, type, port;

  /* Read portmap file */
  if((f=fopen("etc/portmap", "r"))) {
    while(ftext(buf, 256, f)) {
      if((s=strchr(buf, '#')))
        *s='\0';
      if(!*buf)
        continue;

      /* Determine type of port to open */
      s=buf;
      while(isspace(*s))
        s++;
      if(!(r=strspc(&s)))
        continue;
      for(type=0;type < NELEM(ports);type++)
        if(!strcasecmp(ports[type], r))
          break;
      if(type == NELEM(ports))
        continue;

      /* Find port value */
      if(!(r=strspc(&s)))
        continue;
      if(isdigit(*r)) {
        port=atoi(r);
        if(port < 1 || port > 65535) {
          log_error("etc/portmap: Invalid %s port %d.", ports[type], port);
          continue;
        }
      } else
        port=tinyport;

      /* Scan current ports to see if it's already open */
      for(d=descriptor_list;d;d=d->next)
        if((d->flags & C_SOCKET) && d->port == port)
          break;

      /* Open port */
      if(!d) {
        d=make_socket(port, type);

        /* Save any available port options */
        for(a=0;a < ENV_SIZE && (r=strspc(&s));a++)
          WPTR(d->env[a], r);
      }
    }
    fclose(f);
  }

  /* Open default port for tinymare if it isn't open already */
  for(d=descriptor_list;d;d=d->next)
    if((d->flags & C_SOCKET) && d->port == tinyport)
      break;
  if(!d && !make_socket(tinyport, SOCKET_GAME)) {
    log_error("Cannot open requested port. Game aborted.");
    exit(1);
  }

#ifndef __CYGWIN__  /* Cygwin-32 does not support lowlevel DNS queries */

  /* Open connection for domain name server queries */
  for(d=descriptor_list;d;d=d->next)
    if(d->flags & C_DNS) {
      resolv=d->fd;
      break;
    }
  if(!d && (resolv=dns_open()) != -1) {
    d=initializesock(resolv);
    d->concid=0;
    d->flags|=C_DNS;
    d->state=-1;
    SPTR(d->addr, "localhost");
    nstat[NS_FILES]++;
    log_io("DNS Resolver Initialized. desc->%d", resolv);
  }
#endif
}

static void open_game()
{
  DESC *d;

  /* Announce that the game is back up */
  if(!(boot_flags & BOOT_QUIET))
    log_main("%s Online.", MUD_NAME);

  /* Reconnect any players already on */
  for(d=descriptor_list;d;d=d->next) {
    if(d->player >= db_top || (d->player != NOTHING && Typeof(d->player) !=
       TYPE_PLAYER)) {
      output2(d, "*** Connection Reset ***\n\n");
      d->state=0;
      d->player=NOTHING;
      d->flags&=C_LINEMODE|C_REMOTE;
      d->flags|=C_RECONNECT|C_LOGOFF;
    } else if(d->player != NOTHING && (d->flags & C_WAITCONNECT)) {
      if(!(boot_flags & BOOT_QUIET))
        output2(d, "*** Reconnecting ***\n");
      d->flags&=~C_WAITCONNECT;
      d->flags|=C_CONNECTED;
    } else
      d->connected_at=now;

    if(d->player != NOTHING)
      db[d->player].data->desc=d;
  }

  if(boot_flags & BOOT_QUIET)
    log_main("%s Online.", MUD_NAME);

  /* Initialize the CPU Usage counter */
  update_cpu_percent();

  /* Initialize timer for infinite-loop detection every 10 seconds */
  alarm(10);

  /* Transfer program to main runtime routine */
  boot_flags=0;
  dberrcheck=0;
  game_online=1;
}

static void close_game()
{
  alarm(0);
  log_main("Server shutting down.");

  if(db_readonly) {
    /* Print a warning message and disconnect all players */
    log_main("Warning: Game running in read-only mode. "
             "Database will not be saved.");
    boot_flags |= BOOT_RESTART;
  }

  close_sockets();

  /* Save the database (only if not in read-only mode) */
  if(!db_readonly)
    dump_database();

  /* Restart Netmare if @rebooting */
  if(shutdown_flag == 2) {
    if(!(boot_flags & BOOT_RESTART))
      log_main("Restarting TinyMare.");
    restart_game();
  }

  /* Reboot failed; remove temporary files */
  unlink("logs/socket_table");

  /* One last chance: If watchdog is running, an exit code of 2 will
     restart the server. */
  exit(shutdown_flag == 2?2:0);
}

/* Attempt to reboot the game. This function will return if it fails. */
void restart_game()
{
#ifdef __CYGWIN__
  char buf[4096];

  /* Remove any double quotes surrounding pathname */
  if(*mainarg[0] == '"')
    mainarg[0][strlen(mainarg[0]++)-1]='\0';

  /* Convert win32 path to posix to be used with execv */
  cygwin_conv_to_posix_path(mainarg[0], buf);

  execvp(buf, mainarg);
  perror(buf);
#else
  execvp(mainarg[0], mainarg);
  perror(mainarg[0]);
#endif
}

/* Check that the netmare program is executable for a @reboot */
int check_executable()
{
  char *p, *path, *name;
  int eacces=0, pathlen, proglen;

#ifdef __CYGWIN__
  char buf[4096], *prog=buf;

  /* Remove any double quotes surrounding pathname */
  if(*mainarg[0] == '"')
    mainarg[0][strlen(mainarg[0]++)-1]='\0';

  /* Convert win32 path to posix to be used with execv */
  cygwin_conv_to_posix_path(mainarg[0], buf);
#else
  char *prog=mainarg[0];
#endif

  /* If filename is absolute, don't walk the path */
  if(strchr(prog, '/'))
    return access(prog, X_OK);

  /* Walk path */

  if(!(path=getenv("PATH")))
    path="";

  pathlen=strlen(path)+1;
  proglen=strlen(prog)+1;
  name=alloca(pathlen+proglen);

  /* Copy /filename at the top */
  name=memcpy(name+pathlen, prog, proglen);
  *--name = '/';

  while(1) {
    p=strchrnul(path, ':');

    /* Copy path before /filename */
    if(p == path)  /* Search current directory */
      prog=name+1;
    else
      prog=memcpy(name-(p-path), path, p-path);

    /* Test access */
    if(!access(prog, X_OK))
      return 0;
    if(errno == EACCES)
      eacces=1;

    /* Forward 'path' pointer after colon */
    if(!*p++)
      break;
    path=p;
  }

  /* Check if at least one attempt resulted in EACCES */
  if(eacces)
    errno=EACCES;

  return -1;
}

/* Current version number for the temporary save data over reboots */
#define SOCKET_TABLE_VERSION 20040612

static void close_sockets()
{
  FILE *f;
  DESC *d, *dnext;
  struct io_queue *ptr;
  int a, b, len, num=0;

  /* Open file to save socket table information to */
  if(shutdown_flag != 2 || (boot_flags & BOOT_RESTART) ||
     !(f=fopen("logs/socket_table", "wb")))
    f=NULL;

  /* Count number of descriptors & print shutdown message */
  for(d=descriptor_list;d;d=dnext) {
    dnext=d->next;
    if(d->flags & (C_SOCKET|C_DNS)) {
      fcntl(d->fd, F_SETFD, 0);
      num++;
      continue;
    }

    if(d->state == 33) {  // Send reboot/shutdown signal to Remotelink server
      if(!f)
        output2(d, tprintf("$%d\n", (shutdown_flag == 2)?18:19));
    } else if(!(d->flags & C_NOMESSAGE) && xcpu) {
      output2(d, "\a\e[1;31m*** The sky is falling!!\n");
      output2(d, "\a*** Trying Emergency Save.\e[m\n");
    } else if(!(d->flags & C_NOMESSAGE) && !(boot_flags & BOOT_QUIET)) {
      if(f)
        output2(d, tprintf("%s Rebooting.\n", MUD_NAME));
      else
        output2(d, "Going Down - Bye.\n");
    }

    if(!f)
      shutdownsock(d);
    else {
      process_output(d, 1);
      fcntl(d->fd, F_SETFD, 0);
    }

    num++;
  }

  if(!f)
    return;

  /* Select database reference number byte-length */
  dbref_len=(db_top+1 < 0xFF)?1:(db_top+1 < 0xFF00)?2:
             (db_top+1 < 0xFF0000)?3:4;

  /* Write socket table header (for reboot) */
  putnum(f, getpid());
  putnum(f, 0);  /* Socket Table Version; filled in when done writing */
  putlong(f, mud_up_time);
  putnum(f, boot_flags);
  putnum(f, num);
  putchr(f, dbref_len);
  putchr(f, ENV_SIZE);
  putchr(f, NET_STATS);
  for(a=0;a < NET_STATS;a++)
    putnum(f, nstat[a]);

  for(;num > 0;num--) {
    d=descriptor_list;
    for(a=1;a < num;a++)
      d=d->next;

    /* Write connection list database */
    putnum(f, d->fd);
    putnum(f, d->flags);
    putnum(f, d->concid);
    putref(f, d->player);
    putlong(f, d->login_time);
    putlong(f, d->last_time);
    putlong(f, d->connected_at);
    for(a=0;a < ENV_SIZE;a++)
      putstring(f, d->env[a]);
    putnum(f, d->timer);
    putnum(f, d->state);
    putshort(f, d->socket);
    putchr(f, d->type);
    putstring(f, d->raw_input ?: "");
    putnum(f, d->output_size);
    putnum(f, d->cmds);
    putnum(f, d->input);
    putnum(f, d->output);
    putstring(f, d->addr);
    putnum(f, d->ipaddr);
    putshort(f, d->port);
    putshort(f, d->rows);
    putshort(f, d->cols);
    putchr(f, d->term);

    /* Write defined-icon list */
    putnum(f, d->icon_size);
    for(a=0;a < d->icon_size;a++)
      putnum(f, d->iconlist[a]);

    /* Write pending input */
    for(ptr=d->io[0];ptr;ptr=ptr->next) {
      len=strlen(ptr->data)+1;
      putnum(f, len);
      fwrite(ptr->data, len, 1, f);
    }
    putnum(f, -1);

    /* Write pending output */
    len=0;
    for(b=2;b >= 1;b--)
      for(ptr=d->io[b];ptr;ptr=ptr->next)
        len+=ptr->len-ptr->offset;
    putnum(f, len);
    for(b=2;b >= 1;b--)
      for(ptr=d->io[b];ptr;ptr=ptr->next)
        fwrite(ptr->data+ptr->offset, ptr->len-ptr->offset, 1, f);

    /* Write concentrator data */
    if(d->flags & C_PARENT) {
      putnum(f, d->conc_io.concid);
      putnum(f, d->conc_io.offset);
      putshort(f, d->conc_io.len);
      if(d->conc_io.len)
        fwrite(d->conc_io.data, d->conc_io.len, 1, f);
    }
  }

  if(!(boot_flags & BOOT_VALIDATE))
    save_queues(f);

  /* Rewind and fill in Socket Table Version */
  fseek(f, 4, SEEK_SET);
  putnum(f, SOCKET_TABLE_VERSION);
  fclose(f);
}

static int open_sockets()
{
  FILE *f=NULL;
  DESC *d, *k;
  struct io_queue *ptr;
  int a, b, c, env, num;

  if(!(f=fopen("logs/socket_table", "rb")))
    return 0;

  // Ensure no two processes will open this file.
  // Note that as long as the file is open, it is still readable.
  unlink("logs/socket_table");

  /* This must be the same process id# */
  if(getpid() != getnum(f)) {
    fclose(f);
    return 0;
  }

  /* Socket table version must match */
  if(getnum(f) != SOCKET_TABLE_VERSION) {
    fclose(f);

    /* Attempt to close all file descriptors */
    if(open_files < 0 || open_files > 1024)
      open_files=1024;
    for(a=0;a < open_files;a++)
      close(a);

    log_main("New TinyMARE save-state version detected.");
    log_main("Closing all connections and restarting.");

    restart_game();
    exit(2);
  }

  mud_up_time=getlong(f);
  boot_flags=getnum(f);
  num=getnum(f);
  dbref_len=fgetc(f);
  env=fgetc(f);
  a=fgetc(f);
  for(b=0;b < a;b++) {
    if(b < NET_STATS)
      nstat[b]=getnum(f);
    else
      getnum(f);
  }
  nstat[NS_EXEC]++;
  nstat[NS_DBSAVE]++;

  for(a=0;a < num;a++) {
    d=initializesock(getnum(f));
    d->flags=getnum(f);
    d->concid=getnum(f);
    d->player=getref(f);
    d->login_time=getlong(f);
    d->last_time=getlong(f);
    d->connected_at=getlong(f);
    for(c=0;c < env;c++) {
      if(c >= ENV_SIZE)
        getstring_noalloc(f);
      else
        getstring(f, d->env[c]);
    }
    d->timer=getnum(f);
    d->state=getnum(f);
    d->socket=getshort(f);
    d->type=fgetc(f);
    getstring(f, d->raw_input);
    d->output_size=getnum(f);
    d->cmds=getnum(f);
    d->input=getnum(f);
    d->output=getnum(f);
    getstring(f, d->addr);
    d->ipaddr=getnum(f);
    d->port=getshort(f);
    d->rows=getshort(f);
    d->cols=getshort(f);
    d->term=fgetc(f);

    /* Read icon list */
    if((c=d->icon_size=getnum(f))) {
      d->iconlist=malloc((c+(~(c-1) & 31))*sizeof(int));
      for(b=0;b < c;b++)
        d->iconlist[b]=getnum(f);
    }

    /* Get pending input */
    while((b=getnum(f)) >= 0) {
      ptr=malloc(sizeof(struct io_queue)+b);
      ptr->next=NULL;
      fread(ptr->data, b, 1, f);
      if(d->io_last[0])
        d->io_last[0]->next=ptr;
      else
        d->io[0]=ptr;
      d->io_last[0]=ptr;
    }

    /* Get pending output */
    if((b=getnum(f))) {
      d->io[2]=malloc(sizeof(struct io_queue)+max(b, IO_CHUNK_SIZE));
      d->io[2]->next=NULL;
      d->io[2]->len=b;
      d->io[2]->offset=0;
      fread(d->io[2]->data, b, 1, f);
      d->io_last[2]=d->io[2];
    }

    /* Get concentrator data */
    if(d->flags & C_PARENT) {
      d->conc_io.concid=getnum(f);
      d->conc_io.offset=getnum(f);
      d->conc_io.len=getshort(f);
      if(d->conc_io.len) {
        d->conc_io.data=malloc(d->conc_io.len);
        fread(d->conc_io.data, d->conc_io.len, 1, f);
      }
    }

    if(d->flags & C_CONNECTED) {
      d->flags &= ~C_CONNECTED;
      d->flags |= C_WAITCONNECT;
    }
  }

  if(!(boot_flags & BOOT_VALIDATE))
    load_queues(f);

  b=getnum(f);
  fclose(f);

  /* Match file descriptors with contentrators & parents */
  for(d=descriptor_list;d;d=d->next) {
    if(!(d->flags & C_REMOTE))
      continue;
    for(k=descriptor_list;k;k=k->next)
      if(k->fd == d->fd && !(k->flags & C_REMOTE)) {
        d->parent=k;
        break;
      }
  }

  return 1;
}

/* Signal Handling */

/* protects against infinite loops */
static void alarm_handler(int sig)
{
  static long last_alarm;
  long tt=time(NULL);
  sigset_t set;

  /* Restart Alarm */
  signal(sig, alarm_handler);
  sigemptyset(&set);
  sigprocmask(SIG_SETMASK, &set, NULL);
  alarm(10);

  /* Check when we were last called */
  if(tt-last_alarm > 11)  /* Must be in debugging mode */
    alarm_count=-1;
  last_alarm=tt;

  if(++alarm_count > 2)
    error_handler(sig);
}

static void sig_handler(int sig)
{
  signal(sig, sig_handler);
  sig_caught = sig+128; /* 8th bit: signal triggered but not processed yet */
}

/* fork() finishes from database save */
static void reaper(int sig)
{
  int status;

  /* terminate children */
  wait(&status);

  if(!WIFEXITED(status) || WEXITSTATUS(status))
    badforksave=1;
  else
    save_flag=1;

  /* reset signal for next dump */
  signal(sig, reaper);
}

static void error_handler(int sig)
{
  DESC *d;
  static int crashes=0;
  int i;

  if(sig != SIGALRM) {
    if(dberrcheck++) {
      log_error("Database consistency check failed on #%d", game_online?
                log_dbref:speaker);
      log_error("%s", "");
      log_error("Unrecoverable error. Aborting.");

      /* Process output */
      for(d=descriptor_list;d;d=d->next)
        if(!(d->flags & C_LOGOFF))
          process_output(d, 1);

      if(!game_online)  /* Failed while loading database? exit. */
        exit(1);

      signal(sig, SIG_DFL);  /* Reset to default handler and trap error. */
      return;
    }

    signal(sig, SIG_DFL);
    signal(sig, error_handler);
  }

  /* Display and log nature of error */
  log_error("Game Driver: NetMARE Runtime Malfunction, %s",
            sig==SIGALRM?"Infinite loop":(char *)strsignal(sig));
  log_error("%s", "");
  if(!Invalid(log_dbref)) {
    log_error("FAILED COMMAND: %s(#%d) in %s(#%d)", db[log_dbref].name,
              log_dbref, db[db[log_dbref].location].name,
              db[log_dbref].location);
  }
  if(*log_text)
    log_error("LOG DATA: %s", log_text);
  if(*log_text || !Invalid(log_dbref))
    log_error("%s", "");

  if(++crashes > 127)
    log_error("Too many crashes. Aborting.");
  else if(sig != SIGALRM && !ERR_SIGNAL)
    log_error("Aborting TinyMare.");
  else if(sig != SIGSEGV)
    log_error("Game Driver restarted.");
  else
    log_error("Checking database integrity...");

  /* Process output */
  for(d=descriptor_list;d;d=d->next)
    if(!(d->flags & C_LOGOFF))
      process_output(d, 1);

  /* Reset globals */
  match_quiet=0;
  immediate_mode=0;

  /* Abort program if not catching error signals */
  if((sig != SIGALRM && !ERR_SIGNAL) || crashes > 127) {
    if(sig == SIGALRM)
      abort();
    signal(sig, SIG_DFL);
    return;
  }

  /* Segmentation faults need db consistency check */
  if(sig == SIGSEGV) {
    i=db_check();
    log_error("%d object%s, %d byte%s RAM check 100%%", db_top,
              db_top==1?"":"s", i, i==1?"":"s");
  }

  nstat[NS_CRASHES]++;

  for(d=descriptor_list;d;d=d->next)
    if(!(d->flags & C_LOGOFF) && (d->flags & C_CONNECTED))
      output2(d, "*** Reconnecting ***\n");

  /* Restart game driver */
  serverport();
}

static void check_signals()
{
  if(!(sig_caught & 128))
    return;

  sig_caught-=128;
  if(sig_caught < 1 || sig_caught >= NSIG)
    return;

  log_error("Game Driver: Caught Signal '%s'", strsignal(sig_caught));

  switch(sig_caught) {
  case SIGHUP:  /* perform database dump */
    do_dump(GOD);
    break;
  case SIGUSR1:  /* reboot system */
    shutdown_flag=2;
    signal(sig_caught, SIG_IGN);
    break;
  case SIGQUIT: /* normal shutdown.. */
  case SIGTERM:
    xcpu=1;
    shutdown_flag=1;
    signal(sig_caught, SIG_IGN);
  }
}

/********************/
/* Main Server Code */
/********************/

static void serverport()
{
  struct timeval timeout;
  static fd_set *input_set, *output_set;
  static int fdset_size;
  DESC *d, *dnext;
  int found=0, passthru, run_queue;
  quad timer;

  /* Main server loop */
  while(1) {
    alarm_count=0;

    if(!game_online) {
      /* Load more database objects */
      if(load_database())
        open_game();  /* Load finished */
    } else {
      check_signals();

      if(save_flag) {
        if(fsize(db_file) >= 0)
          log_main("%s Checkpoint Done. Filesize: %ld bytes", MUD_NAME,
                   fsize(db_file));
        save_flag=0;
        epoch_fail=0;
        halt_dump=0;
      } else if(badforksave) {
        badforksave=0;
        no_dbdump(1);
        halt_dump=0;
      }

      /* Begin queue time-slice of 100ms */
      timer=get_time();
      if(time_slice-timer < 1 || time_slice-timer > 100) {
        time_slice=timer+100;
        run_queue=1;
      } else
        run_queue=0;

      process_commands();

      if(shutdown_flag && !halt_dump)  /* Exit main server engine */
        break;

      /* Process queue events only once per 100ms */
      if(run_queue)
        dispatch();

      /* Clear crash log data */
      log_dbref=NOTHING;
      *log_text='\0';
    }
    
    /* Dynamically grow file descriptor sets when needed (for >1024 users) */
    if(!input_set || (fdset_size-8)*8 <= top_desc) {
      free(input_set);
      free(output_set);
      fdset_size=(top_desc/64+2)*8;
      input_set=malloc(fdset_size);
      output_set=malloc(fdset_size);
    }

    /* Clear input/output file descriptor sets */
    memset(input_set, 0, fdset_size);
    memset(output_set, 0, fdset_size);

    /* Set which file descriptors are ready to read input or send output */
    passthru=0;
    for(d=descriptor_list;d;d=d->next) {
      if(!d->io[0] && !(d->flags & C_NONBLOCK))
        FD_SET(d->fd, input_set);
      if(d->io[2] || (d->io[1] && !(d->flags & (C_OUTPUTOFF|C_WAITCONNECT))) ||
         (d->flags & C_NONBLOCK))
        FD_SET(d->fd, output_set);
      if((d->flags & C_LOGOFF) || ((d->flags & C_REMOTE) && !d->parent))
        passthru=1;
    }

    /* The select() delay is the remainder of 100ms time-slice not used */
    timer=time_slice-get_time();
    timeout.tv_sec = 0;
    timeout.tv_usec = (game_online && timer > 0 && timer <= 100)?timer*1000:0;
    
    found=select(top_desc+1, input_set, output_set, NULL, &timeout);
    if(found == -1) {
      if(errno != EINTR)
        perror("select");
      if(passthru) {
        memset(input_set, 0, fdset_size);
        memset(output_set, 0, fdset_size);
      }
    }

    now=time(NULL);
    if(found <= 0 && !passthru)
      continue;

    /* There are descriptors ready. go into input/output processing */

    /* Process all input */
    for(d=descriptor_list;d;d=dnext) {
      dnext=d->next;
      if(!(d->flags & C_REMOTE) && FD_ISSET(d->fd, input_set)) {
        if(d->flags & C_SOCKET)
          new_connection(d);
#ifndef __CYGWIN__
        else if(d->flags & C_DNS)
          dns_receive();
#endif
        else if(!process_input(d))
          shutdownsock(d);
      }
    }

    /* Process all output */
    for(d=descriptor_list;d;d=dnext) {
      dnext=d->next;
      if(d->flags & C_LOGOFF)
        shutdownsock(d);
      else if(FD_ISSET(d->fd, output_set)) {
        if(d->flags & C_NONBLOCK) {
          d->flags &= ~C_NONBLOCK;
          if(!server_command[d->state].fun(d, NULL))
            shutdownsock(d);
        } else if(!process_output(d, 0))
          shutdownsock(d);
      }
    }

    /* Finally, remove unparented remote connections */
    for(d=descriptor_list;d;d=dnext) {
      dnext=d->next;
      if((d->flags & C_LOGOFF) || ((d->flags & C_REMOTE) && !d->parent))
        shutdownsock(d);
    }
  }

  /* Shutdown server */
  close_game();
}

static DESC *make_socket(int port, int type)
{
  struct sockaddr_in server;
  char buf[32];
  DESC *d;
  int s, on=1;
  
  sprintf(buf, "port %d", port);

  /* create socket */
  if((s=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror(buf);
    return NULL;
  }

  /* Set option on socket to allow port to be reused. This is mandatory to be
     able to reopen the login port when rebooting or after the game crashes,
     and players are still waiting for input from a unexisting process. */

  if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
    perror(buf);
    close(s);
    return NULL;
  }

  /* Bind the socket to an address:port */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(port);
  if(bind(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror(buf);
    close(s);
    return NULL;
  }

  /* initialize file descriptor */
  listen(s, 128);
  d=initializesock(s);
  d->concid=++nstat[NS_LOGINS];
  d->flags|=C_SOCKET;
  d->state=-1;
  d->type=type;
  nstat[NS_FILES]++;

  /* record internet address */
  d->port=port;
  SPTR(d->addr, "localhost");

  log_io("Socket on Port %d Initialized. desc->%d", port, s);
  return d;
}

static void new_connection(DESC *w)
{
  DESC *d;
  struct sockaddr_in addr;
  int newsock, addr_len=sizeof(struct sockaddr);

  /* Find incoming address */
  if((newsock=accept(w->fd, (struct sockaddr *)&addr, &addr_len)) < 0) {
    perror("new_connection: accept");
    return;
  }

  /* Initialize file descriptor */
  d=initializesock(newsock);
  d->concid=++nstat[NS_LOGINS];
  d->socket=w->port;
  d->type=w->type;

  /* Save internet address */
  d->port=ntohs(addr.sin_port);
  d->ipaddr=addr.sin_addr.s_addr;
  SPTR(d->addr, inet_ntoa(addr.sin_addr));

  /* Check if we exceed site limit */
  nstat[NS_FILES]++;
  if((unsigned int)open_files < 0x7fffffff && nstat[NS_FILES] >
     (int)(open_files-6)) {
    log_io("User Limit Exceeded %d: %s. desc->%d", d->concid, d->addr, d->fd);
    output2(d, tprintf("Sorry, this port is at its maximum of %d users.\n\n",
            (int)(open_files-6)));
    display_textfile(d, "msgs/toomany.txt");
    shutdownsock(d);
    return;
  }

  /* Initialize non-blocking user authentication */
  d->flags |= C_RESOLVER;
  get_ident(d);

  init_connect(d);
}

/* Initialize the new connection */
DESC *initializesock(int s)
{
  DESC *d;
  int a;

  if(s > top_desc)
    top_desc=s;

  /* By default, connection will disappear on exec() */
  fcntl(s, F_SETFD, 1);

  /* Allocate a new descriptor structure and zero its contents */
  d=calloc(1, sizeof(DESC));

  /* Initialize data on descriptor pointer */
  d->fd=s;
  d->player=NOTHING;

  /* Concentrator info */
  d->flags=C_LOGIN;
  d->login_time=now;
  d->last_time=now;
  d->connected_at=now;

  /* Connection environment */
  for(a=0;a < ENV_SIZE;a++)
    SPTR(d->env[a], "");

  d->next=descriptor_list;
  descriptor_list=d;
  return d;
}

// Non-blocking loop to lookup hostname and identity with a 9-second timeout,
// refuse sites from bad IP addresses, and print the login banner on success.
//
int init_connect(DESC *d)
{
  char *s, *t;
  
  /* Attempt to find hostname */
  if(d->flags & C_RESOLVER) {
    if(!(s=dns_lookup(d->ipaddr)))
      return 0;
    d->flags &= ~C_RESOLVER;

    /* Insert hostname after user@ string */
    if((d->flags & C_IDENT) || !(t=strchr(d->addr, '@')))
      WPTR(d->addr, s);
    else {
      *t='\0';
      WPTR(d->addr, tprintf("%s@%s", d->addr, s));
    }
  }

  if(d->flags & (C_RESOLVER|C_IDENT))
    return 0;

  /* Check IP address for sitelock */
  if(forbidden_site(d, LOCKOUT_LOGIN)) {
    log_io("Refused %d: %s. desc->%d", d->concid, d->addr, d->fd);
    display_textfile(d, "msgs/lockout.txt");
    d->flags|=C_LOGOFF;
    return 1;
  }

  /* Connection Succeeded - Print Login Banner */

  /* Initialize for a special socket connection */
  switch(d->type) {
    case SOCKET_HTTP: d->state=37; d->flags|=C_SUPPRESS; break;
    case SOCKET_MESG: d->state=38; d->flags|=C_SUPPRESS; break;
    default:
      log_io("Login %d: %s. desc->%d", d->concid, d->addr, d->fd);
  }

  /* Enable transmit Telnet EOR, NAWS, and TTYPE */
  if(d->type != SOCKET_HTTP && d->type != SOCKET_MESG) {
    output2(d, "\377\373\031");  // Will EOR
    output2(d, "\377\375\037");  // Do NAWS
    output2(d, "\377\375\030");  // Do TTYPE
    output2(d, "\377\372\030\001\377\360");  // Negotiate TTYPE
  }

  /* Display login information (title screen) to user */
  if(d->type == SOCKET_GAME) {
    output2(d, "\ec\e(U\e[1;5]\e[2J\e[H\n");
    display_textfile(d, "msgs/title.txt");
  } else if(d->type == SOCKET_ADMIN) {
    output2(d, tprintf("%s: %s (fd%d) #%d\n", mud_version,
            system_info, d->fd, d->concid));
    output2(d, tprintf("%s -- %s %d\n\n", mud_copyright, MUD_NAME, tinyport));
  }

  return 1;
}

// Determine if an incoming connection is locked out of the TinyMare for a
// certain lockout class. Allowable classes are LOCKOUT_GUEST, LOCKOUT_CREATE,
// LOCKOUT_LOGIN.
//
int forbidden_site(DESC *d, int class)
{
  struct sitelock *ptr;
  struct in_addr ip;
  char *ip_string, *host;

  /* Can't sitelock against 'localhost' */
  if(!d->ipaddr || ntohl(d->ipaddr) >> 24 == 127)
    return 0;

  /* Parse dotted-decimal IP notation */
  ip.s_addr=d->ipaddr;
  ip_string=inet_ntoa(ip);

  /* Find host portion of connection address */
  if((host=strchr(d->addr, '@')))
    host++;

  /* Scan defined @sitelocks */
  for(ptr=sitelock_list;ptr;ptr=ptr->next) {
    if(class > ptr->class)
      continue;

    /* Check against: 1) dotted-decimal, 2) full address, 3) hostname only */
    if(wild_match(ip_string, ptr->host, 0) ||
       wild_match(d->addr, ptr->host, 0) ||
       (host && wild_match(host, ptr->host, 0)))
      return 1;
  }

  return 0;
}


/* Reads input from connected players */
int process_input(DESC *d)
{
  unsigned char buf[4096];
  int len;

  /* Read data & check that socket is still alive */
  if((len = read(d->fd, buf, 4096)) <= 0) {
    if(len == -1 && errno == EINTR)
      return -1;
    return 0;  /* Connection has unexpectedly closed */
  }

  nstat[NS_INPUT] += len;
  d->input += len;

  /* Concentrator processes input data specially */
  if(d->flags & C_PARENT)
    return service_conc(d, buf, len);

  process_input_internal(d, buf, len);
  return 1;
}

/* Back end of process_input() that stores buffer into command queue */
void process_input_internal(DESC *d, unsigned char *buf, int len)
{
  struct io_queue *new;
  unsigned char temp[8192];
  int a, b;

  if(!d->raw_input) {
    *temp=0;
    b=0;
  } else {
    b=strlen(d->raw_input);
    memcpy(temp, d->raw_input, b+1);
  }

  /* filter input */
  for(a=0;a < len;a++) {
    /* Process telnet command characters */
    if(buf[a] == IAC && a < len-1) {
      a+=telnet_iac(d, buf+a+1, len-a-1);
      continue;
    }

    if(buf[a] == 8 || buf[a] == 127) {  /* backspace */
      if(b > 0)
        b--;
      continue;
    }
    if(buf[a] == 10) {  /* newline */
      /* Remove following spaces */
      while(b > 0 && temp[b-1] == ' ')
        b--;
      temp[b]=0;

      /* Save command to input queue */
      new=malloc(sizeof(struct io_queue)+b+1);
      memcpy(new->data, temp, b+1);
      if(d->io_last[0])
        d->io_last[0]->next=new;
      else
        d->io[0]=new;
      d->io_last[0]=new;
      new->next=NULL;

      *temp=0;
      b=0;
      continue;
    }
    if(buf[a] < 32 || buf[a] > 127 || b > 7999)
      continue;
    temp[b++]=buf[a];
  }
  temp[b]=0;

  /* Save input for next read */
  WPTR(d->raw_input, temp);
}

// process_output(): Sends pending output to a connected session.
//  poll=1: Game must call select() to make sure socket is writable first.
//
int process_output(DESC *d, int poll)
{
  char buffer[32768];
  struct io_queue *next, *new;
  int q, amt, len, size=0;
  
  if((d->flags & C_REMOTE) && !d->parent)
    return 0;

  /* Construct a giant buffer (up to 32768 bytes) to do an atomic send over
     the network. Any data left over gets tacked onto the beginning of the
     server output queue (d->io[2]) for the next successful select(). */

  /* Loop: Process server output before regular game output */
  for(q=2;q>=1 && size < 32768;q--) {
    if(q == 1) {
      if(d->flags & (C_OUTPUTOFF|C_WAITCONNECT))
        break;
      if(d->flags & C_OUTPUTFLUSHED) {
        if(size > 32768-18)
          break;
        memcpy(buffer+size, (d->player != NOTHING &&
              terminfo(d->player, TERM_HIGHFONT))?"\256Output Flushed\257\r\n":
              "<Output Flushed>\r\n", 18);
        size += 18;
        d->flags &= ~C_OUTPUTFLUSHED;
      }
    }

    /* Concatenate queue data to buffer */
    while(d->io[q] && size < 32768) {
      next=d->io[q]->next;
      amt=d->io[q]->len - d->io[q]->offset;
      if(amt > 32768-size)
        amt=32768-size;
      memcpy(buffer+size, d->io[q]->data+d->io[q]->offset, amt);
      size+=amt;
      if(q == 1)
        d->output_size -= amt;

      if((d->io[q]->offset += amt) == d->io[q]->len) {
        free(d->io[q]);
        d->io[q]=next;
      }
    }

    if(!d->io[q])
      d->io_last[q]=NULL;
  }

  if(!size)
    return 1;  /* Nothing to send */

  /* Remote: Send to concentrator's output queue */
  if(d->flags & C_REMOTE) {
    struct {
      int concid;
      unsigned short len;
    } __attribute__((__packed__)) header={htonl(d->concid), htons(size)};

    queue_output(d->parent, 1, &header, sizeof(header));
    queue_output(d->parent, 1, buffer, size);
    d->output += size;
    return 1;
  }

  /* If poll == 1, then use select() to determine if the socket is writable. */
  if(poll) {
    struct timeval tv={0, 0};
    fd_set *output_set=alloca((d->fd/64+2)*8);

    memset(output_set, 0, (d->fd/64+2)*8);
    FD_SET(d->fd, output_set);

    if(select(d->fd+1, NULL, output_set, NULL, &tv) == 1)
      poll=0;  /* Socket is writable */
  }

  /* Send data buffer */
  len=0;
  if(poll || (len=write(d->fd, buffer, size)) < size) {
    if(len == -1) {
      if(errno != EINTR)
        return 0;  /* Connection has unexpectedly closed */
      len=0;
    }

    /* Move unsent data to highest priority on server queue */
    new=malloc(sizeof(struct io_queue)+((!d->io[2] && size-len<IO_CHUNK_SIZE)?
               IO_CHUNK_SIZE:(size-len)));
    new->next=d->io[2];
    new->len=size-len;
    new->offset=0;
    memcpy(new->data, buffer+len, size-len);
    d->io[2]=new;
    if(!d->io_last[2])
      d->io_last[2]=new;
  }

  nstat[NS_OUTPUT] += len;
  d->output += len;
  return 1;
}

/* Telnet "Is A Command" Character Processing */
static int telnet_iac(DESC *d, unsigned char *s, int len)
{
  unsigned char buf[32];
  int i, j;

  if(*s < 236)
    return 0;

  /* Telnet EOR-Style Prompts */
  if(len > 1 && *s == DO && *(s+1) == TELOPT_EOR)
    d->flags|=C_LINEMODE;

  /* Handle Subnegotiation */
  else if(len > 1 && *s == SB) {
    for(i=1,j=0;i < len && j < 32;i++) {
      if(*(s+i) == IAC && *(s+i+1) == SE) {
        i++;
        break;
      } if(*(s+i) == IAC && *(s+i+1) == IAC) {
        buf[j++]=IAC;
        i++;
      } else
        buf[j++]=*(s+i);
    }

    /* Select Terminal Type */
    if(buf[0] == TELOPT_TTYPE && !buf[1]) {
      /* Convert to lowercase */
      buf[j]='\0';
      for(j--;j>1;j--)
        buf[j]=tolower(buf[j]);

      /* Find type. Supports maximum features at type 3, minimum at 0. */
      if(!strcmp("mare", buf+2))
        d->term=3;
      else if(match_word("linux uisg", buf+2))
        d->term=2;
      else if(!buf[2] || match_word("xterm xterm-color ansi vt100 vt102 vt200 "
              "vt220 vt320 screen crt cygwin zmud mushclient pcansi rxvt "
              "dec-vt100", buf+2))
        d->term=1;
      else {
        if(!match_word("dumb unknown dtterm hpterm", buf+2))
          log_io("Session %d: Unknown terminal type '%s'.", d->concid, buf+2);
        d->term=0;
      }
    }

    /* Now Adjust Window Size */
    else if(buf[0] == TELOPT_NAWS && j > 4) {
      if((d->cols=(buf[1]<<8)|buf[2]) > 500)
        d->cols=500;
      if((d->rows=(buf[3]<<8)|buf[4]) > 500)
        d->rows=500;

    }

    return i;
  }

  /* Are you there? */
  else if(*s == AYT)
    output2(d, tprintf("\e[1m[%s %d]\e[m\n", MUD_NAME, d->socket));

  return (*s < 251)?1:2;
}

/* close remote-user socket connection */
static void shutdownsock(DESC *d)
{
  DESC *k;
  dbref player=d->player;
  int flags=d->flags;

  /* prevent people from disconnecting while database is still loading */
  if(!game_online)
    return;

  /* if not reconnecting, clear everything */
  if(!(d->flags & C_RECONNECT)) {
    if(!Invalid(player)) {
      if(terminfo(player, TERM_MUSIC))
        output2(d, "\e[0;32;0;7;36t");
      d->flags&=~C_OUTPUTOFF;
      if(terminfo(player, TERM_ANSI))  /* reset screen */
        output2(d, "\e7\e[r\e8\e[A\n");
      display_textfile(d, "msgs/leave.txt");
      process_output(d, 1);
      log_io("Disconnect %d: %s(#%d). desc->%d", d->concid, db[player].name,
             player, d->fd);
      if(d->flags & C_CONNECTED)
        announce_disconnect(player);
    } else {
      process_output(d, 1);
      if(!(d->flags & (C_SUPPRESS|C_IDENTSOCK)))
        log_io("%s%s %d: %s. desc->%d", d->flags&C_PARENT?
               "Concentrator ":"", d->flags&(C_SOCKET|C_DNS)?"Closing Port":
               "Logout", d->concid, d->addr, d->fd);
    }
  } else {
    int a;

    d->flags&=~(C_OUTPUTOFF|C_CONNECTED);
    process_output(d, 1);
    for(a=0;a < ENV_SIZE;a++)  /* clear environment */
      WPTR(d->env[a], "");

    if(!Invalid(player)) {
      log_io("Reconnect %d: %s(#%d). desc->%d", d->concid, db[player].name,
             player, d->fd);
      announce_disconnect(player);
    } else
      log_io("Relog %d: %s. desc->%d", d->concid, d->addr, d->fd);
  }

  /* clear children */
  if(flags & C_PARENT)
    for(k=descriptor_list;k;k=k->next)
      if(k->parent == d)
        k->parent=NULL;

  /* free session waiting on ident authentication */
  if(flags & C_IDENTSOCK)
    for(k=descriptor_list;k;k=k->next)
      if((k->flags & C_IDENT) && k->concid == d->concid) {
        k->flags &= ~C_IDENT;
        init_connect(k);
        break;
      }

  /* reset resolver file descriptor */
  if(flags & C_DNS)
    resolv=-1;

  /* clear descriptor lookup pointer */
  if(d->player != NOTHING) {
    db[d->player].data->desc=NULL;
    d->player=NOTHING;
  }

  /* Erase half-created characters, @nuked players, or Guests */
  if(!Invalid(player) && Typeof(player) == TYPE_PLAYER &&
     (Guest(player) || (flags & C_DESTROY) || !db[player].data->last)) {
    if(!(flags & C_CONNECTED))
      log_io("Autonuke: %s(#%d).", db[player].name, player);
    destroy_player(player);
  }

  /* logging off */
  if(!(d->flags & C_RECONNECT)) {
    if(d->flags & C_REMOTE) {  /* Tell parent about the logoff */
      struct {
        int remote_concid;
        unsigned short len;
        char cmd;
        int concid;
      } __attribute__((__packed__)) message={0, htons(5), 2, htonl(d->concid)};

      if(d->parent)
        queue_output(d->parent, 1, &message, sizeof(message));
    } else {
      shutdown(d->fd, 2);
      close(d->fd);
      nstat[NS_FILES]--;
    }
    for(k=descriptor_list;k;k=k->next)
      if(k->next == d)
        break;
    if(k)
      k->next=d->next;
    else
      descriptor_list=d->next;

    free_io(d);
    free(d);
  } else {
    /* lets reconnect */
    d->state=0;
    d->connected_at=now;
    d->flags&=C_LINEMODE|C_REMOTE;
    d->flags|=C_LOGIN;
  }
}

static void free_io(DESC *d)
{
  struct io_queue *ptr, *next;
  int a;

  for(a=0;a < 3;a++)
    for(ptr=d->io[a];ptr;ptr=next) {
      next=ptr->next;
      free(ptr);
    }

  for(a=0;a < ENV_SIZE;a++)
    free(d->env[a]);
  free(d->conc_io.data);
  free(d->raw_input);
  free(d->addr);
}

void queue_output(DESC *d, int queue, void *msg, int len)
{
  struct io_queue *new;
  int size, offset=0;

  while(offset < len) {
    if(!d->io_last[queue] || d->io_last[queue]->len >= IO_CHUNK_SIZE) {
      new=malloc(sizeof(struct io_queue)+IO_CHUNK_SIZE);
      new->next=NULL;
      new->len=new->offset=0;

      if(d->io_last[queue])
        d->io_last[queue]->next=new;
      else
        d->io[queue]=new;
      d->io_last[queue]=new;
    }
    size=IO_CHUNK_SIZE - d->io_last[queue]->len;
    size=(len-offset < size) ? (len-offset) : size;
    memcpy(d->io_last[queue]->data+d->io_last[queue]->len,
           (char *)msg+offset, size);
    d->io_last[queue]->len += size;
    offset += size;
  }
}

/* adds <msg> to the output queue for a particular descriptor */
void output(DESC *d, char *msg)
{
  struct io_queue *tmp;
  int len, size;
  char *s=term_emulation(d, msg, &len);
 
  queue_output(d, 1, s, len);

  d->output_size += len;
  if(MAX_OUTPUT < 0)
    MAX_OUTPUT=32768;
  if((size=d->output_size-MAX_OUTPUT) < 1)
    return;

  /* Truncate output buffer to fit MAX_OUTPUT bytes */
  d->flags |= C_OUTPUTFLUSHED;
  d->output_size = MAX_OUTPUT;

  while(size > 0) {
    if(d->io[1]->len - d->io[1]->offset > size) {
      d->io[1]->offset += size;
      return;
    }
    size -= d->io[1]->len - d->io[1]->offset;
    tmp=d->io[1]->next;
    free(d->io[1]);
    d->io[1]=tmp;
  }
}

/* adds <msg> to server output queue */
void output2(DESC *d, char *msg)
{
  int len;
  char *s=term_emulation(d, msg, &len);
 
  queue_output(d, 2, s, len);
}

/* adds <msg> to high-priority output escaping all Telnet IAC characters */
void output_binary(DESC *d, unsigned char *msg, int len)
{
  unsigned char buf[8192];
  int i=0, j=0;

  while(j < len) {
    buf[i]=msg[j];
    if(msg[j] == 255)
      buf[++i]=255;
    i++, j++;
    if(i >= 8191) {
      queue_output(d, 2, buf, i);
      i=0;
    }
  }

  if(i)
    queue_output(d, 2, buf, i);
}

static void process_commands()
{
  DESC *d, *dnext;
  struct io_queue *ptr;
  int count;

  /* go through the entire list of users online */
  for(d=descriptor_list;d;d=dnext) {
    dnext=d->next;
    if(!d->io[0] || (d->flags & (C_LOGOFF|C_INPUTOFF|C_RESOLVER|C_IDENT)) ||
       ((d->flags & C_REMOTE) && !d->parent))
      continue;

    /* determine max commands per user */
    if(d->flags & C_SERVICE)
      count=250;
    else if((d->flags & C_CONNECTED) && power(d->player, POW_SECURITY))
      count=50;  /* wizards can execute up to 50 commands per timeslice */
    else
      count=1;

    /* process any pending commands */
    for(;count;count--) {
      if(!d->io[0] || (d->flags & C_INPUTOFF))
        break;
      ptr=d->io[0];
      if(!(d->io[0]=ptr->next))
        d->io_last[0]=NULL;

      if(!do_command(d, ptr->data)) {
        shutdownsock(d);
        break;
      }

      free(ptr);
    }
  }
}

static int do_command(DESC *d, char *command)
{
  char *s;
  int a, unidle=0;

  /* Set cause (%#) of command */
  if(!Invalid(d->player))
    speaker=d->player;

  /* Clear environment %0 through %9 */
  for(a=0;a<10;a++)
    *env[a]='\0';

  /* Remove leading spaces */
  if(d->state != 32 && d->state != 36)
    while(*command && *command == ' ')
      command++;

  /* Command unidles player from over 1 hour */
  if(now-d->last_time >= 3600 && (d->flags & C_CONNECTED)) {
    unidle=1;
    s=time_format(TMS, now-d->last_time);
    log_io("Game: %s has unidled. (%s)", db[d->player].name, s);
    do_cnotify(d->player, tprintf("\e[1mNotify:\e[m %s has unidled. "
               "\e[1;36m(\e[37m%s\e[36m)\e[m", db[d->player].name, s), 0);

    if(*(s=atr_get(d->player, A_AUNIDLE))) {
      sprintf(env[0], "%ld", now-d->last_time);
      parse_que(d->player, s, d->player);
      *env[0]='\0';
    }
  }

  d->last_time=now;
  d->cmds++;

  /* Set command for crash reporting */
  log_dbref=NOTHING;
  sprintf(log_text, "Id#%d: %s", d->concid, command);

  /* Priority commands */
  if(d->state < 31 || d->state > 32) {
    if(!strcmp(command, "QUIT"))
      return 0;
    if(d->state > 4 && !strcmp(command, "RECONNECT")) {
      process_output(d, 1);
      output2(d, "*** Reconnecting ***\n\n");
      d->flags|=C_RECONNECT;
      return 0;
    }
  }

  /* Print output prefix */
  if(d->state == 10 && *(s=atr_parse(d->player, d->player, A_PREFIX))) {
    strcat(s, "\n");
    output(d, s);
  }

  /* Process individual command */
  if(!server_command[d->state].fun(d, command))
    return 0;

  /* Clear crash tracking */
  log_dbref=NOTHING;
  *log_text='\0';

  if(d->state == 10) {
    /* Print output suffix */
    if(*(s=atr_parse(d->player, d->player, A_SUFFIX))) {
      strcat(s, "\n");
      output(d, s);
    }

    /* Check for new mail when unidling */
    if(unidle && (a=mail_stats(d->player, 1)))
      notify(d->player, "You have \e[1mnew\e[m mail: %d message%s.", a,
             a==1?"":"s");

    /* Display prompt */
    if(*(s=atr_parse(d->player, d->player, A_PROMPT))) {
        if(!strcasecmp(s, "on"))
          s=">";

      s=tprintf("%s %s", s, PROMPT(d));
      if(d->flags & C_OUTPUTOFF)
        output2(d, s);
      else
        output(d, s);
    }
  }

  return 1;
}

// Flush descriptor I/O buffers.
// Index=-1: All Buffers
// Index= 0: Input
// Index= 1: Output
// Index= 2: Server Output
//
void flush_io(DESC *d, int index)
{
  struct io_queue *ptr, *next;
  char buf[4096];
  int q;

  for(q=0;q<3;q++) {
    if(index != q && index != -1)
      continue;

    /* clear text buffer */
    for(ptr=d->io[q];ptr;ptr=next) {
      next=ptr->next;
      free(ptr);
    }
    d->io_last[q]=d->io[q]=NULL;
    if(q == 1)
      d->output_size=0;
  }

  /* Flush unread input (necessary during C_INPUTOFF) */
  if(index < 1) {
    fcntl(d->fd, F_SETFL, O_NONBLOCK);
    while(read(d->fd, buf, 4096) > 0);
    fcntl(d->fd, F_SETFL, 0);
  }
}

/* Clear input/output buffers on individual player */
void do_flush(dbref player, char *arg1, char *arg2)
{
  const char *type[3]={"Input", "Output", "Server"};
  dbref a, thing=match_thing(player, arg1, MATCH_EVERYTHING);
  char *r, *s=arg2;
  DESC *d;

  /* Make sure we have a valid player */
  if(thing == NOTHING)
    return;
  if(Typeof(thing) != TYPE_PLAYER) {
    notify(player, "That is not a player.");
    return;
  } if(!(d=Desc(thing))) {
    notify(player, "That player is not connected.");
    return;
  }

  /* If no options present, flush all */
  if(!*arg2) {
    flush_io(d, ALL_IO);
    if(!Quiet(player))
      notify(player, "Buffers flushed.");
    return;
  }

  while((r=strspc(&s))) {
    for(a=0;a<3;a++)
      if(string_prefix(type[a], r))
        break;
    if(a == 3)
      notify(player, "No such option '%s'.", r);
    else {
      flush_io(d, a);
      if(!Quiet(player))
        notify(player, "%s%s flushed.", type[a], a==2?" output":"");
    }
  }
}
