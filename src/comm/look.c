/* comm/look.c */
/* commands that display (not modify) rooms, contents and object examination */

#include "externs.h"

static void look_atrs(dbref player, dbref thing, int inherit);


/* display contents of object or room */
static void look_contents(dbref player, dbref loc, char *title)
{
  int a, thing, count=0;
  
  if(IS(loc, TYPE_ROOM, ROOM_XZONE))
    return;

  /* search contents list */
  DOLIST(thing, db[loc].contents)
    if((a=can_see(player, thing))) {
      disp_hidden=a;
      if(*title && !count++)
        notify(player, "\e[0;1;37m%s\e[m", title);
      if(IS(player, TYPE_PLAYER, PLAYER_TERSE))
        notify(player, "%s%s\e[m", unparse_object(player, thing),
               unparse_objheader(player, thing));
      else
        notify(player, "%s", unparse_caption(player, thing));
    }
  disp_hidden=0;
}

/* display exits in room */
static void look_exits(dbref player, dbref loc, char *title)
{
  char buf[8520];
  int a, thing;

  /* search exits list */
  *buf=0;
  DOLIST(thing, db[loc].exits)
    if((a=can_see(player, thing))) {
      if(!*buf)
        notify(player, "\e[0;1;37m%s\e[m", title);
      else
        strcat(buf, "  ");
      if(strlen(buf) > 8000) {
        strcat(buf, "...");
        break;
      }
      strcat(buf, unparse_exitname(player, thing, 0));
      if(a == 2)
        strcat(buf, "(dark)");
      else if(a == 4)
        strcat(buf, "(secret)");
    }

  /* process and display */
  if(!*buf)
    notify(player, "\e[0;1;37mNo obvious exits.\e[m");
  else
    notify(player, "%s", buf);
}

/* expand exitlist for PLAYER_TERSE flag */
static char *exitlist(dbref player, dbref loc)
{
  static char buf[1024];
  int thing, count=0;

  /* search exits list */
  strcpy(buf, "   \e[1m(");
  DOLIST(thing, db[loc].exits)
    if(can_see(player, thing) == 1) {
      if(count++)
        strcat(buf, ",");
      if(strlen(buf) > 970) {
        strcat(buf, ",...");
        break;
      }
      strmaxcpy(buf+strlen(buf), db[thing].name, 40);
    }

  if(!count)
    return "";
  strcat(buf, ")\e[m");
  return buf;
}

