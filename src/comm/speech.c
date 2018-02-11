/* comm/speech.c */
/* Commands which involve speaking */

#include "externs.h"
#include <stdarg.h>

/* Cause of messages (%#) */
dbref speaker=NOTHING;

/* Track @redirect command */
dbref redirect_plr=NOTHING;
dbref redirect_rcpt=NOTHING;

/* Track @iterate command */
dbref iterate_player=NOTHING;
char *iterate_pattern;
char *iterate_command;

/* If 1, notify_except() and notify_room() will not check @vlock of speaker */
static int pass_vlock;

/* If valid, denotes that notify_room() should send messages to another room */
dbref notify_location=NOTHING;


// Returns 1 if <player> can speak in <room> due to @slock restrictions,
// and 0 if not. If <msg> = 1, then show @sfail and trigger @asfail.
//
int pass_slock(dbref player, dbref room, int msg)
{
  if(power(player, POW_SPOOF) || could_doit(player, room, A_SLOCK))
    return 1;

  if(msg)
    ansi_did_it(player, room, A_SFAIL, "The room is muted.", 0, 0, A_ASFAIL);
  return 0;
}

// Check for spoofing when @emitting to a room. Spoofing is when a player
// begins the message with the name of another player or object in the room.
//
// skip=1: Don't spoof-check first line of message.
//
int can_emit_msg(dbref player, dbref target, dbref loc, char *msg, int skip)
{
  char mybuf[8192], *r, *s=msg;
  dbref thing, first;

  /* Scan <msg> for newline characters, copying the first word of each line to
     'mybuf' (stripping ansi codes and bell characters) */
  do {
    r=mybuf;
    while(*s && *s != ' ' && *s != '\n' && *s != '\t') {
      if(*s == '\e')
        skip_ansi(s);
      else if(*s == '\a')
        s++;
      else
        *r++=*s++;
    }
    *r='\0';

    /* Skip any lines for spoof-checking? */
    if(skip) {
      skip--;
      continue;
    }

    /* Check if we're spoofing a channel name */
    if(*mybuf == '[' && !power(player, POW_SPOOF))
      return 0;

    /* Check if we're spoofing a player name */
    if(r-mybuf < 3 || r-mybuf > 16)  /* Names must be between 3 and 16 chars */
      continue;
    if((thing=lookup_player(mybuf)) > NOTHING && thing != target &&
       !strcmp(db[thing].name, mybuf) && !controls(player, thing, POW_SPOOF))
      return 0;

    /* Check if we're spoofing a player in the room */
    if(Invalid(loc))
      continue;
    DOLIST(first, db[loc].contents)
      if(first != target && match_plane(player, first) &&
         !strcmp(db[first].name, mybuf) && !controls(player, first, POW_SPOOF))
        return 0;
  } while(*s && (s=strchr(s, '\n'), s++));

  return 1;
}

