/* comm/com.c */
/* Comsystem - world-wide player communications channels */

#include "externs.h"

struct comlock *comlock_list;


// Returns the byte offset + 1 from the start of the @channel attribute if the
// player is currently listening to <channel>, or 0 otherwise.
//
int is_on_channel(dbref player, char *channel)
{
  char *r, *s, *t;

  /* Search channel list for wordmatch */
  s=t=atr_get(player, A_CHANNEL);
  while((r=strspc(&s)))
    if(!strcmp(r, channel))
      return r-t+1;

  return 0;
}

// Internal function to send <message> to all players online currently
// listening to channel <channel>. The global variable `default_color' should
// be set to the channel highlight color prior to calling.
//
void com_send(char *channel, char *message)
{
  DESC *d;
  char buf[8192];

  /* Build message text */
  sprintf(buf, "%s[%s] %s%s", textcolor(7, default_color), channel, message,
          textcolor(default_color, 7));

  /* Send message to all on channel */
  for(d=descriptor_list;d;d=d->next)
    if((d->flags & C_CONNECTED) && !IS(db[d->player].location, TYPE_ROOM,
       ROOM_XZONE) && is_on_channel(d->player, channel))
      notify(d->player, "%s", buf);
}

// Internal command to list who's currently on a particular channel.
//
static void com_who(dbref player, char *channel)
{
  DESC *d;
  int count=0, hcount=0;

  /* Display non-hidden players on the channel */
  for(d=descriptor_list;d;d=d->next)
    if((d->flags & C_CONNECTED) && is_on_channel(d->player, channel)) {
      count++;
      if(controls(player, d->player, POW_WHO) ||
         could_doit(player, d->player, A_HLOCK))
        notify(player, "%s is on channel %s, idle %s.",
               unparse_object(player, d->player), channel,
               time_format(TMA, getidletime(d->player)));
      else
        hcount++;
    }

  /* Display counts */
  if(!hcount)
    notify(player, "There %s %d player%s on channel '%s'.",
           count==1?"is":"are", count, count==1?"":"s", channel);
  else
    notify(player, "There %s %d player%s (%d hidden) on channel '%s'.",
           count==1?"is":"are", count, count==1?"":"s", hcount, channel);
}

// +com: Sends a message to all other players on a specific channel.
//
void do_com(dbref player, char *arg1, char *arg2)
{
  char chan[16], message[16384], *r, *s, *t=NULL;
  dbref thing;

  /* Check if player can use +com */
  if(Typeof(player) != TYPE_PLAYER) {
    notify(player, "Non-players can not send +com messsages.");
    return;
  } if(IS(db[player].location, TYPE_ROOM, ROOM_XZONE)) {
    notify(player, "Your communicator isn't picking up a frequency.");
    return;
  }

  /* Use default channel if none was specified */
  if(!*arg1) {
    s=atr_get(player, A_CHANNEL);
    if((r=strchr(s, ' ')))
      *r='\0';
    strmaxcpy(chan, s, 16);
    arg1=chan;
  }

  /* Check if player is on this channel */
  if(!*arg1) {
    notify(player, "You aren't on any channels!");
    notify(player, "To join a general discussion channel, type: +ch +public");
    return;
  } if(!is_on_channel(player, arg1)) {
    notify(player, "You aren't on channel '%s'.", arg1);
    return;
  } if(!strcasecmp(arg2, "who")) {
    com_who(player, arg1);
    return;
  }

  /* Check if player's voice was removed from channel */
  s=atr_get(player, A_COMLIMIT);
  while((r=strspc(&s)))
    if(!strcmp(arg1, r)) {
      notify(player, "You have been silenced on channel '%s'.", arg1);
      return;
    }
  
  /* Check if this message is a spoof */
  if(!can_emit_msg(player, NOTHING, NOTHING, arg2, 1)) {
    notify(player, "Sorry, your message contains a spoof.");
    return;
  }

  /* Charge +com cost */
  if(!charge_money(player, COM_COST, 1))
    return;

  /* Compose message using comtitle or to-recipient method */
  default_color=unparse_color(player, player, 14);
  s=message+sprintf(message, "%s", colorname(player));

  /* Check for a to-recipient */
  if(*arg2 == '\'' && (t=strchr(arg2, ' '))) {
    *t='\0';
    if(*(arg2+1) && (thing=lookup_player(arg2+1)) > NOTHING &&
       (db[thing].flags & PLAYER_CONNECT) && (controls(player, thing, POW_WHO)
       || could_doit(player, thing, A_HLOCK)) && is_on_channel(thing, arg1)) {
      s+=sprintf(s, " [to %s]", colorname(thing));
      arg2=t?t+1:"";
      if(*arg2 == ';')
        s+=sprintf(s, ": %s", colorname(player));
    } else {
      *t=' ';
      t=NULL;
    }
  }

  /* Append player's comtitle */
  if(ALLOW_COMTITLES && !t && *arg2 != ':' && *arg2 != ';') {
    strcpy(env[0], arg1);
    if(*(t=atr_parse(player, player, A_COMTITLE)))
      s+=sprintf(s, " <%s>", t);
    *env[0]='\0';
  }

  /* Append text to message */
  sprintf(s, "%s %s", (*arg2 == ':')?"":((*arg2 == ';')?
          possname(db[player].name):":"), (*arg2 == ':' || *arg2 == ';')?
          arg2+1:arg2);
  message[8000]='\0';

  /* Send it */
  com_send(arg1, message);

  /* Restore defualt color */
  default_color=7;
}

