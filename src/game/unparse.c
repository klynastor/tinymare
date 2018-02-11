/* game/unparse.c */

#include "externs.h"


struct flagstr flaglist[]={
  {"Abode",       'A', TYPE_ROOM,   ROOM_ABODE},
  {"Builder",     'B', TYPE_PLAYER, PLAYER_BUILD},
  {"Chown_Ok",    'C', NOTYPE,      CHOWN_OK},
  {"Dark",        'D', NOTYPE,      DARK},
  {"Exit",        'E', TYPE_EXIT,   0},
  {"Floating",    'F', TYPE_ROOM,   ROOM_FLOATING},
  {"Going",       'G', NOTYPE,      GOING},
  {"Jump_Ok",     'J', TYPE_ROOM,   ROOM_JUMP_OK},
  {"Link_Ok",     'L', TYPE_ROOM,   ROOM_LINK_OK},
  {"Player",      'P', TYPE_PLAYER, 0},
  {"Room",        'R', TYPE_ROOM,   0},
  {"Transparent", 'T', TYPE_EXIT,   EXIT_TRANSPARENT},
  {"Universal",   'U', TYPE_ZONE,   ZONE_UNIVERSAL},
  {"Visible",     'V', NOTYPE,      VISIBLE},
  {"X-Zone",      'X', TYPE_ROOM,   ROOM_XZONE},
  {"Zone",        'Z', TYPE_ZONE,   0},
  {"Bearing",     'b', NOTYPE,      BEARING},
  {"Connected",   'c', TYPE_PLAYER, PLAYER_CONNECT},
  {"Destroy_Ok",  'd', TYPE_THING,  THING_DESTROY_OK},
  {"Enter_Ok",    'e', TYPE_THING,  THING_ENTER_OK},
  {"Grab_Ok",     'g', TYPE_THING,  THING_GRAB_OK},
  {"Haven",       'h', NOTYPE,      HAVEN},
  {"Inactive_Ok", 'i', TYPE_PLAYER, PLAYER_INACTIVE_OK},
  {"Light",       'l', NOTYPE,      LIGHT},
  {"Notice",      'n', TYPE_PLAYER, PLAYER_NOTICE},
  {"Opaque",      'o', NOTYPE,      OPAQUE},
  {"Puppet",      'p', NOTYPE,      PUPPET},
  {"Quiet",       'q', NOTYPE,      QUIET},
  {"Restricted",  'r', TYPE_ZONE,   ZONE_RESTRICTED},
  {"Slaved",      's', TYPE_PLAYER, PLAYER_SLAVE},
  {"Terse",       't', TYPE_PLAYER, PLAYER_TERSE},
  {"Unsaved",     'u', TYPE_THING,  THING_UNSAVED},
  {"Verbose",     'v', NOTYPE,      VERBOSE},
  {"Wander",      'w', TYPE_THING,  THING_WANDER},
  {0}};

void list_flags(dbref player)
{
  char buf[512], *s;
  int a, b=0, count=0, rows, cols=(get_cols(NULL, player)+2)/20;

  if(cols < 2)
    cols=4;
  notify(player, "List of available flags in alphabetical order:");
  notify(player, "%s", "");

  /* print list in column-oriented display */
  while(flaglist[count].name)
    count++;
  rows=(count+cols-1)/cols;
  for(a=0;a<rows;a++) {
    s=buf;
    for(b=0;b<cols;b++) {
      s+=sprintf(s, "%s%-15.15s %c", s==buf?"":"   ",
                 flaglist[rows*b+a].name, flaglist[rows*b+a].flag);
      if(rows*(b+1)+a >= count) {
        notify(player, "%s", buf);
        break;
      }
    }
  }
  notify(player, "%s", "");
  notify(player, "Total %d flags.", count);
}

/* prints flag list; typ=0: Print flag letters only
                     typ=1: Print entire words of all non-objtype flags */
