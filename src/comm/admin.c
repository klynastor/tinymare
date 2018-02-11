/* comm/admin.c */
/* Administrative commands */

#include "externs.h"


/* Match a list of object flags for a search criteria */
static int convert_flags(dbref player, char *s, int *mask, int *type)
{
  char last_flag=' ';
  int i;
  
  for(;*s;s++) {
    for(i=0;flaglist[i].flag && flaglist[i].flag != *s;i++);
    if(!flaglist[i].flag) {
      notify(player, "Unknown flag '%c'.", *s);
      return 0;
    }

    /* Check that we're not mixing up object types */
    if(flaglist[i].objtype != NOTYPE) {
      if(*type == NOTYPE) {
        *type=flaglist[i].objtype;
        last_flag=*s;
      } else if(flaglist[i].objtype != *type) {
        notify(player, "Flag '%c' conflicts with '%c'.", last_flag,
               flaglist[i].flag);
        return 0;
      }
    }

    /* check flag searching permissions */
    if((*s == 'c' && !power(player, POW_WHO)) ||
       (*s == 'n' && !power(player, POW_SECURITY))) {
      notify(player, "You can't search for flag '%c'.", *s);
      return 0;
    }

    /* Add flag to bitmask */
    *mask |= flaglist[i].bits;
  }
  
  return 1;
}

