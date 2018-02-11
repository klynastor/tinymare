/* comm/give.c */
/* Routines for moving objects around */

#include "externs.h"

// Picks up an object and puts it into the player's inventory.
//
void do_take(dbref player, char *obj)
{
  char buf[640];
  dbref thing;

  /* Check that the enactor is a valid container */
  if(Typeof(player) == TYPE_EXIT || Typeof(player) == TYPE_ROOM ||
     Typeof(player) == TYPE_ZONE) {
    notify(player, "You can't pick up anything!");
    return;
  } if(Typeof(db[player].location) != TYPE_ROOM &&
       Typeof(db[player].location) != TYPE_THING) {
    notify(player, "You can't take anything from %s.",
           db[db[player].location].name);
    return;
  } if(player == db[player].location) {
    notify(player, "You can't take anything while inside yourself.");
    return;
  }

  /* Match object from contents of room */
  if((thing=match_thing(player, obj, MATCH_CON|(power(player, POW_REMOTE) ?
     MATCH_ABS:0))) == NOTHING)
    return;
  if(db[thing].location == player) {
    notify(player, "You already have that!");
    return;
  }

  /* Check type */
  if(player == thing) {
    notify(player, "Interesting concept.");
    return;
  } if(Typeof(thing) != TYPE_THING) {
    notify(player, "You cannot pick up %ss.", typenames[Typeof(thing)]);
    return;
  }

  /* Check lock */
  if(!could_doit(player, thing, A_LOCK) || hold_player(thing)) {
    did_it(player, thing, A_FAIL,"You can't pick that up.",
           A_OFAIL, NULL, A_AFAIL);
    return;
  } if(!could_doit(thing, db[thing].location, A_LLOCK)) {
    did_it(player, db[thing].location, A_LFAIL, "You can't pick that up.",
                A_OLFAIL, NULL, A_ALFAIL);
    return;
  }

  enter_room(thing, player);
  notify(thing, "You have been picked up by %s.", db[player].name);
  sprintf(buf, "takes %s.", db[thing].name);
  ansi_did_it(player, thing, A_SUCC, "Taken.", A_OSUCC, buf, A_ASUCC);
}

// Drops an object from the inventory to the player's location.
//
void do_drop(dbref player, char *obj)
{
  char buf[640];
  dbref thing, loc=db[player].location;
  
  /* Check permissions on object */
  if(Typeof(player) == TYPE_EXIT || Typeof(player) == TYPE_ROOM ||
     Typeof(player) == TYPE_ZONE) {
    notify(player, "You can't drop anything!");
    return;
  }

  if(player == loc) {
    notify(player, "You can't drop anything here!");
    return;
  }

  /* Match object from inventory */
  if((thing=match_thing(player, obj, MATCH_INVEN)) == NOTHING)
    return;
  /* Keep players from getting teleported into other players */
  if(Typeof(thing) == TYPE_PLAYER && Typeof(loc) == TYPE_PLAYER) {
    notify(player, "You can't drop players when carried by someone.");
    return;
  }

  if(!could_doit(thing, loc, A_ELOCK) || (RESTRICT_TELEPORT &&
     !Builder(player) && !controls(thing, loc, POW_MODIFY) &&
     !controls(player, loc, POW_MODIFY))) {
    did_it(player, loc, A_EFAIL, "You can't drop that here.", A_OEFAIL,
           NULL, A_AEFAIL);
    return;
  }

  if(IS(loc, TYPE_ROOM, ROOM_XZONE) && db[thing].link != loc) {
    notify(thing,
         "You dissolve forever into the wispy aether of space and time.");
    notify_except(player, player, "%s drops %s, dissolving it forever "
         "into the wispy aether of space and time.",
         db[player].name, db[thing].name);
    enter_room(thing, HOME);
    return;
  }

  notify(thing, "Dropped.");
  sprintf(buf, "dropped %s.", db[thing].name);
  ansi_did_it(player, thing, A_DROP, "Dropped.", A_ODROP, buf, A_ADROP);

  if(Typeof(loc) == TYPE_ROOM) {
    enter_plane=db[player].plane;
    enter_room(thing, (db[loc].link != NOTHING) ? db[loc].link : loc);

    /* Trigger drop actions on room */
    did_it(thing, loc, A_DROP, NULL, A_ODROP, NULL, A_ADROP);
  } else
    enter_room(thing, loc);
}

