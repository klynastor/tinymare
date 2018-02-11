/* comm/create.c */
/* commands that create new objects */

#include "externs.h"

int clone_depth;  /* Protection from @aclone recursion */


// Checks permissions to allow if <player> can link <thing> to <room>.
// Returns 1 if successful.
//
static int can_link(dbref player, dbref thing, dbref room)
{
  dbref *ptr;

  /* Check powers */
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return 0;
  } if(Guest(player)) {
    notify(player, "Guests cannot @link anything.");
    return 0;
  } if(RESTRICT_TELEPORT && !Builder(player)) {
    if(Typeof(thing) == TYPE_THING && !controls(player, room, POW_MODIFY)) {
      notify(player, "Only authorized builders can link objects to unowned "
             "rooms.");
      return 0;
    } if(Typeof(thing) == TYPE_ROOM) {
      notify(player, "Only authorized builders may set droptos on rooms.");
      return 0;
    } if(room == HOME) {
      notify(player, "Only authorized builders may link to Home.");
      return 0;
    } if(Typeof(thing) != TYPE_EXIT && mainroom(thing) != mainroom(room)) {
      notify(player, "You can only link to rooms and objects nearby.");
      return 0;
    }
  } if(room == NOTHING)  /* Clearing a room's dropto */
    return 1;

  if(room == HOME) {
    if(Typeof(thing) != TYPE_EXIT && Typeof(thing) != TYPE_ROOM) {
      notify(player, "%s need a firm reference point to link to.",
             Typeof(thing) == TYPE_ZONE ? "Zones":"Objects");
      return 0;
    }
  } else if(room == AMBIGUOUS) {
    if(Typeof(thing) != TYPE_EXIT) {
      notify(player, "%ss cannot be variably linked.",
             typenames[Typeof(thing)]);
      return 0;
    }
  } else {
    if((Typeof(thing) == TYPE_THING && (Typeof(room) == TYPE_EXIT ||
       Typeof(room) == TYPE_ZONE)) || (Typeof(thing) != TYPE_THING &&
       Typeof(room) != TYPE_ROOM)) {
      notify(player, "Object type %s may not be linked to object type %s.",
             typenames[Typeof(thing)], typenames[Typeof(room)]);
      return 0;
    }

    /* Check standard flags */
    if(!controls(player, room, POW_MODIFY) &&
      ((Typeof(thing) != TYPE_PLAYER && !(db[room].flags & ROOM_LINK_OK)) ||
       (Typeof(thing) == TYPE_PLAYER && !(db[room].flags & ROOM_ABODE)))) {
      notify(player, "Permission denied.");
      return 0;
    }

    /* Check room lock */
    if(Typeof(thing) != TYPE_ZONE && !could_doit(player, room, A_LLINK)) {
      notify(player, "Permission denied.");
      return 0;
    }

    /* Check restricted zone */
    if(Typeof(thing) == TYPE_EXIT) {
      for(ptr=db[db[thing].location].zone;ptr && *ptr != NOTHING;ptr++)
        if(!controls(player, *ptr, POW_MODIFY) &&
           !inlist(db[room].zone, *ptr)) {
          notify(player, "Cannot link into a restricted zone.");
          return 0;
        }
      for(ptr=db[room].zone;ptr && *ptr != NOTHING;ptr++)
        if(!controls(player, *ptr, POW_MODIFY) &&
           !inlist(db[db[thing].location].zone, *ptr)) {
          notify(player, "Cannot link from a restricted zone.");
          return 0;
        }
    }
  }

  if(Typeof(thing) == TYPE_EXIT) {
    /* Linking exits is just like opening an exit. check builder permissions */
    if(RESTRICT_BUILD && !Builder(player)) {
      notify(player, "Only authorized builders may link exits.");
      return 0;
    }

    /* Linking exits requires a small fee */
    if(!power(player, POW_FREE) && Typeof(thing) == TYPE_EXIT &&
       db[thing].link == NOTHING && !charge_money(player, LINK_COST, 0)) {
      notify(player, "It costs %d %s to link an exit.",
             LINK_COST, (LINK_COST==1)?MONEY_SINGULAR:MONEY_PLURAL);
      return 0;
    }
  }

  return 1;
}

