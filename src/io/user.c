/* io/user.c */
/* Commands dealing with user lists and connections */

#include "externs.h"

/* whoflag format identifiers */

#define WF_RJUST  0x1	// Right-justify column data
#define WF_WIZARD 0x2	// POW_WHO required to view contents
#define WF_EXTEND 0x4	// Column data can extend width by 2 characters
#define WF_COMBAT 0x8	// Column available only if combat system is on

void do_who(dbref player, char *flags, char *names)
{
  static struct {
    char *name;
    char len, shortlen;
    char format;
  } who_flags[]={
    {"Name",       16, 9,  0},
    {"Alias",      10, 5,  0},
    {"Flags",      4,  4,  0},
    {"Online",     9,  9,  WF_RJUST},
    {"Idle",       4,  4,  WF_RJUST},
    {"Concid",     6,  4,  WF_WIZARD|WF_EXTEND},
    {"Port",       4,  4,  WF_WIZARD|WF_EXTEND},
    {"Desc",       4,  4,  WF_WIZARD|WF_EXTEND},
    {"Cmds",       5,  5,  WF_WIZARD|WF_EXTEND},
    {"Input",      6,  6,  WF_WIZARD|WF_EXTEND},
    {"Output",     7,  7,  WF_WIZARD|WF_EXTEND},
    {"Level",      10, 5,  WF_COMBAT},
    {"Experience", 18, 10, WF_COMBAT},
    {"Class",      10, 6,  0},
    {"Gender",     6,  1,  0},
    {"Chapter",    8,  2,  WF_COMBAT},
    {"Race",       11, 6,  WF_COMBAT},
    {"Guild",      11, 5,  WF_COMBAT},
    {"Doing",      36, 22, 0},
    {"Where",      36, 22, 0},
    {"Address",    15, 15, WF_WIZARD},
    {"Hostname",   29, 20, WF_WIZARD},
    {"Poll",       0,  0,  0},
    {"Single",     0,  0,  0},
  };

  /* Default @whoflag lists. Priority is given to the first line to match
     the columns, #users online, and wizard mode. */

  static struct {
    int cols;		// User must have at least this many columns on screen.
    int users;		// Number of players online.
    int wizmode;	// 1=Matches only if player has POW_WHO.
    int combat_long, combat_short;	// Bits shown if combat is turned on.
    int normal_long, normal_short;	// If combat is turned off.
  } def_bitmask[]={
    {200,   0, 1,  0x010800, 0x64471d,  0x242021, 0x4847de},
    {192,   0, 1,  0x010800, 0x64471d,  0x242001, 0x4847de},
    {180,   0, 1,  0x010800, 0x64471d,  0x242001, 0x48471e},
    {160,   0, 1,  0x010800, 0x64471d,  0x242001, 0x40471e},
//  {132,   0, 1,  0x010800, 0x64471d,  0x040001, 0x60071e},   // Untested ^
    {128,   0, 1,  0x030801, 0x64401c,  0x040001, 0x40001a},
    {100,   0, 1,  0x030801, 0x444018,  0x040001, 0x40001a},
    {  0,   0, 1,  0x010800, 0x444019,  0x040001, 0x40001a},   // Wiz Default
    {  0, 175, 0,  0x000000, 0x400801,  0x000001, 0x400000},
    {132,   0, 0,  0x030801, 0x4c401a,  0x040001, 0x40001a},
    {100,   0, 0,  0x030801, 0x444018,  0x040001, 0x40001a},
    {  0,   0, 0,  0x010800, 0x444019,  0x040001, 0x40001a}};  // Default

  DESC *d;
  int count=0, header=1, cols=get_cols(NULL, player);
  int line=0, total=0, hidden=0, admin=0;
  int a, b, pow, small, len=0, groups, lower=0, upper=0;
  char *r, *s, buf[1024], temp[32], format[32];
  dbref obj, *namelist=NULL;

  /* request for flag list */
  if(*flags == '?') {
    *buf=0;
    for(a=0;a < NELEM(who_flags);a++) {
      if(who_flags[a].format & WF_COMBAT)
        continue;
      if(*buf)
        strcat(buf, ", ");
      strcat(buf, who_flags[a].name);
    }
    notify(player, "List of possible flags: %s", buf);
    return;
  }

  /* determine layout flags */
  strmaxcpy(buf, *flags ? flags:atr_get(player, A_WHOFLAGS), 1024);
  if(!*buf)
    strmaxcpy(buf, atr_get(0, A_WHOFLAGS), 1024);
  s=buf;
  while((r=parse_up(&s, ' '))) {
    small=0;
    for(a=0;a < NELEM(who_flags);a++)
      if(!(who_flags[a].format & WF_COMBAT) &&
         string_prefix(who_flags[a].name, r)) {
        if(*r >= 'a' && *r <= 'z')
          lower|=1<<a;
        else
          upper|=1<<a;
        small=1;
      }
    if(!small) {
      notify(player, "Unknown who flag '%s'.", r);
      notify(player, "Type 'who ?' for a list of available flags.");
      return;
    }
  }

  /* if no flags specified, use default bitmask */
  if(!upper && !lower) {
    /* quick count of users online */
    for(a=0,d=descriptor_list;d;d=d->next)
      if(d->flags & C_CONNECTED)
        a++;

    /* scan for default bitmask */
    pow=power(player, POW_WHO);
    for(b=0;def_bitmask[b].cols;b++)
      if(cols >= def_bitmask[b].cols && a >= def_bitmask[b].users && 
         (!def_bitmask[b].wizmode || pow))
        break;

    /* set defaults for long & short forms */
    upper=def_bitmask[b].normal_long;
    lower=def_bitmask[b].normal_short;
  }

  /* determine # groups in output */
  if(cols < 4)
    cols=4;
  for(a=0;a < NELEM(who_flags);a++) {
    if(!who_flags[a].len)
      continue;
    if(upper & (1<<a))
      len+=who_flags[a].len+2;
    else if(lower & (1<<a))
      len+=who_flags[a].shortlen+2;
  }
  len+=2;
  groups=(cols+2)/len;
  if(groups < 1 || ((upper|lower) & 0x800000))
    groups=1;
  upper &= ~0x800000;
  lower &= ~0x800000;

  /* process specified names */
  if(*names) {
    s=names;
    while((r=parse_up(&s, ' '))) {
      if((obj=lookup_player(r)) <= NOTHING)
        notify(player, "No such player '%s'.", r);
      else if(!inlist(namelist, obj))
        PUSH_L(namelist, obj);
    }
    if(!namelist)
      return;
  }

  /* display the poll */
  default_color=9;
  if(((upper|lower) & 0x400000) && *(s=parse_function(player, player,
     atr_get(0, A_DOING))))
    notify(player, "\e[1;31mQuote of the Day: %s\e[m", s);
  default_color=7;
  if((upper|lower) == 0x400000) {
    if(!*s)
      notify(player, "There has been no Quote of the Day set.");
    CLEAR(namelist);
    return;
  }

  *buf=0;
  d=descriptor_list;

  /* main loop */
  while(d) {
    /* Make sure this player has connected to the game. */
    if(!(d->flags & C_CONNECTED)) {
      d=d->next;
      continue;
    }
    
    /* check namelist for player */
    if(namelist && !inlist(namelist, d->player)) {
      d=d->next;
      continue;
    }

    if(!header) {
      total++;
      if(!could_doit(player, d->player, A_HLOCK)) {
        hidden++;
        if(!controls(player, d->player, POW_WHO)) {
          d=d->next;
          continue;
        }
      }
      if(Immortal(d->player))
        admin++;
    }

    for(a=0;a < NELEM(who_flags);a++) {
      /* determine column format */
      if(!who_flags[a].len)
        continue;
      if(!(small=(lower & (1<<a))) && !(upper & (1<<a)))
        continue;
      b=small?who_flags[a].shortlen:who_flags[a].len;
      if(!header && (who_flags[a].format & WF_EXTEND))
        sprintf(format, "%%-%d.%ds", b+2, b+2);
      else
        sprintf(format, (who_flags[a].format & WF_RJUST)?"%%%d.%ds  ":
                "%%-%d.%ds  ", b, b);

      if(header) {
        strcat(buf, tprintf(format, who_flags[a].name));
        continue;
      } if((who_flags[a].format & WF_WIZARD) && !controls(player,
           d->player, POW_WHO)) {
        strcat(buf, tprintf(format, "?"));
        continue;
      }

      switch(a) {
      case 0: s=db[d->player].name; break;
      case 1: s=atr_get(d->player, A_ALIAS); break;
      case 2: 
        temp[0]=!could_doit(player, d->player, A_HLOCK)?'H':' ';
        temp[1]=(now-d->last_time >= 300 && *(atr_get(d->player, A_IDLE)))?
                'I':' ';
        temp[2]=!could_doit(player, d->player, A_PLOCK)?'P':' ';
        temp[3]=Builder(d->player)?'B':' ';
        temp[4]='\0';
        s=temp;
        break;
      case 3: s=time_format(TML, now-d->login_time); break;
      case 4: s=time_format(TMS, now-d->last_time); break;
      case 5: s=tprintf("%d", d->concid); break;
      case 6: s=tprintf("%d", d->port); break;
      case 7: s=tprintf("%d", d->fd); break;
      case 8: s=tprintf("%d", d->cmds); break;
      case 9: s=tprintf("%d", d->input); break;
      case 10: s=tprintf("%d", d->output); break;

      case 13:
        if(!HIDDEN_ADMIN || Immortal(player))
          s=classnames[class(d->player)];
        else
          s="?";
        break;

      case 14: s=db[d->player].data->gender?"Female":"Male"; break;

      case 18:
        s=strip_ansi(atr_parse(d->player, player, A_DOING));
        for(r=s;*r;r++)  /* remove unprintable characters */
          if(*r < 32)
            *r=' ';
        break;

      case 19: s=(!controls(player, d->player, POW_EXAMINE) &&
                 (db[d->player].flags & DARK))?"You can't tell.":
                 db[db[d->player].location].name; break;
      case 20: s=ipaddr(d->ipaddr); break;
      case 21: s=d->addr; break;
      default: s="";
      }

      sprintf(buf+strlen(buf), format, s);
    }

    /* space between groups */
    if(++count < groups) {
      strcat(buf, "  ");
      if(!header)
        d=d->next;
      continue;
    }

    /* trim excess spaces */
    r=strnul(buf);
    while(*--r == ' ')
      *r='\0';

    /* print output line */
    if(header) {
      notify(player, "\e[1;4;35m%s\e[m", buf);
      header=0;
    } else {
      notify(player, "\e[%s36m%s\e[m", (line^=1)?"1;":"", buf);
      d=d->next;
    }

    count=0;
    *buf=0;
  }

  CLEAR(namelist);

  /* print last line (if incomplete) */
  if(*buf) {
    r=strnul(buf);
    while(*--r == ' ')
      *r='\0';
    notify(player, "\e[%s36m%s\e[m", (line^=1)?"1;":"", buf);
  }

  notify(player, "\e[1m\304\304\304\e[m");

  if(!power(player, POW_WHO) && RESTRICT_HIDDEN)
    total-=hidden;
  sprintf(buf, "Total Players: %d", total);
  if(admin && (!HIDDEN_ADMIN || Immortal(player)))
    strcat(buf, tprintf("   Administrators: %d", admin));
  if(hidden && (!RESTRICT_HIDDEN || power(player, POW_WHO)))
    strcat(buf, tprintf("   Hidden: %d", hidden));
  notify(player, "\e[1;32m%s.  \e[33m%s\e[m", buf, grab_time(player));
}


