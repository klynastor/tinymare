/* comm/move.c */

#include "externs.h"

dbref run_exit=NOTHING;  // Bypass @lock checking after a random_exit() call.
int walk_verb;           // Forces 'walk' verb when run_exit is set.

int enter_plane=-1;

static void show_exitmsg(dbref player, dbref exit, dbref link, int type);
static void find_money(dbref player);


// Determines if a player can reach <location> (via move or @teleport) due to
// locks on zones. Also checks ZONE_RESTRICTED flag to allow players to
// @teleport within restricted zones but not cross them.
//
// Type=+1: Player is attempting to @teleport to the destination.
//      +2: Player is trying to reach Home.
//      +4: Don't display "You can't go that way." with random exit checks.
//
int can_move(dbref player, dbref thing, dbref loc, int type)
{
  dbref *zone, *zone2;

  /* Check leaving zones */
  for(zone=Zone(thing);zone && *zone != NOTHING;zone++) {
    for(zone2=Zone(loc);zone2 && *zone2 != NOTHING;zone2++)
      if(*zone == *zone2)
        break;
    if(!zone2 || *zone2 == NOTHING) {
      if(!could_doit(thing, *zone, A_LLOCK)) {
        if(!(type & 4))
          ansi_did_it(thing, *zone, A_LFAIL, "You can't go that way.",
                      A_OLFAIL, NULL, A_ALFAIL);
        return 0;
      }
      if((type & 1) && RESTRICT_TELEPORT && (db[*zone].flags&ZONE_RESTRICTED) &&
         !controls(thing, *zone, POW_TELEPORT) && !Builder(thing)) {
        notify(player, "Can't reach %s.", (type & 2)?"home":"destination");
        return 0;
      }
    }
  }

  /* Check entering zones */
  for(zone=Zone(loc);zone && *zone != NOTHING;zone++) {
    for(zone2=Zone(thing);zone2 && *zone2 != NOTHING;zone2++)
      if(*zone == *zone2)
        break;
    if(!zone2 || *zone2 == NOTHING) {
      if(!could_doit(thing, *zone, A_ELOCK)) {
        if(!(type & 4))
          ansi_did_it(thing, *zone, A_EFAIL, "You can't go that way.",
                      A_OEFAIL, NULL, A_AEFAIL);
        return 0;
      }
      if((type & 1) && RESTRICT_TELEPORT && (db[*zone].flags&ZONE_RESTRICTED) &&
         !controls(thing, *zone, POW_TELEPORT) && !Builder(thing)) {
        notify(player, "Can't reach %s.", (type & 2)?"home":"destination");
        return 0;
      }
    }
  }

  return 1;
}