/* relay message. bypass puppet and check for ! events */
static void notify_relay(dbref player, char *msg)
{
  static int relay=0, depth=0;
  int a, len[10], plane=NOTHING, pow_plane, pow_spoof;
  char buf[3][8192], *saveptr[10], *r, *s, *t=NULL;
  dbref thing;

  /* Check if string matches @listen wildcards */
  if(*(s=atr_get(player, A_LISTEN))) {
    /* Save environment %0 through %9 */
    for(a=0;a<10;a++) {
      len[a]=strlen(env[a])+1;
      memcpy(saveptr[a]=alloca(len[a]), env[a], len[a]);
    }

    if(wild_match(msg, s, 1)) {
      /* Activate @ahear on object */
      if(*(s=atr_get(player, A_AHEAR)))
        parse_que(player, s, speaker);

      /* Relay message to all other objects listed in @relay attribute */
      if(!relay && *(s=atr_get(player, A_RHEAR))) {
        strcpy(buf[2], s);

        /* Fill environment %0 through %9 with non-ansi-stripped text */
        wild_match(msg, atr_get(player, A_LISTEN), WILD_ANSI|1);

        pronoun_substitute(buf[0], player, speaker, buf[2]);
        relay=1;

        pow_spoof=power(player, POW_SPOOF);
        pow_plane=power(player, POW_PLANE);

        /* Loop on each object stored in @relay */
        strcpy(buf[2], atr_get(player, A_RELAY));
        pronoun_substitute(buf[1], player, speaker, buf[2]);
        s=buf[1];
        while((r=parse_up(&s, ' '))) {
          /* Determine if we relay to only the object or its contents list */
          if(*r == '=')
            a=1, r++;
          else if(*r == '+')
            a=2, r++;
          else
            a=0;

          if(pow_plane && (t=strchr(r, ':'))) {
            *t++='\0';
            plane=atoi(t);  /* Grab destination plane# */
          }
          if((thing=match_thing(player, r, MATCH_ME|MATCH_HERE|MATCH_ABS|
             MATCH_PLAYER|MATCH_QUIET)) != NOTHING) {
            if((thing == player && db[speaker].location == player) ||
               (thing == db[player].location && speaker != player &&
                db[speaker].location == db[player].location))
              continue;
            if((!nearby(player,thing) && !controls(player,thing,POW_REMOTE)) ||
               (!pow_spoof && !can_emit_msg(player, NOTHING, thing, buf[0],0)))
              continue;

            /* Relay message, looping through <thing> and its contents list */

            if(a == 2)
              thing=db[thing].contents;
            while(thing != NOTHING) {
              if(player != thing && (Typeof(db[thing].location) != TYPE_ROOM ||
                 (t && (plane == NOTHING || db[thing].plane == NOTHING ||
                   db[thing].plane == plane)) ||
                 (!t && db[speaker].location != NOTHING &&
                  match_plane(speaker, thing)))) {
                if(IS(thing, TYPE_THING, PUPPET) &&
                   db[thing].location == db[db[thing].owner].location &&
                   match_plane(thing, db[thing].owner))
                  notify_relay(thing, buf[0]);
                else
                  notify(thing, "%s", buf[0]);
              }

              if(a == 1)
                break;
              thing=(a ? db[thing].next:db[thing].contents);
              a=2;
            }
          }
        }

        relay=0;
      }
    }

    /* Restore environment %0 through %9 */
    for(a=0;a<10;a++)
      memcpy(env[a], saveptr[a], len[a]);
  }

  /* Now check for multi listeners */
  if(++depth < 3 && could_doit(speaker, player, A_ULOCK))
    atr_match(player, speaker, '!', msg);
  depth--;
}

/* send message to user */
void notify(dbref player, char *fmt, ...)
{
  DESC *d=NULL;
  va_list args;
  char message[16384];

  if(Invalid(player))
    return;
  if(player == redirect_plr)
    player=redirect_rcpt;

  /* Determine if we need to parse the message */
  if(!(speaker == player && player == iterate_player) &&
     Typeof(player) == TYPE_PLAYER &&
     (!(db[player].flags & PLAYER_CONNECT) || !(d=Desc(player))))
    return;
   
  /* Parse message format */
  va_start(args, fmt);
  vsprintf(message, fmt, args);
  message[8000]='\0';
  va_end(args);

  /* Redirect output through @iterate command */
  if(speaker == player && player == iterate_player) {
    if(wild_match(message, iterate_pattern, 1))
      parse_que(player, iterate_command, player);
    return;
  }

  /* Messages to players go directly to network output */
  if(Typeof(player) == TYPE_PLAYER) {
    if(d) {
      output(d, message);
      output(d, "\n");
    }
    return;
  }

  /* If player is a puppet, relay message to puppet's owner */
  if((db[player].flags & PUPPET) && IS(db[player].owner, TYPE_PLAYER,
     PLAYER_CONNECT) && (d=Desc(db[player].owner))) {
    output(d, db[player].name);
    output(d, "> ");
    output(d, message);
    output(d, "\n");
  }

  notify_relay(player, message);
}

/* regular speech command */
void do_say(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  char *s;

  if(!pass_slock(player, db[player].location, 1))
    return;

  s=parse_function(player, cause, pure);

  pass_vlock=1;

  notify(player, "You say, \"%s\"", s);
  notify_except(player, player, "%s says, \"%s\"", db[player].name, s);

  pass_vlock=0;
}