/* looks in the room or at an object */
void do_look(dbref player, char *arg1)
{
  dbref thing=db[player].location, link=NOTHING;
  char buff[8192], *s=NULL;
  int seethru=0, plane=-1, temp_plane=0, lit;

  if(IS(thing, TYPE_ROOM, LIGHT)) {
    notify(player, "It's too bright to see here.");
    return;
  }

  /* Match for an object */
  if(*arg1 && (thing=match_thing(player, arg1, MATCH_POSSESSIVE|
     (power(player,POW_REMOTE)?MATCH_EVERYTHING:MATCH_RESTRICT))) == NOTHING)
    return;
  /* Look in depth at a room */
  if(!*arg1 || Typeof(thing) == TYPE_ROOM || thing == db[player].location) {
    if(IS(player, TYPE_PLAYER, PLAYER_TERSE)) {
      db[player].flags &= ~PLAYER_TERSE; /* unset the flag for normal look */
      look_room(player, thing, 0);
      db[player].flags |= PLAYER_TERSE; /* then reset it */
    } else
      look_room(player, thing, 0);
    if(*arg1 && controls(player, thing, POW_EXAMINE))
      look_atrs(player, thing, 0);
    return;
  }

  /* Display object header */
  if(Typeof(thing) != TYPE_THING || db[thing].location == player ||
     !(db[thing].flags & DARK) || *atr_get(thing, A_CAPTION)) {
    disp_hidden=can_see(player, thing);
    notify(player, "%s", unparse_caption(player, thing));
    disp_hidden=0;
  }

  /* Display description messages */
  *buff=0;
  if(Typeof(thing) == TYPE_EXIT && !(db[thing].flags & OPAQUE) &&
     ((db[thing].flags & EXIT_TRANSPARENT) || could_doit(player,thing,A_LOCK)))
    seethru=1;
  if(!TIMEOFDAY)
    strcpy(buff, atr_get(thing, A_NDESC));
  if(!*buff)
    strcpy(buff, atr_get(thing, A_DESC));
  if(!*buff && !seethru)
    strcpy(buff, Typeof(thing) == TYPE_PLAYER ? (Guest(thing)?
           "A guest player!  Please be courteous and helpful.":
           "You see someone special and unique."):"You see nothing special.");
  if(*(s=parse_function(thing, player, buff)))
    notify(player, "%s", s);

  if(player == thing)
    sprintf(buff, "looks at %sself.", (Typeof(thing) != TYPE_PLAYER)?"it":
            db[thing].data->gender?"her":"him");
  else
    sprintf(buff, "looks %s%s.", (Typeof(thing) != TYPE_EXIT)?"at ":"",
            db[thing].name);
  did_it(player,thing,0,NULL,A_ODESC,buff,(player == thing)?0:A_ADESC);

  if(seethru && (link=db[thing].link) == AMBIGUOUS)
    link=resolve_link(player, thing, &plane);

  /* We are viewing a general object */
  if(!seethru || Invalid(link)) {
    look_contents(player, thing, "Carrying:");

    return;
  }

  /* Since variable exits can change planes, temporarily set player's plane */
  if(Typeof(player) != TYPE_ROOM && Typeof(player) != TYPE_ZONE &&
     plane != -1) {
    temp_plane=db[player].plane;
    db[player].plane=plane;
  }

  /* See through exits, as long as you pass its @lock */
  notify(player, "You peer through to %s...",
         unparse_object(player, link));
  *buff=0;
  lit=can_see(player, link);
  if(!TIMEOFDAY || ((!lit || lit == 2) && (db[link].flags & DARK)))
    strcpy(buff, atr_get(link, A_NDESC));
  if(!*buff)
    strcpy(buff, atr_get(link, A_DESC));

  default_color=unparse_color(player, link, 4);
  if(*(s=parse_function(link, player, buff)))
    notify(player, "%s%s%s", textcolor(7, default_color), s,
           textcolor(default_color, 7));
  else
    notify(player, "You see nothing descriptive on the other side.");
  default_color=7;

  look_contents(player, link, "You also notice:");

  /* Reset player's plane */
  if(Typeof(player) != TYPE_ROOM && Typeof(player) != TYPE_ZONE && plane != -1)
    db[player].plane=temp_plane;
}

/* General look in current room */
void look_room(dbref player, dbref loc, int type)
{
  char buf[8192], music[100], *s;
  int lit;

  if(Invalid(loc)) {
    notify(player, "%s", unparse_object(player, loc));
    return;
  }

  /* When moving rooms, change music */
  *music='\0';
  if(type && terminfo(player, TERM_MUSIC)) {
    if(!*(s=atr_get(loc, A_SOUND)))
      s=atr_zone(loc, A_SOUND);
    if(*s && *(s=parse_function(loc, player, s)))
      sprintf(music, "\e[%.90st", s);
  }

  /* Execute non-player description messages and actions */
  if(Typeof(loc) != TYPE_ROOM)
    did_it(player,loc,0,NULL,A_OIDESC,NULL,(player == loc)?0:A_AIDESC);
  else {
    did_it(player,loc,0,NULL,A_ODESC,NULL,(player == loc)?0:A_ADESC);
    if(Typeof(loc) == TYPE_ROOM) {
      if(could_doit(player, loc, A_LOCK))
        did_it(player,loc,0,NULL,A_OSUCC,NULL,A_ASUCC);
      else
        did_it(player,loc,0,NULL,A_OFAIL,NULL,A_AFAIL);
    }
  }

  /* Check if player is blind */
  if(db[player].location == loc) {
    if(IS(loc, TYPE_ROOM, LIGHT)) {
      notify(player, "%s\e[1;37mThe light here is too bright for you "
             "to see anything.\e[m", music);
      return;
    }
  }

  lit=can_see(player, loc);

  /* Terse players only see room title, caption, and exit list */
  if(IS(player, TYPE_PLAYER, PLAYER_TERSE))
    notify(player, "%s%s%s", music, unparse_caption(player, loc),
           exitlist(player, loc));
  else {
    /* Display room title, caption, and description */
    notify(player, "%s%s", music, unparse_caption(player, loc));

    *buf=0;
    if(Typeof(loc) != TYPE_ROOM)
      strcpy(buf, atr_get(loc, A_IDESC));
    if(!*buf && (!TIMEOFDAY || ((!lit || lit == 2) && (db[loc].flags & DARK))))
      strcpy(buf, atr_get(loc, A_NDESC));
    if(!*buf)
      strcpy(buf, atr_get(loc, A_DESC));
    default_color=unparse_color(player, loc, 4);
    if(*(s=parse_function(loc, player, buf)))
      notify(player, "%s%s", textcolor(7, default_color), s);

    if(Typeof(loc) == TYPE_ROOM) {
      if(could_doit(player, loc, A_LOCK))
        did_it(player,loc,A_SUCC,NULL,0,NULL,0);
      else
        did_it(player,loc,A_FAIL,NULL,0,NULL,0);
    }
    default_color=7;

  }

  /* Display contents */
  look_contents(player, loc, "Contents:");
  if(Typeof(loc) != TYPE_PLAYER) {
    if(!IS(player, TYPE_PLAYER, PLAYER_TERSE))
      look_exits(player, loc, "Obvious exits:");
  }
}

