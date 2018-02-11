/* prog/ctrl.c */
/* Softcoding program control commands */

#include "externs.h"

void do_switch(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  static int depth;
  int a;

  if(!argv[1])
    return;
  if(depth >= 20) {
    notify(player, "Too many nested @switch commands!");
    return;
  }

  /* Do the wildcard matching */
  for(a=1;a<99 && argv[a] && argv[a+1];a+=2)
    if(wild_match(arg1, argv[a], WILD_BOOLEAN)) {
      depth++;
      immediate_que(player, argv[a+1], cause);
      depth--;
      return;
    }                              

  /* Default command if no matching elements */
  if((a<100) && argv[a]) {
    depth++;
    immediate_que(player, argv[a], cause);
    depth--;
  }
}

void do_foreach(player, list, command, argv, pure, cause)
dbref player, cause;
char *list, *command, **argv, *pure;
{
  char *s;

  while((s=parse_perfect(&list, ' ')))
    if(*s) {
      strcpy(env[0], s);
      parse_que(player, command, cause);
    }
}

extern dbref iterate_player;
extern char *iterate_pattern;
extern char *iterate_command;

void do_iterate(player, pattern, arg2, argv, pure, cause)
dbref player, cause;
char *pattern, *arg2, **argv, *pure;
{
  if(!*pattern || !*arg2 || !argv[1] || !*argv[1] || !argv[2] || !*argv[2])
    return;
  if(iterate_player != NOTHING) {
    notify(player, "Nested @iterate statements not allowed.");
    return;
  }

  /* Set up iteration variables */
  iterate_player=player;
  WPTR(iterate_pattern, pattern);
  WPTR(iterate_command, argv[2]);

  /* Execute iteration command */
  immediate_que(player, argv[1], cause);

  /* Clear iteration mode */
  iterate_player=NOTHING;
}

