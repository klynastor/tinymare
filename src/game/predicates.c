/* game/predicates.c */
/* predicates for testing various conditions */

#include "externs.h"


// Returns 1 if <who> can see attribute <atr> on object <what>.
//
int can_see_atr(dbref who, dbref what, ATTR *atr)
{
  if((atr->flags & AF_HIDDEN) && who != GOD)
    return 0;
  if(!(atr->flags & AF_OSEE) && !controls(who, what, POW_EXAMINE) &&
     !(db[what].flags & VISIBLE))
    return 0;
  if(atr->flags & AF_DARK) {
    if(atr->obj == NOTHING && !power(who, POW_SECURITY))
      return 0;
    if(atr->obj != NOTHING && !(power(who, POW_SECURITY) || ((atr->obj == what)
       && controls(who, what, POW_SECURITY))))
      return 0;
  }
  return 1;
}

// Returns 1 if <who> can SET attribute <atr> on object <what>.
//
int can_set_atr(dbref who, dbref what, ATTR *atr)
{
  if(!controls(who, what, POW_MODIFY))
    return 0;
  if(!can_see_atr(who, what, atr))
    return 0;
  if(atr->flags & AF_WIZ) {
    if(atr->obj == NOTHING && !power(who, POW_WIZATTR))
      return 0;
    if(atr->obj != NOTHING && !controls(who, atr->obj, POW_WIZATTR))
      return 0;
  }
  if(atr->flags & AF_COMBAT) {
    if(atr->obj == NOTHING && !power(who, POW_COMBAT))
      return 0;
    if(atr->obj != NOTHING && !controls(who, atr->obj, POW_COMBAT))
      return 0;
  }
  if((atr->flags & AF_PRIVS) && atr->obj != what)
    return 0;
  return 1;
}

// Internal function used to find the main container of an object, by
// recursively following its location up to the Room of origin.
//
// If player is a valid object, this function is guaranteed to return a valid
// dbref# even if its location is NOTHING. If the player is inside itself or
// inside of an object inside itself, this returns the dbref# of the player.
//
int mainroom(dbref player)
{
  int dep;
  dbref var=player;

  if(Invalid(player) || Invalid(db[player].location))
    return player;

  for(dep=0;dep<100;dep++) {
    if(db[var].location == var)
      return var;
    var=db[var].location;
    if(Invalid(var))
      return db[player].location;
    if(Typeof(var) == TYPE_ROOM)
      return var;
  }

  return db[player].location;
}

// Returns 1 if a player is able to get the location# of an object/player.
//
int locatable(dbref player, dbref it)
{
  dbref room;

  /* No sense if trying to locate a bad object. */
  if(Invalid(it))
    return 0;

  /* Succeed if we can examine the target, if we are the target,
   * or if we can examine the location. */

  if((db[player].location == db[it].location && match_plane(player, it)) ||
     db[player].location == it || controls(player, it, POW_FUNCTIONS) ||
     controls(player, db[it].location, POW_FUNCTIONS))
    return 1;

  /* Succeed if we control the containing room or if it is a findable
   * player and the containing room is also findable. */

  room=mainroom(it);

  if(controls(player, room, POW_FUNCTIONS) ||
     (Typeof(it) == TYPE_PLAYER && !(db[it].flags & DARK) &&
     (IS(room, TYPE_ROOM, ROOM_JUMP_OK|ROOM_ABODE|ROOM_LINK_OK) ||
      (db[room].flags & VISIBLE))))
    return 1;

  /* Succeed if the target is in a room zoned to the inactor. 
   * This does not work on any universal zones owned by the player. */

  if(Typeof(room) == TYPE_ROOM && inlist(db[room].zone, player))
    return 1;

  return 0;  /* We can't do it. */
}

// Returns 1 if player is in thing's location, player is the container for
// thing, or thing is the container for player. If both are in the same
// location, then and only then must both be in the same plane.
//
int nearby(dbref player, dbref thing)
{
  /* Check valid objects */
  if(Invalid(player) || Invalid(thing) || db[player].location == NOTHING ||
     db[thing].location == NOTHING)
    return 0;

  return (player == db[thing].location || db[player].location == thing ||
          (db[player].location == db[thing].location &&
           match_plane(player, thing)));
}