// Wizard command to emit a message on a specific channel.
//
void do_cemit(dbref player, char *channel, char *msg)
{
  if(!*msg) {
    notify(player, "No message.");
    return;
  }

  default_color=unparse_color(player, player, 14);
  com_send(channel, msg);
  default_color=7;

  if(!Quiet(player) && !is_on_channel(player, channel))
    notify(player, "Channel '%s' just saw: '%s'.", channel, msg);
}

// Returns 1 if <player> is allowed to join channel <channel>.
//
static int ok_channel_change(dbref player, char *channel)
{
  struct comlock *ptr;
  char *r, *s;

  /* Check permission to join admin-restricted channels */
  if(power(player, POW_SECURITY))
    return 1;
  if(*channel == '*' || (*channel == '.' && !power(player, POW_WHO)) ||
     (*channel == '_' && !Immortal(player)))
    return 0;

  if(!power(player, POW_WHO) && (*channel == '%' || *channel == '$'))
    return 0;
  /* Check channel lock list */
  for(ptr=comlock_list;ptr;ptr=ptr->next)
    if(!strcmp(ptr->channel, channel)) {
      if(!inlist(ptr->list, player))
        return 0;
      break;
    }

  /* Deny from blocked channels */
  s=atr_get(player, A_COMLIMIT);
  while((r=strspc(&s)))
    if(*r == '@' && !strcmp(channel, r+1))
      return 0;

  return 1;
}

// Removes all channels from <player> beginning with the symbol <type>. This is
// used when declassing a player from a higher administration position.
//
void remove_channel_type(dbref player, char type)
{
  char buf[8192], *r, *s=atr_get(player, A_CHANNEL), *t=buf;

  *buf='\0';
  while((r=strspc(&s)))
    if(*r != type) {
      if(*buf)
        *t++=' ';
      t+=sprintf(t, "%s", r);
    }

  atr_add(player, A_CHANNEL, buf);
}