// Move through an exit leading out of the room.
//
void do_move(dbref player, char *direction)
{
  dbref loc=db[player].location, exit, link;
  int plane=-1, lock=0;

  /* Check object type */
  if(Typeof(player) == TYPE_ROOM || Typeof(player) == TYPE_EXIT ||
     Typeof(player) == TYPE_ZONE) {
    notify(player, "%ss are not allowed to move.", typenames[Typeof(player)]);
    return;
  }

  if(RESTRICT_TELEPORT && !power(player,POW_TELEPORT) && hold_player(player)) {
    notify(player, "You can't move while carrying a player.");
    return;
  }

  /* First match all visible exits, then invisible ones */
  exit=match_thing(player, direction, MATCH_EXIT|MATCH_HOME|MATCH_QUIET);
  if(exit == NOTHING && (exit=match_thing(player, direction,
     MATCH_EXIT|MATCH_INVIS|MATCH_QUIET)) == NOTHING) {
    notify(player, "You can't go that way.");
    return;
  }

  /* Homeward bound? */
  if(exit == HOME) {
    if(loc == db[player].link) {
      notify(player, "You are already home.");
      return;
    }
    if(RESTRICT_TELEPORT && !Builder(player) && !power(player,POW_TELEPORT)) {
      if(!is_zone(player, ZONE_RESTRICTED)) {
        notify(player, "Yes, home.  How I long for the day "
               "when I can travel home.");
        return;
      }
      if(!can_move(player, player, db[player].link, 3))
        return;
    }

    /* Display messages */
    notify(player, "\e[1mThere's no place like home...\e[m");
    notify_except(player, player, "%s goes home.", db[player].name);
    enter_room(player, HOME);
    return;
  }

  /* Check current location's leave-lock */
  if(!could_doit(player, loc, A_LLOCK)) {
    ansi_did_it(player, loc, A_LFAIL, "You can't go that way.", A_OLFAIL, NULL,
                A_ALFAIL);
    return;
  }

  /* Resolve link on variable-destination exits */
  if(db[exit].link == AMBIGUOUS)
    link=resolve_link(player, exit, &plane);
  else
    link=db[exit].link;

  /* Check lock on exit; if `run_exit' is set, @lock is already secured */
  if(link == NOTHING || (exit != run_exit && !could_doit(player,exit,A_LOCK)))
    lock=1;

  /* Objects can only move to owned rooms */
  if(!lock && RESTRICT_TELEPORT && !Builder(player) && link != HOME &&
     Typeof(player) == TYPE_THING && !controls(player, link, POW_MODIFY))
    lock=1;

#if 0
  /* In a restricted zone, players can only go through exits that have
     a return exit pointing back to the source room */
  if(!lock && !Builder(player) && is_zone(exit, ZONE_RESTRICTED) &&
     link != HOME && !controls(player, exit, POW_MODIFY)) {
    DOLIST(a, db[link].exits)
      if(db[a].link == loc)
        break;
    if(a == NOTHING)
      lock=1;
  }
#endif

  /* Locked */
  if(lock) {
    ansi_did_it(player, exit, A_FAIL, "You can't go that way.",
                A_OFAIL, NULL, A_AFAIL);
    return;
  }

  if(link != HOME) {
    /* Check if you can pass through zones */
    if(!can_move(player, player, link, 0))
      return;

    /* Check destination's enter-lock */
    if(!could_doit(player, link, A_ELOCK)) {
      ansi_did_it(player, link, A_EFAIL, "You can't go that way.",
                  A_OEFAIL, NULL, A_AEFAIL);
      return;
    }
  }

  show_exitmsg(player, exit, link, 0);

  enter_plane=plane;  /* Plane to move to */
  enter_room(player, link);
  show_exitmsg(player, exit, link, 1);

}