/* whisper aloud to someone in the room */
void do_whisper(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  char *s;
  dbref who, first;
  
  if(Typeof(player) != TYPE_PLAYER) {
    notify(player, "Only players may whisper.");
    return;
  }

  if((who=lookup_player(arg1)) <= NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  } if(player == who) {
    notify(player, "That's rather hard.");
    return;
  } if(!power(player,POW_REMOTE) && (db[who].location != db[player].location ||
       !(db[who].flags & PLAYER_CONNECT))) {
    notify(player, "That player isn't here.");
    return;
  }

  s=parse_function(player, cause, arg2);

  if(pass_slock(player, db[player].location, 0))
    DOLIST(first, db[db[player].location].contents) {
      if(first == player || first == who || !match_plane(player, first))
        continue;
      notify(first, "You notice %s whisper something in %s%s ear.",
             db[player].name, db[who].name, possname(db[who].name));
    }

  if(*s == ':' || *s == ';')  {
    notify(player, "You whisper-posed %s with \"%s%s %s\".", db[who].name,
           db[player].name, *s==';'?possname(db[player].name):"", s+1);
    notify(who, "%s whisper-poses: %s%s %s", db[player].name, db[player].name,
           *s==';'?possname(db[player].name):"", s+1);
  } else {
    notify(player, "You whisper \"%s\" to %s.", s, db[who].name);
    notify(who, "%s whispers \"%s\" to you.", db[player].name, s);
  }
}

/* pose a message */
void do_pose_cmd(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  do_pose(player, cause, pure, 0);
}

void do_pose(dbref player, dbref cause, char *msg, int type)
{
  char *s;
  
  if(!pass_slock(player, db[player].location, 1))
    return;

  s=parse_function(player, cause, msg);

  pass_vlock=1;

  notify_except(player, NOTHING, "%s%s %s", db[player].name,
         type?possname(db[player].name):"", s);

  pass_vlock=0;
}


/* general emote commands */

enum { EMIT, PEMIT, REMIT, OEMIT, OREMIT, ZEMIT, PRINT,
       NOPARSE=0x80 };

void do_echo(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  notify(player, "%s", parse_function(player, cause, arg1));
}

void do_necho(player, arg1, arg2, argv, pure)
dbref player;
char *arg1, *arg2, **argv, *pure;
{
  notify(player, "%s", pure);
}

void do_emit(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, cause, 0, arg1, EMIT);
}

void do_nemit(player, arg1, arg2, argv, pure)
dbref player;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, player, 0, pure, EMIT|NOPARSE);
}

void do_pemit(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, cause, arg1, arg2, PEMIT);
}

void do_npemit(player, arg1, arg2, argv, pure)
dbref player;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, player, arg1, pure, PEMIT|NOPARSE);
}

void do_remit(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, cause, arg1, arg2, REMIT);
}

void do_oemit(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, cause, arg1, arg2, OEMIT);
}

void do_oremit(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, cause, arg1, arg2, OREMIT);
}

void do_zemit(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, cause, arg1, arg2, ZEMIT);
}

void do_print(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  room_emote(player, cause, 0, arg1, PRINT);
}

void room_emote(player, cause, target, msg, type)
dbref player, cause;
char *target, *msg;
int type;
{
  dbref thing=((type & ~NOPARSE) == PRINT)?cause:NOTHING;
  dbref loc=db[player].location, *p;
  char buff[8192], *s=buff;
  
  /* Parse message */
  if(type & NOPARSE) {
    type &= ~NOPARSE;
    s=msg;
    if(type == PEMIT)
      while(*s)
        if(*s++ == '=')
          break;
  } else
    pronoun_substitute(buff, player, cause, msg);

  /* Check target */
  if(thing == NOTHING && target) {
    if((thing=match_thing(player, target, MATCH_EVERYTHING)) == NOTHING)
      return;

    // @pemit, @oemit, and @oremit can send messages to players/objects in
    // owned rooms or to anyone in the invoker's zone if the invoker is a
    // Zone Object.

    if((type == PEMIT || type == OEMIT || type == OREMIT) &&
       !nearby(player, thing) && !controls(player, thing, POW_REMOTE) && 
       !controls(player, db[thing].location, POW_REMOTE) && 
       (Typeof(player) != TYPE_ZONE || (!(db[player].flags & ZONE_UNIVERSAL) &&
        inlist(Zone(thing), player)))) {
      if(!Quiet(player))
        notify(player, "That object is not nearby.");
      return;
    }

    switch(type) {
    case PEMIT: case OEMIT: case OREMIT:
      loc=db[thing].location;
      break;
    case REMIT:
      if(thing != db[player].location && !controls(player, thing, POW_REMOTE)) {
        if(!Quiet(player))
          notify(player, "You are not in that room.");
        return;
      }
      loc=thing;
      thing=NOTHING;
      break;
    case ZEMIT:
      if(!controls(player, thing, POW_MODIFY) ||
         !can_emit_msg(player, NOTHING, NOTHING, s, 0)) {
        notify(player, "Permission denied.");
        return;
      } if(Typeof(thing) != TYPE_ZONE) {
        notify(player, "That is not a zone object.");
        return;
      }
      loc=NOTHING;
    }
  } else if(type == PRINT && Typeof(player) != TYPE_ZONE &&
            (!match_plane(player, thing) || mainroom(player)!=mainroom(thing)))
    return;

  if(loc != NOTHING) {
    /* Check @slock */
    if(!pass_slock(player, loc, 1))
      return;

    /* Check if first word of message contains someone's name in the room */
    if(((type!=PEMIT && type!=PRINT) || !controls(player, thing, POW_SPOOF)) &&
       !can_emit_msg(player, thing, loc, s, 0)) {
      notify(player, "Permission denied.");
      return;
    }
  }
  
  /* Display the message */

  if(type == EMIT) {
    pass_vlock=1;
    notify_except(player, NOTHING, "%s", s);
    pass_vlock=0;
  } else if(type == PEMIT || type == PRINT)
    notify(thing, "%s", s);
  else if((type == OEMIT || type == OREMIT) && !Invalid(thing))
    notify_room(thing, thing, type==OEMIT?NOTHING:db[thing].location, "%s", s);
  else if(type == ZEMIT) {
    pass_vlock=1;
    for(p=db[thing].zone;p && *p != NOTHING;p++)
      notify_except(*p, NOTHING, "%s", s);
    pass_vlock=0;
  } else {
    pass_vlock=1;
    if(db[loc].contents != NOTHING)
      notify_except(db[loc].contents, thing, "%s", s);
    pass_vlock=0;
  }
}