// Utility to test if <player> can see <thing> in normal conditions.
// Result=0: Player cannot see the object
//        1: Player can see object
//        2: Wizard looking at (hidden) object
//        3: Player with infravision sees red objects (Combat only)
//        4: Player sees hidden exit in cave (Combat only)
//        5: Object is totally obscured due to fog
//
// Of the above, 0 and 5 are considered unseen.
//
int can_see(dbref player, dbref thing)
{        
  dbref loc=db[thing].location;
  int dark=(controls(player, thing, POW_DARK) &&
            !IS(player, TYPE_PLAYER, PUPPET))?2:0;

  /* Normal look doesn't include player himself, disconnected players,
     or those that aren't in the same dimensional plane. If the plane is
     NOTHING (-1), then the object exists in all planes. */

  if(player == thing || !match_plane(player, thing) ||
     (Typeof(thing) == TYPE_PLAYER && (!(db[thing].flags & PLAYER_CONNECT))))
  return 0;

  /* Check bright room and visual-lock */
  if(IS(loc, TYPE_ROOM, LIGHT) || !could_doit(player, thing, A_VLOCK))
    return dark;
  /* Objects in inventory and things set Light are always visible. */
  if(loc == player || (Typeof(thing) != TYPE_PLAYER &&
     (db[thing].flags & LIGHT)))
    return 1;


  /* Fog obstructs a player's view of anything in the room */
  if(IS(db[thing].location, TYPE_ROOM, ROOM_FOG))
    return dark;

  /* Rooms set dark or locked with @vlock, or objects set opaque */
  if((Typeof(db[thing].location) == TYPE_ROOM &&
     ((db[db[thing].location].flags & DARK) ||
      !could_doit(player, db[thing].location, A_VLOCK))) ||
     (db[player].location != db[thing].location &&
      (db[db[thing].location].flags & OPAQUE)))
    return dark;

  /* Dark objects are hidden to normal eye. */
  if((db[thing].flags & DARK) &&
     (Typeof(thing) == TYPE_THING || Typeof(thing) == TYPE_EXIT))
    return dark;

  /* Everything else is a normal, bright room */
  return 1;
}

// Command to list visible exits and their destinations, one exit per line.
//
void do_exits(dbref player, char *name)
{
  char buf[2100];
  dbref loc, link, thing;
  int a, count=0;

  if(*name) {
    if((loc=match_thing(player, name, MATCH_EVERYTHING)) == NOTHING)
      return;
    if(!controls(player, loc, POW_EXAMINE) && !nearby(player, loc)) {
      notify(player, "Too far away to see.");
      return;
    }
  } else
    loc=db[player].location;

  if(Typeof(loc) != TYPE_ROOM) {
    notify(player, "Only rooms can have exits.");
    return;
  }

  /* Players cannot see in a dark (or very bright) room */
  if(IS(loc, TYPE_ROOM, LIGHT)) {
    notify(player, "You can't see the exits!");
    return;
  }

  /* Search exits list */
  DOLIST(thing, db[loc].exits)
    if((a=can_see(player, thing))) {
      if(!count++) {
        if(loc == db[player].location)
          notify(player, "\e[1mVisible exits:\e[m");
        else
          notify(player, "\e[1mVisible exits from %s(#%d):\e[m",
                 db[loc].name, loc);
      }

      if(!(db[thing].flags & EXIT_TRANSPARENT) &&
         !could_doit(player, thing, A_LOCK))
        strcpy(buf, "You can't tell.");
      else {
        if((link=db[thing].link) == AMBIGUOUS)
          link=resolve_link(player, thing, NULL);
        if(link == NOTHING)
          strcpy(buf, "Unlinked.");
        else
          strcpy(buf, unparse_object(player, link));
      }
      notify(player, "  %s -> %s%s", unparse_exitname(player, thing, 1),
             buf,a==2?"  \e[1;30m(dark)\e[m":a==4?"  \e[1;30m(secret)\e[m":"");
    }

  /* Process and display */
  if(!count)
    notify(player, "\e[1;37mNo obvious exits.\e[m");
}