// Internal function that moves an object or exit to a new location.
// <player>=Object to move, <loc>=Destination room
// Also takes the global 'enter_plane' as input, if not -1.
//
void enter_room(dbref player, dbref loc)
{
  dbref old=db[player].location, *ptr, prev=speaker;
  int invis=0, dead=0, money=1;

  speaker=player;

  /* Check object validity */
  if(Typeof(player) == TYPE_ROOM || Typeof(player) == TYPE_ZONE) {
    enter_plane=-1;
    notify(player, "%ss are not allowed to move.", typenames[Typeof(player)]);
    speaker=prev;
    return;
  }

  /* Home defaults to the player's @link */
  if(loc == HOME) {
    loc=db[player].link;
  }

  /* When moving objects to the location they're already in.. */
  if(old == loc && (enter_plane == -1 || db[player].plane == enter_plane)) {
    enter_plane=-1;
    if(loc == NOTHING) {
      speaker=prev;
      return;
    }

    /* Move object to front of stack */
    if(Typeof(player) == TYPE_EXIT) {
      db[loc].exits = remove_first(db[loc].exits, player);
      ADDLIST(player, db[loc].exits);
    } else {
      db[loc].contents = remove_first(db[loc].contents, player);
      ADDLIST(player, db[loc].contents);
    }

    /* Look at the room */
    look_room(player, loc, 1);

    speaker=prev;
    return;
  }

  /* Check object & destination types */
  if(loc != NOTHING && (Typeof(loc) == TYPE_EXIT || Typeof(loc) == TYPE_ZONE ||
     (Typeof(player) == TYPE_PLAYER && Typeof(loc) == TYPE_PLAYER))) {
    log_error("Attempt to move %s %s(#%d) into %s %s(#%d)",
        Typeof(player)==TYPE_PLAYER?"player":"object", db[player].name, player,
        typenames[Typeof(loc)], db[loc].name, loc);
    notify(player, "Cannot move to %s.", Typeof(loc)==TYPE_PLAYER ?
           "player":Typeof(loc)==TYPE_EXIT?"exit":"zone");
    enter_plane=-1;
    speaker=prev;
    return;
  }

  /** Move is now guaranteed **/

  /* Update number of steps */
  if(loc != NOTHING && Typeof(player) == TYPE_PLAYER &&
     Typeof(loc) == TYPE_ROOM && !IS(loc, TYPE_ROOM, ROOM_XZONE))
    db[player].data->steps++;

  if(Typeof(player) == TYPE_PLAYER && !(db[player].flags & PLAYER_CONNECT))
    invis=1;

  if(old != NOTHING) {
    /* Remove object from old contents list */
    if(Typeof(player) == TYPE_EXIT)
      db[old].exits = remove_first(db[old].exits, player);
    else
      db[old].contents = remove_first(db[old].contents, player);

    ansi_did_it(player, player, A_MOVE, NULL, (invis || dead)?0:A_OMOVE,
                NULL, A_AMOVE);

    /* Trigger @aoleave on other objects in room */
    room_trigger(player, A_AOLEAVE);

    /* Run @leave messages on zones */
    for(ptr=Zone(old);ptr && *ptr != NOTHING;ptr++)
      if(loc == NOTHING || !inlist(Zone(loc), *ptr))
        ansi_did_it(player, *ptr, A_LEAVE, NULL, (invis || dead)?0:A_OLEAVE,
                    NULL, A_ALEAVE);

    /* Run @leave messages on room */
    ansi_did_it(player, old, A_LEAVE, NULL, (invis || dead)?0:A_OLEAVE,
                invis?NULL:dead?"has died.":"has left.", A_ALEAVE);
  }
  
  /* Switch planes */
  if(enter_plane != -1)
    db[player].plane=enter_plane;
  enter_plane=-1;

  /* Move object */
  db[player].location = loc;

  if(loc != NOTHING) {
    /* Put object into new contents list */
    if(Typeof(player) == TYPE_EXIT)
      ADDLIST(player, db[loc].exits);
    else
      ADDLIST(player, db[loc].contents);

    /* Trigger @aoenter on other objects in room */
    room_trigger(player, A_AOENTER);

    /* Run @enter messages on zones */
    for(ptr=Zone(loc);ptr && *ptr != NOTHING;ptr++)
      if(old == NOTHING || !inlist(Zone(old), *ptr)) {
        sprintf(env[0], "#%d", old);
        ansi_did_it(player,*ptr,A_ENTER,NULL,invis?0:A_OENTER,NULL,A_AENTER);
      }

    /* Run @enter messages on room */
    sprintf(env[0], "#%d", old);
    ansi_did_it(player, loc, A_ENTER, NULL, invis?0:A_OENTER,
                invis?NULL:dead?"arrives as a ghost.":"has arrived.",
                A_AENTER);
  }
  
  /* Autolook */
  look_room(player, loc, 1);

  /* Check if this is a valid room to find money */
  if(loc != NOTHING && Typeof(player) == TYPE_PLAYER &&
     !IS(loc, TYPE_ROOM, ROOM_XZONE)) {
    /* Penny check */
    if(!invis && money)
      find_money(player);

  }

  speaker=prev;
}

static void find_money(dbref player)
{
  int a, b, val;

  /* Check rate of finding money */
  if(rand_num(FIND_MONEY))
    return;
  /* Determine amount to reward based on inverse curve */
  if(MAX_DISCOVER < 1)
    return;
  if(MAX_DISCOVER > 10000)
    MAX_DISCOVER=10000;  /* Truncate variable down to acceptable limit */
  if(MAX_DISCOVER == 1)
    a=1;
  else {
    b=(MAX_DISCOVER*(MAX_DISCOVER+1))/2;
    if((val=rand_num(b+b/10)) >= b)
      a=0;
    else {
      for(a=0,b=0;b <= val;b+=a+1,a++);
      a=MAX_DISCOVER+1-a;
    }
  }

  if(a == 1) {
    notify_except(player, player, "%s finds a %s!", db[player].name,
                  MONEY_SINGULAR);
    notify(player, "You find a %s!", MONEY_SINGULAR);
  } else if(a > 1) {
    notify_except(player, player, "%s finds \e[1;32m%d\e[m %s on the ground!",
           db[player].name, a, MONEY_PLURAL);
    notify(player, "You find \e[1;32m%d\e[m %s on the ground!", a,
           MONEY_PLURAL);
  } else
    notify(player, "You thought you found a %s on the ground, "
           "but it turned out to be a pebble.", MONEY_SINGULAR);

  giveto(player, a);
}