/* Finds objects which match specific categories in the database */
void do_search(dbref player, char *arg1, char *arg3)
{
  char buf[4096], *arg2, *string="";
  int owner, category=0, type=NOTYPE, value=0;
  int i, thing, flag=1, count=0;

  /* Available categories */
  enum { SEARCH_ATTR=1, SEARCH_CLASS, SEARCH_FLAGS, SEARCH_INZONE,
         SEARCH_LINKS, SEARCH_POWERS };

  if(!charge_money(player, SEARCH_COST, 1))
    return;
  
  /* Determine arguments */
  if((arg2=strchr(arg1, ' ')))   /* @search <arg1> <arg2>=<arg3> */
    *arg2++='\0';
  else if(!*arg3)                /* @search <arg1> */
    arg2="";
  else {                         /* @search <arg2>=<arg3> */
    arg2=arg1;
    arg1="";
  }
  
  /* List disconnected rooms? */
  if(strlen(arg1) > 3 && string_prefix("disconnected", arg1)) {
    if(!power(player, POW_DB)) {
      notify(player, "Permission denied.");
      return;
    }

    i=report_disconnected_rooms(player);
    if(!i)
      notify(player, "No disconnected rooms found.");
    else
      notify(player, "%d object%s found.", i, i==1?"":"s");
    return;
  }

  /* Search a specific player? */
  if(*arg1) {
    if((owner=lookup_player(arg1)) == AMBIGUOUS)
      owner=db[player].owner;
    else if(owner == NOTHING) {
      notify(player, "No such player '%s'.", arg1);
      return;
    } else if(!controls(player, owner, POW_STATS)) {
      notify(player, "You need a search warrant to do that.");
      return;
    }
  } else  /* Default restriction */
    owner=power(player, POW_STATS)?AMBIGUOUS:db[player].owner;

  /* Check category restrictions */

  if(!*arg2) {
    /* No Class Restrictions */
  } else if(string_prefix("attributes", arg2)) {
    /* Matches <attr>:<value> */

    category=SEARCH_ATTR;
    if((string=strchr(arg3, ':')))
      *string++='\0';
    else
      string="";
  } else if(string_prefix("class", arg2)) {
    /* Matches Class Name */

    category=SEARCH_CLASS;
    type=TYPE_PLAYER;
    for(value=0;value<NUM_CLASSES;value++)
      if(!strcasecmp(classnames[value], arg3))
        break;
    if(value == NUM_CLASSES) {
      notify(player, "No such class name.");
      return;
    }
  } else if(string_prefix("flags", arg2)) {
    /* Matches Flags */

    category=SEARCH_FLAGS;
    if(!convert_flags(player, arg3, &value, &type))
      return;
  } else if(string_prefix("powers", arg2)) {
    /* Matches Powers */

    category=SEARCH_POWERS;
    type=TYPE_PLAYER;
    for(value=0;value<NUM_POWS;value++)
      if(!strcasecmp(powers[value].name, arg3))
        break;
    if(value == NUM_POWS) {
      notify(player, "No such power.");
      return;
    }
  } else if(string_prefix("inzone", arg2)) {
    /* Matches Inzone */

    category=SEARCH_INZONE;
    type=TYPE_ROOM;
    if((value=match_thing(player, arg3, MATCH_EVERYTHING)) == NOTHING)
      return;
  } else if(string_prefix("links", arg2)) {
    /* Matches Links to <object> */

    category=SEARCH_LINKS;
    if((value=match_thing(player, arg3, MATCH_EVERYTHING)) == NOTHING)
      return;
  } else if(string_prefix("name", arg2)) {
    /* Matches Name of any object */
  } else {
    /* Matches Type Category */

    if(string_prefix("type", arg2)) {
      arg2=arg3;
      arg3="";
    }

    for(type=0;type < 8;type++)
      if(!strcasecmp(typenames[type], arg2))
        break;
    if(type == 8) {
      notify(player, "Unknown type restriction.");
      return;
    }
  }

  /* Main search loop */

  /* Check through all valid object types */
  for(i=0;i<5;i++) {
    if(type != NOTYPE && type != i)
      continue;
    if(!flag)
      flag=2;

    for(thing=0;thing<db_top;thing++) {
      if(Typeof(thing) != i || (owner != AMBIGUOUS && db[thing].owner != owner))
        continue;

      /* Check value restrictions */
      switch(category) {
      case SEARCH_ATTR:
        if(!match_attr(player, thing, arg3, 1, string)) continue;
        break;
      case SEARCH_CLASS:
        if(class(thing) != value) continue;
        break;
      case SEARCH_FLAGS:
        if((db[thing].flags & value) != value) continue;
        break;
      case SEARCH_INZONE:
        if(!inlist(db[thing].zone, value)) continue;
        break;
      case SEARCH_LINKS:
        if(db[thing].link != value) continue;
        break;
      case SEARCH_POWERS:
        if(!power(thing, value)) continue;
        break;
      default:
        if(*arg3 && !string_match(db[thing].name, arg3)) continue;
      }

      /* Print type header */
      if(flag) {
        if(flag == 2)
          notify(player, "--");
        notify(player, "%s%s:", typenames[i], (i == TYPE_GARBAGE)?"":"s");
        notify(player, "%s", "");
        flag=0;
      }

      strcpy(buf, unparse_object(player, thing));

      if(controls(player, thing, POW_EXAMINE)) {
        if(i == TYPE_EXIT)
          sprintf(buf+strlen(buf), "  [from %s to %s]",
                  unparse_object(player, db[thing].location),
                  unparse_object(player, db[thing].link));
        else if(i == TYPE_PLAYER || (i == TYPE_THING && owner != AMBIGUOUS))
          sprintf(buf+strlen(buf), "  [location: %s]",
                  unparse_object(player, db[thing].location));
        else if(owner == AMBIGUOUS)
          sprintf(buf+strlen(buf), "  [owner: %s]",
                  unparse_object(player, db[thing].owner));
      }

      notify(player, "%s", buf);
      count++;
    }
  }

  if(count)
    notify(player, "%d object%s found.", count, count==1?"":"s");
  else
    notify(player, "Nothing found.");
}

/* Find owned objects in database */
void do_find(dbref player, char *name)
{
  char buf[2400];
  int i, count=0;

  /* Check money */
  if(!charge_money(player, FIND_COST, 1))
    return;

  /* Search database */
  for(i=0;i<db_top;i++)
    if(Typeof(i) != TYPE_EXIT && Typeof(i) != TYPE_GARBAGE && controls(player,
       i, POW_STATS) && (!*name || string_match(db[i].name, name))) {
      strcpy(buf, unparse_object(player, i));
      if(Typeof(i) == TYPE_THING && db[i].location != i)
        sprintf(buf+strlen(buf), "  [location: %s]",
                unparse_object(player, db[i].location));
      notify(player, "%s", buf);
      ++count;
    }

  if(!count)
    notify(player, "Nothing found.");
  else
    notify(player, "%d object%s found.", count, count==1?"":"s");
}