// Returns 1 if player is in the same plane as thing. Rooms and Zones do not
// have planes and can see everything or be seen by everything. Objects not in
// a room have the same plane as their container (and thus can only be seen by
// those who see the container). Objects whose planes are -1 coexist in all
// planes.
//
int match_plane(dbref player, dbref thing)
{
  if(Typeof(player) == TYPE_ROOM || Typeof(player) == TYPE_ZONE ||
     Typeof(thing) == TYPE_ROOM || Typeof(thing) == TYPE_ZONE)
    return 1;

  if(db[player].location == NOTHING || db[thing].location == NOTHING ||
     Typeof(db[player].location) != TYPE_ROOM ||
     Typeof(db[thing].location) != TYPE_ROOM)
    return 1;

  return (db[player].plane == db[thing].plane || db[player].plane == -1 ||
          db[thing].plane == -1);
}

// Finds the shortest path between <player> and <target>, returning a list of
// exits to take up to <depth> tries. Returns NULL if <target> is unfindable
// given the size of <depth>. If <privs> is not NOTHING, each room the player
// enters is checked to make sure privs can control it using POW_FUNCTIONS.
//
// Variable exits, destination planes, @locks, and @vlocks are taken into
// account based on the player's function evaluation for each room moved.
//
dbref *find_path(dbref player, dbref target, int max_depth, dbref privs)
{
  int exits[max_depth], solution[max_depth], planes[max_depth], *rooms;
  int depth=0, steps=max_depth+1, plane, orig_func=func_total;
  int i, top_room=1, total_rooms=300, dest_plane;
  dbref *list, link, dest=db[target].location;

  if((Typeof(player) != TYPE_PLAYER && Typeof(player) != TYPE_THING) ||
     Typeof(dest) != TYPE_ROOM ||
     (db[player].location == dest && match_plane(player, target)))
    return NULL;

  /* Set up initial state */
  dest_plane=(Typeof(target) == TYPE_ROOM)?-1:db[target].plane;
  exits[0]=db[db[player].location].exits;
  planes[0]=db[player].plane;

  rooms=malloc(300*sizeof(int));
  rooms[0]=db[player].location;
  rooms[1]=db[player].plane;
  rooms[2]=0;

  /* Perform depth-first search */
  while(1) {
    if(exits[depth] == NOTHING) {
      if(!depth--)
        break;
      db[player].plane=planes[depth];
      exits[depth]=db[exits[depth]].next;
      continue;
    }

    if(match_plane(player, exits[depth])) {
      func_total=0;
      if(db[exits[depth]].link == AMBIGUOUS)
        link=resolve_link(player, exits[depth], &plane);
      else {
        link=db[exits[depth]].link;
        plane=-1;
        if(link == HOME) {
          link=db[player].link;
        }
      }

      /* Make sure we have privileges to scan this far */
      if(link == NOTHING || (privs != NOTHING &&
          !controls(privs, link, POW_FUNCTIONS))) {
        exits[depth]=db[exits[depth]].next;
        continue;
      }
      if(plane == -1)
        plane=db[player].plane;

      /* See if room:plane combination is already in our visited-list, and
         only try it if it takes less steps to get there than before. */
      for(i=0;i < top_room;i++)
        if(rooms[i*3] == link && (rooms[i*3+1] == -1 || plane == rooms[i*3+1]))
          break;

      if((i == top_room || depth < rooms[i*3+2]) &&
         could_doit(player, exits[depth], A_VLOCK) &&
         could_doit(player, exits[depth], A_LOCK) &&
         can_move(player, player, link, 4)) {
        /* Check if the target is in the destination room */
        if(dest == link && (dest_plane == -1 ||
            plane == -1 || dest_plane == plane)) {
          steps=depth+1;
          memcpy(solution, exits, steps*sizeof(int));
          exits[depth]=NOTHING;
          continue;
        }

        if(depth+1 < max_depth && depth+2 < steps) {
          exits[++depth]=db[link].exits;
          planes[depth]=plane;
          db[player].plane=plane;

          if(i == top_room) {
            if(!(top_room++ % 100))
              rooms=realloc(rooms, (total_rooms+=300)*sizeof(int));
            rooms[i*3]=link;
            rooms[i*3+1]=plane;
          }
          rooms[i*3+2]=depth-1;
          continue;
        }
      }
    }

    exits[depth]=db[exits[depth]].next;
  }

  free(rooms);
  func_total=orig_func;

  if(steps == max_depth+1)
    return NULL;

  /* Return solution */
  list=malloc((steps+1)*sizeof(int));
  memcpy(list, solution, steps*sizeof(int));
  list[steps]=NOTHING;
  return list;
}