// +channel: Changes the default, or joins/leaves a communications channel.
//
void do_channel(dbref player, char *arg1)
{
  char buf[8192], *r, *s, *t;
  int i;
  
  if(Typeof(player) != TYPE_PLAYER) {
    notify(player, "Objects can not use channels.");
    return;
  } if(strchr(arg1, ' ')) {
    notify(player, "Sorry, channel names can not have spaces in them.");
    return;
  }

  /* List channels? */
  if(!*arg1) {
    /* Build a list of blocked channels */
    s=atr_get(player, A_COMLIMIT);
    t=buf;
    while((r=strspc(&s)))
      if(*r == '@')
        t+=sprintf(t, "%s%s", t==buf?"":" ", r+1);
    *t='\0';

    if(*(s=atr_get(player, A_CHANNEL)))
      notify(player, "You are currently on the following channels: %s", s);
    else {
      notify(player, "You are currently not on any channels.");
      if(!*buf)
        notify(player, "For a general chatting session, try: +ch +public");
    }
    if(*buf)
      notify(player, "You are blocked from the following channels: %s", buf);
    return;
  }
  
  /* Delete channel */
  if(*arg1 == '-') {
    if(!*++arg1) {
      notify(player, "Usage: +ch -<channel>");
      return;
    } if(!(i=is_on_channel(player, arg1))) {
      notify(player, "You are not on that channel.");
      return;
    }

    default_color=unparse_color(player, player, 14);
    com_send(arg1, tprintf("* %s has left this channel.", colorname(player)));
    default_color=7;

    /* Remove channel from list */
    s=atr_get(player, A_CHANNEL);
    strmaxcpy(buf, s, i);
    if((s=strchr(s+i, ' ')))
      strcat(buf, s+1);
    else if(i > 1)
      buf[i-2]='\0';
    atr_add(player, A_CHANNEL, buf);
    return;
  }

  if(*arg1 == '+') {  /* Join channel */
    if(!*++arg1) {
      notify(player, "Usage: +ch +<channel>");
      return;
    } if(is_on_channel(player, arg1)) {
      notify(player, "You are already on that channel.");
      return;
    }
  }

  /* Validate channel name */
  if(*arg1 == '+' || *arg1 == '-' || *arg1 == '@') {
    notify(player, "That is an invalid channel name.");
    return;
  } if(strlen(arg1) > 15) {
    notify(player, "Channel names can only be 15 characters long.");
    return;
  }

  /* Make sure the channel name is ASCII */
  for(s=arg1;*s;s++)
    if(*s < 32 || *s >= 127) {
      notify(player, "That is an invalid channel name.");
      return;
    }

  /* Set default channel */
  if((i=is_on_channel(player, arg1))) {
    s=atr_get(player, A_CHANNEL);
    strmaxcpy(buf, s, i);
    if((s=strchr(s+i, ' ')))
      strcat(buf, s+1);
    else if(i > 1)
      buf[i-2]='\0';
    atr_add(player, A_CHANNEL, tprintf("%s%s%s", arg1, *buf?" ":"", buf));
  } else {
    if(IS(db[player].location, TYPE_ROOM, ROOM_XZONE)) {
      notify(player, "Can't join channels from here.");
      return;
    } if(!ok_channel_change(player, arg1)) {
      notify(player, "You have no access to that channel.");
      return;
    }

    s=atr_get(player, A_CHANNEL);
    if(strlen(s)+strlen(arg1) > 7999) {
      notify(player, "Error: Too many channels joined.");
      return;
    }

    atr_add(player, A_CHANNEL, tprintf("%s%s%s", arg1, *s?" ":"", s));
    default_color=unparse_color(player, player, 14);
    com_send(arg1, tprintf("* %s has joined this channel.",colorname(player)));
    default_color=7;
  }

  notify(player, "Channel %s is now default.", arg1);
}