/* @stats database checking command */
void do_stats(dbref player, char *name)
{
  int a, b, c, i, total, obj[8], pla[NUM_CLASSES];
  int count, owner=AMBIGUOUS, cols=get_cols(NULL, player)-26;
  char buf[512], buf2[32];
  
  /* Find player to stat on */
  if(*name && (owner = lookup_player(name)) == AMBIGUOUS)
    owner=db[player].owner;
  else if(owner == NOTHING) {
    notify(player, "No such player '%s'.", name);
    return;
  } if(owner != AMBIGUOUS && !controls(player, owner, POW_STATS)) {
    notify(player, "You need a search warrant to do that.");
    return;
  }
  
  /* Calculate the number of objects the player (or database) has */
  calc_stats(owner, &total, obj, pla);

  /* Lets print it out */
  notify(player, "\e[1;32m%s Database Breakdown:\e[m",
         owner==AMBIGUOUS?MUD_NAME:db[owner].name);
  i=(owner==AMBIGUOUS)?db_top:total;
  notify(player, "\e[1m%7d \e[36mTotal Object%s\e[m", i, i==1?"":"s");
  if(owner == AMBIGUOUS)
    notify(player, "\e[1m%7d \e[36mFully Active\e[m", total);

  for(i=0;i<8;i++)
    if(obj[i] > 0 && (i != TYPE_PLAYER || obj[i] > 1))
      notify(player, "\e[1m%7d \e[36m%s%s\e[m", obj[i],
             typenames[i], (i == TYPE_GARBAGE || obj[i]==1)?"":"s");
  if(owner != AMBIGUOUS || (HIDDEN_ADMIN && !Immortal(player)))
    return;

  notify(player, "\e[1;34m\304\304\304\304\304\304\304\304\304\304\304\304\304"
         "\304\304\304\304\304\304\304\304\e[m");
  for(i=0;i<NUM_CLASSES;i++) {
    if(!pla[i])
      continue;
    if(!i || pla[i] > cols/2) {
      notify(player, "\e[1m%7d \e[36m%s%s\e[m", pla[i],
             classnames[i], pla[i]==1?"":"s");
      continue;
    }
    count=0;
    *buf=0;
    for(c=0;c<db_top;c++)
      if(Typeof(c) == TYPE_PLAYER && class(c) == i) {
        if(*buf)
          strcat(buf, ", ");
        if(strlen(buf)+strlen(db[c].name) > cols) {
          buf[strlen(buf)-1]=0;
          if(count++)
            notify(player, "%22s\e[1;33m%s\e[m", "", buf);
          else {
	    strcpy(buf2, pla[i]==1?classnames[i]:pluralize(classnames[i]));
            b=13-strlen(buf2);
            strcat(buf2, "\e[34m");
            for(a=0;a<b;a++)
              strcat(buf2, "\371");
            notify(player, "\e[1m%7d \e[36m%s \e[33m%s\e[m", pla[i], buf2,buf);
          }
          *buf=0;
        }
        strcat(buf, db[c].name);
      }

    if(count)
      notify(player, "%22s\e[1;33m%s\e[m", "", buf);
    else {
      sprintf(buf2, "%s%s", classnames[i], pla[i]==1?"":"s");
      c=13-strlen(buf2);
      strcat(buf2, "\e[34m");
      for(a=0;a<c;a++)
        strcat(buf2, "\371");
      notify(player, "\e[1m%7d \e[36m%s \e[33m%s\e[m", pla[i], buf2, buf);
    }
  }
}

void calc_stats(dbref owner, int *total, int *obj, int *pla)
{
  dbref thing, i;
  
  /* Zero out count stats */
  *total=0;
  for(i=0;i<8;i++)
    obj[i]=0;
  for(i=0;i<NUM_CLASSES;i++)
    pla[i]=0;

  /* Find objects */
  for(thing=0;thing<db_top;thing++)
    if(owner == AMBIGUOUS || owner == db[thing].owner) {
      ++obj[Typeof(thing)];
      if(Typeof(thing) == TYPE_PLAYER)
        ++pla[class(thing)];
      if(Typeof(thing) != TYPE_GARBAGE)
        ++*total;
    }
}