// Return a random exit visible and accessible by the player.
// If loc != NOTHING, then ignore exits leading to <loc>.
// If loc == AMBIGUOUS, then enable picking locked exits.
//
dbref random_exit(dbref player, dbref loc)
{
  INIT_STACK;
  dbref obj, link;
  int a;

  if(Invalid(player) || Invalid(db[player].location))
    return NOTHING;

  // Pass 1: Pick a random exit visible to the player.
  // Pass 2: Pick an exit that would have been visible if the room was lit.
  // Pass 3: Pick any remaining dark, unlocked exits.

  for(a=0;a<3;a++) {
    if(a)
      CLEAR_STACK;

    DOLIST(obj, db[db[player].location].exits) {
      if(db[obj].link == NOTHING)
        continue;

      /* Match exits based on current pass */
      if(a == 0 && !can_see(player, obj))
        continue;
      if(a > 0 &&
         (!match_plane(player, obj) || !could_doit(player, obj, A_VLOCK)))
        continue;
      if(a == 1 && (db[obj].flags & DARK))
        continue;

      /* Validate exit's link */
      if(loc != AMBIGUOUS) {
        if(!could_doit(player, obj, A_LOCK))
          continue;
        link=db[obj].link == AMBIGUOUS ? resolve_link(player, obj, NULL) :
             db[obj].link == HOME ? db[player].link : db[obj].link;
        if(link == NOTHING || !can_move(player, player, link, 4))
          continue;
      }

      PUSH(obj);
    }

    if(!STACKTOP())
      return STACKELEM(dbref, rand_num(STACKSZ(dbref)));
  }

  return NOTHING;
}

// Returns the contents of the first non-blank attribute found in the zone
// list of loc(player), or "" if not found.
//
char *atr_zone(dbref player, unsigned char num)
{
  dbref *zone;
  char *s;

  for(zone=Zone(player);zone && *zone != NOTHING;zone++)
    if(*(s=atr_get(*zone, num)))
      return s;
  return "";
}

// Same as above, but parses the attribute returned.
// Note: A blank string can still be returned by an attr that parses to "".
// If <room> = 1, then also check for the attribute on the player's location.
//
char *parse_atr_zone(dbref loc, dbref player, unsigned char num, int room)
{
  static char buf[8192];
  dbref *zone;
  char *s;

  loc=mainroom(loc);

  /* Check attribute on room first */
  if(room && *(s=atr_get(loc, num))) {
    int len=strlen(s)+1;
    char text[len];

    memcpy(text, s, len);
    pronoun_substitute(buf, loc, num, text);
    if(*buf)
      return buf;
  }

  for(zone=Zone(loc);zone && *zone != NOTHING;zone++)
    if(*(s=atr_get(*zone, num))) {
      int len=strlen(s)+1;
      char text[len];

      memcpy(text, s, len);
      pronoun_substitute(buf, *zone, player, text);
      return buf;
    }

  return "";
}

// Search all zones on <loc> and return 1 if any flag matches.
//
int is_zone(dbref loc, int flag)
{
  dbref *zone;

  for(zone=Zone(loc);zone && *zone != NOTHING;zone++)
    if(db[*zone].flags & flag)
      return 1;
  return 0;
}


