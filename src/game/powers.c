/* game/powers.c */

#include "externs.h"

char *classnames[NUM_CLASSES]=
  {"Wanderer", "Adventurer", "Explorer", "Knight", "Warlord", "Wizard"};

char *typenames[8]=
  {"Room", "Thing", "Exit", "Player", "Zone", "0x5", "0x6", "Garbage"};

// Note: If powers are either added or deleted from the following table,
// please remember to change the corresponding values in hdrs/powers.h
// and 'make clean' before recompiling. Powers start from class 1,
// Adventurer, and does not include the guest class, Wanderer.

const struct pow_list powers[NUM_POWS]={

     /* Adventurer Explorer Knight Warlord Wizard */

 {"Animation", "---YY", "--YYY", "Ability to perform ansi animation"},
 {"Announce",  "--YYY", "-YYYY", "Ability to announce for free"},
 {"Backstage", "-YYYY", "YYYYY", "Ability to see coding behind the scenes"},
 {"Boot",      "-LLLY", "-LLLY", "Ability to @boot players off the game"},
 {"Broadcast", "---YY", "---YY", "Ability to @broadcast a message"},
 {"Build",     "---YY", "-YYYY", "Ability to use building commands & features"},
 {"Capture",   "--LLL", "-LLLL", "Ability to put people into jail"},
 {"Chown",     "----Y", "---LY", "Ability to change ownership of an object"},
 {"Com",       "--LLL", "-LLLL", "Ability to lock players from +com channels"},
 {"Combat",    "---YY", "YYYYY", "Ability to do combat/soul related things"},
 {"Config",    "-----", "----Y", "Ability to configure environment variables"},
 {"Console",   "----Y", "---LY", "Ability to change connection settings"},
 {"Dark",      "---YY", "YYYYY", "Ability to see dark objects in rooms"},
 {"Database",  "---YY", "--YYY", "Ability to use database lookup utilities"},
 {"Examine",   "---EY", "-LEEY", "Ability to examine objects freely"},
 {"Free",      "--YYY", "YYYYY", "Ability to build for free"},
 {"Functions", "--EYY", "-EYYY", "Ability to get database values on objects"},
 {"Join",      "--YYY", "-YYYY", "Ability to join players"},
 {"Member",    "-YYYY", "YYYYY", "Ability to change your name"},
 {"Modify",    "----Y", "---LY", "Ability to modify other people's objects"},
 {"Money",     "----Y", "---YY", "Ability to set and view monetary values"},
 {"Newpass",   "---LL", "--LLL", "Ability to reset passwords on players"},
 {"Nuke",      "----L", "---LL", "Ability to delete players from the game"},
 {"Pcreate",   "---YY", "--YYY", "Ability to create new players"},
 {"Pfile",     "-LLLY", "-LLLY", "Ability to file player incident reports"},
 {"Plane",     "----Y", "---YY", "Ability to control multi-dimensional planes"},
 {"Queue",     "--YYY", "-EYYY", "Ability to see all commands in the queue"},
 {"Remote",    "----Y", "---YY", "Ability to do things remotely"},
 {"Security",  "----Y", "----Y", "Ability to do security-related things"},
 {"Shutdown",  "----Y", "---YY", "Ability to @shutdown the game"},
 {"Sitelock",  "--YYY", "-YYYY", "Ability to @sitelock hosts/IP addresses"},
 {"Slave",     "--LLL", "-LLLL", "Ability to lockout player characters"},
 {"Spoof",     "---YY", "--YYY", "Ability to do unlimited @emit, etc."},
 {"Stats",     "--LYY", "-LYYY", "Ability to view database @stats on players"},
 {"Summon",    "---LY", "--LEY", "Ability to summon other players"},
 {"Teleport",  "----Y", "---YY", "Ability to use unlimited @teleport"},
 {"Who",       "--LYY", "-YYYY", "Ability to see all the users connected"},
 {"Wizattr",   "----Y", "---YY", "Ability to set wizard attributes"},
 {"Wizflags",  "---YY", "--YYY", "Ability to set wizard flags"}};

