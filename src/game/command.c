/* game/command.c */

#include "externs.h"


/* list the commands */
/* A=#args.. 0=none parsed, 1=arg1 parsed, 2=arg2 parsed, 3=argv parsed,
             &4=combat mode only, &8=direct mode only, &16=don't parse arg2. */
/* B=alias.. 0=command typed in full, 1=alias for command */

/* Note: This allows you to specify POW_ANIMATION in the 'perms' table */
#define POW_ANIMATION -1

static struct cmdlist {
  char *name;
  void (*cmd)();
  unsigned int arg:5;
  unsigned int alias:1;
  char perms;
} lcmd[]={
  /* command name         function name         A,B     perms */
  {"+c",		do_com,			2,1},
  {"+ch",		do_channel,		1,1},
  {"+chan",		do_channel,		1,1},
  {"+channel",		do_channel,		1,0},
  {"+co",		do_com,			2,1},
  {"+com",		do_com,			2,0},
  {"+con",		do_player_config,	2,1},
  {"+conf",		do_player_config,	2,1},
  {"+config",		do_player_config,	2,0},
  {"+f",		do_finger,		2,1},
  {"+finger",		do_finger,		2,0},
  {"+i",		do_info,		1,1},
  {"+in",		do_info,		1,1},
  {"+inf",		do_info,		1,1},
  {"+info",		do_info,		1,0},
  {"+l",		do_laston,		1,1},
  {"+la",		do_laston,		1,1},
  {"+last",		do_laston,		1,1},
  {"+laston",		do_laston,		1,0},
  {"+m",		do_mail,		18,1},
  {"+ma",		do_mail,		18,1},
  {"+mail",		do_mail,		18,0},
  {"+mo",		do_motd,		0,1},
  {"+motd",		do_motd,		0,0},
  {"+t",		do_term,		2,1},
  {"+term",		do_term,		2,0},
  {"+tz",		do_tzone,		1,1},
  {"+tzone",		do_tzone,		1,0},
  {"+u",		do_uptime,		0,1},
  {"+up",		do_uptime,		0,1},
  {"+uptime",		do_uptime,		0,0},
  {"+v",		do_version,		0,1},
  {"+ver",		do_version,		0,1},
  {"+vers",		do_version,		0,1},
  {"+version",		do_version,		0,0},
  {"@add",		do_addparent,		2,1},
  {"@addp",		do_addparent,		2,1},
  {"@addparent",	do_addparent,		2,0},
  {"@addz",		do_addzone,		2,1},
  {"@addzone",		do_addzone,		2,0},
  {"@boot",		do_boot,		2,0, POW_BOOT},
  {"@br",		do_broadcast,		0,1, POW_BROADCAST},
  {"@broadcast",	do_broadcast,		0,0, POW_BROADCAST},
  {"@cancel",		do_cancel,		2,0},
  {"@capture",		do_capture,		2,0, POW_CAPTURE},
  {"@cboot",		do_cboot,		1,0, POW_BOOT},
  {"@cemit",		do_cemit,		2,0, POW_SPOOF},
  {"@ch",		do_chown,		2,1},
  {"@charge",		do_charge,		2,0, POW_MONEY},
  {"@cho",		do_chown,		2,1},
  {"@chown",		do_chown,		2,0},
  {"@chownall",		do_chownall,		2,0, POW_CHOWN},
  {"@class",		do_class,		2,0, POW_SECURITY},
  {"@clone",		do_clone,		2,0},
  {"@comblock",		do_comblock,		2,0, POW_COM},
  {"@comlock",		do_comlock,		2,0, POW_COM},
  {"@comvoice",		do_comvoice,		2,0, POW_COM},
  {"@config",		do_config,		10,0, POW_CONFIG},
  {"@console",		do_console,		2,0, POW_CONSOLE},
  {"@cpage",		do_cpage,		2,0, POW_CONSOLE},
  {"@crash",		do_crash,		0,0, POW_SHUTDOWN},
  {"@create",		do_create,		2,0},
  {"@ct",		do_ctrace,		0,1, POW_CONSOLE},
  {"@ctrace",		do_ctrace,		0,0, POW_CONSOLE},
  {"@cy",		do_cycle,		3,1},
  {"@cyc",		do_cycle,		3,1},
  {"@cycle",		do_cycle,		3,0},
  {"@db",		do_dbtop,		2,1, POW_DB},
  {"@dbt",		do_dbtop,		2,1, POW_DB},
  {"@dbtop",		do_dbtop,		2,0, POW_DB},
  {"@dec",		do_decompile,		1,1},
  {"@decomp",		do_decompile,		1,1},
  {"@decompile",	do_decompile,		1,0},
  {"@def",		do_defattr,		2,1},
  {"@defattr",		do_defattr,		2,0},
  {"@del",		do_delparent,		2,1},
  {"@delp",		do_delparent,		2,1},
  {"@delparent",	do_delparent,		2,0},
  {"@delz",		do_delzone,		2,1},
  {"@delzone",		do_delzone,		2,0},
  {"@destroy",		do_destroy,		2,0},
  {"@dig",		do_dig,			3,0},
  {"@dump",		do_dump,		0,0, POW_DB},
  {"@echo",		do_echo,		1,0},
  {"@ed",		do_edit,		3,1},
  {"@edit",		do_edit,		3,0},
  {"@emit",		do_emit,		1,0},
  {"@empower",		do_empower,		2,0, POW_SECURITY},
  {"@find",		do_find,		1,0},
  {"@flush",		do_flush,		2,0, POW_CONSOLE},
  {"@fo",		do_force,		2,1},
  {"@for",		do_force,		2,1},
  {"@force",		do_force,		2,0},
  {"@fore",		do_foreach,		2,1},
  {"@foreach",		do_foreach,		2,0},
  {"@halt",		do_halt,		2,0},
  {"@inactivity",	do_inactivity,		1,0},
  {"@input",		do_input,		3,0, POW_CONSOLE},
  {"@it",		do_iterate,		3,1},
  {"@iter",		do_iterate,		3,1},
  {"@iterate",		do_iterate,		3,0},
  {"@link",		do_link,		2,0},
  {"@list",		do_list,		2,0},
  {"@log",		do_log,			2,0, POW_SECURITY},
  {"@mem",		do_memory,		1,1},
  {"@memory",		do_memory,		1,0},
  {"@misc",		do_misc,		0,0},
  {"@name",		do_name,		2,0},
  {"@necho",		do_necho,		0,0},
  {"@nemit",		do_nemit,		0,0},
  {"@np",		do_npemit,		2,1},
  {"@npe",		do_npemit,		2,1},
  {"@npem",		do_npemit,		2,1},
  {"@npemit",		do_npemit,		2,0},
  {"@nuke",		do_nuke,		1,0, POW_NUKE},
  {"@oemit",		do_oemit,		2,0},
  {"@open",		do_open,		2,0},
  {"@oremit",		do_oremit,		2,0},
  {"@output",		do_output,		2,0, POW_CONSOLE},
  {"@passwd",		do_passwd,		9,0},
  {"@paste",		do_paste,		9,1},
  {"@pcreate",		do_pcreate,		9,0, POW_PCREATE},
  {"@pe",		do_pemit,		2,1},
  {"@pem",		do_pemit,		2,1},
  {"@pemit",		do_pemit,		2,0},
  {"@pennies",		do_pennies,		2,0, POW_MONEY},
  {"@pfile",		do_pfile,		2,0, POW_PFILE},
  {"@plane",		do_plane,		2,0, POW_PLANE},
  {"@poll",		do_poll,		0,0},
  {"@pow",		do_powers,		1,1},
  {"@power",		do_powers,		1,1},
  {"@powers",		do_powers,		1,0},
  {"@pr",		do_print,		1,1},
  {"@pri",		do_print,		1,1},
  {"@print",		do_print,		1,0},
  {"@ps",		do_ps,			1,0},
  {"@pu",		do_push,		1,1},
  {"@push",		do_push,		1,0},
  {"@re",		do_remit,		2,1},
  {"@reboot",		do_reboot,		1,0, POW_SHUTDOWN},
  {"@redef",		do_redefattr,		2,1},
  {"@redefattr",	do_redefattr,		2,0},
  {"@redir",		do_redirect,		2,1},
  {"@redirect",		do_redirect,		2,0},
  {"@remit",		do_remit,		2,0},
  {"@sea",		do_search,		2,1},
  {"@sear",		do_search,		2,1},
  {"@search",		do_search,		2,0},
  {"@sem",		do_semaphore,		2,1},
  {"@sema",		do_semaphore,		2,1},
  {"@semaphore",	do_semaphore,		2,0},
  {"@set",		do_set,			2,0},
  {"@shutdown",		do_shutdown,		0,0, POW_SHUTDOWN},
  {"@sitelock",		do_sitelock,		2,0, POW_SITELOCK},
  {"@st",		do_stats,		1,1},
  {"@stat",		do_stats,		1,1},
  {"@stats",		do_stats,		1,0},
  {"@sw",		do_switch,		3,1},
  {"@swi",		do_switch,		3,1},
  {"@swit",		do_switch,		3,1},
  {"@switch",		do_switch,		3,0},
  {"@tel",		do_teleport,		2,1},
  {"@teleport",		do_teleport,		2,0},
  {"@term",		do_terminate,		1,1, POW_CONSOLE},
  {"@terminate",	do_terminate,		1,0, POW_CONSOLE},
  {"@text",		do_text,		3,0},
  {"@ti",		do_time,		0,1},
  {"@time",		do_time,		0,0},
  {"@tr",		do_trigger,		3,1},
  {"@tri",		do_trigger,		3,1},
  {"@trig",		do_trigger,		3,1},
  {"@trigger",		do_trigger,		3,0},
  {"@uncapture",	do_uncapture,		1,0, POW_CAPTURE},
  {"@undef",		do_undefattr,		1,1},
  {"@undefattr",	do_undefattr,		1,0},
  {"@undestroy",	do_undestroy,		1,0},
  {"@unlink",		do_unlink,		1,0},
  {"@unlock",		do_unlock,		1,0},
  {"@up",		do_update,		1,1},
  {"@update",		do_update,		1,0},
  {"@upfront",		do_upfront,		1,0},
  {"@wait",		do_wait,		2,0},
  {"@wemit",		do_wemit,		0,0, POW_BROADCAST},
  {"@wipe",		do_wipeattr,		2,1},
  {"@wipeattr",		do_wipeattr,		2,0},
  {"@wipeout",		do_wipeout,		2,0, POW_NUKE},
  {"@zemit",		do_zemit,		2,0},
  {"@zone",		do_zone,		2,0},
  {"ann",		do_announce,		0,1},
  {"announce",		do_announce,		0,0},
  {"clear",		do_cls,			0,0},
  {"cls",		do_cls,			0,1},
  {"dr",		do_drop,		1,1},
  {"drop",		do_drop,		1,0},
  {"en",		do_enter,		1,1},
  {"ent",		do_enter,		1,1},
  {"enter",		do_enter,		1,0},
  {"ex",		do_examine,		2,1},
  {"exam",		do_examine,		2,1},
  {"examine",		do_examine,		2,0},
  {"exit",		do_exits,		1,1},
  {"exits",		do_exits,		1,0},
  {"get",		do_take,		1,0},
  {"give",		do_give,		2,0},
  {"go",		do_move,		1,1},
  {"goto",		do_move,		1,0},
  {"gr",		do_grab,		2,1},
  {"grab",		do_grab,		2,0},
  {"h",			do_help,		0,1},
  {"help",		do_help,		0,0},
  {"i",			do_inventory,		0,1},
  {"inv",		do_inventory,		0,1},
  {"inventory",		do_inventory,		0,0},
  {"j",			do_join,		1,1, POW_JOIN},
  {"join",		do_join,		1,0, POW_JOIN},
  {"l",			do_look,		1,1},
  {"leave",		do_leave,		0,0},
  {"lo",		do_look,		1,1},
  {"look",		do_look,		1,0},
  {"mo",		do_money,		1,1},
  {"mon",		do_money,		1,1},
  {"money",		do_money,		1,0},
  {"move",		do_move,		1,0},
  {"p",			do_page,		18,1},
  {"page",		do_page,		18,0},
  {"pose",		do_pose_cmd,		0,0},
  {"put",		do_put,			2,0},
  {"run",		do_run,			0,0},
  {"sa",		do_say,			0,1},
  {"say",		do_say,			0,0},
  {"sea",		do_area_search,		1,1},
  {"sear",		do_area_search,		1,1},
  {"search",		do_area_search,		1,0},
  {"su",		do_summon,		1,1, POW_SUMMON},
  {"summon",		do_summon,		1,0, POW_SUMMON},
  {"t",			do_talk,		1,1},
  {"ta",		do_take,		1,1},
  {"take",		do_take,		1,0},
  {"talk",		do_talk,		1,0},
  {"to",		do_to,			0,0},
  {"use",		do_use,			3,0},
  {"w",			do_whisper,		18,1},
  {"wh",		do_whisper,		18,1},
  {"where",		do_whereis,		1,1},
  {"whereis",		do_whereis,		1,0},
  {"whi",		do_whisper,		18,1},
  {"whisper",		do_whisper,		18,0},
  {"who",		do_who,			2,0},

};