void do_trigger(player, object, arg2, argv, pure, cause)
dbref player, cause;
char *object, *arg2, **argv, *pure;
{
  dbref thing;
  ATTR *attr;
  char *s;
  int a;

  /* validate target */
  if(!(s=strchr(object, '/'))) {
    notify(player, "You must specify an attribute name.");
    return;
  }
  *s++='\0';
  if((thing=match_thing(player, object, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* Validate attribute */
  if(!(attr=atr_str(thing, s))) {
    notify(player, "No match.");
    return;
  } if(!can_see_atr(player, thing, attr)) {
    notify(player, "Permission denied.");
    return;
  } if(attr->flags & (AF_HAVEN|AF_LOCK|AF_FUNC)) {
    notify(player, "Cannot trigger that attribute.");
    return;
  }

  /* Trigger attribute */
  for(a=0;a<10;a++)
    if(argv[a+1])
      strcpy(env[a], argv[a+1]);
    else
      *env[a]='\0';
  trigger_attr(thing, cause, attr->obj, attr->num);

  if(!Quiet(player))
    notify(player, "%s - Triggered.", db[thing].name);
}

/* Internal function to execute attribute #obj.num on <thing> */
void trigger_attr(dbref thing, dbref cause, int obj, unsigned char num)
{
  char *s=atr_get_obj(thing, obj, num);
  int dep=0;

  /* Skip around !, $, or ^ events. */
  if(*s == '!' || *s == '$' || *s == '^') {
    s++;
    while(*s && (s=strchr(s, ':')) && *(s-1) == '\\')
      s++;
    if(!s || !*s)
      return;

    /* Skip possible event-lock */
    if(*++s == '/') {
      while(*++s && (*s != '/' || dep)) {
        if(*s == '[')
          dep++;
        else if(*s == ']')
          dep--;
      }
      if(!*s++)
        return;
    }
  }

  if(*s)
    parse_que(thing, s, cause);
}

/* Instantly triggers <attr> on all objects in the database */
void trigger_all(unsigned char attr)
{
  dbref thing;
  char *s;
  int a;

  // Special precaution "create_time < now" for immediate commands
  // that create new objects in the database.

  for(thing=0;thing<db_top;thing++)
    if(!(db[thing].flags & (GOING|HAVEN)) && Typeof(thing) != TYPE_GARBAGE &&
       (Typeof(thing) != TYPE_PLAYER || (db[thing].flags & PLAYER_CONNECT)) &&
       db[thing].create_time < now && *(s=atr_get(thing, attr))) {
      immediate_que(thing, s, thing);
      for(a=0;a<10;a++)
        *env[a]='\0';
    }
}

// Forces an object to execute <command>.
//
void do_force(player, what, command, argv, pure, cause)
dbref player, cause;
char *what, *command, **argv, *pure;
{
  dbref victim=match_thing(player, what, MATCH_EVERYTHING);

  /* Check permissions */
  if(victim == NOTHING) {
    notify(player, "Sorry.");
    return;
  } if(!controls(player, victim, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* Put the command into the queue */
  parse_que(victim, command, cause);
}

void instant_force(player, thing, command, cause)
dbref player, thing, cause;
char *command;
{
  static int is_force;

  /* check permissions */
  if(Invalid(thing) || !controls(player, thing, POW_MODIFY) ||
     Typeof(thing) == TYPE_GARBAGE) {
    notify(player, "Permission denied.");
    return;
  } if(is_force) {
    notify(player, "Nested instant-force statements not allowed.");
    return;
  }

  /* immediately execute commands */
  is_force=1;
  process_command(thing, command, player, 0);
  is_force=0;
}

/* redirects output of a command to another object or player */
void do_redirect(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  extern dbref redirect_plr;
  extern dbref redirect_rcpt;
  dbref thing;

  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_SPOOF)) {
    notify(player, "Permission denied.");
    return;
  } if(redirect_plr != NOTHING) {
    notify(player, "Nested @redirect statements not allowed.");
    return;
  }

  redirect_plr=player;
  redirect_rcpt=thing;
  process_command(player, arg2, cause, 0);
  redirect_plr=NOTHING;
}

// Utility to time how long it takes for a command to execute.
// Command execution time is displayed in milliseconds.
//
void do_time(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  static int time_in_use;
  quad stop, start;

  /* Protect against recursion */
  if(time_in_use) {
    notify(player, "Multiple @time statements detected.");
    return;
  }

  /* Time command */
  time_in_use=1;
  start=get_time();

  process_command(player, pure, cause, 0);

  time_in_use=0;
  stop=get_time();

  /* Print results */
  notify(player, "Command execution: %.3f seconds", (stop-start)/1000.0);
}

// @misc command. Used for robots who expect normal TinyMUD error messages.
//
void do_misc(dbref player)
{
  notify(player, "Huh?  (Type \"help\" for help.)");
}

// Command to use an object in the room or inventory. Triggers @ause.
//
void do_use(dbref player, char *arg1, char *arg2, char **args)
{
  char buf[600], bufs[600];
  dbref obj;
  int a;
  
  if(!*arg1) {
    notify(player, "Use what?");
    return;
  }

  /* Check permissions */
  if((obj=match_thing(player, arg1, power(player, POW_REMOTE)?MATCH_EVERYTHING:
     MATCH_RESTRICT)) == NOTHING)
    return;

  /* Find out if player is locked from usage */
  if(!could_doit(player, obj, A_ULOCK)) {
    ansi_did_it(player, obj, A_UFAIL, "You cannot use that object.", A_OUFAIL,
                NULL, A_AUFAIL);
    return;
  } if(!(*atr_get(obj, A_USE)) || !(*atr_get(obj, A_AUSE))) {
    notify(player, "You don't know how to use that object.");
    return;
  }

  /* Display the messages and active @ause */
  sprintf(buf, "You use %s.", db[obj].name);
  sprintf(bufs, "uses %s.", db[obj].name);
  for(a=0;a<10 && args[a+1];a++)
    strcpy(env[a], args[a+1]);
  ansi_did_it(player, obj, A_USE, buf, A_OUSE, bufs, A_AUSE);
}

// Command to search the area or a specific object in the room.
//
void do_area_search(dbref player, char *arg1)
{
  static int depth;
  dbref thing=db[player].location;
  char buf[1024], buff[1024], *s;

  if(depth) {
    notify(player, "You can't use the search command in an @asearch.");
    return;
  }

  /* Match for an object */
  if(*arg1 && (thing=match_thing(player, arg1, MATCH_ME|MATCH_HERE|MATCH_CON))
     == NOTHING)
    return;
  if(Typeof(thing) == TYPE_PLAYER) {
    if(db[player].owner == thing)
      notify(player, "You can't search yourself.");
    else
      notify(player, "You need a search warrant to do that.");
    return;
  }

  /* Execute @asearch attribute on target */
  if(could_doit(player, thing, A_LSEARCH) && *(s=atr_get(thing, A_ASEARCH))) {
    depth++;
    immediate_que(thing, s, player);
    depth--;
    return;
  }

  /* Display failed messages to those in room */
  if(thing == db[player].location) {
    sprintf(buf, "searches the ground by %s feet, but finds nothing.",
            gender_subst(player, POSS));
    did_it(player, thing, A_FSEARCH,
           "You search the ground by your feet.. but you find nothing.",
           A_OFSEARCH, buf, 0);
  } else {
    sprintf(buf, "You search the area around %s.. but you find nothing.",
            db[thing].name);
    sprintf(buff, "searches the area around %s, but finds nothing.",
            db[thing].name);
    did_it(player, thing, A_FSEARCH, buf, A_OFSEARCH, buff, 0);
  }
}