/* Illusionate exit messages. This is fun. :-) */
static char *exitmsg(dbref player, dbref exit, dbref link, int type, int *verb)
{
  static char rslt[576];
  char buf[100], *p, *q=NULL, *r;
  int a;

  if(exit == NOTHING) {  /* In @odrop, if return exit not available */
    *verb=7;  /* come */
    sprintf(rslt, "from %s.", db[link].name);
    return rslt;
  }

  strmaxcpy(buf, db[exit].name, 100);
  if((p=strchr(buf, '<'))) {
    *p='\0';
    q=p+1;
    if((r=strchr(q, '>')))
      *r='\0';
  } else if((p=strchr(buf, '('))) {
    *p='\0';
    q=p+1;
    if((r=strchr(q, ')')))
      *r='\0';
  }

  /* Truncate end spacing and lowercase all letters */
  for(p=strnul(buf)-1;p >= buf;p--) {
    if(*p == ' ' && !*(p+1))
      *p=0;
    else
      *p=tolower(*p);
  }

  *verb=0;  /* walk */
  if(!*buf || Invalid(link))
    return "";

  if(match_word("north northeast east southeast south southwest west "
                       "northwest aft starboard port", buf)) {
    sprintf(rslt, "%s the %s", type?"in from":"to", buf);
    *verb=0|32; /* walk */
  } else if(match_word("up upstairs uphill upward upwards", buf)) {
    sprintf(rslt, "%s%s", type?"down":"up", buf+2);
    *verb=8|16|32;
  } else if(match_word("down downstairs downhill downward downwards", buf)) {
    sprintf(rslt, "%s%s", type?"up":"down", buf+4);
    *verb=8|16|32;
  } else if(match_word("climb above", buf)) {
    sprintf(rslt, "%s the room above", type?"down from":"up to");
    *verb=1|32; /* climb */
  } else if(match_word("below", buf)) {
    sprintf(rslt, "%s the room below", type?"up from":"down to");
    *verb=1|32;
  } else if((a=match_word("ladder ledges vine vines stairs stairway stairwell",
            buf))) {
    if(type) {
      sprintf(rslt, "off of the %s", buf);
      *verb=(a > 4)?8:9;
    } else {
      sprintf(rslt, "onto the %s", buf);
      *verb=(a > 4)?0:1;
    }
  } else if(match_word("forward", buf)) {
    strcpy(rslt, buf);
    *verb=8|32;
  } else if(match_word("hill hillside knoll", buf)) {
    sprintf(rslt, "along the %s", buf);
    *verb=8|32;
  } else if(match_word("down downstairs", buf)) {
    sprintf(rslt, "%s the room below", type?"up from":"down to");
    *verb=1|32;
  } else if(match_word("out outside leave exit", buf)) {
    strcpy(rslt, type?"from outside":"back outside");
    *verb=0;
  } else if(match_word("in inside enter entrance cabin house cottage shack "
            "hut shrine temple", buf)) {
    strcpy(rslt, type?"from inside":"inside");
    *verb=0|(type?0:32);
  } else if(match_word("back backward", buf)) {
    strcpy(rslt, type?"from ahead":buf);
    *verb=8|(type?0:32);
  } else if(match_word("ahead", buf)) {
    strcpy(rslt, type?"from behind":buf);
    *verb=8|(type?0:32);
  } else if(match_word("ledges ridges thorns woods forest", buf)) {
    sprintf(rslt, "through the %s", buf);
    *verb=8|16;
  } else if(match_word("tunnel cave cavern tunnels caverns", buf)) {
    strcpy(rslt, !*env[0]?"the cave":"");
    *verb=10|16; /* 2. follow */
  } else if(match_word("hole", buf)) {
    sprintf(rslt, "%s the %s", type?"from":"into", buf);
    *verb=9|(type?0:32);
  } else if(match_word("hall hallway steps trail path footpath walkway "
                       "passage branch branches", buf)) {
    sprintf(rslt, "the %s", buf);
    *verb=10|16;
  } else if(match_word("debris rockslide rockslides", buf)) {
    sprintf(rslt, "%s the %s", type?"from":"beyond", buf);
    *verb=0;
  } else if(match_word("jump", buf)) {
    sprintf(rslt, "down%s", type?" from above":"");
    *verb=11|32; /* 3. jump */
  } else if(match_word("ledge ridge", buf)) {
    sprintf(rslt, "%s the %s", type?"from":"to", buf);
    *verb=8;
  } else if(match_word("gate gates door doors doorway marsh", buf)) {
    sprintf(rslt, "%s the %s", type?"from":"through", buf);
    *verb=8;
  } else if(match_word("bridge road vine", buf)) {
    sprintf(rslt, "%s the %s", type?"from":"onto", buf);
    *verb=8;
  } else if(match_word("cliff cliffs beach shore river fire", buf)) {
    sprintf(rslt, "up by the %s", buf);
    *verb=8;
  } else if(match_word("lake", buf)) {
    strcpy(rslt, type?"in":"");
    *verb=12|16|32; /* 4. flow */
  } else if(match_word("crawl crawlspace shaft", buf)) {
    strcpy(rslt, type && !*env[0]?"in":"");
    *verb=13; /* 5. crawl */
  } else if(match_word("foyer", buf)) {
    sprintf(rslt, "%s the %s", type?"inside from":"back to", buf);
    *verb=0|32;
  } else if(strchr(buf, ' ')) {
    sprintf(rslt, "%s %s", type?"from":"into", buf);
    *verb=8;
  } else if((!q || !*q) && !Invalid(link)) {
    sprintf(rslt, "%s %s", type?"from":"into", db[link].name);
    *verb=0;
  } else {
    sprintf(rslt, "%s the %s", type?"from":"into", buf);
    *verb=8;
  }

  if(run_exit != NOTHING && !walk_verb)
    *verb=(*verb & ~7)|6;

  if(q && *q && (*verb & 8)) {
    static char *dirtable[]={
      "N", "north", "NE", "northeast", "E", "east", "SE", "southeast",
      "S", "south", "SW", "southwest", "W", "west", "NW", "northwest"};

    if((a=match_word("U D", q)))
      sprintf(rslt+strlen(rslt), "%s%s", *rslt?" ":"", type ?
              (a==1?"from above":"from below"):(a==1?"upward":"downward"));
    else {
      for(a=0;a < NELEM(dirtable);a+=2)
        if(!strcasecmp(q, dirtable[a])) {
          sprintf(rslt+strlen(rslt), "%s%s the %s", *rslt?" ":"",
                  type?(*verb & 16?"from":"in"):"to", dirtable[a+1]);
          break;
        }
    }
  }

  strcat(rslt, ".");
  return rslt;
}