/* End Animation Fix */
#undef POW_ANIMATION


/* List all commands available to a player */
void list_commands(dbref player)
{
  char buf[512];
  int a, col=get_cols(NULL, player);

  notify(player, "List of available hardcoded commands in alphabetical order:");
  notify(player, "%s", "");
  strcpy(buf, "");
  for(a=0;a < NELEM(lcmd);a++) {
    if(lcmd[a].alias || (lcmd[a].perms && !power(player, lcmd[a].perms)))
      continue;
    if(*buf && strlen(buf)+strlen(lcmd[a].name) >= col-2) {
      notify(player, "%s", buf);
      strcpy(buf, "");
    }
    if(*buf)
      strcat(buf, ", ");
    strcat(buf, lcmd[a].name);
  }
  if(*buf)
    notify(player, "%s", buf);
  notify(player, "%s", "");
}

/* Match a list of things for events */
static int list_check(thing, player, type, str)
dbref thing, player;
int type;
char *str;
{
  int match=0;

  DOLIST(thing, thing)
    if((thing == player || Typeof(thing) != TYPE_PLAYER) &&
       match_plane(player, thing) && atr_match(thing, player, type, str))
      match=1;

  return match;
}

/* Match a command word in a ^ event on specified object */
static int cmd_match(player, cmd, name, argv)
dbref player;
char *cmd, *name, **argv;
{
  dbref thing;
  int a;
  
  if((thing=match_thing(player, name, MATCH_QUIET|(power(player, POW_REMOTE)?
     MATCH_EVERYTHING:MATCH_RESTRICT))) == NOTHING)
    return 0;

  /* Set environment %0 through %9 */
  for(a=0;a<10 && argv[a+1];a++)
    strcpy(env[a], argv[a+1]);

  return atr_match(thing, player, '^', cmd);
}