void list_powers(dbref player, char *arg2)
{
  int a, b=atoi(arg2)-1;

  notify(player, "Powers list.  Default/Maximum; Y=Yes E=Eq L=Lt  "
         "Further right is higher rank.");
  notify(player, "%s", "");

  for(a=0;a<NUM_POWS;a++)
    if(!*arg2 || (b >= 0 && a == b) || string_prefix(powers[a].name, arg2))
      notify(player, "%3d: %-10.10s  %s  %s  %s", a+1,
             powers[a].name, powers[a].init, powers[a].max, powers[a].desc);
}

static unsigned char get_pow(dbref player, unsigned char pow)
{
  player=db[player].owner;

  /* god has unlimited powers */
  if(player == GOD)
    return PW_YES;
  if(pow >= NUM_POWS)
    return PW_NO;

  return (db[player].data->powers[pow/4] >> ((pow%4)*2)) & 3;
}

/* checks if <player> can control <recipt> with specific power */
int controls(dbref player, dbref recipt, unsigned char pow)
{
  unsigned char pows;

  player=db[player].owner;
  if(recipt <= NOTHING)
    recipt=NOTHING;
  else
    recipt=db[recipt].owner;

  if(player == GOD || player == recipt)
    return 1;
  if(player != GOD && recipt == GOD)
    return 0;

  /* check power level */
  if(!(pows=get_pow(player, pow)))
    return 0;
  if(pows == PW_YES || recipt == NOTHING)
    return 1;
  if(pows == PW_EQ && class(recipt) <= class(player))
    return 1;
  if(pows == PW_LT && class(recipt) < class(player))
    return 1;
  return 0;
}

/* set class on player and initialize startup powers */
void initialize_class(dbref player, char class)
{
  int i, val;

  if(Typeof(player) != TYPE_PLAYER)
    return;
  if(class < 0)
    class=CLASS_PLAYER;
  if(class >= NUM_CLASSES || player == GOD)
    class=NUM_CLASSES-1;

  db[player].data->class=class;
  memset(db[player].data->powers, 0, (NUM_POWS+3)/4);

  if(class == CLASS_GUEST)
    return;

  /* set initial powers for class */
  for(i=0;i<NUM_POWS;i++) {
    switch(powers[i].init[(int)class-1]) {
      case 'Y': val = PW_YES; break;
      case 'E': val = PW_EQ; break;
      case 'L': val = PW_LT; break;
      default:  val = PW_NO;
    }

    if(player == GOD)
      val=PW_YES;
    db[player].data->powers[i/4] |= (val << ((i%4)*2));
  }
}

/* reclassify player */
void do_class(dbref player, char *arg1, char *arg2)
{
  dbref who;
  int level;

  if((who=lookup_player(arg1)) <= NOTHING) {
    notify(player, "No such player '%s'.", arg1);
    return;
  }

  /* find requested level assignment */
  for(level=0;level<NUM_CLASSES;level++)
    if(!strcasecmp(arg2, classnames[level]))
      break;
  if(level == NUM_CLASSES) {
    notify(player, "No such classification rank name.");
    return;
  }

  /* insure player has the power to make that assignment */
  /* GOD can reclass without restriction */
  if(player != GOD && (level >= class(db[player].owner) ||
     class(who) >= class(db[player].owner))) {
    notify(player, "You don't have the authority to do that.");
    return;
  } if(Guest(who)) {
    notify(player, "You cannot promote guest characters.");
    return;
  } if(level == CLASS_GUEST) {
    notify(player, "You cannot transform players into guests.");
    return;
  }

  /* GOD must remain wizard.  This is a safety feature. */
  if(who == GOD && level != CLASS_WIZARD) {
    notify(player, "Sorry, %s cannot resign %s position.",
           unparse_object(player, who), db[who].data->gender?"her":"his");
    return;
  }

  log_main("%s has reclassified %s(#%d) to %s.", db[player].name,
           db[who].name, who, classnames[level]);
  notify(player, "%s is now reclassified as: %s", db[who].name,
         classnames[level]);
  notify(who, "You have been reclassified as: %s", classnames[level]);

  initialize_class(who, level);
}