void do_announce(player, arg1, argc, argv, arg2)
dbref player;
char *arg1, *argc, **argv, *arg2;
{
  char buf[9000];
  DESC *d;

  /* check permissions */
  if(!*arg2) {
    notify(player, "No message.");
    return;
  } if(Typeof(player) != TYPE_PLAYER && !power(player, POW_SPOOF)) {
    notify(player, "You can't do that.");
    return;
  }

  /* guests cannot announce */
  if(Guest(player)) {
    notify(player, "Guests cannot announce messages. Use the [public] channel "
           "instead, by typing \"=your message\" on a blank line (without the "
           "quotes).");
    return;
  }

  /* only users who've played more than 2 hours can announce messages */
  if(ANNOUNCE_COST && Typeof(player) == TYPE_PLAYER &&
     db[player].data->age < 7200) {
    notify(player,
         "This command announces your message to everyone online and costs "
         "%d %s. That probably isn't what you want right now. Try using the "
         "[public] channel instead, by typing \"=your message\" on a blank "
         "line (without the quotes).", ANNOUNCE_COST, MONEY_PLURAL);
    return;
  }
  
  /* check costs */
  if(ANNOUNCE_COST && !power(player, POW_ANNOUNCE)) {
    if(!charge_money(player, ANNOUNCE_COST, 0)) {
      notify(player, "It takes %d %s to make an announcement.",
             ANNOUNCE_COST, MONEY_PLURAL);
      return;
    }
    notify(player, "You have been charged %d %s for that announcement.",
           ANNOUNCE_COST, MONEY_PLURAL);
  }

  if(*arg2 == ':' || *arg2 == ';')
    sprintf(buf, "%s announce-poses: %s%s %s\n", db[player].name,
            db[player].name, (*arg2==':')?"":possname(db[player].name),arg2+1);
  else
    sprintf(buf, "%s announces: %s\n", db[player].name, arg2);

  for(d=descriptor_list;d;d=d->next)
    if(d->flags & C_CONNECTED)
      output(d, buf);
}

void do_broadcast(player, arg1, argc, argv, arg2)
dbref player;
char *arg1, *argc, **argv, *arg2;
{
  DESC *d;
  
  if(!*arg2) {
    notify(player, "No message.");
    return;
  }
  
  for(d=descriptor_list;d;d=d->next)
    if(!(d->flags & C_NOMESSAGE))
      output2(d, tprintf("Official broadcast from %s: %s\n", db[player].name,
              arg2));
}

