/* mare/runtime.c */
/* Runtime control and database invocation commands */

#include "externs.h"
#include <unistd.h>
#include <sys/resource.h>
#include <errno.h>

int epoch_fail;
int halt_dump;


void do_dump(dbref player)
{
  if(db_readonly) {
    notify(player, "Cannot save the database. "
           "%s was loaded in read-only mode.", MUD_NAME);
    return;
  }
  if(halt_dump) {
    notify(player, "Sorry, a database save is already in effect.");
    return;
  }
  notify(player, "Dumping...");
  fork_and_dump();
  dump_interval=0;
}

void do_shutdown(dbref player)
{
  log_main("SHUTDOWN: %s(#%d)", db[player].name, player);
  shutdown_flag=1;
}

void do_reboot(dbref player, char *options)
{
  const char *flags[3]={"Restart", "Quiet", "Validate"};
  char tmp[32], *r, *s=options;
  int i;

  /* Make sure the executable file still exists */
  if(check_executable()) {
    if(errno == EACCES)
      notify(player, "Error: Cannot reboot; "
             "Netmare program is not executable.");
    else
      notify(player, "Error: Cannot reboot; "
             "Netmare executable file is missing.");
    return;
  }

  boot_flags=0;
  *tmp='\0';

  while((r=parse_up(&s,' '))) {
    for(i=0;i<3;i++)
      if(string_prefix(flags[i], r))
        break;
    if(i == 3) {
      notify(player, "No such bootflag '%s'.", r);
      return;
    }

    boot_flags |= 1 << i;
    sprintf(tmp+strlen(tmp), "%s%s", *tmp?", ":" - ", flags[i]);
  }

  log_main("REBOOT: %s(#%d)%s", db[player].name, player, tmp);
  shutdown_flag=2;
}

/* For use with background database dumping only */
static void close_fds()
{
  DESC *d;

  for(d=descriptor_list;d;d=d->next)
    if(!(d->flags & C_REMOTE))
      close(d->fd);
}

static int dump_database_internal()
{
  char tmpfile[256];
  FILE *f;
  
  sprintf(tmpfile, "%s.#%d", db_file, nstat[NS_DBSAVE]-1);
  unlink(tmpfile);
  sprintf(tmpfile, "%s.#%d", db_file, nstat[NS_DBSAVE]);

  if(!(f=fopen(tmpfile, "wb"))) {
    perror(tmpfile);
    return 0;
  }

  if(!db_write(f, tmpfile)) {
    fclose(f);
    unlink(tmpfile);
    return 0;
  }

  /* Send all data to disk immediately */
  fflush(f);
  fsync(fileno(f));
  fclose(f);

  if(rename(tmpfile, db_file)) {
    perror(tmpfile);
    return 0;
  }

  return 1;  /* Save successful */
}

void dump_database()
{
  /* Make sure the game has 2 objects and that GOD is a player before dumping */
  if(db_top < 2 || Typeof(GOD) != TYPE_PLAYER)
    return;

  nstat[NS_DBSAVE]++;
  log_main("Database Save: %s.#%d", db_file, nstat[NS_DBSAVE]);
  if(!dump_database_internal()) {
    if(shutdown_flag)
      log_main("%s Dump Error. Database Lost!!", MUD_NAME);
    else
      log_main("%s Dump Failed.", MUD_NAME);
  } else
    log_main("%s Dumping Done.", MUD_NAME);
}

void fork_and_dump()
{
  int child;
  
  /* Make sure the game has 2 objects and that GOD is a player before dumping */
  if(db_top < 2 || Typeof(GOD) != TYPE_PLAYER)
    return;

  /* Notify people that their progress is being saved */
  if(DB_NOTIFY)
    broadcast(tprintf("%s Autosave...!\n", MUD_NAME));

  nstat[NS_DBSAVE]++;
  log_main("Database Checkpointing: %s.#%d", db_file, nstat[NS_DBSAVE]);
  
  if(DB_FORK)
    child=fork();
  else
    child=0;
  
  if(child < 0) {
    perror("dbsave: fork()");
    no_dbdump(1);
    return;
  }

  if(child > 0) {  /* Parent process */
    halt_dump=1;
    return;
  }

  /* Child process or non-forking save: */

  if(DB_FORK) {
    /* Unset virtual timer and lower background process priority */
    alarm(0);

    /* Set all signals to default handlers */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGFPE, SIG_DFL);

    /* Close remaining files */
    close_fds();

#ifdef PRIO_PROCESS
    /* Enter low background priority */
    setpriority(PRIO_PROCESS, 0, getpriority(PRIO_PROCESS, 0)+4);
#endif
  }

  if(!dump_database_internal()) {
    if(DB_FORK)
      exit(1);  /* Save unsuccessful; notify parent with an exit code 1 */
    no_dbdump(0);
  }
  if(DB_FORK)
    exit(0);  /* Forked save successful */

  epoch_fail=0;
  if(fsize(db_file) >= 0)
    log_main("%s Checkpoint Done. Filesize: %ld bytes", MUD_NAME,
             fsize(db_file));
}

void no_dbdump(int background)
{
  broadcast(tprintf("\aGame Driver: Database save #%d failed. "
            "Please take appropriate precautions.\n", nstat[NS_DBSAVE]));
  epoch_fail += background;

  if(epoch_fail > 9) {
    broadcast("\aGame Driver: Manually Saving. "
              "Response may be slow for a few minutes.\n");
    epoch_fail=0;
    dump_database();
    dump_interval=0;
  } else
    dump_interval=DUMP_INTERVAL-300;  /* 5-minute delay */
}

void do_crash(dbref player)
{
  DESC *d;

  /* Notify online */
  log_main("%s(#%d) executes @crash. G'nite World!", db[player].name, player);
  for(d=descriptor_list;d;d=d->next) {
    output2(d, "Crashing...!\n");
    process_output(d, 1);
  }

  /* Reboot without saving */
  restart_game();

  /* Reboot failed; terminate. */
  exit(2);
}


/* Kick-start game execution */
void start_game()
{
  char *s;
  dbref thing;
  int a;

  /* Enable the game driver for command processing */
  db[GAME_DRIVER].flags &= ~HAVEN;

  /* Execute @startup attribute */
  for(thing=0;thing<db_top;thing++)
    if(!(db[thing].flags & (GOING|HAVEN)) && Typeof(thing) != TYPE_GARBAGE &&
       db[thing].create_time < now && *(s=atr_get(thing, A_STARTUP))) {
      immediate_que(thing, s, thing);
      for(a=0;a<10;a++)
        *env[a]='\0';
    }
}

/* Initialize database configuration upon startup */
void init_config()
{
  /* Initialize gametime on new mare database */
  if(!BEGINTIME) {
    log_main("MARE Startdate - January 1, 0001: %s", strtime(now));
    log_main("%s", "");
    BEGINTIME=now;
    FIRST_YEAR=1;
  }

  /* Update database accounting information */
  init_dbstat();

}

/* Boot up into "single-user mode" and read database file */
void test_database()
{
  FILE *f;

  /* Initialize a limited system environment */
  init_compress();
  disable_startups=1;

  /* Open database file */
  if(!(f=fopen(db_file, "rb"))) {
    perror(db_file);
    return;
  }

  db_read_file=f;
  log_main("Reading file '%s'...", db_file);

  log_main(" Date: %s", strtime(fdate(db_file)));
  log_main(" Size: %ld bytes", fsize(db_file));

  /* Read entire db file */
  while(!load_database());

  db_print(stdout);
}