// Wizard command to block or unblock a specific channel from a player's
// accessible channel list. Blocking a channel that a player is currently
// listening to will knock him off.
//
void do_comblock(dbref player, char *arg1, char *arg2)
{
  char buf[8192], *r, *s, *t;
  dbref thing;
  int i;

  if(!*arg1 || !*arg2) {
    notify(player, "Usage: @comblock <player>=[!]<channel>");
    return;
  } if(strchr(arg2, ' ')) {
    notify(player, "Channel names can not have spaces in them.");
    return;
  }

  /* Determine target player */
  if((thing=lookup_player(arg1)) == AMBIGUOUS)
    thing=db[player].owner;
  else if(thing == NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  }

  if(*arg2 != '!') {
    if(!controls(player, thing, POW_COM)) {
      notify(player, "Permission denied.");
      return;
    } if(power(thing, POW_COM)) {
      notify(player, "Administrators can't be blocked from channels.");
      return;
    }
  }

  /* Unblock a channel */
  if(*arg2 == '!') {
    *arg2='@';

    s=t=atr_get(thing, A_COMLIMIT);
    while((r=strspc(&s)))
      if(!strcmp(r, arg2))
        break;
    if(!r) {
      notify(player, "%s has not been blocked from that channel.",
             db[thing].name);
      return;
    }

    /* Remove channel block */
    s=atr_get(thing, A_COMLIMIT);
    strmaxcpy(buf, s, r-t+1);
    if((s=strchr(s+(r-t)+1, ' ')))
      strcat(buf, s+1);
    else if(r-t)
      buf[r-t-1]='\0';
    atr_add(thing, A_COMLIMIT, buf);

    notify(thing, "Your access to channel '%s' has been restored by %s.",
           arg2+1, db[player].name);
    log_main("COM: %s has restored %s%s access to channel '%s'.",
             db[player].name, db[thing].name, possname(db[thing].name),
             arg2+1);
    notify(player, "Restored %s%s access to channel '%s'.", db[thing].name,
           possname(db[thing].name), arg2+1);
    return;
  }

  /* Block a channel */

  /* Validate channel name */
  if(*arg2 == '+' || *arg2 == '-' || *arg2 == '@') {
    notify(player, "That is an invalid channel name.");
    return;
  } if(strlen(arg2) > 15) {
    notify(player, "Channel names can only be 15 characters long.");
    return;
  }

  /* Make sure the channel name is ASCII */
  for(s=arg2;*s;s++)
    if(*s < 32 || *s >= 127) {
      notify(player, "That is an invalid channel name.");
      return;
    }

  s=atr_get(thing, A_COMLIMIT);
  if(strlen(s)+strlen(arg2) > 7998) {
    notify(player, "%s has too many channels blocked.", db[thing].name);
    return;
  }

  /* Check if channel is already blocked */
  sprintf(buf, "@%s", arg2);
  while((r=strspc(&s)))
    if(!strcmp(r, buf))
      break;
  if(r) {
    notify(player, "%s is already blocked from that channel.", db[thing].name);
    return;
  }

  /* Restrict channel */
  s=atr_get(thing, A_COMLIMIT);
  atr_add(thing, A_COMLIMIT, tprintf("%s%s%s", s, *s?" ":"", buf));

  /* Kick off player */
  if((i=is_on_channel(thing, arg2))) {
    default_color=unparse_color(thing, thing, 14);
    com_send(arg2, tprintf("* %s has been banned from this channel.",
             colorname(thing)));
    default_color=7;

    /* Remove channel from list */
    s=atr_get(thing, A_CHANNEL);
    strmaxcpy(buf, s, i);
    if((s=strchr(s+i, ' ')))
      strcat(buf, s+1);
    else if(i > 1)
      buf[i-2]='\0';
    atr_add(thing, A_CHANNEL, buf);
  }

  notify(thing, "You have been banned from channel '%s' by %s.", arg2,
         db[player].name);
  log_main("COM: %s has banned %s from channel '%s'.", db[player].name,
           db[thing].name, arg2);
  notify(player, "%s has been banned from channel '%s'.", db[thing].name,
         arg2);
}

