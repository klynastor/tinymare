/* db/destroy.c */

#include "externs.h"

/* free list statistics */
int ndisrooms;
int treedepth;

dbref next_free;
dbref *freelist;

long next_going;
struct goingobj {
  struct goingobj *next;
  dbref obj;
  long when;
} *goinglist;


/* clears an object's contents and recycles it for later use */
void do_destroy(player, arg1, arg2, argv, pure, cause, direct)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
int direct;
{
  struct goingobj *ptr=NULL;
  long delay=now;
  dbref thing;
  
  /* check permissions */
  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY) && (Typeof(thing) != TYPE_THING ||
     db[thing].location != player || !(db[thing].flags & THING_DESTROY_OK))) {
    notify(player, "Permission denied.");
    return;
  } if(thing < 2) {
    notify(player, "Don't you think that's sorta an odd thing to destroy?");
    return;
  } if(Typeof(thing) == TYPE_PLAYER) {
    notify(player, "Destroying players isn't allowed. Try @nuke instead.");
    return;
  }

  /* find object in Going list */
  if(db[thing].flags & GOING)
    for(ptr=goinglist;ptr;ptr=ptr->next)
      if(ptr->obj == thing)
        break;

  /* check if object is already set Going */
  if(!*arg2 && ptr) {
    notify(player, "%s is already scheduled for destruction in %s.",
           unparse_object(player, thing), time_format(TMF, ptr->when-now));
    return;
  }

  /* set destruction time (in seconds) */
  if(*arg2) {
    if((delay=get_date(arg2, player, now)) == -1) {
      notify(player, "Invalid time format string.");
      return;
    } if(delay > now+604800) {
      notify(player, "Sorry, the maximum delay for destruction is 1 week.");
      return;
    }
  } else {
    /* rooms and zones have extended periods of time just in case */
    if(!power(player, POW_SECURITY) && db[player].owner != db[thing].owner &&
       !IS(thing, TYPE_THING, THING_DESTROY_OK)) {
      notify(player, "Warning: You do not own that object.");
      delay+=600;
    } else if(Typeof(thing) == TYPE_ROOM || Typeof(thing) == TYPE_ZONE) {
      delay+=600;
    } else if(db[thing].children) {
      notify(player, "Warning: It has children.");
      delay+=60;
    } else if(!IS(thing, TYPE_THING, THING_DESTROY_OK) && direct &&
              terminfo(player, CONF_SAFE))
      delay+=60;
  }
  
  /* print destruction message.. */
  if(delay > now) {
    if(!ptr) {
      ptr=malloc(sizeof(struct goingobj));
      ptr->next=goinglist;
      ptr->obj=thing;
      goinglist=ptr;
    }
    ptr->when=delay;
    if(!next_going || delay < next_going)
      next_going=delay;

    db[thing].flags |= GOING;
    if(Typeof(thing) == TYPE_ROOM)
      notify_except(thing, NOTHING, "The room shakes and begins to crumble.");
    notify(player, "%s is now scheduled for destruction in %s.",
           unparse_object(player, thing), time_format(TMF, delay-now));
    return;
  }

  /* If executing queue immediately, destroy object at next chance */
  if(immediate_mode)
    parse_que(player, "@destroy %#", thing);
  else {
    /* ..or immediately destroy object */
    notify(player, "%s has been destroyed.", unparse_object(player, thing));
    recycle(thing);
    validate_links();
  }
}

/* undestroy an object already scheduled for destruction */
void do_undestroy(dbref player, char *arg1)
{
  struct goingobj *last=NULL, *ptr;
  dbref thing;
  
  if((thing=match_controlled(player, arg1, POW_MODIFY)) == NOTHING)
    return;
  if(!(db[thing].flags & GOING)) {
    notify(player, "Sorry, that object is not scheduled for destruction.");
    return;
  }

  /* remove destruction schedule from going-list */
  for(ptr=goinglist;ptr;ptr=ptr->next) {
    if(ptr->obj == thing) {
      if(last)
        last->next=ptr->next;
      else
        goinglist=ptr->next;
      free(ptr);
      break;
    }
    last=ptr;
  }

  db[thing].flags &= ~GOING;
  notify(player, "%s has been saved from destruction.",
         unparse_object(player, thing));
}

