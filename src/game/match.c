/* game/match.c */
/* Matching routines for finding objects in room, inventory, or database. */

#include "externs.h"

/* 1=Suppress "I don't see '...' here." message, for function calls */
int match_quiet;

static int match_exit(dbref player, char *match_name, char *match_prefix,
                      dbref absolute, int tpye, int *count, int accept);
static int match_list(dbref player, dbref loc, char *match_name,
                      char *match_prefix, dbref absolute, int type, int *full,
                      int *count, int accept);


// Internal function to return a single #dbref, or the keywords NOTHING,
// AMBIGUOUS, or HOME depending on the types of matches allowed. Used virtually
// everywhere.
//
dbref match_thing(dbref player, char *name, int type)
{
  char shortname[512], *r, *s;
  dbref match_abs=NOTHING, exact_match=NOTHING, obj, alt_location;
  int full, *ptr, count=0, accept;

  if(Invalid(player))
    return NOTHING;
  if(!*name) {
    if(!(type & MATCH_QUIET) && !match_quiet)
      notify(player, "No object specified.");
    return NOTHING;
  }

  strmaxcpy(shortname, name, 512);

  /* Fullname: When matching players, only accept if full name is typed */
  full=(type & MATCH_FULLNAME)?1:0;

  /* accept: Player wants to match Nth item of same name */
  accept=1;
  if((s=strrchr(shortname, ' '))) {
    if(isnumber(s+1)) {
      accept=atoi(s+1);
      *s='\0';
    }
  }

  /* Find absolute global database reference number */
  if(*name == '#' && isdigit(*(name+1)) && !Invalid(atoi(name+1)))
    match_abs = atoi(name+1);

  /* Match dbref */
  if(type & MATCH_ABS)
    exact_match = match_abs;

  /* Match player name */
  if((type & MATCH_PLAYER) && exact_match == NOTHING && *name == '*' &&
     (obj=lookup_player(name+1)) > NOTHING)
    exact_match = obj;

  /* Match reflexive */
  if((type & MATCH_ME) && exact_match == NOTHING)
    if(!strcasecmp(name, "me"))
      exact_match = player;

  /* Match home */
  if((type & MATCH_HOME) && exact_match == NOTHING)
    if(!strcasecmp(name, "home"))
      exact_match = HOME;

  /* Match variable */
  if((type & MATCH_VARIABLE) && exact_match == NOTHING)
    if(!strcasecmp(name, "variable"))
      exact_match = AMBIGUOUS;

  /* Match linked zone#s */
  if((type & MATCH_ZONE) && exact_match == NOTHING)
    for(ptr=Zone(player);ptr && *ptr != NOTHING;ptr++)
      if(match_abs == *ptr)
        exact_match = *ptr;

  /* Match container */
  if((type & MATCH_HERE) && exact_match == NOTHING)
    if(!strcasecmp(name, "here") && !Invalid(db[player].location))
      exact_match = db[player].location;

  /* Match exits in room */
  if((type & MATCH_EXIT) && exact_match == NOTHING &&
     !Invalid(db[player].location))
    exact_match = match_exit(player, name, shortname, match_abs, type,
                             &count, accept);

  /* Match inventory objects */
  if((type & MATCH_INVEN) && exact_match == NOTHING)
    exact_match = match_list(player, player, name, shortname, match_abs,
                             type, &full, &count, accept);

  /* Match contents in room */
  if((type & MATCH_CON) && exact_match == NOTHING &&
     !Invalid(db[player].location) && db[player].location != player) {
    obj=match_list(player, db[player].location, name, shortname, match_abs,
                   type, &full, &count, accept);
    if(!Invalid(obj))
      exact_match=obj;
  }

  /* Check possessive */
  if((type & MATCH_POSSESSIVE) && exact_match == NOTHING &&
     (s=strchr(shortname, '\'')) && s > shortname && (*(s+1)|32) == 's' &&
     *(s+2) == ' ') {
    char buf[512];

    strcpy(buf, shortname);
    r=s-shortname+buf+3;
    *(r-3)='\0';

    while(*r == ' ')
      r++;

    /* Match the contents of the possessive */
    if(*r && (alt_location=match_thing(player, buf, MATCH_QUIET|
       (type & ~MATCH_POSSESSIVE))) != NOTHING) {
      count=0;
      obj=match_list(player, alt_location, r, r, NOTHING, type, &full,
                     &count, accept);
      if(!Invalid(obj))
        exact_match=obj;
    }
  }

  /* Destroyed objects do not exist */
  if(!Invalid(exact_match) && !(type&MATCH_DESTROYED) && Typeof(exact_match)
     == TYPE_GARBAGE)
    exact_match=NOTHING;

  /* Return results */
  if(exact_match == NOTHING) {
    /* Full name of the player not specified? */
    if(full == 2) {
      if(!(type & MATCH_QUIET))
        notify(player, "You must type out the player's name in full.");
      return AMBIGUOUS;
    } else if(!(type & MATCH_QUIET) && !match_quiet)
      notify(player, "I don't see '%s' here.", name);
  }

  return exact_match;
}