/* Routine to check for $ and ! events and execute them */
int atr_match(thing, player, type, str)
dbref thing, player;
char type; /* A symbol not modified by compress(). '$', '^' or '!' */
char *str; /* String to match */
{
  struct all_atr_list *ptr, *list;
  char event[8192], *lock, *s, *t;
  int dep, dep2, match=0, locked;

  if(db[thing].flags & HAVEN)
    return 0;

  /* Does the player pass the @ulock? */
  if((locked=!could_doit(player, thing, A_ULOCK)) && type == '!')
    return 0;

  for(list=ptr=all_attributes(thing);ptr && ptr->type;ptr++) {
    /* Don't check unimportant attributes */
    if(!ptr->type || (ptr->type->flags & (AF_LOCK|AF_FUNC|AF_DATE|AF_BITMAP|
       AF_DBREF|AF_HAVEN)) || *ptr->value != type)
      continue;

    /* Search for first unescaped : */
    strcpy(s=t=event, uncompress(ptr->value)+1);
    while(*s && *s != ':') {
      if(*s == '\\' && *(s+1))
        *t++=*s++;
      *t++=*s++;
    }
    if(!*s || !*++s)
      continue;
    *t='\0';

    /* Make sure event matches */
    if((type == '^' && strcasecmp(str, event)) ||
       (type != '^' && !wild_match(str, event, 1)))
      continue;

    /* Search for a locked attribute */
    if(*s == '/') {
      lock=++s;
      dep=0;
      while(*s && (dep || *s != '/')) {
        if(*s == '(')
          dep++;
        else if(*s == ')')
          dep--;
        else if(*s == '[') {
          for(++s,dep2=1;*s && dep2;s++) {
            if(*s == '[')
              dep2++;
            else if(*s == ']')
              dep2--;
          }
          continue;
        }
        s++;
      }
      if(!*s)
        continue;
      *s++='\0';

      /* Check lock */
      if(!*s || !eval_boolexp(player, thing, lock))
        continue;
    }

    /* Is object locked against the player? */
    if(locked) {
      free(list);

      /* Only show 'No such command' message if obj has no @ufail/@aufail */
      if(*atr_get(thing, A_UFAIL) || *atr_get(thing, A_AUFAIL)) {
        ansi_did_it(player,thing,A_UFAIL,NULL,A_OUFAIL,NULL,A_AUFAIL);
        return 1;
      }
      return 0;
    }

    parse_que(thing, s, player);
    match=1;
  }

  free(list);
  return match;
}