// Displays the proper exit messages to everyone in <player>'s room.
// Uses the previous function, exitmsg(), to determine the sentence syntax.
//
// Type = 0: Show @succ/@osucc.
//        1: Show @odrop.
//
static void show_exitmsg(dbref player, dbref exit, dbref link, int type)
{
  static char *verbs[]={"walk", "climb", "follow", "jump",
                        "flow", "crawl", "run", "come"};

  char buf[576], succ[64], temp[64], *s, *msg;
  dbref oexit=NOTHING;
  int verb;

  /* Find return exit = <oexit>. Only needed for @odrop. */
  if(type && link != HOME)
    DOLIST(oexit, db[link].exits)
      if(db[oexit].link == db[exit].location || (db[oexit].link == AMBIGUOUS &&
         resolve_link(player, oexit, NULL) == db[exit].location))
        break;

  /* Get @succverb */
  msg=exitmsg(player, type?oexit:exit, type?db[exit].location:link,type,&verb);

  if((verb & 7) == 6)
    *succ='\0';
  else {
    sprintf(env[0], "%d", verb & 7);
    strmaxcpy(succ, atr_parse(player, player, A_SUCCVERB), 51);
    if((s=strchr(succ, '\n')))
      *s='\0';
  }
  if(*succ) {
    if((verb & 32) && (s=strchr(succ, ' ')))
      *s='\0';
  } else {
    if((verb & 7) == 6 && Typeof(player) != TYPE_PLAYER)
      strcpy(succ, "flee");
    else
      strcpy(succ, verbs[verb & 7]);
  }

  /* Display @succ/@drop */
  strcpy(env[0], succ);
  if(!type && *msg)
    sprintf(buf, "You %s %s", succ, msg);
  ansi_did_it(player, exit, type?A_DROP:A_SUCC, (!type && *msg)?buf:NULL,
              0, NULL, 0);

  /* Grab @osuccverb or @odropverb */
  sprintf(env[0], "%d", verb);
  if(*(s=atr_parse(player, player, type?A_ODROPVERB:A_OSUCCVERB))) {
    strmaxcpy(env[0], s, 51);
    if((s=strchr(env[0], '\n')))
      *s='\0';
  } else {
    /* Otherwise, pluralize @succverb */
    if((s=strchr(succ, ' '))) {
      strcpy(temp, s);
      *s='\0';
    } else
      *temp='\0';
    strcpy(env[0], pluralize(succ));
    strcat(env[0], temp);
  }

  /* Display @osucc */
  if(*msg)
    sprintf(buf, "%s %s", env[0], msg);
  ansi_did_it(player, exit, 0, NULL, type?A_ODROP:A_OSUCC, *msg?buf:NULL, 0);

  /* Trigger @asucc/@adrop */
  *env[0]='\0';
  if(*(s=atr_get(exit, type?A_ADROP:A_ASUCC)))
    parse_que(exit, s, player);
}