// Returns 1 if <player> is allowed to match <thing>. A player may not match
// something if its @vlock is locked against him, or if <thing> is a
// disconnected player.
//
static int can_match(dbref player, dbref thing, int invis)
{
  /* Matching self is always allowed, POW_DARK matches everything else */
  if(player == thing || (controls(player, thing, POW_DARK) &&
     !IS(player, TYPE_PLAYER, PUPPET)))
    return 1;

  if((Typeof(thing) == TYPE_PLAYER && !(db[thing].flags & PLAYER_CONNECT)) ||
     !could_doit(player, thing, A_VLOCK) || (!invis &&
     Typeof(thing) == TYPE_EXIT && !can_see(player, thing)))
    return 0;
  return 1;
}

// Utility for scanning all the exits in the room for matches.
//
static int match_exit(player, match_name, match_prefix, absolute, type,
                      count, accept)
dbref player, absolute;
char *match_name, *match_prefix;
int type, *count, accept;
{
  dbref exit, cmatch=NOTHING;
  int a;

  /* Search room's exitlist */
  DOLIST(exit, db[db[player].location].exits) {
    /* Check exit name (cheapest operation first) */
    if(exit == absolute || !strcasecmp(db[exit].name, match_name) ||
       exit_alias(exit, A_ALIAS, match_name))
      a=0;  /* Name/alias fully matched */
    else if(!strcasecmp(db[exit].name, match_prefix) ||
            exit_alias(exit, A_ALIAS, match_prefix))
      a=1;  /* Name/alias matched prefix string (without Nth number) */
    else
      continue;

    /* Check plane & @vlock */
    if(!match_plane(player, exit) ||
       !can_match(player, exit, (type & MATCH_INVIS)))
      continue;

    if(!a)
      return exit;
    if(++*count == accept)
      cmatch=exit;
  }

  return cmatch;
}

// Utility for scanning all the contents in the room for matches.
//
static int match_list(player, loc, match_name, match_prefix, absolute, type,
                      full, count, accept)
dbref player, loc, absolute;
char *match_name, *match_prefix;
int type, *full, *count, accept;
{
  dbref first, cmatch=NOTHING;
  int a;

  if(IS(loc, TYPE_ROOM, ROOM_XZONE))
    return NOTHING;

  DOLIST(first, db[loc].contents) {
    a=(Typeof(first) == TYPE_THING);

    /* Check object name (cheapest operation first) */
    if(first == absolute || !strcasecmp(db[first].name, match_name) ||
       (a && exit_alias(first, A_ALIAS, match_name)))
      a=0;  /* Name/alias fully matched */
    else if(string_match(db[first].name, match_prefix) ||
            (a && exit_alias(first, A_ALIAS, match_prefix)))
      a=1;  /* Partial name/alias matched */
    else
      continue;

    /* Check plane & @vlock */
    if(!match_plane(player, first) ||
       !can_match(player, first, (type & MATCH_INVIS)))
      continue;

    if(!a)
      return first;
    if(*full && Typeof(first) == TYPE_PLAYER)
      *full=2;
    else if(++*count == accept)
      cmatch=first;
  }

  return cmatch;
}

// Accepts only nonempty matches starting at the beginning of a word.
//
char *string_match(char *src, char *sub)
{
  if(!*sub)
    return NULL;

  while(*src) {
    if(string_prefix(src, sub))
      return src;

    /* Else scan to beginning of next word */
    while(*src && *src != ' ' && *src != '-')
      src++;
    while(*src && (*src == ' ' || *src == '-'))
      src++;
  }
  return NULL;
}

// Checks if a string matches a list of semicolon-separated exit aliases.
// Spaces surrounding the alias are ignored. Returns 1 if a match exists.
//
int exit_alias(dbref thing, unsigned char attr, char *str)
{
  char *r, *s=atr_get(thing, attr), *t;
  int len=strlen(str);

  while(*s) {
    r=s;
    while(*s && *s != ';')
      s++;

    /* Ignore surrounding spaces */
    while(*r == ' ')
      r++;
    for(t=s;t > r && *(t-1) == ' ';t--);

    /* Test for a match */
    if(t-r == len && !strncasecmp(r, str, t-r))
      return 1;
    if(*s)
      s++;
  }
  return 0;
}

