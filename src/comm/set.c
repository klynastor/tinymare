/* comm/set.c */
/* commands that modify settings, flags, and attributes on objects */

#include "externs.h"

/* checks permissions for modification */
int match_controlled(dbref player, char *name, char power)
{
  dbref thing;

  if((thing=match_thing(player, name, MATCH_EVERYTHING)) == NOTHING)
    return NOTHING;
  if(!controls(player, thing, power)) {
    if(!match_quiet)
      notify(player, "Permission denied.");
    return NOTHING;
  }
  return thing;
}

/* renames an object */
void do_name(dbref player, char *arg1, char *name)
{
  char *p, *s, *alias=NULL;
  dbref thing;

  if(!*name) {
    notify(player, "Please specify a new name.");
    return;
  }

  if((thing = match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  if(Typeof(thing) == TYPE_PLAYER) {
    if(!power(player, POW_MEMBER)) {
      notify(player, "Sorry, you may not change your name.");
      return;
    } if(Guest(thing)) {
      notify(player, "Guest players cannot be renamed.");
      return;
    } if(strcasecmp(db[thing].name, name) && !ok_player_name(name, 0)) {
      if(player == thing)
        notify(player, "You can't name yourself that.");
      else
        notify(player, "You can't give a player that name.");
      return;
    }

    /* Set the name */
    if(FORCE_UC_NAMES)
      *name=toupper(*name);
    log_main("NAME CHANGE: %s(#%d) to %s", db[thing].name, thing, name);
    notify_except(thing, thing, "%s is now known as %s.", db[thing].name,name);

    if(player == thing || db[player].location != db[thing].location)
      notify(player, "%s -> %s. Set.", db[thing].name, name);

    delete_player(thing);
    WPTR(db[thing].name, name);
    add_player(thing);

    if(player != thing)
      notify(thing, "Your name has been changed by %s to %s.",
             db[player].name, db[thing].name);
    return;
  }

  /* Check object name */
  while(*name == ' ')
    name++;
  if((s=strchr(name, ';'))) {
    *s='\0';
    for(alias=s+1;*alias == ' ';alias++);
    for(p=strnul(alias);p > alias && *(p-1) == ' ';*--p='\0');
    if(!*alias)
      alias=NULL;
  } else
    s=strnul(name);
  while(s > name && *(s-1) == ' ')
    *--s='\0';
  
  if(!ok_name(name)) {
    notify(player, "That is not a reasonable name.");
    return;
  }

  notify(player, "%s -> %s. Set.", db[thing].name, name);
  WPTR(db[thing].name, name);
  if(alias && (Typeof(thing) == TYPE_EXIT || Typeof(thing) == TYPE_THING)) {
    atr_add(thing, A_ALIAS, alias);
    if(strchr(alias, ';'))
      notify(player, "Aliases set.");
    else
      notify(player, "Alias set.");
  }
}

/* unlinks an object from its destination */
void do_unlink(dbref player, char *arg1)
{
  dbref thing=match_thing(player, arg1, MATCH_EVERYTHING);
  
  if(thing == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  } if(Typeof(thing) != TYPE_EXIT && Typeof(thing) != TYPE_ZONE &&
       Typeof(thing) != TYPE_ROOM) {
    notify(player, "That object cannot be unlinked.");
    return;
  } if(db[thing].link == NOTHING) {
    notify(player, "Already unlinked.");
    return;
  }

  db[thing].link = NOTHING;
  notify(player, Typeof(thing) == TYPE_ROOM?"Dropto removed.":"Unlinked.");
}

/* change ownership of object */
void do_chown(dbref player, char *name, char *newobj)
{
  dbref thing, owner=db[player].owner;
  ATRDEF *k;
  
  /* check object */
  if(power(player, POW_CHOWN))
    thing = match_thing(player, name, MATCH_EVERYTHING);
  else
    thing = match_thing(player, name, MATCH_INVEN|MATCH_HERE|MATCH_EXIT|
                        MATCH_ZONE);
  if(thing == NOTHING)
    return;

  /* check owner */
  if(*newobj && (owner = lookup_player(newobj))) {
    if(owner == NOTHING) {
      notify(player, "No such player '%s'.", newobj);
      return;
    } if(owner == AMBIGUOUS)
      owner=db[player].owner;
  }

  if(Typeof(thing) == TYPE_PLAYER) {
    notify(player, "You may not chown players.");
    return;
  } if(db[thing].owner == owner) {
    if(db[player].owner == owner)
      notify(player, "You already own that!");
    else
      notify(player, "%s already owns that.", db[owner].name);
    return;
  } if(!controls(player, owner, POW_CHOWN) || (!controls(player, thing,
       POW_CHOWN) && !(db[thing].flags & CHOWN_OK))) {
    notify(player, "Permission denied.");
    return;
  } if(RESTRICT_BUILD && !Builder(player)) {
    notify(player, "Only authorized Builders may chown that object.");
    return;
  } if(Guest(owner)) {
    notify(player, "Guests cannot own any objects.");
    return;
  }

  halt_object(thing, NULL, 0);
  db[thing].owner = owner;
  if(db[thing].flags & CHOWN_OK) {
    db[thing].flags &= ~CHOWN_OK;
    db[thing].flags |= HAVEN;
  }

  /* Remove 'privs' attribute options */
  for(k=db[thing].atrdefs;k;k=k->next)
    k->atr.flags &= ~AF_PRIVS;

  notify(player, "Owner changed.");
}

/* unlocks an object */
void do_unlock(dbref player, char *name)
{
  dbref thing;

  if((thing=match_controlled(player, name, POW_MODIFY)) == NOTHING)
    return;

  if(Typeof(thing) == TYPE_EXIT && !Builder(player) &&
     is_zone(thing, ZONE_RESTRICTED))
    notify(player, "You can't lock or unlock exits in this zone.");
  else {
    atr_add(thing, A_LOCK, "");
    notify(player, "Unlocked.");
  }
}

// Main command to set flags or attributes on a database object.
//
void do_set(dbref player, char *name, char *flag)
{
  int f, i, cost, inv, check=0;
  dbref match, thing;
  char *r, *s;
  
  /* Find thing */
  if((thing = match_thing(player, name, MATCH_EVERYTHING)) == NOTHING)
    return;
  
  /* Check for attribute set first */
  if((s=strchr(flag, ':'))) {
    ATTR *attr;

    *s++='\0';

    /* Match #num.attr format */
    if(*flag == '#' && isdigit(*(flag+1)) && (r=strchr(flag+2, '.')) &&
       *(r+1) && !Invalid((match=atoi(flag+1)))) {
      if(!(attr=def_atr_str(match, r+1))) {
        notify(player, "No such user-defined attribute on object #%d.", match);
        return;
      }
    } else {
      if(!(attr=atr_str(thing, flag))) {
        notify(player, "Sorry, that isn't a valid attribute.");
        return;
      }
    }
    
    /* Check permissions */
    if(!can_set_atr(player, thing, attr)) {
      notify(player, "You can't set that attribute.");
      return;
    } if(*s && attr->obj == -1 && attr->num == A_ALIAS &&
         Typeof(thing) == TYPE_PLAYER && !ok_player_name(s, 1)) {
      if(strcasecmp(s, "me") && lookup_player(s) != NOTHING)
        notify(player, "There is already someone with that alias.");
      else
        notify(player, "That is an invalid alias.");
      return;
    } if(attr->obj == -1 && attr->num == A_LOCK && !Builder(player) &&
         Typeof(thing) == TYPE_EXIT && is_zone(thing, ZONE_RESTRICTED)) {
      notify(player, "You can't lock or unlock exits in this zone.");
      return;
    }

    /* Attribute that may optionally require POW_WIZATTR */
    if(attr->obj == -1 && !power(player, POW_WIZATTR) &&
       ((attr->num == A_HLOCK && RESTRICT_HIDDEN) ||
        (attr->num == A_COMTITLE && !ALLOW_COMTITLES) ||
        (attr->num == A_VLOCK && (Typeof(thing) == TYPE_PLAYER ||
         !Builder(player))))) {
      notify(player, "You can't set that attribute.");
      return;
    }

    /* Check possible object genders */
    if(attr->obj == -1 && attr->num == A_SEX && Typeof(thing) == TYPE_PLAYER) {
      if(!power(player, POW_MEMBER)) {
        notify(player, "You can't change your gender.");
        return;
      }

      if(*s == 'm' || *s == 'M')
        db[thing].data->gender=0;
      else if(*s == 'f' || *s == 'F' || *s == 'w' || *s == 'W')
        db[thing].data->gender=1;
      else {
        notify(player, "Invalid player gender.");
        return;
      }
      if(!Quiet(player))
        notify(player, "%s - Set.", db[thing].name);
      return;
    }

    /* Check for valid locks */
    if(attr->flags & AF_LOCK) {
      if((r=process_lock(player, thing, s))) {
        atr_add_obj(thing, attr->obj, attr->num, r);
        if(!Quiet(player))
          notify(player, *r?"Locked.":"Unlocked.");
      }
      return;
    }

    /* Delete old alias from playerlist */
    if(Typeof(thing) == TYPE_PLAYER && attr->obj == -1 && attr->num == A_ALIAS)
      delete_player(thing);

    /* Do the set */
    if(attr->num)
      atr_add_obj(thing, attr->obj, attr->num, s);

    /* Add new alias to playerlist */
    if(Typeof(thing) == TYPE_PLAYER && attr->obj == -1 && attr->num == A_ALIAS)
      add_player(thing);

    if(!Quiet(player))
      notify(player, "%s - %s.", db[thing].name, *s?"Set":"Cleared");
    return;
  }
  
  s=flag;
  while((r=parse_up(&s, ' '))) {
    if(*r == '!') {
      r++;
      inv=1;
    } else
      inv=0;
    if(!*r)
      continue;

    check |= 2;
    for(i=0;flaglist[i].name;i++)
      if((flaglist[i].objtype == NOTYPE || flaglist[i].objtype==Typeof(thing))
         && flaglist[i].bits && string_prefix(flaglist[i].name, r))
        break;

    if(!flaglist[i].name) {
      notify(player, "No such flag '%s'.", r);
      continue;
    }

    f=flaglist[i].bits;

    if(f != PLAYER_SLAVE) {
      if(!(check & 1) && !controls(player, thing, POW_MODIFY)) {
        notify(player, "Permission denied.");
        return;
      }
      check |= 1;
    }

    /* Check existence of flag */
    if((!inv && (db[thing].flags & f)) || (inv && !(db[thing].flags & f))) {
      if(!Quiet(player))
        notify(player, "Flag already %sset.", inv?"un":"");
      continue;
    }

    /* Check for restricted flag */
    if(f == PLAYER_CONNECT && Typeof(thing) == TYPE_PLAYER) {
      notify(player, "Use QUIT instead.");
      continue;
    } else if(f == GOING) {
      notify(player, "Use @destroy instead.");
      continue;
    } else if(f == THING_MONSTER && Typeof(thing) == TYPE_THING) {
      notify(player, "Use @enemy instead.");
      continue;
    }

    /* Check wizflags */
    if(!inv && !power(player, POW_WIZFLAGS)) {
      switch(Typeof(thing)) {
      case TYPE_ROOM:
        if(f & (ROOM_ARENA|ROOM_NEST|ROOM_XZONE|ROOM_SANCTUARY|ROOM_FOG|
                LIGHT)) {
          notify(player, "Permission denied.");
          continue;
        }
        break;
      case TYPE_ZONE:
        if(f & (ZONE_RESTRICTED|ZONE_UNIVERSAL)) {
          notify(player, "Permission denied.");
          continue;
        }
        break;
      case TYPE_EXIT:
        if((f & DARK) && !Builder(player) && is_zone(thing, ZONE_RESTRICTED)) {
          notify(player, "You can't set exits dark here.");
          continue;
        }
        break;
      }
    }

    /* Player wizflags cannot be unset by the player */
    if(Typeof(thing) == TYPE_PLAYER && !power(player, POW_WIZFLAGS) &&
       (f & (PLAYER_BUILD|PLAYER_INACTIVE_OK))) {
      notify(player, "Permission denied.");
      continue;
    }

    if(Typeof(thing) == TYPE_PLAYER && f == PLAYER_SLAVE) {
      if(thing == GOD || !controls(player, thing, POW_SLAVE) ||
         class(db[player].owner) <= class(thing)) {
        notify(player, "Permission denied.");
        continue;
      }
      log_main("%s(#%d) %snslaved %s(#%d)", db[player].name, player,
               inv?"u":"e", db[thing].name, thing);
      notify(thing, "Your character has been temporarily disabled by %s.",
             db[player].name);
      boot_off(thing);              /* Disconnect from game */
      halt_object(thing, NULL, 0);  /* Halt all commands */
    }

    if(Typeof(thing) == TYPE_PLAYER && f == PLAYER_NOTICE &&
       !power(player, POW_SECURITY)) {
      notify(player, "Permission denied.");
      continue;
    }

    /* Flag costs */
    cost=0;
    if(!inv && !power(player, POW_FREE)) {
      if(Typeof(thing) == TYPE_ROOM) {
        if(f == ROOM_LINK_OK) cost=LINK_OK_COST;
        else if(f == ROOM_JUMP_OK) cost=JUMP_OK_COST;
        else if(f == ROOM_ABODE) cost=ABODE_COST;
        else if(f == ROOM_HEALING) cost=HEALING_COST;
        else if(f == ROOM_ILLNESS) cost=ILLNESS_COST;
        else if(f == ROOM_SHAFT) cost=SHAFT_COST;
        else if(f == ROOM_OCEANIC) cost=OCEANIC_COST;
        else if(f == DARK) cost=DARK_COST;
      } else if(Typeof(thing) == TYPE_EXIT && f == DARK)
        cost=DARK_COST;

      if(!charge_money(player, cost, 1))
        continue;
    }

    /* Log the builder flag */
    if(Typeof(thing) == TYPE_PLAYER && f == PLAYER_BUILD) {
      log_main("%s(#%d) %s Builder permissions %s %s(#%d)",
               db[player].name, player, inv?"took":"gave", inv?"from":"to",
               db[thing].name, thing);
      notify(thing, "%s %s Builder permissions.", db[player].name,
             inv?"removed your":"gave you");
    }

    /* We're clear. Set the flag. */
    if(inv)
      db[thing].flags &= ~f;
    else
      db[thing].flags |= f;

    /* Maintain Universal Zones */
    if(Typeof(thing) == TYPE_ZONE && f == ZONE_UNIVERSAL) {
      if(inv)
        PULL(uzo, thing);
      else
        PUSH_L(uzo, thing);
    }

    /* Maintain wander count */
    if(Typeof(thing) == TYPE_THING && f == THING_WANDER)
      wander_count += inv?-1:1;

    if(Quiet(player))
      continue;

    if(cost)
      notify(player, "Flag set. You have been charged %d %s.", cost,
             MONEY_PLURAL);
    else
      notify(player, "Flag %sset.", inv?"un":"");
  }

  if(!(check & 2) && !Quiet(player))
    notify(player, "Nothing to set.");
}

/* check for abbreviated set command */
int test_set(dbref player, char *cmd, char *arg1, char *arg2)
{
  ATTR *a;
  char buff[8192], *s;
  dbref thing;
  
  /* builtin attribute */
  if((*cmd != '$') && (a=builtin_atr_str(cmd+1)) &&
     (power(player, POW_WIZATTR) || !(a->flags & (AF_WIZ|AF_HIDDEN))) &&
     (power(player, POW_COMBAT) || !(a->flags & AF_COMBAT))) {
    sprintf(buff, "%s:%s", cmd+1, arg2);
    do_set(player, arg1, buff);
    return 1;
  }

  /* match object */
  if((thing=match_thing(player, arg1, (*cmd == '&')?MATCH_EVERYTHING:
     MATCH_EVERYTHING|MATCH_QUIET)) == NOTHING)
    return (*cmd == '&');

  /* create user-defined attribute with & if it doesn't exist */
  if(*cmd == '&' && *arg2 && !atr_str(thing, cmd+1)) {
    *buff='\0';
    sprintf(buff+1, "#%d/%s", thing, cmd+1);
    do_defattr(player, buff+1, buff);
    if(!atr_str(thing, cmd+1))
      return 1;
  }

  /* check valid user-defined attribute */
  if(*cmd == '&' || ((a=atr_str(thing, cmd+1)) &&
      (power(player, POW_WIZATTR) || !(a->flags & (AF_WIZ|AF_HIDDEN))) &&
      (power(player, POW_COMBAT) || !(a->flags & AF_COMBAT))) ||
     (*cmd == '@' && *(cmd+1) == '#' && isdigit(*(cmd+2)) &&
      (s=strchr(cmd+3, '.')) && *(s+1))) {
    sprintf(buff, "%s:%s", cmd+1, arg2);
    do_set(player, arg1, buff);
    return 1;
  }
  return 0;
}

void do_pennies(dbref player, char *arg1, char *arg2)
{
  dbref thing=match_controlled(player, arg1, POW_MODIFY);
  int value=atoi(arg2);

  if(thing == NOTHING)
    return;

  if(Typeof(thing) != TYPE_PLAYER && Typeof(thing) != TYPE_ROOM) {
    notify(player, "Money values can only be set on players or rooms.");
    return;
  }
  if(value < 0)
    value=0;
  if(value > 999999999)
    value=999999999;
  db[thing].pennies=value;

  if(!Quiet(player))
    notify(player, "%s - Set.", db[thing].name);
}

/* edit attributes */
void do_edit(dbref player, char *name, char *argc, char **argv)
{
  dbref thing;
  char fl[640], temp[8192], *new, *pt;
  struct all_atr_list *al, *list;
  int num, count=0;

  /* check arguments */
  if(!argv[1] || !*argv[1]) {
    notify(player, "Nothing to do.");
    return;
  }
  new=argv[2]?:"";

  if(!strcmp(argv[1], "^"))
    if(!*new) {
      notify(player, "Nothing to prepend.");
      return;
    }
  if(!strcmp(argv[1], "$"))
    if(!*new) {
      notify(player, "Nothing to append.");
      return;
    }

  /* find objname */
  strmaxcpy(fl, name, 640);
  if(!(pt=strchr(fl, '/'))) {
    notify(player, "No attribute specified.");
    return;
  }
  *pt++=0;
  if((thing = match_thing(player, fl, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* search attribute list */
  for(list=al=all_attributes(thing);al && al->type;al++) {
    if(!can_see_atr(player, thing, al->type))
      continue;
    if(wild_match(al->type->name, pt, 0)) {
      count++;
      if(!can_set_atr(player, thing, al->type) ||
         (al->type->obj == NOTHING && al->type->num == A_ALIAS) ||
         (!power(player, POW_WIZATTR) && (al->type->flags & AF_LOCK))) {
        notify(player, "\e[1m%s\e[m - Cannot edit attribute.", al->type->name);
        continue;
      }

      if((num=edit_string(temp, atr_get_obj(thing, al->type->obj,
                      al->type->num), argv[1], new))) {
        atr_add_obj(thing, al->type->obj, al->type->num, temp);
        notify(player, "\e[1m%s\e[m -> %s", al->type->name, temp);
        if(num != 1)
          notify(player, "%d changes made.", num);
      } else
        notify(player, "\e[1m%s\e[m - No matches found.", al->type->name);
    }
  }
  free(list);

  if(!count)
    notify(player, "No match.");
}

/* cycle contents of an attribute through a list of values */
void do_cycle(dbref player, char *arg1, char *arg2, char **argv)
{
  ATTR *attr;
  char *s, *text;
  int i, thing;

  /* validate target */
  if(!(s=strchr(arg1, '/'))) {
    notify(player, "You must specify an attribute name.");
    return;
  } if(!argv[1]) {
    notify(player, "Nothing to cycle.");
    return;
  }

  *s++='\0';
  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* validate attribute */
  if(!(attr=atr_str(thing, s))) {
    notify(player, "No match.");
    return;
  } if(!can_set_atr(player, thing, attr)) {
    notify(player, "You can't set that attribute.");
    return;
  } if(attr->flags & AF_LOCK) {
    notify(player, "Lock attributes cannot be cycled.");
    return;
  }

  /* find next item in cycle */
  text=atr_get_obj(thing, attr->obj, attr->num);
  for(i=1;i<100 && argv[i];i++)
    if(!strcasecmp(text, argv[i])) {
      i++;
      break;
    }
  if(i == 100 || !argv[i])
    i=1;

  /* if an alias, make sure we can set it easily */
  if(Typeof(thing)==TYPE_PLAYER && attr->obj == -1 && attr->num == A_ALIAS) {
    if(*argv[i] && !ok_player_name(argv[i], 1)) {
      if(strcasecmp(argv[i], "me") && lookup_player(argv[i]) != NOTHING)
        notify(player, "There is already someone with that alias.");
      else
        notify(player, "That is an invalid alias.");
      return;
    }

    /* delete old alias from playerlist */
    delete_player(thing);
  }

  /* set attribute */
  atr_add_obj(thing, attr->obj, attr->num, argv[i]);

  /* add new alias to playerlist */
  if(Typeof(thing) == TYPE_PLAYER && attr->obj == -1 && attr->num == A_ALIAS)
    add_player(thing);

  /* notify success */
  if(!Quiet(player))
    notify(player, "%s - %s.", db[thing].name, *argv[i]?"Set":"Cleared");
}

// Remove all modifyable attributes on an object.
//
void do_wipeattr(dbref player, char *arg1, char *arg2)
{
  ALIST *ptr, *last=NULL, *next;
  ATTR *attr;
  dbref thing;

  /* Check permissions */
  if((thing=match_controlled(player, arg1, POW_MODIFY)) == NOTHING)
    return;

  /* Search and check attributes */
  for(ptr=db[thing].attrs;ptr;ptr=next) {
    next=ptr->next;
    if(!(attr=atr_num(ptr->obj, ptr->num)))
      continue;
    if(!((attr->obj == NOTHING && attr->num == A_ALIAS) ||
       !can_set_atr(player, thing, attr) || (*arg2 &&
       !match_word(arg2, attr->name)))) {
      /* Remove it */
      if(last)
        last->next=next;
      else
        db[thing].attrs=next;
      free(ptr);
      db_attrs--;
    } else
      last=ptr;
  }

  notify(player, "%s - Attributes wiped.", db[thing].name);
}

/* Change Quote of the Day */
void do_poll(dbref player, char *arg1, char *arg2, char **argv, char *pure)
{
  static long poll_count=0;
  static dbref poll_who=0;

  if(now < poll_count && player != poll_who && !power(player, POW_WHO) &&
     *(atr_get(0, A_DOING))) {
    notify(player, "Sorry, the Quote of the Day cannot be changed for %s.",
           time_format(TMF, poll_count-now));
    return;
  } if(!power(player, POW_WHO) && Typeof(player) != TYPE_PLAYER) {
    notify(player, "Only players may change the Quote of the Day.");
    return;
  }

  /* lock poll from changing for 15 minutes */
  if(player != poll_who) {
    poll_count = now+900;
    poll_who = player;
  }

  atr_add(0, A_DOING, pure);
  notify(player, "Quote of the Day %sed.", *pure?"chang":"clear");

  /* log the unworthy */
  if(!power(player, POW_WHO))
    log_main("Poll change by %s(#%d): %s", db[player].name, player, pure);
}

/* 'touch' an object, updating its modification time */
void do_update(dbref player, char *arg1)
{
  dbref thing;

  /* check permissions */
  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* update time */
  db[thing].mod_time = now;
  if(!Quiet(player))
    notify(player, "%s - Updated.", db[thing].name);
}

/* set dimensional-plane value on an object */
void do_plane(dbref player, char *arg1, char *arg2)
{
  dbref thing;

  /* check permissions */
  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  } if(Typeof(thing) == TYPE_ROOM || Typeof(thing) == TYPE_ZONE) {
    notify(player, "%ss cannot have their plane set.",
           typenames[Typeof(thing)]);
    return;
  } if(Typeof(db[thing].location) != TYPE_ROOM) {
    notify(player, "Only objects in a room can have a plane set.");
    return;
  }

  db[thing].plane=*arg2 ? atoi(arg2) : (Typeof(thing) == TYPE_EXIT ? -1 : 0);
  if(!Quiet(player))
    notify(player, "%s - %s.", db[thing].name, db[thing].plane ==
           (Typeof(thing) == TYPE_EXIT ? -1 : 0) ? "Reset":"Set");
}