// Charge Building Expense
//
// Type=0: Charge amount without notification
//      1: Creating item in database, checks POW_FREE and notifies
//      2: Checks amount available but does not subtract
int charge_money(dbref player, int cost, int type)
{
  player=db[player].owner;
  if(cost < 1 || (type && power(player, POW_FREE)))
    return 1;
  if(db[player].pennies < cost) {
    if(type)
      notify(player, "That costs %d %s.", cost, cost==1?
             MONEY_SINGULAR:MONEY_PLURAL);
    return 0;
  }

  if(type != 2) {
    db[player].pennies -= cost;
  }
  return 1;
}

// Modifies a player's money value.
//
void giveto(dbref player, int pennies)
{
  player=db[player].owner;

  db[player].pennies+=pennies;

  if(db[player].pennies < 0)
    db[player].pennies=0;
  else if(db[player].pennies > 999999999)
    db[player].pennies=999999999;

}


// Returns gender number (0 to 3) specific to <player>.
//
int get_gender(dbref player)
{
  char gender;

  if(Typeof(player) == TYPE_PLAYER)
    return db[player].data->gender;  /* player's gender flag */

  /* determine gender by first character of attribute */

  gender=tolower(*atr_get(player, A_SEX));
  if(gender == 'm')
    return MALE;
  if(gender == 'f' || gender == 'w')
    return FEMALE;
  if(gender == 'p')
    return PLURAL;
  return NEUTER;
}

// Returns pronoun for gender substitution.
// <Type> may be one of: SUBJ, OBJN, POSS, or APOSS.
//
char *gender_subst(dbref player, int type)
{
  static char *subst[16]={"he",  "she",  "it",  "they",
			  "him", "her",  "it",  "them",
			  "his", "her",  "its", "their",
			  "his", "hers", "its", "theirs"};

  return subst[type+get_gender(player)];
}

// Returns a copy of string <str> with important symbol characters turned
// into spaces. Such a string can be used with %-substitution and not risk
// side-effects when executed by either the command or function parser.
//
// The buffer of <str> itself is overwritten with the new secure string.
//
char *secure_string(char *str)
{
  char *r=str, *s=str;

  while(*s) {
    /* Escape Character? Copy ansi codes */
    if(*s == '\e') {
      if(*(s+1)) {
        *r++=*s++;
        if(*s == '[') {  /* variable-length CSI code */
          *r++=*s++;
          while(*s && *s < 64)
            *r++=*s++;
        } else if(*s && *s < 48)  /* 2-byte code */
          *r++=*s++;
        if(*s)
          *r++=*s++;
      } else
        s++;
      continue;
    }

    /* Set all matching characters in string to spaces. Insert a space only if
       the previous character was not a space. */
    if(strchr("()[]{}^$%;,\n\t\\", *s)) {
      if(r > str && *(r-1) != ' ')
        *r++=' ';
    } else
      *r++=*s;

    s++;
  }

  /* Trim ending spaces */
  while(r > str && *(r-1) == ' ')
    r--;
  *r='\0';

  return str;
}

// Returns 1 if <name> can be used for a valid attribute name.
//
int ok_attribute_name(char *name)
{
  char *s;

  if(!*name || strlen(name) > 64)
    return 0;

  for(s=name;*s;s++)
    if(!(isalpha(*s) || (*s > ' ' && *s <= '9' && *s != '(' && *s != ')') ||
       *s == '_') || *s == '.')
      return 0;
  return 1;
}

// Returns 1 if <name> is an acceptable filename to view a textfile with.
// It is a main security precaution against files beginning with . or /
//
int ok_filename(char *name)
{
  char *s=name;
  int dot=1;

  if(!*name || strlen(name) > 250)
    return 0;

  /* main directory . is always safe to read */
  if(!strcmp(name, "."))
    return 1;

  while(*s) {
    if(*s < 33 || *s > 126 || (dot && *s == '.'))
      return 0;
    dot=(*s++ == '/')?1:0;
  }
  return 1;
}

// Returns 1 if <name> can be used for the name of a database object.
//
int ok_name(char *name)
{
  char *s;

  if(!*name || *name == '*' || *name == '#' || *name == '!' || *name == ' ' ||
     *name == '.' || *name == '_' || *name == '$' || *name == '&' ||
     *name == '\'')
    return 0;

  for(s=name;*s;s++)
    if(*s == ';' || *s == '=' || *s < 32 || (*s > 90 && *s < 95) || *s > 122)
      return 0;

  if(!*name || strlen(name) > 50 || match_word("me home here quit", name))
    return 0;

  return 1;
}