/* links an object's home or an exit's destination */
void do_link(dbref player, char *arg1, char *arg2)
{
  dbref thing, room=NOTHING;

  /* check restrictions */
  if((thing = match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(Typeof(thing) != TYPE_ROOM || *arg2)
    if((room=match_thing(player, arg2, MATCH_EVERYTHING|MATCH_HOME|
       MATCH_VARIABLE)) == NOTHING)
      return;
  if(!can_link(player, thing, room))
    return;

  /* set the link */
  if(Typeof(thing) == TYPE_ROOM) {
    if(room == thing)
      room=NOTHING;
    if(db[thing].link == NOTHING && room == NOTHING) {
      notify(player, "Dropto already unlinked.");
      return;
    }
  }
  db[thing].link = room;

  /* display success message */
  if(Typeof(thing) == TYPE_ZONE)
    notify(player, "Origin Room set to %s.", unparse_object(player, room));
  else if(Typeof(thing) == TYPE_ROOM) {
    if(room == NOTHING)
      notify(player, "Dropto removed.");
    else
      notify(player, "Dropto set to %s.", unparse_object(player, room));
  } else if(room == AMBIGUOUS)
    notify(player, "%s is now pointing to a variable destination.",
           db[thing].name);
  else
    notify(player, "%s linked to %s.", db[thing].name,
           unparse_object(player, room));
}

// Determines if <exit> has permissions to link to <room> on the fly.
// <room> must be of type TYPE_ROOM.
//
int can_variable_link(dbref exit, dbref room)
{
  dbref loc=mainroom(exit), *ptr;

  /* Check standard flags and link-lock */
  if(Typeof(loc) != TYPE_ROOM || (!controls(exit, room, POW_MODIFY) && 
     (!(db[room].flags & ROOM_LINK_OK) || !could_doit(exit, room, A_LLINK))))
    return 0;

  /* Check restricted zones */
  for(ptr=db[loc].zone;ptr && *ptr != NOTHING;ptr++)
    if(!controls(exit, *ptr, POW_MODIFY) &&
       !inlist(db[room].zone, *ptr))
      return 0;  /* Linking into a restricted zone */

  for(ptr=db[room].zone;ptr && *ptr != NOTHING;ptr++)
    if(!controls(exit, *ptr, POW_MODIFY) &&
       !inlist(db[loc].zone, *ptr))
      return 0;  /* Linking from a restricted zone */

  return 1;  /* Link successful */
}

// Resolves a variable link on <exit> based on exit owner's permissions.
//
dbref resolve_link(dbref player, dbref exit, int *plane)
{
  char buf[8192], dest[8192], *s;
  int temp_plane=-1;
  dbref room;

  if(plane)
    *plane=-1;  /* No plane change by default */

  /* @vlink function must return a #dbref */
  strcpy(buf, atr_get(exit, A_VLINK));
  pronoun_substitute(dest, exit, player, buf);
  if(*dest != '#' || !isdigit(*(dest+1)))
    return NOTHING;

  room=atoi(dest+1);
  if(plane && (s=strchr(dest, ':')) && (isdigit(*(s+1)) || *(s+1) == '-') &&
     power(exit, POW_PLANE))
    temp_plane=atoi(s+1);

  /* Check destination type */
  if(Invalid(room) || Typeof(room) != TYPE_ROOM)
    return NOTHING;

  /* Check link permissions */
  if(!can_variable_link(exit, room))
    return NOTHING;

  if(plane)
    *plane=temp_plane;
  return room;
}

/* creates exit in loc. if destination != nothing, links it for a fee */
static int open_exit(dbref player, char *name, dbref loc, dbref dest)
{
  char *p, *s, *alias=NULL;
  dbref exit;
  
  if(RESTRICT_BUILD && !Builder(player)) {
    notify(player, "Only authorized builders may create exits.");
    return 0;
  } if(Guest(player)) {
    notify(player, "Guests cannot open exits.");
    return 0;
  } if(Invalid(loc) || Typeof(loc) != TYPE_ROOM) {
    notify(player, "An exit is not allowed from %s.", db[loc].name);
    return 0;
  } if(!*name) {
    notify(player, "You must specify a name for the exit.");
    return 0;
  } if(!controls(player, loc, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return 0;
  }

  /* check exit name */
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
    notify(player, "That is an unacceptable exit name.");
    return 0;
  } if(!charge_money(player, EXIT_COST, 1))
    return 0;

  if((exit=new_object()) == NOTHING) {
    notify(player, "The database has reached the maximum number of objects. "
           "Please @destroy something first.");
    return 0;
  }

  /* initialize everything */
  WPTR(db[exit].name, name);
  if(alias)
    atr_add(exit, A_ALIAS, alias);
  db[exit].owner = db[player].owner;
  db[exit].flags = TYPE_EXIT;
  db[exit].plane = -1;
  db[exit].create_time = now;
    
  /* add exit to roomlist */
  ADDLIST(exit, db[loc].exits);
  db[exit].location = loc;
  db[exit].link = NOTHING;

  /* check to see if we should link to destination */
  if(dest != NOTHING && can_link(player, exit, dest)) {
    db[exit].link = dest;
    if(dest == AMBIGUOUS)
      notify(player, "%s opened and variably linked.", db[exit].name);
    else
      notify(player, "%s opened; linked to %s.", db[exit].name,
             unparse_object(player, dest));
    return 1;
  }

  /* exit unsuccessfully set a destination */
  notify(player, "%s opened.", db[exit].name);
  return 0;
}

// Creates a new room. Possible arguments argv[1] and argv[2] may contain a
// destination exit and return exit, respectively.
//
void do_dig(dbref player, char *name, char *argc, char **argv)
{
  dbref room, *ptr;
  int cost;
  char *s;

  /* Check building permissions */
  if(RESTRICT_BUILD && !Builder(player)) {
    notify(player, "Only authorized builders may dig rooms.");
    return;
  } if(Guest(player)) {
    notify(player, "Guests cannot dig rooms.");
    return;
  } if(!Builder(player) && (!controls(player, db[player].location,
       POW_MODIFY) || Typeof(db[player].location) != TYPE_ROOM)) {
    notify(player, "You can't @dig from there.");
    return;
  } if(!*name) {
    notify(player, "You must specify a name for the room.");
    return;
  }
  
  /* Room names cannot have aliases */
  while(*name == ' ')
    name++;
  if((s=strchr(name, ';')))
    *s='\0';
  else
    s=strnul(name);
  while(s > name && *(s-1) == ' ')
    *--s='\0';

  if(!ok_name(name)) {
    notify(player, "That is an unacceptable room name.");
    return;
  }

  /* Check cost of exits */
  cost=ROOM_COST + ((argv[1] && *argv[1])?(EXIT_COST+LINK_COST):0) +
                   ((argv[2] && *argv[2])?(EXIT_COST+LINK_COST):0);
  if(!charge_money(player, cost, 2) || !charge_money(player, ROOM_COST, 1))
    return;

  if((room=new_object()) == NOTHING) {
    notify(player, "The database has reached the maximum number of objects. "
           "Please @destroy something first.");
    return;
  }

  /* Initialize everything */
  WPTR(db[room].name, name);
  db[room].owner = db[player].owner;
  db[room].flags = TYPE_ROOM;
  db[room].location = room;
  db[room].create_time = now;
  atr_add(room, A_FLOOR, atr_get(db[player].location, A_FLOOR));
  atr_add(room, A_COLOR, atr_get(db[player].location, A_COLOR));
  atr_add(room, A_WEATHER, atr_get(db[player].location, A_WEATHER));

  notify(player, "%s created with room number #%d.", name, room);

  /* Copy zones */
  for(ptr=db[db[player].location].zone;ptr && *ptr != NOTHING;ptr++) {
    PUSH_L(db[room].zone, *ptr);
    PUSH_L(db[*ptr].zone, room);
  }

  /* Link proper exits if specified and if enough funds are present */
  if(argv[1] && *argv[1] &&
     open_exit(player, argv[1], db[player].location, room))
    if(argv[2] && *argv[2])
      open_exit(player, argv[2], room, db[player].location);
}

/* use this to create/link an exit */
void do_open(dbref player, char *arg1, char *arg2)
{
  dbref dest=NOTHING;

  if(*arg2 && (dest=match_thing(player, arg2, MATCH_EVERYTHING|MATCH_HOME|
     MATCH_VARIABLE)) == NOTHING)
    return;

  open_exit(player, arg1, db[player].location, dest);
}

/* use this to create an object */
void do_create(dbref player, char *name)
{
  char *p, *s, *alias=NULL;
  dbref thing;
  
  /* check building permissions */
  if(Typeof(player) == TYPE_ZONE) {
    notify(player, "Zones cannot create objects.");
    return;
  } if(RESTRICT_BUILD && !Builder(player)) {
    notify(player, "Only authorized builders may create objects.");
    return;
  } if(Guest(player)) {
    notify(player, "Guests cannot create objects.");
    return;
  } if(!*name) {
    notify(player, "You must specify a name for the object.");
    return;
  }

  /* check object name */
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
    notify(player, "That is an unacceptable object name.");
    return;
  } if(!charge_money(player, THING_COST, 1))
    return;

  if((thing=new_object()) == NOTHING) {
    notify(player, "The database has reached the maximum number of objects. "
           "Please @destroy something first.");
    return;
  }

  /* initialize everything */
  WPTR(db[thing].name, name);
  if(alias)
    atr_add(thing, A_ALIAS, alias);
  db[thing].owner = db[player].owner;
  db[thing].flags = TYPE_THING;
  db[thing].plane = -1;
  db[thing].create_time = now;

  /* move object to player's inventory */
  db[thing].location=(Typeof(player) == TYPE_EXIT)?db[player].location:player;
  ADDLIST(thing, db[db[thing].location].contents);

  /* set a default link */
  if(Typeof(db[player].location) == TYPE_ROOM &&
     controls(player, db[player].location, POW_MODIFY))
    db[thing].link = db[player].location;
  else
    db[thing].link = player;

  notify(player, "%s created.", unparse_object(player, thing));
}

void do_clone(dbref player, char *arg1, char *name)
{
  char *p, *s, *alias=NULL;
  dbref thing, clone, *ptr;
  
  if(clone_depth) {
    notify(player, "Recursive @clone detected.");
    return;
  }

  /* Check building permissions */
  if(RESTRICT_BUILD && !Builder(player)) {
    notify(player, "Only authorized builders may clone objects.");
    return;
  } if(Guest(player)) {
    notify(player, "Guests cannot clone objects.");
    return;
  } if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING) {
    return;
  } if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  } if(Typeof(thing) != TYPE_THING) {
    notify(player, "You can only clone objects.");
    return;
  }
  
  /* Check for a valid newname */
  if(*name) {
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
      notify(player, "That is an unacceptable object name.");
      return;
    }
  } else
    name=db[thing].name;

  if(!charge_money(player, THING_COST, 1))
    return;

  if((clone=new_object()) == NOTHING) {
    notify(player, "The database has reached the maximum number of objects.\n"
           "Please @destroy something first.");
    return;
  }

  WPTR(db[clone].name, name);
  db[clone].owner = db[player].owner;
  db[clone].create_time = now;
  db[clone].flags = db[thing].flags;
  db[clone].plane = db[thing].plane;

  /* Fix up parents/children list */
  for(ptr=db[thing].parents;ptr && *ptr != NOTHING;ptr++) {
    if(db[*ptr].flags & BEARING || controls(player, *ptr, POW_MODIFY)) {
      PUSH_L(db[clone].parents, *ptr);
      PUSH_L(db[*ptr].children, clone);
    }
  }

  atr_cpy(clone, thing);

  /* Replace alias if specified */
  if(alias)
    atr_add(clone, A_ALIAS, alias);
    
  notify(player, "%s cloned with number #%d.", unparse_object(player, thing),
         clone);

  /* Push object into the room's (or player's) contents */
  db[clone].location=(Typeof(player) == TYPE_EXIT) ? db[player].location:
     (Typeof(player) == TYPE_ZONE) ? db[thing].location:player;
  ADDLIST(clone, db[db[clone].location].contents);

  /* Set a default link */
  if(Typeof(db[player].location) == TYPE_ROOM && controls(player,
     db[player].location, POW_MODIFY))
    db[clone].link = db[player].location;
  else
    db[clone].link = player;

  /* Trigger enemy startup */
  if(*(s=atr_get(clone, A_ACLONE))) {
    clone_depth=1;
    immediate_que(clone, s, player);
    clone_depth=0;

    /* Exit if the object destroyed itself prematurely */
    if(Typeof(clone) == TYPE_GARBAGE)
      return;
  }

  /* Trigger @aoenter on other objects in room */
  room_trigger(clone, A_AOENTER);

  /* Run @enter messages on player */
  ansi_did_it(clone, db[clone].location, A_ENTER, NULL, A_OENTER,
              "has arrived.", A_AENTER);

  /* Display @oclone in player's inventory */
  did_it(clone, clone, 0, NULL, A_OCLONE, NULL, 0);
}