/* Attribute Examination */

/* Display attribute with #dbref's unparsed */
static char *unparse_dbref(dbref player, char *arg, int type)
{
  static char buf[8192];
  char temp[strlen(arg)+1], *s;
  int a, num, count=0, dep=0;

  strcpy(temp, arg);

  for(a=0;temp[a] && count < 8000;a++) {
    if(!type && temp[a] == '[')
      dep++;
    else if(dep > 0 && temp[a] == ']')
      dep--;

    /* Parse #dbref */
    if(!dep && temp[a] == '#' && isdigit(temp[a+1])) {
      num=atoi(temp+a+1);
      if(Invalid(num) || Typeof(num) == TYPE_GARBAGE) {
        buf[count++]=temp[a];  /* Print #dbref as is */
        continue;
      }

      buf[count]='\0';
      s=unparse_object(player, num);
      if(strlen(s)+count < 8000) {
        strcpy(buf+count, s);
        count=strlen(buf);
        while(temp[a+1] && isdigit(temp[a+1]))  /* Skip over rest of number */
          a++;
        continue;
      }
    }

    /* Just add character to string */
    buf[count++]=temp[a];
  }

  buf[count]='\0';
  return buf;
}

/* Examine a single attribute */
static void look_atr(player, thing, match, allatrs)
dbref player, thing, match;
struct all_atr_list *allatrs;
{
  char inh[16], dbr[16], hex[16], *s=uncompress(allatrs->value);
  int flags=allatrs->type->flags;

  notify(player, "\e[1m%s%s%s%s:\e[m%s", allatrs->numinherit?
         (sprintf(inh, "%d>", allatrs->numinherit), inh):"", Invalid(match)?"":
         (sprintf(dbr, "#%d.", match), dbr), allatrs->type->name,
         (flags & AF_FUNC)?"()":"", (flags & AF_DATE)?mktm(player, atol(s)):
         (flags & AF_LOCK)?unparse_dbref(player, s, 0):  /* top level only */
         (flags & AF_DBREF)?unparse_dbref(player, s, 1): /* in functions too */
         (flags & AF_BITMAP)?(sprintf(hex, "0x%08x", atoi(s)), hex):s);
}

/* Examine all visible attributes on a thing */
static void look_atrs(dbref player, dbref thing, int inherit)
{
  struct all_atr_list *al, *list;
  int count=0;

  for(list=al=all_attributes(thing);al && al->type;al++)
    if(!(al->type->obj == NOTHING && al->type->num == A_DESC) &&
       can_see_atr(player, thing, al->type) &&
       (inherit || (!inherit && !al->numinherit))) {
      if(!count++)
        notify(player, "\e[1mAttribute list:\e[m");
      look_atr(player, thing, NOTHING, al);
    }
  free(list);
}

ATTR *match_attr(player, thing, name, type, value)
dbref player, thing;
char *name, *value;
int type;
{
  struct all_atr_list *al, *list;

  for(list=al=all_attributes(thing);al && al->type;al++)
    if((!type || can_see_atr(player, thing, al->type)) &&
       wild_match(al->type->name, name, 0) &&
       (!value || !*value || wild_match(uncompress(al->value), value, 0)))
      break;
  free(list);

  return (al && al->type)?al->type:NULL;
}