// Returns 1 if <name> can be used for the name of a spell/skill/technique.
//
int ok_itemname(char *name)
{
  char *s;

  if(!isalnum(*name) || strlen(name) > 64)
    return 0;

  for(s=name+1;*s;s++)
    if(*s == ',' || *s == ':' || *s == ';' || *s == '=' || *s < 32 ||
       (*s > 90 && *s < 95) || *s > 122)
      return 0;

  return 1;
}

// Checks if a player can be named <name>. 
//
// Type=0: Player is setting his own name. Must be 3-16 characters long.
//      1: Player is setting his alias. Unrestricted length.
//
int ok_player_name(char *name, int type)
{
  static char *rstr_list;
  static int rstr_size=0;
  FILE *f;
  char buf[512], *r, *s, *ptr;
  int j;

  if((!type && (isdigit(*name) || strlen(name) < 3)) || strlen(name) > 16)
    return 0;
  if(!ok_name(name) || !strcasecmp(name, MUD_NAME))
    return 0;

  /* Load restricted-names list from msgs/badplayers.txt (if not in memory) */
  if(!rstr_list) {
    if(!(f=fopen("msgs/badplayers.txt", "r"))) {
      if(rstr_size != -1) {
        perror("msgs/badplayers.txt");
        rstr_size=-1;
      }
    } else {
      rstr_size=j=0;
      while(ftext(buf, 512, f)) {
        if((s=strchr(buf, '#')))
          *s='\0';
        s=buf;
        while(1) {
          while(isspace(*s))
            s++;
          if(!*s)
            break;
          r=s;
          while(*s && !isspace(*s))
            s++;
          if(j+(s-r)+2 >= rstr_size) {
            rstr_size+=1024;
            if(!(ptr=realloc(rstr_list, rstr_size))) {
              perror("ok_player_name()");
              free(rstr_list);
              rstr_list=NULL;
              rstr_size=0;
              fclose(f);
              goto out;
            }
            rstr_list=ptr;
          }
          if(j)
            rstr_list[j++]=' ';
          memcpy(rstr_list+j, r, s-r);
          j+=s-r;
        }
      }
      rstr_list[j]='\0';
      fclose(f);
    }
  }

  /* Return 0 if <name> is one of the restricted names */
  if(rstr_list && match_word(rstr_list, name))
    return 0;

out:

  for(s=name;*s;s++)
    if((*s < 48 || (*s > 57 && *s < 65)) &&
       *s != '.' && *s != '-' && *s != '\'')
      return 0;

  if(string_prefix(name, GUEST_PREFIX))
    return 0;
  if(lookup_player(name) != NOTHING)
    return 0;

  return 1;
}

// Returns 1 if <password> is an acceptable game password.
//
int ok_password(char *password)
{
  int len=strlen(password);
  char *s;

  if(len < 3 || *password < 33 || password[len-1] < 33)
    return 0;

  for(s=password;*s;s++)
    if(*s < 32 || *s > 126)
      return 0;
  return 1;
}                  

// Returns the suffix to be used for possessive form given the name.
//
char *possname(char *name)
{
  if(!name || !*name)
    return "";
  if(name[strlen(name)-1] == 's')
    return "'";
  return "'s";
}