// Delete all objects (or objects of a certain type) owned by a player.
//
void do_wipeout(dbref player, char *who, char *arg2)
{
  dbref thing=lookup_player(who);
  int a, type=NOTYPE;
  
  /* Find player to destroy stuff from */
  if(thing <= NOTHING) {
    notify(player, "No such player '%s'.", who);
    return;
  } if(!controls(player,thing,POW_MODIFY) || !controls(player,thing,POW_NUKE)) {
    notify(player, "Permission denied.");
    return;
  } if(player != GOD && class(thing) >= class(db[player].owner)) {
    notify(player, "You may only wipeout players of lower rank.");
    return;
  }

  /* Check object type (if specified) */
  if(*arg2) {
    for(type=0;type < 8;type++)
      if(!strcasecmp(typenames[type], arg2))
        break;
    if(type == 8) {
      notify(player, "Unknown object type '%s'.", arg2);
      return;
    }
  }

  for(a=2;a<db_top;a++)
    if(db[a].owner == thing && a != thing && Typeof(a) != TYPE_GARBAGE &&
       (type == NOTYPE || Typeof(a) == type))
      recycle(a);
  validate_links();

  if(type == NOTYPE)
    notify(player, "Destroyed everything %s owned.", db[thing].name);
  else
    notify(player, "Destroyed all %ss owned by %s.", typenames[type],
           db[thing].name);

  if(db[0].owner == thing && (type == NOTYPE || Typeof(0) == type))
    notify(player, "Warning: %s still exists.", unparse_object(player, 0));
}

/* Command to change ownership with everything from <from> to <rcpt> */
void do_chownall(dbref player, char *arg1, char *arg2)
{
  dbref from, rcpt, a;
  
  if(!*arg1 || !*arg2) {
    notify(player, "Usage: @chownall <from player>=<to player>");
    return;
  }

  /* Find source player */
  if((from = lookup_player(arg1)) == AMBIGUOUS)
    from=db[player].owner;
  if(from == NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  }

  /* Find destination player */
  if((rcpt = lookup_player(arg2)) == AMBIGUOUS)
    rcpt=db[player].owner;
  if(rcpt == NOTHING) {
    notify(player, "No such destination player '%s'.", arg2);
    return;
  } if(Guest(rcpt)) {
    notify(player, "Guests cannot own any objects.");
    return;
  }

  if(from == rcpt) {
    notify(player, "Both players are the same.");
    return;
  }

  /* Check permissions */
  if(!controls(player, from, POW_CHOWN) || !controls(player,rcpt,POW_CHOWN)) {
    notify(player, "Permission denied.");
    return;
  }

  /* Chown everything */
  for(a=0;a<db_top;a++)
    if(db[a].owner == from && a != from) {
      halt_object(a, NULL, 0);
      db[a].owner = rcpt;
    }

  notify(player, "Objects owned by %s are now owned by %s.",
         db[from].name, db[rcpt].name);
}

/* Send high-priority message to connection-id */
void do_cpage(dbref player, char *name, char *msg)
{
  DESC *d;
  int concid=atoi(name);

  /* Check concid */
  if(concid < 0) {
    notify(player, "That is an invalid connection id number.");
    return;
  }

  /* Search for the correct concid */
  for(d=descriptor_list;d && !(d->flags & C_IDENTSOCK) && d->concid != concid;
      d=d->next);

  if(!d)
    notify(player, "No such connection id number found.");
  else if(d->flags & C_NOMESSAGE)
    notify(player, "You cannot send messages to sockets or services.");
  else {
    output2(d, tprintf("Incoming message from %s(#%d): %s\n",
            db[player].name, player, msg));
    if(!Quiet(player))
      notify(player, "Message sent to Connection ID %d: %s", concid,
             conc_title(d));
  }
}

/* Boots a connection id */
void do_cboot(dbref player, char *name)
{
  DESC *d;
  int concid=atoi(name);

  /* Check concid */
  if(concid < 0) {
    notify(player, "That is an invalid connection id number.");
    return;
  }

  /* Search for the correct concid */
  for(d=descriptor_list;d && !(d->flags & C_IDENTSOCK) && d->concid != concid;
      d=d->next);

  if(!d)
    notify(player, "No such connection id number found.");
  else if((d->flags & C_NOMESSAGE) && !power(player, POW_CONSOLE))
    notify(player, "Permission denied.");
  else if(d->flags & C_DNS)
    notify(player, "You cannot kill the domain name service resolver.");
  else if(!Invalid(d->player) && !controls(player, d->player, POW_BOOT))
    notify(player, "Permission denied.");
  else {
    /* Knock em offline */
    notify(player, "Connection ID %d: %s - Booted.", concid, conc_title(d));
    log_main("BOOT: %s(#%d) has booted Connection Id %d: %s",
             db[player].name, player, concid, conc_title(d));
    d->flags |= C_LOGOFF;
  }
}