/* Log the command to be executed */
static void log_command(dbref player, char *cmd, int direct)
{
  static int err;
  FILE *log;

  if(COMMAND_LOGSIZE < 2)
    return;

  if((log=fopen("logs/commands", "a"))) {
    fprintf(log, "%s(%d): %s\n", db[player].name, player, cmd);
    fclose(log);
    err=0;
  } else if(!err++)  /* only report error once */
    perror("logs/commands");

  // Limit # commands in current log, containing the most recent
  // commands entered into the game for TinyMare crash detection/recovery.

  if(!(nstat[NS_NCMDS] % COMMAND_LOGSIZE))
    rename("logs/commands", "logs/commands~");

  /* Is the player set Notice? If so, log his commands. */
  if(direct && IS(player, TYPE_PLAYER, PLAYER_NOTICE)) {
    if((log=fopen("logs/playerlog", "a"))) {
      fprintf(log, "%s(%d): %s\n", db[player].name, player, cmd);
      fclose(log);
      err=0;
    } else if(!err++)
      perror("logs/playerlog");
  }

  /* Log guest accounts */
  if(Guest(player)) {
    if((log=fopen("logs/guestlog", "a"))) {
      fprintf(log, "%s(%d): %s\n", db[player].name, player, cmd);
      fclose(log);
      err=0;
    } else if(!err++)
      perror("logs/guestlog");
  }
}