// Send contents of object home & destroy its exits. Clear object completely.
//
// WARNING: validate_links() MUST be called after all recycle() functions
// are processed and before any other command is executed.
//
void recycle(dbref thing)
{
  struct goingobj *dest;
  ATRDEF *k, *k2;
  ALIST *atr, *atr2;
  dbref i, next, *ptr;
  
  speaker=thing;

  /* Mask object in destruction list */
  for(dest=goinglist;dest;dest=dest->next)
    if(dest->obj == thing) {
      dest->obj=NOTHING;
      break;
    }

  /* Display room messages */
  if(Typeof(thing) == TYPE_ROOM) {
    notify_except(thing,NOTHING,"The floor disappears beneath your feet..!");
    notify_except(thing,NOTHING,"It is the end of the world.. as we know it.");
  }

  /* First, clear all commands in the object's queue */
  db[thing].flags &= ~PUPPET;
  db[thing].flags |= HAVEN|GOING;
  halt_object(thing, NULL, 0);

  if(Typeof(thing) == TYPE_ZONE) {
    /* Remove <thing> from the room's zone list */
    for(ptr=db[thing].zone;ptr && *ptr != NOTHING;ptr++)
      PULL(db[*ptr].zone, thing);

    /* Remove object from global list of Universal Zones */
    if(db[thing].flags & ZONE_UNIVERSAL)
      PULL(uzo, thing);
  } else if(Typeof(thing) == TYPE_ROOM) {
    /* Remove <thing> from the zone's inzone list */
    for(ptr=db[thing].zone;ptr && *ptr != NOTHING;ptr++)
      PULL(db[*ptr].zone, thing);
  }

  /* When destroying a special object, switch to room #0 */
  if(thing == PLAYER_START)
    PLAYER_START=0;
  if(thing == GUEST_START)
    GUEST_START=0;
  if(thing == GAME_DRIVER)
    GAME_DRIVER=0;
  if(thing == DUNGEON_CELL)
    DUNGEON_CELL=0;

  /* Clear out exits in room */
  while(db[thing].exits != NOTHING)
    recycle(db[thing].exits);

  /* Clear out contents */
  for(i=db[thing].contents;i != NOTHING;i=next) {
    next=db[i].next;
    if(i == thing)
      continue;

    if(db[i].link == thing || Typeof(db[i].link) == TYPE_GARBAGE)
      db[i].link=(Typeof(i) == TYPE_PLAYER)?PLAYER_START:db[i].owner;

    if(db[thing].location != thing && db[thing].location != NOTHING &&
       (Typeof(i) != TYPE_PLAYER || Typeof(db[thing].location) != TYPE_PLAYER))
      enter_room(i, db[thing].location);
    else {
      enter_room(i, HOME);
    }
  }

  /* Move object to location NOTHING */
  if(db[thing].location != NOTHING && Typeof(thing) != TYPE_ROOM &&
     Typeof(thing) != TYPE_ZONE) {
    if(!game_online && nstat[NS_EXEC] == 1) {
      if(Typeof(thing) == TYPE_EXIT)
        db[db[thing].location].exits =
             remove_first(db[db[thing].location].exits, thing);
      else
        db[db[thing].location].contents =
             remove_first(db[db[thing].location].contents, thing);
    } else
      enter_room(thing, NOTHING);
  }

  /* Clear object-type-dependent data */
  if(Typeof(thing) == TYPE_PLAYER)
    free(db[thing].data);
  db[thing].data=NULL;

  /* Decrement flag counts */
  if(IS(thing, TYPE_THING, THING_WANDER))
    wander_count--;
  /* Now let's clean up the object completely. */

  WPTR(db[thing].name, "");
  db[thing].owner=GOD;
  db[thing].flags=TYPE_GARBAGE;
  db[thing].plane=0;
  db[thing].location=NOTHING;
  db[thing].link=NOTHING;
  db[thing].exits=NOTHING;
  db[thing].contents=NOTHING;

  /* Clear attributes and attribute definitions */
  for(atr=db[thing].attrs;atr;atr=atr2) {
    atr2=atr->next;
    free(atr);
    db_attrs--;
  }
  for(k=db[thing].atrdefs;k;k=k2) {
    k2=k->next;
    free(k->atr.name);
    free(k);
  }
  db[thing].attrs=NULL;
  db[thing].atrdefs=NULL;

  for(ptr=db[thing].children;ptr && *ptr != NOTHING;ptr++) {
    atr_clean(*ptr, thing, -1);
    PULL(db[*ptr].parents, thing);
  }
  for(ptr=db[thing].parents;ptr && *ptr != NOTHING;ptr++)
    PULL(db[*ptr].children, thing);

  CLEAR(db[thing].children);
  CLEAR(db[thing].parents);
  CLEAR(db[thing].zone);

  /* Set the marker to the next object# to create */
  if(thing < next_free)
    next_free=thing;
}