char *unparse_flags(dbref player, dbref thing, int type)
{
  static char buf[512];
  char *s=buf;
  int a;
  
  if(!type) {
    switch(Typeof(thing)) {
      case TYPE_PLAYER: *s++ = 'P'; break;
      case TYPE_EXIT:   *s++ = 'E'; break;
      case TYPE_ROOM:   *s++ = 'R'; break;
      case TYPE_ZONE:   *s++ = 'Z'; break;
    }
  } else
    *buf='\0';

  for(a=0;flaglist[a].name;a++)
    if((flaglist[a].objtype == NOTYPE || flaglist[a].objtype == Typeof(thing))
       && flaglist[a].bits && (db[thing].flags & flaglist[a].bits)) {

      if(flaglist[a].objtype == TYPE_PLAYER) {
        /* Don't show 'Connected' flag if player is hidden! */
        if(flaglist[a].bits == PLAYER_CONNECT && !(could_doit(player, thing,
           A_HLOCK) || controls(player, thing, POW_WHO)))
          continue;

        /* Never show the 'Notice' flag to anyone but Wizards. */
        if(flaglist[a].bits == PLAYER_NOTICE && !power(player, POW_SECURITY))
          continue;
      }

      if(!type)
        *s++ = flaglist[a].flag;
      else {
        if(s > buf)
          *s++=' ';
        strcpy(s, flaglist[a].name);
        s+=strlen(s);
      }
    }

  if(!type)
    *s='\0';
  return buf;
}

// Return a color code for printing an object on the screen.
//   type & 1=Darken intensity on text (mainly for object captions).
//   type & 2=Increase intensity on text (mainly for +com messages).
//   type & 4=Remove special color effects (blink/rainbow) for printing text.
//   type & 8=Suppress combat color adjustments.
//   type & 16=Object scanned under infravision.
//   type & 32=Remove special effects like &4 but leave Rainbow for names.
int unparse_color(dbref player, dbref obj, int type)
{
  int a, color=7, field=0, bright=0, len[10];
  char buf[8192], temp[8192], *s, *sptr[10];
  dbref link, loc=db[player].location;

  /* Determine if player should be shown the bright or dark room color */

  bright=(((type & 8) || loc == NOTHING || !IS(loc, TYPE_ROOM, DARK)) &&
          TIMEOFDAY);
  /* Preserve environment %0 to %9 */
  for(a=0;a<10;a++) {
    len[a]=strlen(env[a])+1;
    memcpy(sptr[a]=alloca(len[a]), env[a], len[a]);
    *env[a]='\0';
  }

  /* determine @color */
  strcpy(temp, atr_get(obj, A_COLOR));
  if(*temp)
    pronoun_substitute(buf, obj, player, temp);
  else
    *buf='\0';

  if(!*buf && Typeof(obj) == TYPE_EXIT) {
    link=db[obj].link;
    if(link == AMBIGUOUS)
      link=resolve_link(player, obj, NULL);
    if(!Invalid(link)) {
      strcpy(temp, atr_get(link, A_COLOR));
      if(*temp)
        pronoun_substitute(buf, link, player, temp);
    }
  }

  /* Restore environment */
  for(a=0;a<10;a++)
    memcpy(env[a], sptr[a], len[a]);

  /* parse @color string */
  for(s=buf-1;s && *(s+1);s=strchr(s+1, ',')) {
    ++field;

    /* match day & night */
    if(bright && (field == 2 || field > 6))
      continue;

    color=color_code(7, s+1);
  }

  /* Channel/Description Text Modifier */
  if(type & 4)
    color&=~0x17f0;
  else if(type & 32)
    color&=~0x13f0;

  /* Darken */
  if(type & 1) {
    if((color & 15) == 7)
      color++;
    else if((color & 15) > 8)
      color-=8;
  }

  /* Brighten */
  if(type & 2) {
    color &= ~0x0800;
    if((color & 15) < 8)
      color+=8;
    else if((color & 15) == 8)
      color--;
  }

  return color;
}

static int disp_exit;
int disp_hidden;