/* Prints a random error message from the file "typo.txt" */
static void error_message(dbref player)
{
  static char **errmsgs;
  static int errsize=0;

  FILE *f;
  char buf[1024], *s, **ptr;

  /* Objects are notified of an error in their command syntax */
  if(Typeof(player) != TYPE_PLAYER) {
    notify(player, "Unrecognizable command.");
    return;
  }

  /* First time through, read in all typo messages from file */
  if(!errmsgs) {
    if(!(f=fopen("msgs/typo.txt", "r"))) {
      if(errsize != -1) {
        perror("msgs/typo.txt");
        errsize=-1;
      }
    } else {
      errsize=0;
      while(ftext(buf, 1024, f)) {
        if((s=strchr(buf, '#')))
          *s='\0';
        if(!*buf)
          continue;

        if(!(errsize % 25)) {
          if(!(ptr=realloc(errmsgs, (errsize+25)*sizeof(char *)))) {
            perror("error_message()");
            break;
          }
          errmsgs=ptr;
        }
        errmsgs[errsize++]=stralloc(buf);
      }
      fclose(f);
    }
  }

  if(!errmsgs)
    notify(player, "Unrecognizable command. Type 'help' for help.");
  else {
    sprintf(env[0], "%d", nstat[NS_NFCMDS]);
    notify(player, "%s",
           parse_function(GOD, player, errmsgs[rand_num(errsize)]));
  }
}