/* give or take powers to/from players. */
void do_empower(dbref player, char *whostr, char *powstr)
{
  char powmax, *i;
  int powval, pow, who;

  if(!*whostr || !(i=strchr(powstr, ':'))) {
    notify(player, "Usage: @empower <player>=<power>:<yes|eq|lt|no>");
    return;
  } if((who=lookup_player(whostr)) <= NOTHING) {
    notify(player, "No such player '%s'.", whostr);
    return;
  } 

  *i++='\0';

  /* grab the power level */
  if((powval=match_word("No Lt Eq Yes", i)))
    powval--;
  else {
    notify(player, "The power value must be one of Yes, Eq, Lt, or No.");
    return;
  }

  for(pow=0;pow<NUM_POWS;pow++)
    if(!strcasecmp(powers[pow].name, powstr))
      break;
  if(pow == NUM_POWS) {
    notify(player, "Unknown power '%s'.", powstr);
    return;
  }

  /* check permissions */
  if((get_pow(player,pow) < powval || class(who) >= class(db[player].owner)) &&
     player != GOD) {
    notify(player, "You may only set powers on those of lower rank.");
    return;
  } if(Guest(who)) {
    notify(player, "Guests cannot get any powers.");
    return;
  }

  /* search for maximum value in class power-tables */
  switch(powers[pow].max[class(who)-1]) {
    case 'Y': powmax = PW_YES; break;
    case 'E': powmax = PW_EQ; break;
    case 'L': powmax = PW_LT; break;
    default:  powmax = PW_NO;
  }

  if(player != GOD && powval > powmax) {  /* God has NO restrictions */
    if(!powmax)
      notify(player, "Sorry, %ss can't be given that power.",
             classnames[class(who)]);
    else
      notify(player, "Sorry, the maximum for %ss is %s.",
             classnames[class(who)], powmax == PW_LT?"Lt":"Eq");
    return;
  }

  /* delete old power */
  db[who].data->powers[pow/4] &= ~(3 << ((pow%4)*2));

  /* add new power */
  db[who].data->powers[pow/4] |= (powval << ((pow%4)*2));

  log_main("%s(#%d) granted %s(#%d) power %s, level %s",
    db[player].name, player, db[who].name, who, powers[pow].name, i);
  if(powval != PW_NO) {
    notify(player, "%s has been given the power of %s.",
           db[who].name, powers[pow].name);
    notify(who, "You have been given the power of %s.", powers[pow].name);
    notify(who, "You can use it on %s.", (powval == PW_YES)?"anyone":
           (powval == PW_EQ)?"people your class and under":
           "people under your class");
  } else {
    notify(player, "%s%s power of %s has been removed.",
           db[who].name, possname(db[who].name), powers[pow].name);
    notify(who, "Your power of %s has been removed.", powers[pow].name);
  }
}

/* list powers on player */
void do_powers(dbref player, char *arg1)
{
  int a, k, who=player, count=0;

  if(*arg1) {
    if((who=lookup_player(arg1)) == NOTHING) {
      notify(player, "No such player '%s'.", arg1);
      return;
    }
    if(who == AMBIGUOUS)
      who=db[player].owner;
  } if(!controls(player, who, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* list them out */
  for(k=0;k<NUM_POWS;k++) {
    if(!(a=get_pow(who, k)))
      continue;

    if(!count++)
      notify(player, "\e[1;32m%s%s Powers:\e[m", db[who].name,
             possname(db[who].name));
    notify(player, "\e[1;36m%15.15s \e[37m%s \e[34m%s\e[m", powers[k].name,
           a==PW_YES?"  ":a==PW_EQ?"EQ":"LT", powers[k].desc);
  }

  if(!count)
    notify(player, "%s has no immortal powers.", db[who].name);
}

/* returns space-separated list of powers on <player> */
void disp_powers(char *buff, dbref player)
{
  char *s=buff;
  int k;

  *buff=0;
  for(k=0;k<NUM_POWS;k++)
    if(get_pow(player, k)) {
      if(s != buff)
        *s++=' ';
      s+=sprintf(s, "%s", powers[k].name);
    }
}