// Pluralizes a title of an object or game item. Obeys simple grammar rules.
//
char *pluralize(char *name)
{
  static char *common[]={
    "fish", "zucchini", "ravioli", "spaghetti", "tortellini", "ramen",
    "moose", "deer"};
  static char *changed[]={
    "tooth", "teeth", "goose", "geese", "louse", "lice", "mouse", "mice",
    "cactus", "cacti", "foot", "feet", "child", "children", "go", "goes"};

  static char buf[8192];
  char *r, *t;
  int i, len;

  if(!*name)
    return "";

  // Copy over the part of the name we want to pluralize.
  // Phrases containing 'of' pluralize the word before the 'of'.

  for(r=name+1;*r;r++)
    if(!strncasecmp(r, " of ", 4)) {
      strncpy(buf, name, r-name);
      buf[r-name]='\0';
      break;
    }
  if(!*r)
    strcpy(buf, name);
  t=strnul(buf);

  // Do not make changes to words that end with common word endings.

  for(i=0;i < NELEM(common);i++) {
    len=strlen(common[i]);
    if(t-buf >= len && !strcasecmp(t-len, common[i])) {
      strcpy(t, r);
      return buf;
    }
  }
  for(i=0;i < NELEM(changed);i+=2) {
    len=strlen(changed[i+1]);
    if(t-buf >= len && !strcasecmp(t-len, changed[i+1])) {
      strcpy(t, r);
      return buf;
    }
  }

  // Replace common word endings with their appropriate plural counterpart.

  for(i=0;i < NELEM(changed);i+=2) {
    len=strlen(changed[i]);
    if(t-buf >= len && !strcasecmp(t-len, changed[i])) {
      t-=len;
      *t=(*t >= 'A' && *t <= 'Z')?toupper(*changed[i+1]):*changed[i+1];
      memcpy(t+1, changed[i+1]+1, len-1);
      strcpy(t+len, r);
      return buf;
    }
  }

  if(t-buf == 1)
    *t++='s';
  else if(*(t-1) == 'x' ||
          (t-buf > 2 && (((*(t-2) == 'c' || *(t-2) == 's') && *(t-1) == 'h') ||
           (*(t-2) == 'z' && *(t-1) == 'z')))) {
    *t++='e';
    *t++='s';
  } else if(t-buf > 2 && *(t-1) == 'y' && *(t-2) != 'a' && *(t-2) != 'e' &&
            *(t-2) != 'i' && *(t-2) != 'o' && *(t-2) != 'u') {
    *(t-1) = 'i';
    *t++='e';
    *t++='s';
  } else if(*(t-1) != 's')
    *t++='s';

  strcpy(t, r);
  return buf;
}

// Determines if the article prefixing <name> should be 'a' or 'an'.
//
char *article(char *name)
{
  static char *common[]={"pants", "socks", "boots", "knuckles", "sandals",
    "sneakers", "shoes", "shorts", "stockings", "slippers", "moccasins",
    "slacks", "jodhpurs"};

  char *t=strnul(name);
  int i, len;

  /* Check for common plural words */
  if(t > name && (*(t-1)|32) == 's')
    for(i=0;i < NELEM(common);i++) {
      len=strlen(common[i]);
      if(t-len > name && !strcasecmp(t-len, common[i]))
        return "a pair of";
    }

  return (strchr("AEIOUaeiou", *name) || ((*name|32) == 'h' &&
          (!strncasecmp(name, "hour", 4) ||
           !strncasecmp(name, "herb", 4))) ? "an":"a");
}

// Pass a string through the function parser without using additional buffers.
// privs=%!, player=%#.
//
// Warning: Only pass a function such as atr_get or uncompress if the result
// is not copied immediately after this function is called.
//
char *parse_function(dbref privs, dbref player, char *str)
{
  static char buf[8192];
  int len=strlen(str)+1;
  char text[len];

  if(len > 1) {
    memcpy(text, str, len);
    pronoun_substitute(buf, privs, player, text);
  } else
    *buf='\0';
  return buf;
}