void announce_disconnect(dbref player)
{
  speaker=player;

  delete_message(player, -1);
  if(db[player].data)
    db[player].data->lastoff=now;
  db[player].flags&=~PLAYER_CONNECT;

  do_cnotify(player, tprintf("\e[1mDisconnect:\e[m %s", db[player].name), 1);
  if(!Invalid(db[player].location)) {
    char *s=atr_parse(player, player, A_ODISC);
    notify_except(player, player, "%s %s", db[player].name,
         *s?s:"has disconnected.");
  }

  global_trigger(player, A_ADISC, TRIG_ALL);

  if(Guest(player))
    destroy_player(player);

}

// Trigger an attribute on several objects surrounding <who>.
// <trig> determines which objects get executed, based on TRIG_ defines.
//
void global_trigger(dbref who, unsigned char acmd, int trig)
{
  dbref thing, *ptr;
  char *s;
  int a;

  /* Players being nuked get this */
  if(Typeof(who) == TYPE_GARBAGE)
    return;

  /* Universal zone commands */
  if(trig & TRIG_UZO)
    for(ptr=uzo;ptr && *ptr != NOTHING;ptr++)
      if(*(s=atr_get(*ptr, acmd)))
        parse_que(*ptr, s, who);

  /* Room zone commands */
  if(trig & TRIG_ZONES)
    for(ptr=Zone(who);ptr && *ptr != NOTHING;ptr++)
      if(!(db[*ptr].flags & ZONE_UNIVERSAL) && *(s=atr_get(*ptr, acmd)))
        parse_que(*ptr, s, who);

  /* Player himself */
  if((trig & TRIG_PLAYER) && *(s=atr_get(who, acmd)))
    parse_que(who, s, who);

  /* Room itself */
  if((trig & TRIG_ROOM) && *(s=atr_get(db[who].location, acmd)))
    parse_que(db[who].location, s, who);

  /* Room contents/exits */
  if(trig & TRIG_CONTENTS)
    for(a=0;a<2;a++)
      DOLIST(thing, a?db[db[who].location].exits:db[db[who].location].contents)
        if(Typeof(thing) != TYPE_PLAYER && match_plane(who, thing) &&
           *(s=atr_get(thing, acmd)))
          parse_que(thing, s, who);

  /* Player inventory */
  if(trig & TRIG_INVENTORY)
    DOLIST(thing, db[who].contents)
      if(Typeof(thing) != TYPE_PLAYER && *(s=atr_get(thing, acmd)))
        parse_que(thing, s, who);
}