/* Regular player boot */
void do_boot(dbref player, char *name, char *msg)
{
  DESC *d;
  dbref thing;

  /* Boot off all players from the game */
  if(!strcasecmp(name, "All")) {
    for(d=descriptor_list;d;d=d->next)
      if(d->player > 1 && !Immortal(d->player)) {
        notify(d->player, "Going down - %s.", *msg?msg:"Bye");
        d->flags|=C_LOGOFF;
      }
    notify(player, "All players booted from the game.");
    log_main("BOOT: %s(#%d) boots all players from the game.%s%s",
             db[player].name, player, *msg?" Reason: ":"", msg);
    return;
  }

  /* Check permissions */
  if((thing=lookup_player(name)) <= NOTHING) {
    notify(player, "No such player '%s'.", name);
    return;
  } if(!controls(player, thing, POW_BOOT)) {
    notify(player, "Permission denied.");
    return;
  } if(thing == GOD) {
    notify(player, "You can't boot God!");
    return;
  } if(!boot_off(thing)) {
    notify(player, "That player is not connected.");
    return;
  }

  /* Log the reason */
  notify(player, "%s - Booted.", db[thing].name);
  log_main("BOOT: %s(#%d) has booted %s%s%s.", db[player].name,
           player, db[thing].name, *msg?" for ":"", msg);
}

/* Joins another player */
void do_join(dbref player, char *arg1)
{
  dbref thing=lookup_player(arg1);
  
  /* Check permissions */
  if(thing <= NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  } if(!controls(player, thing, POW_JOIN)) {
    notify(player, "Permission denied.");
    return;
  } if(Typeof(player) != TYPE_PLAYER && !power(player, POW_BUILD)) {
    notify(player, "Only players may join others.");
    return;
  } if(!IS(thing, TYPE_PLAYER, PLAYER_CONNECT) && !power(player, POW_WHO)) {
    notify(player, "But that player isn't even connected!");
    return;
  }

  /* Match dimensional planes with the target */
  if(!(db[player].plane == db[thing].plane || db[player].plane == NOTHING ||
       db[thing].plane == NOTHING) && Typeof(db[thing].location)==TYPE_ROOM) {
    notify(player, "\e[1;35mYou make a rip in the space-time continuum.\e[m");
    enter_plane=db[thing].plane;
  }

  enter_room(player, db[thing].location);
}

/* Summons other players to your location */
void do_summon(dbref player, char *arg1)
{
  dbref thing=lookup_player(arg1);
  
  /* Check permissions */
  if(thing <= NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  } if(!controls(player, thing, POW_SUMMON)) {
    notify(player, "Permission denied.");
    return;
  } if(Typeof(player) != TYPE_PLAYER && !power(player, POW_BUILD)) {
    notify(player, "Only players may summon others.");
    return;
  } if(!IS(thing, TYPE_PLAYER, PLAYER_CONNECT) && !power(player, POW_WHO)) {
    notify(player, "But that player isn't even connected!");
    return;
  }

  /* Match dimensional planes with the source */
  if(!(db[player].plane == db[thing].plane || db[player].plane == NOTHING ||
       db[thing].plane == NOTHING) && Typeof(db[player].location)==TYPE_ROOM) {
    notify(player, "\e[1;35mYou make a rip in the space-time continuum.\e[m");
    enter_plane=db[player].plane;
  }

  enter_room(thing, db[player].location);
}

/* Sends a player to the dungeon */
void do_capture(dbref player, char *arg1, char *arg2)
{
  dbref thing=lookup_player(arg1);
  
  /* Check permissions */
  if(thing <= NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  } if(player == thing) {
    notify(player, "You can't jail yourself, silly!");
    return;
  } if(!controls(player, thing, POW_CAPTURE)) {
    notify(player, "Permission denied.");
    return;
  } if(DUNGEON_CELL < 1 || Typeof(DUNGEON_CELL) != TYPE_ROOM) {
    notify(player, "Sorry, no Dungeon Cell room is currently configured.");
    return;
  } if(db[thing].location == DUNGEON_CELL) {
    notify(player, "%s is already in the dungeon.", db[thing].name);
    return;
  }

  if(!(db[DUNGEON_CELL].flags & ROOM_XZONE))
    notify(player, "Warning: %s is not set X-Zone!",
           unparse_object(player, DUNGEON_CELL));

  if(!Quiet(player))
    notify(player, "%s - Jailed.", db[thing].name);
  log_main("CAPTURE: %s(#%d) has jailed %s%s%s.", db[player].name,
           player, db[thing].name, *arg2?" for ":"", arg2);
  notify(thing, "You have been jailed by %s%s%s.", db[player].name,
         *arg2?" for ":"", arg2);
  enter_plane=0;
  enter_room(thing, DUNGEON_CELL);
}