// Parse string 'str' into specified buffer space 'buf', max size 8000 chars.
// privs=%!, player=%#. 'buf' and 'str' are buffers supplied by the caller.
//
// Warning: Depending on the data in <str>, this function may be recursive.
// Do NOT call this function with <str> as atr_get, uncompress, etc. because
// the static char* in those functions can be overwritten with subsequent
// calls. Only call this function with a 'char name[]' that was defined in
// the caller's stack. (See function above for example).
//
void pronoun_substitute(char *buf, dbref privs, dbref player, char *str)
{
  char temp[8192], c, *p, *r, *s;
  int var, pos=0, color=default_color;

#ifdef DEBUG
  /* Catch invalid calls to the pronoun parser */
  if(Invalid(privs) || Invalid(player)) {
    log_error("FUNC: #%d<-#%d %s", privs, player, str);
    *buf='\0';
    return;
  }
#endif

  for(s=str;*s && pos < 8000;s++) {
    /* Function Substitution */
    if(*s == '[') {
      r=s++; p=temp;
      while(*s && *s != '(')
        *p++=*s++;
      *p='\0';
      if(*s++ == '(' && function(&s, temp, privs, player)) {
        temp[8000-pos]=0;
        strcpy(buf+pos, temp);
        pos=strlen(buf);
        if(*s != ']')
          s--;
        continue;
      }
      s=r;
    }

    /* Environment Variable Substitution */
    if(*s == '%') {
      c=*(++s); *temp=0;
      switch(tolower(c)) {
      case '0' ... '9':
        strcpy(temp, env[c-'0']);
        secure_string(temp);
        break;
      case 'a':
        strcpy(temp, gender_subst(player, APOSS)); break;
      case 'b':
        buf[pos++]=' '; break;

      case 'g':
        buf[pos++]='\a'; break;
      case 'l':
        sprintf(temp, "#%d", db[player].location); break;
      case 'n':
        strcpy(temp, db[player].name); break;
      case 'o':
        strcpy(temp, gender_subst(player, OBJN)); break;
      case 'p':
        strcpy(temp, gender_subst(player, POSS)); break;
      case 'r':
        buf[pos++]='\n'; break;
      case 's':
        strcpy(temp, gender_subst(player, SUBJ)); break;
      case 't':
        buf[pos++]='\t'; break;
      case 'u':
        strcpy(temp, db[privs].name); break;
      case 'v':
        if(!(var=tolower(*(++s)))) {
          s--;
          break;
        } if(var < 'a' || var > 'z')
          buf[pos++]=*s;
        else
          strcpy(temp, atr_get(privs, 100+var-'a'));

        /* Secure string */
        for(var=0;temp[var];var++)
          if(temp[var] == ';' || temp[var] == ',')
            temp[var]=' ';
        break;
      case 'x':
        if(!*++s) {
          s--;
          break;
        }
        var=0;
        p=s;
        while(*p && !isdigit(*p)) {
          temp[var++]=*p;
          if(*(p+1) != '%' || (*(p+2) != 'x' && *(p+2) != 'X'))
            break;
          p+=3;
        }
        if(!*(s=p)) {
          s--;
          *temp='\0';
        } else {
          temp[var]='\0';
          var=color_code(default_color, temp);
          strcpy(temp, textcolor(default_color, var));
          default_color=var;
        }
        break;
      case '!':
        sprintf(temp, "#%d", privs); break;
      case '#':
        sprintf(temp, "#%d", player); break;
      case '$':
        *temp='\0'; break;  /* Reserved for program accumulator */
      case '?':
        sprintf(temp, "%d %d", func_total, func_recur); break;
      case '|':
        for(p=s+1;*p && *p != '|';p++);
        if(!*p)
          break;
        *p='\0';
        var=color_code(7, s+1);
        strcpy(temp, textcolor(default_color, var));
        default_color=var;
        *p='|';
        s=p;
        break;
      default:
        buf[pos++]=*s;
      }

      if(*temp) {
        if(c >= 'A' && c <= 'Z')  /* Capitalize first character */
          *temp=toupper(*temp);
        temp[8000-pos]=0;

        strcpy(buf+pos, temp);
        pos=strlen(buf);
      }
      continue;
    }

    if(*s == 27) {  /* Escape character */
      buf[pos++]=27;
      if(*(s+1))
        buf[pos++]=(*(++s));
      continue;
    } if(*s == '\\') {  /* Literal escape */
      if(!(var=(*(++s)))) {
        s--;
        break;
      }
      buf[pos++]=var;
      continue;
    }

    buf[pos++]=*s;
  }

  buf[pos]='\0';

  /* Set color back to default */
  if(color != default_color) {
    strcpy(temp, textcolor(default_color, color));
    if(strlen(buf)+strlen(temp) > 8000)
      strcpy(buf+8000-strlen(temp), temp);
    else
      strcat(buf, temp);
    default_color=color;
  }
}