/* Display an object in color, with optional (#dbref/flags) */
char *unparse_object(dbref player, dbref loc)
{
  static char cyclebuf[3][1024];
  static int cyc;
  char temp[8192], *buf, *r, *s;
  int num, idle, color;

  /* Predefined names for non-existing objects */
  if(loc == NOTHING)
    return "*NOTHING*";
  else if(loc == AMBIGUOUS)
    return "*AMBIGUOUS*";
  else if(loc == HOME)
    return "*HOME*";
  else if(loc < -3 || loc >= db_top)
    return "*UNDEFINED*";

  buf=cyclebuf[cyc];
  if(++cyc > 2)
    cyc=0;

  /* Determine color of object */
  color=unparse_color(player, loc, (disp_exit==3?42:disp_hidden==3?16:0));
  if(!(color & 0x2000) && *(s=atr_get(loc, A_CNAME))) {
    int a, len=strlen(s)+1, ptrlen[10];
    char *sptr[10], attr[len];

    /* Preserve environment %0 to %9 */
    for(a=0;a<10;a++) {
      ptrlen[a]=strlen(env[a])+1;
      memcpy(sptr[a]=alloca(ptrlen[a]), env[a], ptrlen[a]);
      *env[a]='\0';
    }

    memcpy(attr, s, len);
    pronoun_substitute(temp, loc, player, attr);

    /* Restore environment */
    for(a=0;a<10;a++)
      memcpy(env[a], sptr[a], ptrlen[a]);
  } else
    *temp='\0';
  s=buf+sprintf(buf, "%s%s", (*temp || (color & 1024))?"":
                textcolor(default_color, color), db[loc].name);

  /* Display any exit aliases */
  if(disp_exit == 2 && Typeof(loc) == TYPE_EXIT && *(r=atr_get(loc, A_ALIAS)))
    s+=sprintf(s, ";%s", r);

  /* Rename object to rainbow colors or @cname if requested */
  if(*temp || (color & 1024)) {
    if(*temp)
      strcpy(buf, color_by_cname(db[loc].name, temp, 960, (disp_exit == 3)));
    else
      strcpy(buf, rainbowmsg(buf, color, 960));
    s=strnul(buf);
    color=default_color;
  }

  /* Check if we need to print #dbref or idle time */
  num=(!disp_exit && (!loc || (db[loc].flags & (CHOWN_OK|VISIBLE)) ||
       IS(loc, TYPE_ROOM, ROOM_JUMP_OK|ROOM_ABODE|ROOM_LINK_OK) ||
       (!IS(player, TYPE_PLAYER, PUPPET) &&
        controls(player, loc, POW_BACKSTAGE))));
  idle=(!disp_exit && IS(loc, TYPE_PLAYER, PLAYER_CONNECT) &&
        getidletime(loc) > 300 && (could_doit(player, loc, A_HLOCK) ||
        controls(player, loc, POW_WHO)));

  /* Print #dbref and/or idle time and restore default color */

  if(num || idle) {
    s+=sprintf(s, "%s", textcolor((color & 1024)?default_color:color,
               (color == 8)?7:(color|8)));
    if(idle)
      s+=sprintf(s, "(%s)", time_format(TMS, getidletime(loc)));
    if(num)
      s+=sprintf(s, "(#%d%s)", loc, unparse_flags(player, loc, 0));
    strcpy(s, textcolor((color == 8)?7:(color|8), default_color));
  } else if(!(color & 1024))
    strcpy(s, textcolor(color, default_color));

  return buf;
}

char *unparse_exitname(dbref player, dbref loc, int alias)
{
  char *s;

  disp_exit=alias?2:1;
  s=unparse_object(player, loc);
  disp_exit=0;

  return s;
}

char *colorname(dbref player)
{
  char *s;

  disp_exit=3;
  s=unparse_object(speaker, player);
  disp_exit=0;

  return s;
}

char *unparse_objheader(dbref player, dbref thing)
{
  return (disp_hidden == 2)?"\e[m (hidden)":"";
}

char *unparse_caption(dbref player, dbref thing)
{
  static char buff[8192];
  char buf[8192], *s, *title="";
  int color;

  /* Display @caption, if available */
  strcpy(buff, atr_get(thing, A_CAPTION));
  pronoun_substitute(buf, thing, player, buff);
  if(!*buf && Typeof(thing) == TYPE_PLAYER && Guest(thing))
    strcpy(buf, "explores the world");
  if(!power(thing, POW_SPOOF)) {
    buf[80]='\0';
    if((s=strchr(buf, '\n')))
      *s='\0';
  }
  strcat(buf, unparse_objheader(player, thing));

  /* Determine caption color */
  color=unparse_color(player, thing, (Typeof(thing) == TYPE_ROOM?4:5) |
                      (disp_hidden == 3?16:0));

  /* Print result output */
  strcpy(buff, unparse_object(player, thing));
  sprintf(buff+strlen(buff), "%s%s%s%s%s\e[m", textcolor(default_color, color),
          *title?" the ":"", *title?title:"", *buf?" ":"", buf);
  return buff;
}