/* Frees someone from the dungeon cell */
void do_uncapture(dbref player, char *arg1)
{
  dbref thing=lookup_player(arg1);
  
  /* Check permissions */
  if(thing <= NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  } if(!controls(player, thing, POW_CAPTURE)) {
    notify(player, "Permission denied.");
    return;
  } if(DUNGEON_CELL < 1) {
    notify(player, "Sorry, no Dungeon Cell room is currently configured.");
    return;
  } if(db[thing].location != DUNGEON_CELL) {
    notify(player, "Sorry, that person is not in jail.");
    return;
  }

  if(!Quiet(player))
    notify(player, "The prisoner is now free to go.");
  log_main("UNCAPTURE: %s(#%d) has freed %s.", db[player].name,
           player, db[thing].name);
  enter_plane=0;
  enter_room(thing, HOME);
}

/* Check if a player is on the inactivity list */
int inactive(dbref player)
{
  long then;

  /* Administrators are never considered inactive */
  if(Immortal(player) || (db[player].flags & PLAYER_INACTIVE_OK) ||
     (db[player].flags & PLAYER_CONNECT))
    return 0;

  /* Determine last logoff time */
  if((then=db[player].data->last) < db[player].data->lastoff)
    then=db[player].data->lastoff;
  if(!then)
    then=db[player].create_time;  /* Player has never connected */
  if(!then)
    return 0;  /* Character existed before database had create_time */

  /* Normal characters get 6 months */
  return (int)((now-then)/86400) >= ((INACTIVENUKE > 1)?INACTIVENUKE:180);
}

/* Command to nuke inactive players */
void do_inactivity(dbref player, char *arg1)
{
  int count=0, owned, i, thing;
  long tt;

  /* Perform inactivity nuke */
  if(!strcasecmp(arg1, "All")) {
    if(!power(player, POW_NUKE)) {
      notify(player, "Permission denied.");
      return;
    }
    log_main("%s(#%d) does @inactivity. Processing...", db[player].name,
             player);
    if((count=inactive_nuke()))
      notify(player, "%d player%s deleted.", count, count==1?"":"s");
    else
      notify(player, "There are no players scheduled for destruction.");
    return;
  }

  /* Check inactivity for a specific player */
  if(*arg1) {
    if((thing=lookup_player(arg1)) == AMBIGUOUS)
      thing=player;
    if(thing == NOTHING || Typeof(thing) != TYPE_PLAYER) {
      notify(player, "No such player '%s'.", arg1);
      return;
    } if(!inactive(thing)) {
      notify(player, "%s is not inactive.", db[thing].name);
      return;
    }

    /* Determine last logoff time */
    if((tt=db[thing].data->last) < db[thing].data->lastoff)
      tt=db[thing].data->lastoff;
    if(!tt)
      tt=db[thing].create_time;  /* Player has never connected */

    notify(player, "%s is %ld day%s inactive.", db[thing].name,
           (now-tt)/86400, ((now-tt)/86400)==1?"":"s");
    return;
  }

  /* Read and/or wipeout inactive player list */
  for(thing=0;thing<db_top;thing++) {
    if(Typeof(thing) != TYPE_PLAYER || !inactive(thing))
      continue;

    /* Determine last logoff time */
    if((tt=db[thing].data->last) < db[thing].data->lastoff)
      tt=db[thing].data->lastoff;
    if(!tt)
      tt=db[thing].create_time;  /* Player has never connected */

    for(i=0,owned=0;i<db_top;i++)
      if(db[i].owner == db[thing].owner && i != thing)
        owned++;
    ++count;
    notify(player, "%s is %ld day%s inactive: %d objects", db[thing].name,
           (now-tt)/86400, ((now-tt)/86400)==1?"":"s", owned);
  }

  notify(player, "%d Player%s scheduled for destruction.", count,
         count==1?"":"s");
}