// Display attributes of an object.
//
void do_examine(player, name, arg2, argv, pure, cause)
dbref player, cause;
char *name, *arg2, **argv, *pure;
{
  dbref *ptr, match=NOTHING, content, exit, thing=db[player].location;
  char buf[2400], *r, *s;
  int i, wild=0, count=0;

  /* Find attribute if specified */
  if((s=strchr(name, '/')))
    *s++='\0';

  if(*name)
    if((thing=match_thing(player, name, MATCH_EVERYTHING|(player == GOD?
       MATCH_DESTROYED:0))) == NOTHING)
      return;

  /* Searching for a pattern in attributes */
  if(strchr(arg2, '*') || strchr(arg2, '?')) {
    if(!s)
      s="*";
    wild=1;  /* Use wildcard value matching */
  }

  /* Are we examining only one attribute on object? */
  if(s) {
    struct all_atr_list *al, *list;

    /* Match #num.attr format */
    if(!(*s == '#' && isdigit(*(s+1)) && (r=strchr(s+2, '.')) && *(r+1) &&
         !Invalid((match=atoi(s+1)))))
      r=NULL;

    /* Search attribute list */
    for(list=al=all_attributes(thing);al && al->type;al++) {
      if(r && al->type->obj == match) {
        if(!can_see_atr(player, thing, al->type)) {
          notify(player, "Permission denied.");
          return;
        }
        if(!wild_match(al->type->name, r+1, 0))
          continue;
      } else {
        if(!can_see_atr(player, thing, al->type))
          continue;
        if(!wild_match(al->type->name, s, 0))
          continue;
      }

      if(wild && !wild_match(uncompress(al->value), arg2, 0))
        continue;

      look_atr(player, thing, match, al);
      count++;
    }
    free(list);

    if(!count)
      notify(player, "No match.");
    return;
  }

  /* Display object header */
  notify(player, "%s", unparse_object(player, thing));
  if(*(s=atr_get(thing, A_DESC)) && can_see_atr(player, thing,
     atr_num(NOTHING, A_DESC)))
    notify(player, "\e[1mDesc:\e[m%s", s);
  else if(Typeof(thing) == TYPE_PLAYER && *(s=atr_parse(thing,player,A_PLAN)))
    notify(player, "%s", s);

  /* Display object owner */
  if(thing != db[thing].owner &&
     (!HIDDEN_ADMIN || class(db[thing].owner) != CLASS_WIZARD ||
      power(player, POW_DB)))
    notify(player, "\e[1mOwner:\e[m %s", db[db[thing].owner].name);

  *buf=0;
  if(*(s=unparse_flags(player, thing, 1)))
    sprintf(buf, "  \e[1mFlags:\e[m %s", s);
  notify(player, "\e[1mType:\e[m %s%s", typenames[Typeof(thing)], buf);
  if(Typeof(thing) == TYPE_ROOM && db[thing].zone) {
    *buf=0;
    for(ptr=db[thing].zone;ptr && *ptr != NOTHING;ptr++) {
      if(strlen(buf) > 1000) {
        strcat(buf, " ...");
        break;
      }
      strcat(buf, ", ");
      strcat(buf, unparse_object(player, *ptr));
    }
    notify(player, "\e[1mZone:\e[m%s\e[m", buf+1);
  }

  /* Show them short description of object if owned by someone else */
  if(!controls(player, thing, POW_EXAMINE) && !(db[thing].flags & VISIBLE)) { 
    look_atrs(player, thing, 0);  /* only show osee attributes */

    /* Check to see if there is anything there owned by other player */
    count=0;
    DOLIST(content, db[thing].contents)
      if(controls(player, content, POW_EXAMINE) &&
         match_plane(player, content)) {
        if(!count++)
          notify(player, "\e[1mContents:\e[m");
        notify(player, "%s", unparse_object(player, content));
      }
    return;
  }
  
  /* Begin regular examine routines and show stats on the object */
  
  /* Display monetary value */
  if(Typeof(thing) == TYPE_PLAYER) {
    *buf=0;
    notify(player, "\e[1m%s:\e[m %d%s", MONEY_PLURAL, db[thing].pennies, buf);
    notify(player, "\e[1mSessions:\e[m %d  \e[1mSteps:\e[m %d",
           db[thing].data->sessions, db[thing].data->steps);
  }

  /* Display memory usage */
  if(Typeof(thing) == TYPE_PLAYER)
    sprintf(buf, "  \e[1mTotal:\e[m %d", playmem_usage(thing));
  else
    *buf=0;
  notify(player, "\e[1mBytes:\e[m %d%s", mem_usage(thing), buf);

  /* Display dimensional plane value if not NOTHING */
  if(power(player, POW_DARK) && db[thing].plane != NOTHING &&
     Typeof(thing) != TYPE_ROOM && Typeof(thing) != TYPE_ZONE)
    notify(player, "\e[1mPlane:\e[m %d", db[thing].plane);

  /* Display creation/modification dates */
  if(db[thing].create_time > 0)
    notify(player, "\e[1mCreated:\e[m %s",
           mktm(player, db[thing].create_time));
  if(db[thing].mod_time > 0)
    notify(player, "\e[1mModified:\e[m %s",
           mktm(player, db[thing].mod_time));

  /* List parents */
  if(db[thing].parents) {
    *buf=0;
    for(ptr=db[thing].parents;ptr && *ptr != NOTHING;ptr++) {
      if(strlen(buf) > 1000) {
        strcat(buf, " ...");
        break;
      }
      strcat(buf, ", ");
      strcat(buf, unparse_object(player, *ptr));
    }
    notify(player, "\e[1mParents:\e[m%s", buf+1);
  }

  /* Show attributes */
  if(db[thing].atrdefs) {
    ATRDEF *k;

    count=0;
    for(k=db[thing].atrdefs;k;k=k->next) {
      if((k->atr.flags & AF_HIDDEN) && player != GOD)
        continue;
      if(!count++)
        notify(player, "\e[1mAttribute definitions:\e[m");
      notify(player, "  %s%s%s%s", k->atr.name,
             (k->atr.flags & AF_FUNC)?"()":"",
             (k->atr.flags)?": ":"", unparse_attrflags(k->atr.flags));
    }
  }

  /* 'All' option: View attributes inherited by parents also. */
  look_atrs(player, thing, !strcasecmp(arg2, "All")?1:0);
  
  /* 'Brief' option: No contents or links displayed. */
  if(!strcasecmp(arg2, "Brief"))
    return;

  /* Show him the contents */
  if(db[thing].contents != NOTHING) {
    int pow_plane=power(player, POW_PLANE);

    notify(player, "\e[1mContents:\e[m");
    DOLIST(content, db[thing].contents) {
      if(!pow_plane && !match_plane(player, content))
        continue;
      if(pow_plane && Typeof(thing) == TYPE_ROOM && db[content].plane)
        notify(player, "%s  \e[1;37m[\e[36mPlane: %d\e[37m]\e[m",
               unparse_object(player, content), db[content].plane);
      else
        notify(player, "%s", unparse_object(player, content));
    }
  }
  
  /* Last, display link/location factors */

  /* Rooms show entrances and exits */
  if(Typeof(thing) == TYPE_ROOM) {
    int var=controls(player, thing, POW_EXAMINE);

    for(i=0,count=0;i<db_top;i++)
      if(Typeof(i) == TYPE_EXIT && db[i].link == thing &&
         (var || controls(player, i, POW_EXAMINE))) {
        if(!count++)
          notify(player, "\e[1mEntrances:\e[m");
        notify(player, "%s", unparse_object(player, i));
      }
    if(!count)
      notify(player, "\e[1mNo entrances.\e[m");

    if(db[thing].exits != NOTHING) {
      notify(player, "\e[1mExits:\e[m");
      DOLIST(exit, db[thing].exits)
        notify(player, "%s", unparse_object(player, exit));
    } else
      notify(player, "\e[1mNo exits.\e[m");

    if(db[thing].link != NOTHING)
      notify(player, "\e[1mDropto:\e[m %s",
             unparse_object(player, db[thing].link));
    return;
  }

  /* Exits show source and destination */
  if(Typeof(thing) == TYPE_EXIT) {
    notify(player, "\e[1mSource:\e[m %s",
           unparse_object(player, db[thing].location));
    if(db[thing].link != NOTHING)
      notify(player, "\e[1mDestination:\e[m %s",
             unparse_object(player, db[thing].link));
    return;
  }

  /* Players, zones, and things have link/location */
  if(db[thing].location != NOTHING && Typeof(thing) != TYPE_ZONE)
    notify(player, "\e[1mLocation:\e[m %s",
           unparse_object(player, db[thing].location));
  if(db[thing].link != NOTHING)
    notify(player, "\e[1m%s:\e[m %s", Typeof(thing) == TYPE_ZONE?
           "Origin Room":"Home", unparse_object(player, db[thing].link));
}