/* Check all links for validity, destroy unlinked exits. */
void validate_links()
{
  int i;

  for(i=0;i<db_top;i++)
    if(!Invalid(db[i].link) && Typeof(db[i].link) == TYPE_GARBAGE) {
      if(Typeof(i) == TYPE_EXIT)
        recycle(i);
      else {
        db[i].link=db[db[i].owner].link;
        if(Typeof(db[i].link) == TYPE_GARBAGE)
          db[i].link=PLAYER_START;
      }
    }
}

/* Validate the consistency of all values stored in RAM */
int db_check()
{
  int amt=0, i;

  for(i=0;i<db_top;i++) {
    log_dbref=i;
    amt+=mem_usage(i);
  }

  log_dbref=NOTHING;
  return amt;
}

/* Returns the #dbref of the next object number to be created */
dbref get_upfront()
{
  if(freelist)
    return *freelist;

  while(next_free < db_top && Typeof(next_free) != TYPE_GARBAGE)
    next_free++;

  return next_free;
}

/* Moves an object# to the top of the free list */
void do_upfront(dbref player, char *arg1)
{
  dbref thing;

  if(Guest(player)) {
    notify(player, "Guests cannot use that command.");
    return;
  }
  
  if((thing=match_thing(player, arg1, MATCH_ABS|MATCH_DESTROYED)) == NOTHING)
    return;
  if(Typeof(thing) != TYPE_GARBAGE) {
    notify(player, "That object does not exist in the free list.");
    return;
  } if(freelist && *freelist == thing) {
    notify(player, "That object is already at the top of the free list.");
    return;
  }

  PULL(freelist, thing);
  PUSH_F(freelist, thing);
  notify(player, "Pushed #%d to the front.", thing);
}

/* Return a recycled (@destroyed) object off the free list, or NOTHING */
dbref free_get()
{
  dbref newobj;

  /* Search freelist for specific objects via @upfront */
  while(freelist) {
    newobj=*freelist;
    PULL(freelist, newobj);

    if(Typeof(newobj) == TYPE_GARBAGE)
      return newobj;
  }

  /* search database for destroyed objects */
  while(next_free < db_top) {
    if(Typeof(next_free) == TYPE_GARBAGE)
      return next_free++;
    next_free++;
  }

  return NOTHING;
}

/* Purge "Going" objects whose timers have expired */
void purge_going_objs()
{
  struct goingobj *ptr, *next, *last=NULL;
  int ndestroyed=0;
  dbref thing;

  if(!game_online) {
    /* Purge all objects previously scheduled for destruction */
    for(thing=0;thing<db_top;thing++)
      if(db[thing].flags & GOING) {
        recycle(thing);
        ++ndestroyed;
      }
  } else {
    /* See if we need to destroy anything */
    if(!next_going || next_going > now)
      return;
    next_going=0;

    /* Destroy objects scheduled for destruction */
    for(ptr=goinglist;ptr;ptr=next) {
      next=ptr->next;
      if(now >= ptr->when) {
        if(ptr->obj != NOTHING) {
          recycle(ptr->obj);
          ++ndestroyed;
        }
        free(ptr);
        if(last)
          last->next=next;
        else
          goinglist=next;
      } else {
        last=ptr;
        if(!next_going || ptr->when < next_going)
          next_going=ptr->when;
      }
    }
  }

  if(ndestroyed)
    validate_links();
}