/* wizard-emit, wall-emit, world-emit, whatever.. */
void do_wemit(player, arg1, argc, argv, arg2)
dbref player;
char *arg1, *argc, **argv, *arg2;
{
  DESC *d;
  
  for(d=descriptor_list;d;d=d->next)
    if(d->flags & C_CONNECTED)
      output(d, tprintf("%s\n", arg2));
}

/* high-priority output message to player's connection */
void do_output(dbref player, char *arg1, char *arg2)
{
  DESC *d;
  dbref thing=match_thing(player, arg1, MATCH_EVERYTHING);

  if(thing == NOTHING)
    return;

  if(Typeof(thing) != TYPE_PLAYER)
    notify(player, "Output messages can only be sent to players.");
  else if(!(d=Desc(thing)))
    notify(player, "That player is not logged in.");
  else
    output2(d, tprintf("%s\n", arg2));
}

void do_page(dbref player, char *arg1, char *arg2)
{
  DESC *d;
  char buf[512], mudname[32], *p, *r, *s;
  int i, j, hidden, count=0, suppress=0, remote=power(player, POW_REMOTE);
  dbref thing, obj[20];

  if(Typeof(player) != TYPE_PLAYER && !power(player, POW_SPOOF)) {
    notify(player, "Objects cannot page anyone.");
    return;
  }

  d=(Typeof(player) == TYPE_PLAYER)?Desc(player):NULL;

  /* Retrieve playerlist from last page command if <arg1> is blank */
  if(!*arg1) {
    if(!d || !*d->env[3]) {
      notify(player, "You must specify a list of players.");
      notify(player, "Usage: page <player list>=<message>");
      return;
    }
    strmaxcpy(buf, d->env[3], 512);
    s=buf;
  } else {
    if(d)
      WPTR(d->env[3], arg1);
    s=arg1;
  }

  if(!*arg2) {
    notify(player, "No message specified.");
    return;
  } if(!can_emit_msg(player, NOTHING, NOTHING, arg2, 1)) {
    notify(player, "Sorry, your message contains a spoof.");
    return;
  }

  /* Store and count recipients */
  while((r=parse_up(&s, ' '))) {
    /* Recipients of the form User@Mudname use RWHO database */
    if((p=strchr(r, '@'))) {
      strmaxcpy(mudname, p+1, 28);
      if(strlen(mudname) < 4 || strcasecmp(mudname+strlen(mudname)-4, "MARE"))
        strcat(mudname, "MARE");
      if(strcasecmp(mudname, MUD_NAME)) {
        notify(player, "TinyMARE RWHO Server not yet supported.");
        continue;
      }
      *p='\0';
    }

    /* Local Server */
    if((obj[count]=lookup_player(r)) <= NOTHING) {
      notify(player, "No such player '%s'.", r);
      continue;
    }
    for(i=0;i<count && obj[i] != obj[count];i++);
    if(i == count && ++count > 20) {
      notify(player, "Too many players listed.");
      return;
    }
  }

  /* Check for sufficient funds */
  if(!charge_money(player, PAGE_COST*count, 0)) {
    notify(player, "You don't have enough %s.", MONEY_PLURAL);
    return;
  }

  /* Validate the list of players */
  for(i=0;i<count;i++) {
    thing=obj[i];
    if(player == thing)
      suppress=-1;

    /* Check paging permissions on target player */
    hidden=0;
    if(!(db[thing].flags & PLAYER_CONNECT) || (!controls(player, thing,
       POW_WHO) && !could_doit(player, thing, A_HLOCK))) {
      if(controls(player, thing, POW_WHO))
        notify(player, "%s is not connected.", db[thing].name);
      else
        notify(player, "%s is seemingly not connected.", db[thing].name);
      if(*(s=atr_parse(thing, player, A_AWAY)))
        notify(player, "Away message from %s: %s", db[thing].name, s);
      if(!(db[thing].flags & PLAYER_CONNECT))
        continue;
      hidden=1;
    }
    if(!could_doit(player, thing, A_PLOCK)) {
      if(!hidden) {
        notify(player, "%s%s is not contactable.", remote?"Warning: ":"",
               db[thing].name);
        if(*(s=atr_parse(thing, player, A_HAVEN)))
          notify(player, "Haven message from %s: %s", db[thing].name, s);
      }
      if(!remote)
        continue;
    }

    /* Check paging permissions on person doing the page */
    if(!could_doit(thing, player, A_PLOCK) && !remote) {
      notify(player, "Your pages are locked to %s.", db[thing].name);
      continue;
    }

    /* Build [Time], Name, and (Alias) Prefix */
    s=buf;
    if(*(atr_get(thing, A_IDLE)))
      s+=sprintf(s, "\e[1;36m[\e[37m%-8.8s\e[36m]\e[m ", mktm(thing, now)+11);
    default_color=unparse_color(thing, player, 14);
    s+=sprintf(s, "%s", textcolor(7, default_color));
    s+=sprintf(s, "%s", colorname(player));
    default_color=7;
    if(*(r=atr_get(player, A_ALIAS)) && strlen(r) <= 10)
      s+=sprintf(s, " (%s)", r);
    s+=sprintf(s, " pages");

    /* Build Recipients list */
    if(count > 1) {
      for(j=0;j<count;j++) {
        if(j && j == count-1) {
          if(count > 2)
            *s++=',';
          strcpy(s, " and");
          s+=4;
        } else if(j)
          *s++=',';
        *s++=' ';
        s+=sprintf(s, "%s", (thing == obj[j])?"you":db[obj[j]].name);
      }
      strcpy(s, " with");
    }

    /* Special formatting for poses */
    j=(*arg2 == ':' || *arg2 == ';');
    notify(thing, "%s: %s%s%s%s\e[m", buf, j?db[player].name:"",
           (*arg2 == ';')? possname(db[player].name):"", j?" ":"", arg2+j);

    if(player != thing && *(s=atr_get(thing, A_APAGE))) {
      strcpy(env[0], arg2);
      parse_que(thing, s, player);
      *env[0]='\0';
    }

    if(hidden)
      continue;

    if(!suppress)
      suppress=1;
    if((j=getidletime(thing)) > 300 && *(s=atr_parse(thing, player, A_IDLE)))
      notify(player, "%s is %s idle: %s", db[thing].name,time_format(TMA,j),s);
  }

  if(suppress != 1)
    return;

  /* Build final names list for pager to view */
  for(s=buf,j=0;j<count;j++) {
    if(j && j == count-1) {
      if(count > 2)
        *s++=',';
      strcpy(s, " and ");
      s+=5;
    } else if(j) {
      *s++=',';
      *s++=' ';
    }
    s+=sprintf(s, "%s%s", db[obj[j]].name, isbusy(obj[j])?"(Delayed)":"");
  }

  j=(*arg2==':' || *arg2==';');
  notify(player, "You paged %s with: %s%s%s%s", buf, j?db[player].name:"",
         (*arg2==';')?possname(db[player].name):"", j?" ":"", arg2+j);
}