/* list items and things in your inventory */
void do_inventory(dbref player)
{
  look_contents(player, player, "You are carrying:");
  /* display wealth */
  if(Typeof(player) == TYPE_PLAYER)
    notify(player, "You have \e[1;32m%d\e[m %s.", db[player].pennies,
           db[player].pennies==1?MONEY_SINGULAR:MONEY_PLURAL);
}

/* print the commands required to best recreate an object */
void do_decompile(dbref player, char *name)
{
  ATTR *atr;
  ALIST *a;
  ATRDEF *k;
  char objname[512], *s;
  int thing;

  if((thing=match_thing(player, name, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!(db[thing].flags & VISIBLE) && !controls(player, thing, POW_EXAMINE)) {
    notify(player, "Permission denied.");
    return;
  }

  switch(Typeof(thing)) {
    case TYPE_ROOM: strcpy(objname, "here"); break;
    case TYPE_ZONE: sprintf(objname, "#%d", thing); break;
    default:
      if(!strcasecmp(name, "me"))
        strcpy(objname, "me");
      else
        strcpy(objname, db[thing].name);
  }

  if(*(s=unparse_flags(player, thing, 1)))
    notify(player, "@set %s=%s", objname, s);

  for(k=db[thing].atrdefs;k;k=k->next) {
    if((k->atr.flags & AF_HIDDEN) && player != GOD)
      continue;
    notify(player, "@defattr %s/%s%s%s", objname, k->atr.name,
           (k->atr.flags)?"=":"", unparse_attrflags(k->atr.flags));
  }

  for(a=db[thing].attrs;a;a=a->next)
    if((atr=atr_num(a->obj, a->num)))
      if(!(atr->flags & AF_UNIMP) && can_see_atr(player, thing, atr))
        notify(player, "@set %s=%s:%s", objname,atr->name,uncompress(a->text));

}

/* Print object in human-readable format */
void db_print(FILE *f)
{
  ATTR *atr;
  ALIST *a;
  ATRDEF *k;
  int i;

  fputs("\n***Begin Dump***\n\n", f);

  for(i=0;i<db_top;i++) {
    fprintf(f, "%s(#%d%s)\n", db[i].name, i, unparse_flags(GOD, i, 0));
    fprintf(f, "Location: #%-10d Owner: %s\n",
            db[i].location, db[db[i].owner].name);
    fprintf(f, "Link:     #%-10d Type:  %s\n",
            db[i].link, typenames[Typeof(i)]);
    fprintf(f, "Bytes:    %-10d  Flags: %s\n",
            mem_usage(i), unparse_flags(GOD, i, 1));

    if(db[i].create_time)
      fprintf(f, "Created: %s\n", strtime(db[i].create_time));
    if(db[i].mod_time)
      fprintf(f, "Modified: %s\n", strtime(db[i].mod_time));
    for(k=db[i].atrdefs;k;k=k->next)
      fprintf(f, "  %s%s%s%s\n", k->atr.name, (k->atr.flags & AF_FUNC)?"()":"",
              k->atr.flags?": ":"", unparse_attrflags(k->atr.flags));

    for(a=db[i].attrs;a;a=a->next)
      if((atr=atr_num(a->obj, a->num)))
        fprintf(f, "%s:%s\n", atr->name, uncompress(a->text));
    fputs("\n", f);
  }

  fputs("***End of Dump***\n", f);
}

/* scope out the location of a distant player */
void do_whereis(dbref player, char *name)
{
  dbref thing;
  
  if((thing = lookup_player(name)) <= NOTHING) {
    notify(player, "No such player '%s'.", name);
    return;
  } if((!(db[thing].flags & PLAYER_CONNECT) || !could_doit(player, thing,
       A_HLOCK)) && !controls(player, thing, POW_WHO)) {
    notify(player, "That player is not connected.");
    return;
  }

  if(!controls(player, thing, POW_EXAMINE) && (db[thing].flags & DARK))
    notify(player, "%s wishes to have some privacy.", db[thing].name);
  else
    notify(player, "%s is at: %s", db[thing].name,
           unparse_object(player, db[thing].location));
}

/* view personal stats on a player */
void do_finger(player, arg1, arg2, argv, pure, cause)
dbref player, cause;
char *arg1, *arg2, **argv, *pure;
{
  char buf[9000], buff[40], *s, *gend;
  dbref thing;
  long tt;

  if(!*arg1) {
    notify(player, "You must specify a valid player name.");
    return;
  }

  if((thing=lookup_player(arg1)) == AMBIGUOUS)
    thing=player;
  if(Invalid(thing) || Typeof(thing) != TYPE_PLAYER) {
    notify(player, "No such player '%s'.", arg1);
    return;
  }
  gend=(db[thing].data->gender)?"She":"He";

  /* people using +finger on God do not see anything but his level & plan */
  if(thing == GOD) {
      notify(player, "%s", unparse_caption(player, thing));
    if(*(s=atr_parse(thing, player, A_PLAN)))
      notify(player, "%s", s);
    return;
  }

  if(Guest(thing))
    notify(player, "%s the %s.", db[thing].name,
           strcasecmp(GUEST_PREFIX, "Visitor")?"Visitor":"Guest");
  else
    notify(player, "%s", unparse_caption(player, thing));

  if(!could_doit(player,thing,A_PLOCK))
    notify(player, "%s is not accepting pages.", db[thing].name);

  /* Display either player's location (if locatable) and idle time, or display
   * when the player last logged on if the player is hidden or offline. */

  if(controls(player, thing, POW_WHO) || could_doit(player, thing, A_HLOCK)) {
    if(db[thing].flags & PLAYER_CONNECT) {
      if(!(db[thing].flags & DARK) || controls(player, thing, POW_EXAMINE))
        notify(player, "%s can be found at: %s", gend,
               unparse_object(player, db[thing].location));
      sprintf(buf, "is %s idle", time_format(TMA, getidletime(thing)));
      if(*(s=atr_parse(thing, player, A_IDLE)))
        strcat(buf, tprintf(": %s", s));
      else
        strcat(buf, ".");
    } else {
      if(!(tt=db[thing].data->last))
        sprintf(buf, "has never connected.");
      else
        sprintf(buf, "hasn't connected in %s.", time_format(TMA, now-tt));
    }
    notify(player, "%s is %s old, and %s", gend,
           time_format(TMA, db[thing].data->age), buf);
    if(!(db[thing].flags & PLAYER_CONNECT) && *(s=atr_get(thing, A_AWAY)))
      notify(player, "Away message from %s: %s", db[thing].name, s);
  }

  if(power(player, POW_WHO) && controls(player, thing, POW_WHO) &&
     *(s=atr_get(thing, A_LASTFROM)))
    notify(player, "%s last connected from the address %s.", gend, s);

  strcpy(buff, gender_subst(thing, POSS));
  *buff=toupper(*buff);
  if(db[thing].flags&PLAYER_CONNECT && (controls(player,thing,POW_WHO) ||
     could_doit(player,thing,A_HLOCK))) {
    if(*(s=atr_parse(thing, player, A_DOING)))
      notify(player, "%s character is currently doing: %s", buff, s);
    else
      notify(player, "%s doesn't seem to be doing anything at the moment.",
             gend);
  }
  if(Immortal(player) || terminfo(thing, CONF_REGINFO)) {
    if(*(s=atr_get(thing, A_RLNAME)))
      notify(player, "%s Real Life Name is %s.", buff, s);
    if(*(s=atr_get(thing, A_EMAIL)))
      notify(player, "%s Email Address is %s", buff, s);
  }

  /* anything in <arg2> disables printing the user's @plan */
  if(*arg2)
    return;

  /* show plan */
  if(*(s=atr_parse(thing, player, A_PLAN))) {
    notify(player, "Plan:");
    notify(player, "%s", s);
  } else
    notify(player, "No plan.");

  /* show playerfile (admin only) */
  if(player != thing && controls(player, thing, POW_PFILE) &&
     *(s=parse_function(player, player, atr_get(thing, A_PFILE)))) {
    notify(player, "%s", "");
    notify(player, "Incident Report:");
    notify(player, "%s", s);
  }
}