/* Creates a zone object */
void do_zone(dbref player, char *name, char *link)
{
  char *s;
  dbref zone, room;
  
  /* check building permissions */
  if(RESTRICT_BUILD && !Builder(player)) {
    notify(player, "Only authorized builders may create zones.");
    return;
  } if(Guest(player)) {
    notify(player, "Guests cannot create zones.");
    return;
  } if(!*name) {
    notify(player, "You must specify a name for the zone.");
    return;
  }
  
  /* zone names cannot have aliases */
  while(*name == ' ')
    name++;
  if((s=strchr(name, ';')))
    *s='\0';
  else
    s=strnul(name);
  while(s > name && *(s-1) == ' ')
    *--s='\0';

  if(!ok_name(name)) {
    notify(player, "That is an unacceptable zone name.");
    return;
  } if(!charge_money(player, ZONE_COST, 1))
    return;

  if((zone=new_object()) == NOTHING) {
    notify(player, "The database has reached the maximum number of objects. "
           "Please @destroy something first.");
    return;
  }

  /* initialize everything */
  WPTR(db[zone].name, name);
  db[zone].owner = db[player].owner;
  db[zone].location = zone;
  db[zone].flags = TYPE_ZONE;
  db[zone].create_time = now;

  notify(player, "Zone %s created.", unparse_object(player, zone));

  /* add origin toom if specified */
  if(*link) {
    if((room=match_thing(player, link, MATCH_EVERYTHING)) == NOTHING)
      return;
    if(!can_link(player, zone, room))
      return;

    db[zone].link = room;
    notify(player, "Origin Room set to %s.", unparse_object(player, room));
  }
}