// Triggers attribute <acmd> on all objects and exits in <who>'s room, except
// for <who> itself. Only objects that can see <who> are triggered.
//
void room_trigger(dbref who, unsigned char acmd)
{
  dbref thing;
  char *s;
  int a;

  /* Loop to scan both room contents and exits */
  for(a=0;a<2;a++)
    DOLIST(thing, a?db[db[who].location].exits:db[db[who].location].contents) {
      if(match_plane(thing, who) && can_see(thing, who) &&
         thing != who && *(s=atr_get(thing, acmd)))
        parse_que(thing, s, who);
    }
}

void broadcast(char *msg)
{
  DESC *d;

  for(d=descriptor_list;d;d=d->next)
    if(d->flags & C_CONNECTED)
      output(d, msg);
}

void check_timeout(DESC *d)
{
  /* immortals are unsusceptable to timeout activity */
  if(!Invalid(d->player) && Immortal(d->player))
    return;

  /* first check limits on services */
  switch(d->state) {
  case 37:  // Web Server Port
    if((now - d->last_time) > 300)
      d->flags |= C_LOGOFF;
    return;
  }

  /* then limits on valid player sessions */
  if(d->state > 2) {
    if(TIMEOUT_SESSION > 0 && (now - d->last_time) > TIMEOUT_SESSION) {
      output2(d, "*** Session Inactivity Timeout ***\n");
      log_io("Session Inactivity %d", d->concid);
      d->flags |= C_LOGOFF;
    } else if(LIMIT_SESSION > 0 && (now - d->connected_at) > LIMIT_SESSION) {
      output2(d, "*** Time Limit Exceeded ***\n");
      log_io("Time Limit Exceeded %d", d->concid);
      d->flags |= C_LOGOFF;
    }
    return;
  }
  
  /* now check limits at Login: prompt */

  if(TIMEOUT_LOGIN > 0 && (now - d->last_time) > TIMEOUT_LOGIN) {
    output2(d, tprintf("\nLogin expired after %d seconds.\n",
            TIMEOUT_LOGIN));
    d->flags |= C_LOGOFF;
  } else if(LIMIT_LOGIN > 0 && (now - d->connected_at) > LIMIT_LOGIN) {
    output2(d, tprintf("\nLogin time limit exceeded after %d seconds.\n",
            LIMIT_LOGIN));
    d->flags |= C_LOGOFF;
  }
}

int isbusy(dbref player)
{
  DESC *d;

  return (Typeof(player) == TYPE_PLAYER && (d=Desc(player)) &&
          (d->flags & C_OUTPUTOFF));
}