int inactive_nuke()
{
  dbref thing;
  int i, owned, count=0;

  /* Turn off infinite-loop timer */
  alarm(0);

  /* Read and/or wipeout inactive player list */
  for(thing=0;thing<db_top;thing++) {
    if(Typeof(thing) != TYPE_PLAYER || !inactive(thing))
      continue;

    for(i=0,owned=0;i<db_top;i++)
      if(db[i].owner == db[thing].owner && i != thing)
        owned++;

    log_main("%s(#%d): %d objects - Nuked.", db[thing].name, thing, owned);
    destroy_player(thing);
    ++count;
  }

  /* Turn infinite-loop timer back on */
  alarm(10);

  /* Log result */
  log_main("%d Player%s nuked.", count, count==1?"":"s");
  return count;
}

/* Find the last time another player logged onto the mud */
void do_laston(dbref player, char *name)
{
  dbref thing;
  long tt;

  if(!*name) {
    notify(player, "You must specify a valid player name.");
    return;
  } if((thing=lookup_player(name)) == AMBIGUOUS)
    thing=player;
  if(thing == NOTHING || Typeof(thing) != TYPE_PLAYER) {
    notify(player, "No such player '%s'.", name);
    return;
  }

  /* Check if the player is hidden */
  if(!controls(player, thing, POW_WHO) && !could_doit(player,thing,A_HLOCK)) {
    notify(player, "%s wishes to have some privacy.", db[thing].name);
    return;
  }

  if(!(tt=db[thing].data->last)) {
    notify(player, "%s has never logged on.", db[thing].name);
    return;
  }
  notify(player, "%s was last logged on: %s.", db[thing].name,
         mktm(player, tt));

  if(controls(player, thing, POW_WHO) && !IS(thing, TYPE_PLAYER,
     PLAYER_CONNECT) && (tt=db[thing].data->lastoff))
    notify(player, "%s last logged off at: %s.", db[thing].name,
           mktm(player, tt));
}

/* Adds an incident report to the player file list (@pfile) for <name> */
void do_pfile(dbref player, char *name, char *report)
{
  dbref thing;
  char buf[8600], *s=buf;

  if(!*name) {
    notify(player, "You must specify a player name.");
    return;
  } if((thing = lookup_player(name)) <= NOTHING) {
    notify(player, "No such player '%s'.", name);
    return;
  } if(Guest(thing)) {
    notify(player, "Incident reports cannot be filed for guest characters.");
    return;
  } if(!controls(player, thing, POW_PFILE)) {
    notify(player, "You can't file incident reports on %s!", db[thing].name);
    return;
  }

  strcpy(buf, atr_get(thing, A_PFILE));

  /* Blank <report> displays current listed incidents */
  if(!*report) {
    if(!*buf) {
      notify(player, "%s has no report filed.", db[thing].name);
      return;
    }
    notify(player, "%s%s Incident Report:", db[thing].name, possname(db[thing].name));
    notify(player, "%s", parse_function(player, player, buf));
    return;
  }

  /* Clear report file if <report> = "clear" */
  if(!strcasecmp(report, "clear")) {
    if(!*buf) {
      notify(player, "That player has no incident report to clear.");
      return;
    }
    atr_add(thing, A_PFILE, "");
    notify(player, "Incident report for %s(#%d) has been cleared.",
           db[thing].name, thing);
    log_main("%s(#%d) cleared the incident report on %s(#%d).",
             db[player].name, player, db[thing].name, thing);
    return;
  }

  /* Add a report */

  if(power(thing, POW_PFILE)) {
    notify(player, "You can't file an incident report on %s!", player == thing?
           "yourself":"administrators");
    return;
  }

  /* Append to @pfile attribute */
  if(*buf) {
    s=strnul(buf);
    *s++='%', *s++='R';
  }
  s+=sprintf(s, "--[time(%ld)] by %s:%%R", now, db[player].name);
  s+=sprintf(s, "%.*s", (s-buf >= 8000)?0:(int)(8000-(s-buf)), report);

  if(s-buf >= 8000)
    notify(player, "Cannot store incident; %s%s playerfile is full!",
           db[thing].name, possname(db[thing].name));
  else {
    atr_add(thing, A_PFILE, buf);
    notify(player, "Incident stored for %s.", db[thing].name);
  }

  log_main("%s(#%d) added the following to %s(#%d)'s playerfile: %s",
           db[player].name, player, db[thing].name, thing, report);
}