// Wizard command to restrict a player's voice from a specific +com channel. 
//
void do_comvoice(dbref player, char *arg1, char *arg2)
{
  char buf[8192], *r, *s, *t;
  dbref thing;

  if(!*arg1 || !*arg2) {
    notify(player, "Usage: @comvoice <player>=[!]<channel>");
    return;
  } if(strchr(arg2, ' ')) {
    notify(player, "Channel names can not have spaces in them.");
    return;
  }

  /* Determine target player */
  if((thing=lookup_player(arg1)) <= NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  } if(*arg2 != '!' && !controls(player, thing, POW_COM)) {
    notify(player, "Permission denied.");
    return;
  }

  if(*arg2 != '!') {
    if(!controls(player, thing, POW_COM)) {
      notify(player, "Permission denied.");
      return;
    } if(power(thing, POW_COM)) {
      notify(player, "You can't use that on administrators.");
      return;
    }
  }

  /* Lift voice restriction */
  if(*arg2 == '!') {
    s=t=atr_get(thing, A_COMLIMIT);
    while((r=strspc(&s)))
      if(!strcmp(r, arg2+1))
        break;
    if(!r) {
      notify(player, "%s has not been silenced from that channel.",
             db[thing].name);
      return;
    }

    /* Set new @comlimit */
    s=atr_get(thing, A_COMLIMIT);
    strmaxcpy(buf, s, r-t+1);
    if((s=strchr(s+(r-t)+1, ' ')))
      strcat(buf, s+1);
    else if(r-t)
      buf[r-t-1]='\0';
    atr_add(thing, A_COMLIMIT, buf);

    notify(thing, "Your voice has been restored to channel '%s' by %s!",
           arg2+1, db[player].name);
    log_main("COM: %s has restored %s%s voice to channel '%s'.", db[player].name,
             db[thing].name, possname(db[thing].name), arg2+1);
    notify(player, "%s%s voice has been restored to channel '%s'.",
           db[thing].name, possname(db[thing].name), arg2+1);
    return;
  }

  /* Silence a player */

  /* Validate channel name */
  if(*arg2 == '+' || *arg2 == '-' || *arg2 == '@') {
    notify(player, "That is an invalid channel name.");
    return;
  } if(strlen(arg2) > 15) {
    notify(player, "Channel names can only be 15 characters long.");
    return;
  }

  /* Make sure the channel name is ASCII */
  for(s=arg2;*s;s++)
    if(*s < 32 || *s >= 127) {
      notify(player, "That is an invalid channel name.");
      return;
    }

  s=atr_get(thing, A_COMLIMIT);
  if(strlen(s)+strlen(arg2) > 7999) {
    notify(player, "%s has too many channels silenced.", db[thing].name);
    return;
  }

  /* Check if channel is already blocked */
  while((r=strspc(&s)))
    if(!strcmp(r, arg2))
      break;
  if(r) {
    notify(player, "%s is already silenced from that channel.",
           db[thing].name);
    return;
  }

  /* Restrict channel */
  s=atr_get(thing, A_COMLIMIT);
  atr_add(thing, A_COMLIMIT, tprintf("%s%s%s", s, *s?" ":"", arg2));

  notify(thing, "Your voice has been silenced from channel '%s' by %s.", arg2,
         db[player].name);
  log_main("COM: %s has silenced %s from channel '%s'.", db[player].name,
           db[thing].name, arg2);
  notify(player, "%s has been silenced from channel '%s'.", db[thing].name,
         arg2);
}