// Send message to all in <player>'s room, except for objects <except> and
// <except2>. This function aliases 'notify_except', which has the same
// arguments but <except2> is missing.
//
// If global `notify_location' is != NOTHING, then the message is sent to that
// location instead of <player>'s room.
//
void notify_room(dbref player, dbref except, dbref except2, char *fmt, ...)
{
  va_list args;
  char message[16384], vlock[8192];
  dbref loc=Invalid(notify_location)?db[player].location:notify_location;
  dbref first;

  if(Invalid(loc) || IS(loc, TYPE_ROOM, ROOM_XZONE))
    return;

  /* Parse message format */
  va_start(args, fmt);
  vsprintf(message, fmt, args);
  message[8000]='\0';
  va_end(args);

  /* Store player's @vlock for fast access */
  if(pass_vlock || Typeof(player) == TYPE_ROOM)
    *vlock='\0';
  else
    strcpy(vlock, atr_get(player, A_VLOCK));

  /* Send message to the room itself */
  if(loc != except && loc != except2 &&
     (!*vlock || eval_boolexp(loc, player, vlock)))
    notify(loc, "%s", message);

  DOLIST(first, db[loc].contents)
    if(match_plane(player, first) && first != except && first != except2 &&
       (!*vlock || eval_boolexp(first, player, vlock))) {
      if(IS(first, TYPE_THING, PUPPET) && db[first].location ==
         db[db[first].owner].location && match_plane(first, db[first].owner))
        notify_relay(first, message);
      else
        notify(first, "%s", message);
    }
}