// Triggers the Succ, Osucc, and Asucc attribute group for a specific action.
// Player is the enactor %#, and <thing> is the object doing the commands (%!).
//
// Succ is displayed to the player. Osucc is displayed to all others in room.
// Asucc is an attribute that contains commands to be executed by <thing>.
// <emit> and <oemit> contain messages that get sent in place of Succ and
// Osucc, respectively, if those attributes are empty.
//
// All arguments except <player> & <thing> are optional (can be 0 or NULL).
//
void did_it(player, thing, Succ, emit, Osucc, oemit, Asucc)
dbref player, thing;
unsigned char Succ, Osucc, Asucc;
char *emit, *oemit;
{
  char *s;

  /* Execute Action */
  if(Asucc && *(s=atr_get(thing, Asucc)))
    parse_que(thing, s, player);

  /* Notify Player */
  if((Succ && *(s=atr_parse(thing, player, Succ))) || ((s=emit) && *s))
    notify(player, "%s", s);

  /* Check lock on @oemit */
  if(!pass_slock(player, db[player].location, 0))
    return;

  /* Notify Others */
  if((Osucc && *(s=atr_parse(thing, player, Osucc))) || ((s=oemit) && *s)) {
    if(*s == '@') {
      s++;
      if(*s && can_emit_msg(thing, player, db[player].location, s, 0))
        notify_except(player, player, "%s", s);
    } else
      notify_except(player, player, "%s %s", db[player].name, s);
  }
}

// Exactly like the function above, except the player is displayed the Succ
// message (atr1 or msg1) in color according to the @color of <thing>.
//
void ansi_did_it(player, thing, atr1, msg1, atr2, msg2, atr3)
dbref player, thing;
unsigned char atr1, atr2, atr3;
char *msg1, *msg2;
{
  char *s;

  /* Execute actions */
  did_it(player, thing, 0, NULL, atr2, msg2, atr3);

  if(!atr1 && !msg1)
    return;

  /* Build output */
  default_color=unparse_color(player, thing, 4);
  if(!atr1 || !*(s=atr_parse(thing, player, atr1)))
    s=msg1;

  if(s && *s)
    notify(player, "%s%s%s", textcolor(7, default_color), s,
           textcolor(default_color, 7));
  default_color=7;
}

// Returns 1 if a player is in <thing>, or in a container in <thing>.
//
int hold_player(dbref thing)
{
  dbref inside;

  DOLIST(thing, db[thing].contents) {
    if(Typeof(thing) == TYPE_PLAYER)
      return 1;
    DOLIST(inside, thing)
      if(Typeof(inside) == TYPE_PLAYER)
        return 1;
  }
  return 0;
}

// Count total memory used by an object.
//
int mem_usage(dbref thing)
{
  ATRDEF *j; ALIST *m;
  int k, *ptr, L=sizeof(void *);

  k = sizeof(struct object)+L;
  if(db[thing].name)
    k += strlen(db[thing].name)+L+1;

  if((ptr=db[thing].parents)) {
    for(;ptr && *ptr != NOTHING;ptr++) k += sizeof(dbref);
    k += sizeof(dbref)+L;
  } if((ptr=db[thing].children)) {
    for(;ptr && *ptr != NOTHING;ptr++) k += sizeof(dbref);
    k += sizeof(dbref)+L;
  } if((ptr=db[thing].zone)) {
    for(;ptr && *ptr != NOTHING;ptr++) k += sizeof(dbref);
    k += sizeof(dbref)+L;
  }

  if(db[thing].data)
    k += sizeof(struct playerdata)+L;

  /* add in attribute memory */
  for(m=db[thing].attrs;m;m=m->next)
    k += sizeof(ALIST)+strlen(m->text)+L+1;
  for(j=db[thing].atrdefs;j;j=j->next) {
    k += sizeof(ATRDEF)+L;
    k += strlen(j->atr.name)+L+1;
  }
  return k;
}

// Counts total memory used by a player.
//
int playmem_usage(dbref player)
{
  int a, b=0;

  for(a=0;a<db_top;a++)
    if(db[a].owner == player)
      b += mem_usage(a);
  return b;
}