// Internal function to handle giving objects or money to other players or
// objects in the room. If <type> is set to 1, the game instead checks for
// objects in a player's inventory and disables giving money.
//
static void do_give_internal(dbref player, char *name, char *obj, int type)
{
  char buf[1280], buff[1280], *s;
  dbref who, thing;
  int amount, cost=0;

  /* Check that the enactor is a valid container */
  if(Typeof(player) == TYPE_EXIT || Typeof(player) == TYPE_ROOM ||
     Typeof(player) == TYPE_ZONE) {
    notify(player, "You can't give anything!");
    return;
  }

  /* Find recipient */
  if((who=match_thing(player, name, MATCH_CON|MATCH_INVEN|
      (power(player, POW_REMOTE)?(MATCH_PLAYER|MATCH_ABS):0))) == NOTHING)
    return;
  if(who == player) {
    notify(player, "You can't give things to yourself!");
    return;
  } if(Typeof(who) == TYPE_PLAYER && !(db[who].flags & PLAYER_CONNECT) &&
       !controls(player, who, POW_MONEY)) {
    notify(player, "%s is not connected.", db[who].name);
    return;
  }

  /* Make sure the user specified something to give */
  if(!*obj) {
    notify(player, "Give what?");
    return;
  }

  /* Check for all digits in string */
  s=obj;
  if(!type)
    for(;*s && (isdigit(*s) || (s == obj && *s == '-'));s++);

  if(*s) {  /* Giving an object.. */
    if((thing=match_thing(player, obj, MATCH_INVEN)) == NOTHING)
      return;
    sprintf(env[0], "#%d", thing);

    /* Check @elock & @lgive on recipient */
    if(!could_doit(player, who, A_LGIVE) ||
       (!controls(player,who,POW_TELEPORT) && (!could_doit(thing,who,A_ELOCK) ||
       (!IS(who,TYPE_THING,THING_ENTER_OK) && Typeof(who) != TYPE_PLAYER)))) {
      sprintf(buf, "You can't give that to %s.", db[who].name);
      did_it(player, who, A_GFAIL, buf, A_OGFAIL, NULL, A_AGFAIL);
      return;
    }

    if(thing == who && !power(player, POW_TELEPORT)) {
      notify(player, "You can't put things inside themselves!");
      return;
    }

    sprintf(buf, "You give %s to %s.", db[thing].name, db[who].name);
    sprintf(buff, "gives %s to %s.", db[thing].name, db[who].name);
    ansi_did_it(player, who, A_GIVE, buf, A_OGIVE, buff, A_AGIVE);
    if(db[player].location != db[who].location)
      notify(who, "%s gave %s to you.", db[player].name, db[thing].name);
    notify(thing, "%s gave you to %s.", db[player].name, db[who].name);
    enter_room(thing, who);
    return;
  }

  /* Must be giving money... */

  if(!(amount=atoi(obj))) {
    notify(player, "You must specify a positive number of %s.", MONEY_PLURAL);
    return;
  } if(amount < 0 && !power(player, POW_MONEY)) {
    notify(player, "You look through your pockets. Nope, no negative %s.",
           MONEY_PLURAL);
    return;
  }

  /* Guest control */
  if(Guest(player) && !power(who, POW_MONEY)) {
    notify(player, "Guests can't give money to other players.");
    return;
  } if(Guest(who) && !power(player, POW_MONEY)) {
    notify(player, "You can't give money to Guests.");
    return;
  }

  sprintf(env[0], "%d", amount);

  /* Paying an object w/@cost */
  if(Typeof(who) != TYPE_PLAYER) {
    if(!(cost=atoi(atr_parse(who, player, A_COST))) ||
       !could_doit(player, who, A_LCOST)) {
      did_it(player, who, A_CFAIL, "That object does not accept any money.",
             A_OCFAIL, NULL, A_ACFAIL);
      return;
    } if(amount < 0) {
      notify(player, "Objects cannot accept negative %s.", MONEY_PLURAL);
      return;
    } if(amount < cost) {
      notify(player, "That costs %d %s.", cost, MONEY_PLURAL);
      return;
    }
  }

  /* Check to see if the player has enough money */
  if((!power(player, POW_MONEY) || Typeof(player) == TYPE_PLAYER) &&
     db[db[player].owner].pennies < amount) {
    notify(player, "You don't have that many %s to give!", MONEY_PLURAL);
    return;
  }

  /* Pay an object? */
  if(Typeof(who) != TYPE_PLAYER) {
    if(cost < 0)
      cost=amount;
    sprintf(buf, "You paid \e[1;32m%d\e[m %s.", cost, cost==1?MONEY_SINGULAR:
            MONEY_PLURAL);
    sprintf(buff, "pays \e[1;32m%d\e[m %s to %s.", cost, cost==1?MONEY_SINGULAR:
            MONEY_PLURAL, db[who].name);

    if(Typeof(player) == TYPE_PLAYER || !power(player, POW_MONEY))
      giveto(player, -cost);
    if(!power(who, POW_MONEY))
      giveto(who, cost);

    did_it(player, who, A_PAY, buf, A_OPAY, buff, A_APAY);
    if(amount-cost > 0)
      notify(player, "You get \e[1;32m%d\e[m in change.", amount-cost);
    return;
  }

  /* Give money to another player */

  notify(player, "You give \e[1;32m%d\e[m %s to %s.", amount,
         amount==1?MONEY_SINGULAR:MONEY_PLURAL, db[who].name);
  notify_room(player, player, who, "%s gives %s \e[1;32m%d\e[m %s.",
              db[player].name, db[who].name, amount,
              amount==1?MONEY_SINGULAR:MONEY_PLURAL);
  notify(who, "%s%s gives you \e[1;32m%d\e[m %s.", terminfo(who, TERM_MUSIC)?
         "\e[53;2t":"", db[player].name, amount,
         amount==1?MONEY_SINGULAR:MONEY_PLURAL);

  if(amount < -db[db[who].owner].pennies)
    amount=-db[db[who].owner].pennies;
  if(Typeof(player) == TYPE_PLAYER || !power(player, POW_MONEY))
    giveto(player, -amount);
  giveto(who, amount);  

  did_it(player, who, A_PAY, NULL, A_OPAY, NULL, A_APAY);

  /* Log newbies giving money to other players */
  if(!WIZLOCK_LEVEL && db[db[player].owner].data->age < 3*3600)
    log_main("NEWBIE: %s(#%d) gave %d %s to %s(#%d)", db[player].name, player,
             amount, amount==1?MONEY_SINGULAR:MONEY_PLURAL, db[who].name, who);
}