/* Execute a MARE Command */

void process_command(dbref player, char *cmd, dbref cause, int direct)
{
  struct cmdlist *cp; /* command pointer */
  static hash *cmdhash=NULL;
  char cmdmain[strlen(cmd)+1], buff[8192], arg1[8192], arg2[8192];
  char *pure, *cmdarg, *argv[101];
  int argc=0, cparg, match, *ptr;
  char *r, *s;

  /* Command Layout Structure:
   *
   * cmdarg pure                               No Arguments
   * cmdarg arg1                               1 Argument
   * cmdarg arg1=arg2                          2 Arguments
   * cmdarg arg1=argv1,argv2,..,argv100        Many Arguments
   */

  /* check that a valid object is executing the command */
  if(Invalid(player) || Invalid(cause) || (player == GOD && cause != GOD) ||
     Invalid(db[player].location))
    return;

  /* erase surrounding whitespace */
  trim_spaces(&cmd);
  if(!*cmd)
    return;

  /* save current command for error detection */
  strcpy(log_text, cmd);
  log_dbref=player;
  dberrcheck=0;

  func_zerolev();  /* reset function invocation counter */
  speaker=player;  /* save %# value for $, !, and ^ events */

  /* initialize command hashtable */
  if(!cmdhash)
    cmdhash=make_hashtab(lcmd);

  /* increment command counters */
  nstat[NS_NCMDS]++;
  nstat[direct ? NS_NPCMDS : NS_NQCMDS]++;
  
  /* log commands for all objects except #0 and #1 */
  if(player > 1)
    log_command(player, cmd, direct);

  /* If an object is set Verbose, print commands to its owner */
  if((db[player].flags & VERBOSE) &&
     IS(db[player].owner, TYPE_PLAYER, PLAYER_CONNECT)) {
    DESC *d=Desc(db[player].owner);

    if(d) {
      output(d, tprintf("%s>> ", db[player].name));
      output(d, cmd);
      output(d, "\n");
    }
  }

  /* remark (does nothing) */
  if(cmd[0] == '@' && cmd[1] == '@')
    return;

  /* pure := everything but the first word of the command line */
  if(*(pure=strchrnul(cmd, ' ')))
    pure++;

  /* check for single-character commands */
  switch(*cmd) {
    case '"': do_say(player, NULL, NULL, NULL, cmd+1, cause); return;
    case '\'': do_to(player, NULL, NULL, NULL, cmd+1); return;
    case ':': do_pose(player, cause, cmd+1, 0); return;
    case ';': do_pose(player, cause, cmd+1, 1); return;
    case '=': do_com(player, "", cmd+1); return;
    case '\\': room_emote(player, cause, NULL, cmd+1, 0); return;
    case '#':
      if(!isdigit(cmd[1]))
        break;
      instant_force(player, atoi(cmd+1), pure, cause);
      return;
  }

  /* Match the entire command-line with an exit alias or cardinal direction */
  if((Typeof(player) == TYPE_PLAYER || Typeof(player) == TYPE_THING) &&
     (match_word("home down d up u north n northeast ne east e southeast se "
                 "south s southwest sw west w northwest nw", cmd) ||
      match_thing(player,cmd,MATCH_EXIT|MATCH_QUIET|MATCH_INVIS) != NOTHING)) {
    do_move(player, cmd);
    return;
  }

  /* Process Builtin Commands */

  /* cmdarg := first word of the command */
  strcpy(cmdarg=cmdmain, cmd);
  if(*(s=strchrnul(cmdmain, ' ')))
    *s++='\0';
  while(*s == ' ')
    s++;
  
  /* Search for a builtin command in the hash table and match powers */
  cp=lookup_hash(cmdhash, cmdarg);
  if(cp && cp->perms && ((cp->perms == -1 && !power(player, POW_ANIMATION)) ||
     (cp->perms > 0 && !power(player, cp->perms))))
    cp=NULL;

  cparg=cp?cp->arg:3;  /* By default evaluate all arguments in command */

  /* Check direct mode */
  if((cparg & 8) && !direct) {
    notify(player, "Command available in direct mode only.");
    return;
  }

  /* Parse Argument 1 */
  if((cparg & 3) == 0)  /* command expects no args, etc. */
    strcpy(arg1, s);
  else
    evaluate(&s, arg1, player, cause, ((cparg & 3) == 1)?'\0':'=');

  /* Parse Argument 2 */
  if((cparg & 3) < 2)
    *arg2='\0';
  else if(cparg & 16)  /* copy arg2 without parsing */
    strcpy(arg2, s);
  else {
    strcpy(r=buff, s);  /* copy buffer space so we can reuse for argv[] */
    evaluate(&r, arg2, player, cause, '\0');
  }

  /* Parse Arguments 3+ (argv) */
  memset(argv, 0, sizeof(argv));
  if((cparg & 3) == 3) {
    for(argc=1;*s && argc < 101;argc++) {
      evaluate(&s, buff, player, cause, ',');
      APTR(argv[argc], buff);
    }
  }

  /* Execute the command procedure */
  if(cp) {
    cp->cmd(player,arg1,arg2,argv,pure,cause,direct);
    return;
  }

  /* Check for '@attr obj=value' setting */
  if(*cmdarg == '@' || *cmdarg == '&' || *cmdarg == '$')
    if(test_set(player, cmdarg, arg1, arg2))
      return;

  /* Leave aliases */
  if(Typeof(db[player].location) == TYPE_THING &&
     exit_alias(db[player].location, A_LALIAS, cmd)) {
    do_leave(player);
    return;
  }

  /* Enter aliases */
  DOLIST(match, db[db[player].location].contents)
    if(Typeof(match) == TYPE_THING && match_plane(player, match) &&
       exit_alias(match, A_EALIAS, cmd)) {
      do_enter(player, tprintf("#%d", match));
      return;
    }

  /* Match a command word event */
  if(*arg1 && cmd_match(player, cmdarg, arg1, argv))
    return;

  /* Try matching user defined commands. First check the universal zones,
   * which take precedence over any player-defined command. Second, check all
   * room zones and nearby objects, executing double occurrences if found. */

  for(match=0,ptr=uzo;ptr && *ptr != NOTHING;ptr++)
    match |= atr_match(*ptr, player, '$', cmd);
  if(match)
    return;

  /* Check room zones */
  for(ptr=Zone(player);ptr && *ptr != NOTHING;ptr++) {
    if(db[*ptr].flags & ZONE_UNIVERSAL)
      continue;
    match |= atr_match(*ptr, player, '$', cmd);
  }

  /* Check all nearby objects */
  match |= atr_match(db[player].location, player, '$', cmd);
  match |= list_check(db[db[player].location].contents, player, '$', cmd);
  match |= list_check(db[db[player].location].exits, player, '$', cmd);
  match |= list_check(db[player].contents, player, '$', cmd);

  if(match)
    return;

  /* No match found. Print a random error message. */

  nstat[NS_NFCMDS]++;
  error_message(player);

  /* Log failed command */
  if(LOG_FAILED_CMDS && Typeof(player) != TYPE_PLAYER) {
    FILE *log;

    if((log=fopen("logs/playerlog","a"))) {
      fprintf(log, "FAILED %s(%d): %s\n", db[player].name, player, cmd);
      fclose(log);
    } else
      perror("logs/playerlog");
    if(game_online)
      log_main("FAILED %s(#%d): %s", db[player].name, player, cmd);
  }

  /* Notify object's owner */
  if(Typeof(player) != TYPE_PLAYER && !(db[player].flags & PUPPET))
    notify(db[player].owner, "Failed Command on %s: %s",
           unparse_object(db[player].owner, player), cmd);
}