// Wizard command to modify channel-specific locks, determining who can join or
// leave the channel at will.
//
void do_comlock(dbref player, char *arg1, char *arg2)
{
  struct comlock *ptr, *last;
  dbref thing=NOTHING;
  int lift=0;
  char *s;

  if(!*arg1 || (*arg1 == '!' && *arg2)) {
    notify(player, "Usage: @comlock <channel>=[!]<player>");
    notify(player, "       @comlock !<channel>");
    return;
  }

  /* Determine if we're lifting a channel lock */
  if(*arg1 == '!') {
    lift=1;
    arg1++;
  }

  /* Validate channel name */
  if(*arg1 == '+' || *arg1 == '-' || *arg1 == '@') {
    notify(player, "That is an invalid channel name.");
    return;
  } if(strchr(arg1, ' ')) {
    notify(player, "Channel names can not have spaces in them.");
    return;
  } if(strlen(arg1) > 15) {
    notify(player, "Channel names can only be 15 characters long.");
    return;
  }

  /* Make sure the channel name is ASCII */
  for(s=arg1;*s;s++)
    if(*s < 32 || *s >= 127) {
      notify(player, "That is an invalid channel name.");
      return;
    }

  if(lift) {
    /* Scan for channel name in list */
    for(last=NULL,ptr=comlock_list;ptr;last=ptr,ptr=ptr->next)
      if(!strcmp(ptr->channel, arg1))
        break;
    if(!ptr) {
      notify(player, "That channel isn't locked.");
      return;
    }

    /* Lift lock */
    if(last)
      last->next=ptr->next;
    else
      comlock_list=ptr->next;
    CLEAR(ptr->list);
    free(ptr);

    if(!Quiet(player))
      notify(player, "Lock for channel '%s' lifted.", arg1);
    return;
  }

  /* Validate player name */
  if(*arg2) {
    if(*arg2 == '!') {
      lift=1;
      arg2++;
    }
    if((thing=lookup_player(arg2)) <= NOTHING) {
      notify(player, "No such player '%s'.", arg2);
      return;
    }
  }

  /* Scan for existing channel lock */
  for(last=NULL,ptr=comlock_list;ptr;last=ptr,ptr=ptr->next)
    if(!strcmp(ptr->channel, arg1))
      break;

  /* Add new channel entry */
  if(!ptr) {
    ptr=malloc(sizeof(struct comlock));
    ptr->next=NULL;
    SPTR(ptr->channel, arg1);
    ptr->list=NULL;

    if(last)
      last->next=ptr;
    else
      comlock_list=ptr;

    if(!*arg2) {
      notify(player, "Added blank lock for channel '%s'.", arg1);
      return;
    }
  }

  /* Make sure we're not duplicating a channel lock */
  if(!*arg2) {
    notify(player, "A lock already exists for channel '%s'.", arg1);
    return;
  }

  /* Remove player from lock list */
  if(lift) {
    if(inlist(ptr->list, thing)) {
      PULL(ptr->list, thing);
      if(!Quiet(player))
        notify(player, "%s removed from lock list for channel '%s'.",
               db[thing].name, arg1);
    } else if(!Quiet(player))
      notify(player, "%s is not in the lock list for channel '%s'.", 
             db[thing].name, arg1);
    return;
  }

  /* Add player to lock list */
  if(!inlist(ptr->list, thing)) {
    PUSH_L(ptr->list, thing);
    if(!Quiet(player))
      notify(player, "%s added to lock list for channel '%s'.", db[thing].name,
             arg1);
  } else if(!Quiet(player))
    notify(player, "%s is already in the lock list for channel '%s'.",
           db[thing].name, arg1);
}

// Lists the current comlocks in effect.
//
void list_comlocks(dbref player)
{
  struct comlock *ptr;
  char buf[512], *s;
  int cols=max(get_cols(NULL, player), 40)-1;
  dbref *i;

  if(!comlock_list) {
    notify(player, "No +com locks are in effect.");
    return;
  }

  notify(player, "Current +com locks:");
  notify(player, "%s", "");

  for(ptr=comlock_list;ptr;ptr=ptr->next) {
    s=buf+sprintf(buf, "%-15.15s -", ptr->channel);
    if(!ptr->list) {
      notify(player, "%s No entries.", buf);
      continue;
    }

    /* List all privileged channel members */
    for(i=ptr->list;*i != NOTHING;i++) {
      if(s-buf+strlen(db[*i].name)+(*(i+1) == NOTHING?1:2) > cols) {
        notify(player, buf);
        s=buf+sprintf(buf, "                 ");
      }
      s+=sprintf(s, " %s%s", db[*i].name, (*(i+1) == NOTHING)?"":",");
    }
    notify(player, buf);
  }
}

// Saves the comlock list to the database.
//
void save_comlocks(FILE *f)
{
  struct comlock *ptr;

  if(!comlock_list)
    return;

  /* Write locks individually */
  for(ptr=comlock_list;ptr;ptr=ptr->next) {
    putstring(f, ptr->channel);
    putlist(f, ptr->list);
  }
  putchr(f, 0);
}

// Loads the comlock list from the database.
//
int load_comlocks(FILE *f)
{
  struct comlock *ptr, *last=NULL;
  int count=0;
  char *s;

  while(*(s=getstring_noalloc(f))) {
    ptr=malloc(sizeof(struct comlock));
    ptr->next=NULL;
    SPTR(ptr->channel, s);
    ptr->list=getlist(f);

    if(last)
      last->next=ptr;
    else
      comlock_list=ptr;
    last=ptr;

    count++;
  }

  return count;
}