// Matches a word from a space-separated list.
// Returns the word position of the match starting at 1, or 0 if no match.
//
int match_word(char *list, char *word)
{
  char *r, *s=list;
  int count=1, len=strlen(word);

  while(*s) {
    r=s;
    while(*s && *s != ' ')
      s++;
    if(s-r == len && !strncasecmp(r, word, s-r))
      return count;
    count++;
    if(*s)
      s++;
  }
  return 0;
}

/* Wildcard Matching Procedures */

// d=string list; command typed in
// s=pattern to check; contains the wildcard symbols
//  flags-
// 1-10: record match data into %0 through %9 (n-1)
// Boolean (16):   Check mathematical boolean args ! > < on *s
// Ansi (32):      Don't strip ansi escape codes
// Recursive (64): Recursive call to wild_match

int wild_match(char *d, char *s, int flags)
{
  static char buff[8192];
  static int wpos=0;
  char temp[(flags & WILD_RECURSIVE)?0:8192], *r, *t;
  int inv=0, rslt=0;

  /* Set initialization parameters first time through */
  if(!(flags & WILD_RECURSIVE)) {
    /* Set arg# to start recording match data */
    wpos=RANGE((flags & 15), 1, 10)-1;

    /* Remove all unprintable characters */
    for(r=temp,t=d;*t;t++) {
      if((*t >= 32 && *t <= 126) || *t == '\n' || *t == '\a')
        *r++=*t;
      else if(*t == '\e') {
        if(flags & WILD_ANSI)
          *r++=*t;
        else {
          skip_ansi(t);  /* Strip ansi escape codes */
          t--;
        }
      }
    }
    *r='\0';
    d=temp;

    /* check for boolean arguments ! > < */
    if(flags & WILD_BOOLEAN) {
      if(*s == '!') {
        s++;
        inv=1;
      }
      if(*s == '<' || *s == '>') {
        s++;
        if(isdigit(*s) || (*s == '-' && isdigit(*(s+1))))
          rslt=(*(s-1) == '<')?(atoi(s) > atoi(d)):(atoi(s) < atoi(d));
        else {
          rslt=strcasecmp(s, d);
          rslt=(*(s-1) == '<')?(rslt > 0):(rslt < 0);
        }
        return inv ? !rslt:rslt;
      }
    }
  }

  /* match characters one at a time until we reach a '*' by itself */
  while(1) {
    if(*s != '*')
      while(*d == '\e')  /* Skip escape codes */
        skip_ansi(d);

    /* skip over excess wildcards '*?' or '**' */
    if(*s == '*') {
      s++;
      if(*s != '*' && *s != '?')
        break;
      continue;
    }

    /* single character match; returns false if at end of data */
    if(*s == '?') {
      if(!*d)
        return inv;
      if((flags & WILD_ENV) && wpos < 10) {
        env[wpos][0]=*d;
        env[wpos++][1]='\0';
      }
    } else {
      /* escape character; force literal match of next character */
      if(*s == '\\')
        s++;

      /* literal character match; if matching end of data, return true */
      if(tolower(*s) != tolower(*d))
        return inv;
      if(!*d)
        return !inv;
    }
    s++; d++;
  }

  /* return true on trailing '*' */
  if(!*s) {
    if((flags & WILD_ENV) && wpos < 10) {
      /* skip leading spaces */
      if(!(flags & WILD_ANSI))
        while(isspace(*d))
          d++;
      strcpy(env[wpos++], d);
    }
    return !inv;
  }

  /* scan for possible matches */
  t=buff;
  while(*d) {
    /* Skip or record ANSI escape codes */
    while(*d == '\e') {
      if(flags & WILD_ENV)
        copy_ansi(t,d);
      else
        skip_ansi(d);
    }
    if(!*d)
      break;

    if(*s == '\\')
      s++;
    if(tolower(*d) == tolower(*s) && wild_match(d+1, s+1, WILD_RECURSIVE)) {
      if((flags & WILD_ENV) && wpos < 10) {
        *t='\0';
        while(t-buff && isspace(*(t-1)))  /* remove following spaces */
          *--t='\0';
        strcpy(env[wpos++], buff);  /* record this * element */
        wild_match(d+1, s+1, flags|WILD_RECURSIVE); /* now record the others */
      }
      return !inv;
    } else if((flags & WILD_ENV) &&              /* skip leading spaces below */
              ((flags & WILD_ANSI) || t-buff || !isspace(*d))) {
      *t++=*d;
    }
    d++;
  }

  return inv;
}