// Give an object or money to another player or object in the room.
// Money must be specified by a string of all digits. Objects may accept
// money only if their @cost is set.
//
void do_give(dbref player, char *name, char *obj)
{
  do_give_internal(player, name, obj, 0);
}

// Puts an object into something carried in your inventory. Same as 'give' but
// checks a player's inventory for the recipient instead.
//
void do_put(dbref player, char *name, char *obj)
{
  do_give_internal(player, name, obj, 1);
}

// Grab something from an object in the room or in your inventory.
//
void do_grab(dbref player, char *arg1, char *obj)
{
  char buff[1200], buff2[1200];
  dbref who, thing;

  /* Find target */
  if((who=match_thing(player,arg1,MATCH_CON|MATCH_INVEN|
      (power(player, POW_REMOTE)?MATCH_PLAYER|MATCH_ABS:0))) == NOTHING)
    return;
  if(who == player) {
    notify(player, "You can't grab things from yourself.");
    return;
  }
  
  /* Check lock on container */
  if(!could_doit(player, who, A_LGRAB) ||
     (!controls(player,who,POW_MODIFY) && !IS(who,TYPE_THING,THING_GRAB_OK))) {
    sprintf(buff, "You can't take anything from %s.", db[who].name);
    did_it(player, who, A_GRFAIL, buff, A_OGRFAIL, NULL, A_AGRFAIL);
    return;
  }

  /* Check for objects in container */
  thing=match_thing(who, obj, MATCH_INVEN|MATCH_QUIET);
  if(thing == NOTHING) {
    notify(player, "%s doesn't have that object.", db[who].name);
    return;
  }

  sprintf(env[0], "#%d", thing);

  /* Check @llock & @lgrab on container */
  if(!controls(player,who,POW_MODIFY) && (!could_doit(player, thing, A_LOCK) ||
     !could_doit(thing, who, A_LLOCK))) {
    sprintf(buff, "You can't take that from %s.", db[who].name);
    did_it(player, who, A_GRFAIL, buff, A_OGRFAIL, NULL, A_AGRFAIL);
    return;
  }

  enter_room(thing, player);
  sprintf(buff2, "You take %s from %s.", db[thing].name, db[who].name);
  sprintf(buff, "takes %s from %s.", db[thing].name, db[who].name);
  ansi_did_it(player, who, A_GRAB, buff2, A_OGRAB, buff, A_AGRAB);
  if(db[player].location != db[who].location)
    notify(who, "%s took %s from you.", db[player].name, db[thing].name);
  notify(thing, "%s took you from %s.", db[player].name, db[who].name);
}