void do_teleport(dbref player, char *arg1, char *arg2)
{
  dbref thing=player, loc;
  int plane=0;
  char *s;
  
  /* Parse destination plane */
  if((s=strchr(*arg2?arg2:arg1, ':'))) {
    *s++='\0';
    plane=atoi(s);
  }

  /* Get victim & destination */
  if((loc=match_thing(player, *arg2?arg2:arg1, MATCH_EVERYTHING|MATCH_HOME)) ==
     NOTHING)
    return;
  if(*arg2 && (thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;

  /* Check object type */
  if(Typeof(thing) == TYPE_ROOM || Typeof(thing) == TYPE_ZONE) {
    notify(player, "%ss are not allowed to move.",
           typenames[Typeof(thing)]);
    return;
  } if(Typeof(thing) == TYPE_EXIT && loc == HOME) {
    notify(player, "Exits have no home.");
    return;
  } if(Typeof(thing) == TYPE_EXIT && Typeof(loc) != TYPE_ROOM) {
    notify(player, "You can only teleport Exits to other rooms.");
    return;
  } if(loc != HOME && (Typeof(loc) == TYPE_ZONE ||
       (Typeof(loc) == TYPE_EXIT && db[thing].location != db[loc].location))) {
    notify(player, "That is not a valid destination.");
    return;
  } if(loc != HOME && Typeof(loc) == TYPE_PLAYER && (Typeof(thing) ==
       TYPE_EXIT || Typeof(thing) == TYPE_PLAYER)) {
    notify(player, "You can't teleport %ss into Players!",
           typenames[Typeof(thing)]);
    return;
  } if(s && !power(player, POW_PLANE)) {
    notify(player, "You don't have the power to teleport across planes.");
    return;
  }

  /* Check permissions for victim */
  if(!controls(player, thing, POW_TELEPORT) &&
     !controls(player, db[thing].location, POW_TELEPORT)) {
    notify(player, "Permission denied.");
    return;
  }

  /* Check global restriction */
  if(RESTRICT_TELEPORT && !power(player, POW_TELEPORT) && !Builder(player) &&
     !is_zone(thing, ZONE_RESTRICTED)) {
    notify(player, "Restricted command.");
    return;
  }

  /* Homeward bound */
  if(loc == HOME) {
    if(RESTRICT_TELEPORT && !power(player, POW_TELEPORT) && !Builder(player) &&
       !can_move(player, thing, db[thing].link, 3))
      return;

    notify(thing, "\e[1mYou awake to find yourself back home...\e[m");
    if(s)
      enter_plane=plane;
    enter_room(thing, loc);
    return;
  }

  /* Destination is exit */
  if(Typeof(loc) == TYPE_EXIT) {
    char buf[20];

    sprintf(buf, "#%d", loc);
    do_move(thing, buf);
    return;
  }

  /* Check permissions for destination */
  if(!power(player, POW_TELEPORT)) {
    if(!controls(player,loc,POW_TELEPORT) && !IS(loc,TYPE_ROOM,ROOM_JUMP_OK)) {
      notify(player, "Permission denied.");
      return;
    }

    /* Check against locks on zones */
    if(!can_move(player, thing, loc, 1))
      return;
  }

  if(s)
    enter_plane=plane;
  enter_room(thing, loc);
}

// Enter an object in the room. Only objects set Enter_Ok can be entered.
//
void do_enter(dbref player, char *what)
{
  dbref thing;

  /* Check permissions */
  if(Typeof(player) == TYPE_EXIT || Typeof(player) == TYPE_ROOM ||
     Typeof(player) == TYPE_ZONE) {
    notify(player, "Only objects may enter things.");
    return;
  }

  if((thing = match_thing(player, what, MATCH_CON)) == NOTHING)
    return;

  if(Typeof(thing) != TYPE_THING) {
    notify(player, "You may only enter things.");
    return;
  }

  if(player == thing || !could_doit(player, thing, A_ELOCK) ||
     (!controls(player, thing, POW_TELEPORT) && !IS(thing, TYPE_THING,
     THING_ENTER_OK))) {
    ansi_did_it(player, thing, A_EFAIL, "You can't enter that.", A_OEFAIL,
                NULL, A_AEFAIL);
    return;
  } if(!could_doit(player, db[player].location, A_LLOCK)) {
    ansi_did_it(player, db[player].location, A_LFAIL,
                "You can't leave the current room.", A_OLFAIL, NULL, A_ALFAIL);
    return;
  }

  enter_room(player, thing);
}          

// Leave, or otherwise climb outside of an object.
//
void do_leave(dbref player)
{
  dbref loc=db[player].location;

  if(Typeof(player) == TYPE_EXIT || Typeof(player) == TYPE_ROOM ||
     Typeof(player) == TYPE_ZONE || Typeof(loc) == TYPE_ROOM ||
     db[loc].location == NOTHING || Typeof(db[loc].location) == TYPE_PLAYER) {
    notify(player, "You can't leave.");
    return;
  }       

  if(!could_doit(player, loc, A_LLOCK) ||
     (RESTRICT_TELEPORT && !Builder(player) &&
      Typeof(player) == TYPE_THING && !controls(player,
      db[loc].location, POW_MODIFY)))
    ansi_did_it(player,loc,A_LFAIL,"You can't leave.",A_OLFAIL,NULL,A_ALFAIL);
  else {
    if(Typeof(db[loc].location) == TYPE_ROOM)
      enter_plane=db[loc].plane;
    enter_room(player, db[loc].location);
  }
}

// Moves in the direction of a random exit from the player's current location.
// `run_exit' is set so that random door locks will not be checked twice.
//
void do_run(dbref player)
{
  dbref exit=random_exit(player, NOTHING);

  if(exit == NOTHING) {
    notify(player, "There is nowhere to run!");
    return;
  }
  run_exit=exit;
  do_move(player, db[exit].name);
  run_exit=NOTHING;
}

/* move exit/object to front of stack in room */
void do_push(dbref player, char *arg1)
{
  dbref thing, loc;

  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;

  loc=db[thing].location;
  if(Invalid(loc) || !controls(player, loc, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  } if(thing == loc || Typeof(thing) == TYPE_ROOM ||
       Typeof(thing) == TYPE_ZONE) {
    notify(player, "You cannot push that object anywhere.");
    return;
  }

  /* push it */
  if(Typeof(thing) == TYPE_EXIT) {
    db[loc].exits = remove_first(db[loc].exits, thing);
    ADDLIST(thing, db[loc].exits);
  } else {
    db[loc].contents = remove_first(db[loc].contents, thing);
    ADDLIST(thing, db[loc].contents);
  }

  if(!Quiet(player))
    notify(player, "Pushed forward in stack.");
}

/* remove the first occurrence of what in list headed by first */
dbref remove_first(dbref first, dbref what)
{
  dbref prev;

  /* special case if it's the first one */
  if(first == what)
    return db[first].next;

  /* have to find it */
  DOLIST(prev, first) {
    if(db[prev].next == what) {
      db[prev].next = db[what].next;
      return first;
    }
  }
  return first;
}