/* Called at game startup before any queue commands */
void destroy_old_objs()
{
  dbref thing;

  /* Destroy unsaved objects and unlinked exits */
  if(nstat[NS_EXEC] == 1 || (boot_flags & BOOT_VALIDATE))
    for(thing=0;thing<db_top;thing++)
      if((Typeof(thing) == TYPE_EXIT && db[thing].link == NOTHING) ||
         (IS(thing, TYPE_THING, THING_UNSAVED) &&
          Typeof(db[thing].location) != TYPE_PLAYER))
        db[thing].flags |= GOING;

  purge_going_objs();
  report_disconnected_rooms(NOTHING);
}

/* Stack-based check for disconnected rooms */
static void dbmark(dbref loc)
{
  INIT_STACK;
  dbref exit, link;
  int depth=0;
  char *s;

  if(Invalid(loc) || Typeof(loc) != TYPE_ROOM || (db[loc].flags & ROOM_MARKED))
    return;

  /* Initial room entry */
  db[loc].flags |= ROOM_MARKED;
  exit=db[loc].exits;

  /* Enter tree search */
  while(1) {
    if(exit == NOTHING) {
      if(!depth--)
        break;
      POP(exit);
      continue;
    }

    /* New exit to check */
    if(Typeof(exit) == TYPE_EXIT)
      link=db[exit].link;
    else {
      link=exit;
      exit=AMBIGUOUS;
    }

    if(link == AMBIGUOUS) {
      PUSH(db[exit].next);
      s=atr_get(exit, A_VLINK);
      while((s=strchr(s, '#')))
        if(isdigit(*++s) && !Invalid(link=atoi(s)) &&
           Typeof(link) == TYPE_ROOM && !(db[link].flags & ROOM_MARKED) &&
           can_variable_link(exit, link))
          PUSH(link);
      ++depth;
      exit=NOTHING;
      continue;
    }

    if(exit != AMBIGUOUS)
      exit=db[exit].next;
    if(Invalid(link) || Typeof(link) != TYPE_ROOM ||
       (db[link].flags & ROOM_MARKED)) {
      if(exit == AMBIGUOUS) {
        exit=NOTHING;
        ++depth;
      }
      continue;
    }

    if(depth >= treedepth)
      treedepth=depth+1;
    db[link].flags |= ROOM_MARKED;
    if(exit != AMBIGUOUS)
      PUSH(exit);
    ++depth;
    exit=db[link].exits;
  }
}

/* Report all disconnected rooms */
int report_disconnected_rooms(dbref player)
{
  dbref thing;

  ndisrooms=0;
  treedepth=0;

  /* First, mark all rooms connected to PLAYER_START and #0 */
  dbmark(PLAYER_START);
  if(PLAYER_START != 0)
    dbmark(0);

  /* Next, mark all floating rooms and exits from them */
  for(thing=0;thing<db_top;thing++)
    if(IS(thing, TYPE_ROOM, ROOM_FLOATING))
      dbmark(thing);

  /* Now, inform owners of disconnected rooms */
  for(thing=0;thing<db_top;thing++) {
    if(Typeof(thing) != TYPE_ROOM)
      continue;

    if(db[thing].flags & ROOM_MARKED)
      db[thing].flags &= ~ROOM_MARKED;
    else if(!(db[thing].flags & GOING)) {
      if(game_online) {
        if(player != NOTHING) {
          if(!ndisrooms) {
            notify(player, "Disconnected Rooms:");
            notify(player, "%s", "");
          }
          notify(player, "%s  [owner: %s]",
                 unparse_object(player, thing),
                 unparse_object(player, db[thing].owner));
        } else
          notify(db[thing].owner, "You own a disconnected room, %s(#%d)",
                 db[thing].name, thing);
      }
      ++ndisrooms;
    }
  }

  return ndisrooms;
}