void do_money(dbref player, char *arg1)
{
  int a, b, i, thing=db[player].owner;

  if(*arg1) {
    if((thing=lookup_player(arg1)) == AMBIGUOUS)
      thing=db[player].owner;
    else if(Invalid(thing) || Typeof(thing) != TYPE_PLAYER) {
      notify(player, "No such player '%s'.", arg1);
      return;
    } if(!controls(player, thing, POW_MONEY) && !power(player, POW_MONEY)) {
      notify(player, "You need a search warrant to do that.");
      return;
    }
  }

  a=db[thing].pennies;
  notify(player, "\e[1;36mCash\e[;36m...........\e[1m: \e[37m%d\e[m", a);

  /* Count material assets */
  for(b=0,i=0;i<db_top;i++)
    if(db[i].owner == thing)
      b+=(Typeof(i) == TYPE_EXIT)?EXIT_COST:(Typeof(i) == TYPE_ROOM)?ROOM_COST:
         (Typeof(i) == TYPE_THING)?THING_COST:(Typeof(i) == TYPE_ZONE)?
         ZONE_COST:0;
  a+=b;
  notify(player, "\e[1;36mMaterial Assets: \e[37m%d\e[m", b);

  notify(player, "%s", "");
  notify(player, "\e[1;36mTotal Net Worth: \e[37m%d %s\e[m",
         a, a==1?MONEY_SINGULAR:MONEY_PLURAL);
}

// Wizard command to charge money for payment to a vendor object. It makes it
// appear to others that <arg1> gave money to the vendor.
//
void do_charge(dbref player, char *arg1, char *arg2)
{
  dbref who;
  int amount=atoi(arg2);

  if((who=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, who, POW_MONEY)) {
    notify(player, "Permission denied.");
    return;
  } if(!amount) {
    if(!Quiet(player))
      notify(player, "No money to charge.");
    return;
  } else if(amount < 0) {
    notify(player, "You can only charge positive amounts.");
    return;
  }

  notify(player, "You charge %s \e[1;32m%d\e[m %s.", db[who].name, amount,
         amount==1?MONEY_SINGULAR:MONEY_PLURAL);
  notify_room(player, player, who, "%s gives %s \e[1;32m%d\e[m %s.",
              db[who].name, db[player].name, amount,
              amount==1?MONEY_SINGULAR:MONEY_PLURAL);
  notify(who, "You give \e[1;32m%d\e[m %s to %s.", amount,
         amount==1?MONEY_SINGULAR:MONEY_PLURAL, db[player].name);

  if(amount > db[db[who].owner].pennies)
    amount=db[db[who].owner].pennies;
  giveto(who, -amount);  
}