static char *sitelock_classes[3]={"Guests", "Create", "Login"};

// Lock or unlock a specific Host/IP Address from connecting to the game.
// Possible lock levels are:
//  1: Lockout guest accounts.
//  2: Lockout character creation.
//  3: Lockout all server login requests.
//
void do_sitelock(dbref player, char *host, char *arg2)
{
  struct sitelock *ptr, *last=NULL;
  int level, len;
  char *s;

  if(!*host) {
    notify(player, "Usage: @sitelock <host/IP>=<level>");
    notify(player, "Lock levels in order are: Guests, Create, Login");
    return;
  }

  /* Check for spaces in the hostname */
  for(s=host;*s > ' ';s++)
    *s=tolower(*s);
  if(*s) {
    notify(player, "Hostnames cannot contain any spaces.");
    return;
  }

  /* Parse lock level */
  if(!*arg2)
    level=0;
  else if(isdigit(*arg2))
    level=atoi(arg2);
  else
    for(level=1;level <= 3;level++)
      if(string_prefix(sitelock_classes[level-1], arg2))
        break;
  if(level < 0 || level > 3) {
    notify(player, "Unknown sitelock level '%s'.", arg2);
    return;
  }

  /* Search through list of sites to see if hostname already exists */
  for(ptr=sitelock_list;ptr;last=ptr,ptr=ptr->next)
    if(!strcasecmp(ptr->host, host))
      break;
  if(ptr) {
    if(level) {
      ptr->class=level;
      log_main("SITELOCK: %s(#%d) locked host '%s' at level %s (%d).",
               db[player].name, player, host, sitelock_classes[level-1],level);
      notify(player, "%s - Locked to level %s (%d).", host,
             sitelock_classes[level-1], level);
      return;
    }
    log_main("SITELOCK: %s(#%d) removed the sitelock from host '%s'.",
             db[player].name, player, host);
    notify(player, "%s - Sitelock cleared.", host);
    if(last)
      last->next=ptr->next;
    else
      sitelock_list=ptr->next;
    free(ptr);
    return;
  }

  if(!level) {
    notify(player, "Sitelock for '%s' is already cleared.", host);
    return;
  }

  /* Create new sitelock structure and link it into the list */

  len=strlen(host)+1;
  ptr=malloc(sizeof(struct sitelock)+len);
  ptr->next=NULL;
  ptr->class=level;
  memcpy(ptr->host, host, len);
  if(last)
    last->next=ptr;
  else
    sitelock_list=ptr;

  log_main("SITELOCK: %s(#%d) locked host '%s' at level %s (%d).",
           db[player].name, player, host, sitelock_classes[level-1], level);
  notify(player, "%s - Locked to level %s (%d).", host,
         sitelock_classes[level-1], level);
}

// Lists all sitelocks currently specified in the database.
//
void list_sitelocks(dbref player)
{
  struct sitelock *ptr=sitelock_list;

  if(!ptr)
    notify(player, "No sitelocks are currently defined.");
  else {
    notify(player, "Current sitelocks:");
    for(;ptr;ptr=ptr->next)
      notify(player, "  %-40s  %s (%d)", ptr->host,
             sitelock_classes[ptr->class-1], ptr->class);
  }
}

void save_sitelocks(FILE *f)
{
  struct sitelock *ptr=sitelock_list;

  if(ptr) {
    for(;ptr;ptr=ptr->next) {
      putchr(f, ptr->class);
      putstring(f, ptr->host);
    }
    putchr(f, 0);
  }
}

int load_sitelocks(FILE *f)
{
  struct sitelock *new, *last=NULL;
  int count=0, class, len;
  char *s;

  while((class=fgetc(f))) {
    s=getstring_noalloc(f);
    len=strlen(s)+1;
    new=malloc(sizeof(struct sitelock)+len);
    new->next=NULL;
    new->class=class;
    memcpy(new->host, s, len);

    if(last)
      last->next=new;
    else
      sitelock_list=new;
    last=new;
    count++;
  }

  return count;
}