// Send message to all objects in the room that are in list <list>, or if <inv>
// = 1, not in list. The speaking player is derived from the first <list> item.
//
void notify_list(dbref *list, int inv, dbref except, char *fmt, ...)
{
  va_list args;
  char message[16384], vlock[8192];
  dbref loc, first;

  if(!list)
    return;
  loc=db[*list].location;
  if(IS(loc, TYPE_ROOM, ROOM_XZONE))
    return;

  /* Parse message format */
  va_start(args, fmt);
  vsprintf(message, fmt, args);
  message[8000]='\0';
  va_end(args);

  /* Send message to the room itself */
  if(inv)
    notify(loc, "%s", message);

  /* Store player's @vlock for fast access */
  if(pass_vlock || Typeof(*list) == TYPE_ROOM)
    *vlock='\0';
  else
    strcpy(vlock, atr_get(*list, A_VLOCK));

  DOLIST(first, db[loc].contents)
    if(match_plane(*list, first) && first != except &&
       (!inlist(list, first)) == inv &&
       (!*vlock || eval_boolexp(first, *list, vlock))) {
      if(IS(first, TYPE_THING, PUPPET) && db[first].location ==
         db[db[first].owner].location && match_plane(first, db[first].owner))
        notify_relay(first, message);
      else
        notify(first, "%s", message);
    }
}

void do_cnotify(dbref player, char *message, int loc)
{
  DESC *d;
  char attr[8192], *s;

  for(d=descriptor_list;d;d=d->next) {
    if(!(d->flags & C_CONNECTED) || d->player == player ||
       (loc && db[player].location == db[d->player].location &&
        match_plane(player, d->player)) ||
       (!could_doit(d->player, player, A_HLOCK) &&
        !controls(d->player, player, POW_WHO)))
      continue;
    strcpy(attr, atr_get(d->player, A_NOTIFY));
    if(!*attr)
      continue;

    if(!strcasecmp(attr, "all") || match_word(attr, db[player].name) ||
       (*(s=atr_get(player, A_ALIAS)) && match_word(attr, s)))
      notify(d->player, "%s", message);
  }
}

void display_cnotify(dbref player)
{
  DESC *d;
  char buf[9000], attr[8192], *s, *t=buf;

  strcpy(attr, atr_get(player, A_NOTIFY));
  if(!*attr)
    return;
  *buf='\0';

  for(d=descriptor_list;d;d=d->next) {
    if(!(d->flags & C_CONNECTED) || player == d->player ||
       (!could_doit(player, d->player, A_HLOCK) &&
       !controls(player, d->player, POW_WHO)))
      continue;

    if(match_word(attr, db[d->player].name) ||
       (*(s=atr_get(d->player, A_ALIAS)) && match_word(attr, s)))
      t+=sprintf(t, " %s", db[d->player].name);
    if(t-buf > 7950)
      break;
  }

  if(t-buf)
    notify(player, "\e[1mConnected:\e[m%s", buf);
}

/* Direct public speech to player in room */
void do_to(player, arg1, arg2, argv, pure)
dbref player;
char *arg1, *arg2, **argv, *pure;
{
  dbref thing;
  char *s;

  if(!pass_slock(player, db[player].location, 1))
    return;
  if(!(s=strchr(pure, ' '))) {
    notify(player, "No message.");
    return;
  }
  *s++='\0';

  if(!*pure) {
    notify(player, "No player mentioned.");
    return;
  } 
  if((thing=lookup_player(pure)) == AMBIGUOUS)
    thing=player;

  pass_vlock=1;

  notify_except(player, NOTHING,
       "%s [to %s]%s%s%s %s", db[player].name, thing==NOTHING ? pure:
       db[thing].name, (*s == ';')?": ":"", (*s == ';')?db[player].name:"",
       (*s == ':')?"":((*s == ';')?possname(db[player].name):":"),
       (*s == ':' || *s == ';') ? s+1 : s);

  pass_vlock=0;
}

void do_talk(dbref player, char *arg1)
{
  dbref thing;
  char buf[528];

  if((thing=match_thing(player, arg1, MATCH_CON)) == NOTHING)
    return;
  if(Typeof(thing) != TYPE_THING) {
    notify(player, "You can only use the 'talk' command on non-player "
           "characters.");
    return;
  }
  
  if(!could_doit(player, thing, A_LTALK) || (!*atr_get(thing, A_TALK) &&
     !*atr_get(thing, A_ATALK)))
    did_it(player, thing, A_FTALK, "You can't talk to that.", A_OFTALK, NULL,
           A_AFTALK);
  else {
    sprintf(buf, "talks to %s.", db[thing].name);
    did_it(player, thing, A_TALK, NULL, A_OTALK, buf, A_ATALK);
  }
}
