/* io/console.c */

#include "externs.h"
#include <dirent.h>


/*
 *
 * Reserved for Linux Console Color Device (screen fading via spells/timeofday)

static unsigned char default_rgb[48]={
     0, 0, 0,     170, 0, 0,    0, 170, 0,    170, 85, 0,
     0, 0, 170,   170, 0, 170,  0, 170, 170,  170, 170, 170, 

     85, 85, 85,  255, 85, 85,  85, 255, 85,  255, 255, 85,
     85, 85, 255, 255, 85, 255, 85, 255, 255, 255, 255, 255};

static char tones[16]={0, 4, 6, 2, 3, 5, 7, 8, 1, 10, 12, 14, 9, 11, 13, 15};
 */

static char fadetab[40]={33, 27, 12, 9, 15, 19, 0, 31, 11, 16,
                         22, 34, 14, 4, 29, 17, 37, 20, 10, 38,
                         21, 25, 36, 3, 35, 30, 6, 13, 8, 39,
                         26, 18, 1, 23, 5, 32, 2, 28, 7, 24};

static char highfont[128]=
  "                                "
  "        ?C#  !<>   ||          ."
  "`^.|-+                |~_'. ||||"
  "                =     *|^.>*  - ";

/* gets #cols from session <d>, or player <player> if not connected */
int get_cols(DESC *d, dbref player)
{
  player=db[player].owner;

  if(!d)
    d=Desc(player);
  if(!d || d->cols < 4)
    return db[player].data->cols;
  return d->cols;
}

/* gets #rows from session <d>, or player <player> if not connected */
int get_rows(DESC *d, dbref player)
{
  player=db[player].owner;

  if(!d)
    d=Desc(player);
  if(!d || d->rows < 4)
    return db[player].data->rows;
  return d->rows;
}

/* console reset */
void clear_screen(DESC *d, int sbar)
{
  char buf[512];
  int a, b;

  if(Invalid(d->player)) {
    output2(d, "\ec\e(U\e[1;5]\e[2J\e[H\n");
    return;
  }

  /* clear screen without ansi */
  if(!Invalid(d->player) && !terminfo(d->player, TERM_ANSI)) {
    b=get_rows(d, d->player)-1;
    for(a=0;a<b;a++)
      buf[a]='\n';
    buf[a]='\0';
    output2(d, buf);
    return;
  }

  /* perform ansi reset escape sequences */
  output2(d, tprintf("\e[m\e[r%s\e[1;5]\e[2J\e[%dH\n", (terminfo(d->player,
          TERM_HIGHFONT) || terminfo(d->player, TERM_GRAPHICS))?"\e(U":"",
          sbar?4:1));

}

/* fade away screen with 2x1 character blocks */
void fade_screen(DESC *d)
{
  char buf[256], *s=buf;
  int i, x, y, count=0, cols, rows;

  if(!terminfo(d->player, TERM_ANSI))
    return;
  cols=get_cols(d, d->player);
  rows=get_rows(d, d->player);

  for(i=0;i<40;i++)
    for(y=1;y<rows;y++)
      for(x=(fadetab[(fadetab[i]+y)%40]*2)+1; x<cols; x+=80) {
        s+=sprintf(s, "\e[%d;%dH  ", y, x);
        if(++count == 11) {
          output2(d, tprintf("\e7%s\e8\e[A\n", buf));
          s=buf, count=0;
        }
      }

  if(s > buf)
    output2(d, tprintf("\e7%s\e8\e[A\n", buf));
}


/**** Terminal Configuration Section *****************************************/

static struct term_config {
  char *name;
  int config;
  int flag;
  char *desc;
} term_config[]={
  /* +term options */
  {"Rows",	0, 0, "The height of your terminal window in character cells."},
  {"Columns",	0, 0, "The number of characters in width."},
  {"Ansi",	0, TERM_ANSI, "Turns on ANSI color and cursor movement enhancements."},
  {"Highfont",	0, TERM_HIGHFONT, "Enables the Extended ASCII line-drawing character set (DOS)."},
  {"Graphics",	0, TERM_GRAPHICS, "Utilizes the TinyMARE font for item icons and graphics."},
  {"Wordwrap",	0, TERM_WORDWRAP, "Wraps long sentences around to the next line at word breaks."},
  {"Beeps",	0, TERM_BEEPS, "Turn on/off audible beeps to your terminal."},
  {"Music",	0, TERM_MUSIC, "Enables background music with the MareTerm client."},
  {"Icons",	0, TERM_ICONS, "Turns on full-color icon graphics and images."},

  /* +config options */
  {"Reginfo",	1, CONF_REGINFO, "Show your Real Name and Email Address to others in +finger."},
  {"Pager",	1, CONF_PAGER, "Enable a -More- prompt with every screenful of Helptext."},
  {"Safe",	1, CONF_SAFE, "@destroy waits 60 seconds before deleting an object."},
};

// Modify current terminal configuration.
//
void do_term(dbref player, char *arg1, char *arg2)
{
  DESC *d;
  dbref thing=db[player].owner;
  int a=0, b;

  /* Match configuration variable */
  if(*arg1) {
    for(a=0;a < NELEM(term_config);a++)
      if(!term_config[a].config && string_prefix(term_config[a].name, arg1))
        break;
    if(a == NELEM(term_config)) {
      if((thing=lookup_player(arg1)) <= NOTHING) {
        notify(player, "Unknown terminal setting.");
        return;
      } else if(!controls(player, thing, POW_EXAMINE)) {
        notify(player, "Permission denied.");
        return;
      }
    }
  }

  d=Desc(thing);

  /* Display terminal configuration */
  if(!*arg1 || !term_config[a].name) {
    char buf[16];

    notify(player, "\e[1;32m%s%s Terminal Configuration:\e[m",
           db[thing].name, possname(db[thing].name));
    for(a=0;a < NELEM(term_config);a++) {
      if(term_config[a].config)
        continue;
      if(a < 2)
        sprintf(buf, "%d", a?get_cols(d, thing):get_rows(d, thing));
      else
        strcpy(buf, (db[thing].data->term & term_config[a].flag)?"On":"Off");

      notify(player, "\e[1;36m%11.11s: \e[37m%-5.5s\e[36m%.60s\e[m",
             term_config[a].name, buf, term_config[a].desc);
    }
    return;
  }

  /* Determine correct argument */
  if(a < 2) {
    b=atoi(arg2);
    if(b < 4 || b > 250) {
      notify(player, "%s may only be between 4 and 250.", term_config[a].name);
      return;
    }
  } else {
    if(!strcasecmp(arg2, "Off"))
      b=0;
    else if(!strcasecmp(arg2, "On"))
      b=1;
    else {
      notify(player, "%s may either be On or Off.", term_config[a].name);
      return;
    }
  }

  /* Do additional things if removing ANSI */
  if(a == 2 && !b) {
    db[thing].data->term &= ~TERM_MUSIC;
  }

  /* Turning off Highfont turns off Graphics too */
  if(a == 3 && !b)
    db[thing].data->term &= ~TERM_GRAPHICS;

  if(!a) {  // Resize Rows
    db[thing].data->rows=b;
    if(d)
      d->rows=b;
  } else if(a == 1) {  // Resize Columns
    db[thing].data->cols=b;
    if(d) {
      /* Special: Resize screen if #cols = 80 or 132 */
      if((b == 80 || b == 132) && terminfo(thing, TERM_ANSI))
        output2(d, tprintf("\e[?3%c\e[A\n", b<132?'l':'h'));

      d->cols=b;
    }
  } else {  // All other +term variables
    if(b) {
      db[thing].data->term |= term_config[a].flag;
      if(a == 4)
        db[thing].data->term |= term_config[a-1].flag;
    } else
      db[thing].data->term &= ~term_config[a].flag;
  }

  if(a < 2)
    notify(player, "%s set to %d.", term_config[a].name, b);
  else
    notify(player, "%s turned %s.", term_config[a].name, b?"On":"Off");
}

/* Refixes player's screen */
void do_cls(dbref player)
{
  DESC *d;

  if(Typeof(player) == TYPE_PLAYER && (d=Desc(player)))
    clear_screen(d, 0);
}

// Modify player preferences.
//
void do_player_config(dbref player, char *arg1, char *arg2)
{
  dbref thing=db[player].owner;
  int a=0, b;

  /* Match configuration variable */
  if(*arg1) {
    for(a=0;a < NELEM(term_config);a++)
      if(term_config[a].config && string_prefix(term_config[a].name, arg1))
        break;
    if(a == NELEM(term_config)) {
      if((thing=lookup_player(arg1)) <= NOTHING) {
        notify(player, "Unknown terminal setting.");
        return;
      } else if(!controls(player, thing, POW_EXAMINE)) {
        notify(player, "Permission denied.");
        return;
      }
    }
  }

  /* Display preferences */
  if(!*arg1 || a == NELEM(term_config)) {
    notify(player, "\e[1;32m%s%s Preferences:\e[m",
           db[thing].name, possname(db[thing].name));
    for(a=0;a < NELEM(term_config);a++) {
      if(!term_config[a].config)
        continue;

      notify(player, "\e[1;36m%11.11s: \e[37m%s  \e[36m%.60s\e[m",
             term_config[a].name,
             (db[thing].data->term & term_config[a].flag)?"On ":"Off",
             term_config[a].desc);
    }
    return;
  }

  if(!strcasecmp(arg2, "Off"))
    b=0;
  else if(!strcasecmp(arg2, "On"))
    b=1;
  else {
    notify(player, "%s may either be On or Off.", term_config[a].name);
    return;
  }

  if(b)
    db[thing].data->term |= term_config[a].flag;
  else
    db[thing].data->term &= ~term_config[a].flag;

  notify(player, "%s turned %s.", term_config[a].name, b?"On":"Off");
}

// Called by term() function to retrieve +term and +config data for a player.
//
void eval_term_config(char *buff, dbref player, char *name)
{
  int i;

  for(i=0;i < NELEM(term_config);i++)
    if(string_prefix(term_config[i].name, name))
      break;

  if(i == NELEM(term_config))
    strcpy(buff, "#-1 No such term setting.");
  else {
    switch(i) {
      case 0: sprintf(buff, "%d", get_rows(NULL, player)); break;
      case 1: sprintf(buff, "%d", get_cols(NULL, player)); break;
      default:
        sprintf(buff, "%d", (db[player].data->term & term_config[i].flag)?1:0);
    }
  }
}


/**** Player Console Settings ************************************************/

void do_console(dbref player, char *name, char *options)
{
  static struct {
    char *name;
    int bitmask;	// Connection flag bitmask
    int inverse;	// Set flag when the option is disabled
  } cflags[]={
    {"Input",      C_INPUTOFF,  1},
    {"Output",     C_OUTPUTOFF, 1},
    {"Sbar",       C_SBAROFF,   1},
    {"Reset"},
    {"Sendoutput"},
    {"Reconnect"},
    {"Fade"},
    {0}};

  DESC *d;
  char *r, *s=options;
  int a, inv, thing;

  /* Match login descriptor to player */
  if((thing=match_thing(player, name, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(Typeof(thing) != TYPE_PLAYER) {
    notify(player, "Console settings can only be changed for players.");
    return;
  } if(!controls(player, thing, POW_CONSOLE)) {
    notify(player, "Permission denied.");
    return;
  } if(thing == GOD) {
    notify(player, "Cannot change console options for %s.",
           unparse_object(player, thing));
    return;
  } if(!(d=Desc(thing)) || !(d->flags & C_CONNECTED)) {
    notify(player, "That player is not connected.");
    return;
  }

  while((r=strspc(&s))) {
    if(*r == '!') {
      inv=1;
      r++;
    } else
      inv=0;
    if(!*r)
      continue;

    for(a=0;cflags[a].name;a++)
      if(string_prefix(cflags[a].name, r))
        break;
    if(!cflags[a].name) {
      notify(player, "No such console option '%s'.", r);
      continue;
    }
    
    /* Trigger a console command */
    if(!cflags[a].bitmask) {
      if(inv) {
        notify(player, "Console commands cannot be inverted.");
        continue;
      }

      switch(a) {
      case 3:  // Reset
        d->state=10;
        break;
      case 4:  // Sendoutput
        process_output(d, 1);
        break;
      case 5:  // Reconnect
        process_output(d, 1);
        output2(d, "*** Reconnecting ***\n\n");
        d->flags |= C_RECONNECT|C_LOGOFF;
        break;
      case 6:  // Fade
        fade_screen(d);
        break;
      }

      if(player != thing && !Quiet(player))
        notify(player, "Command triggered.");
      continue;
    }

    /* Descriptor flag setting */
    if((!(inv ^ cflags[a].inverse) && (d->flags & cflags[a].bitmask)) ||
         ((inv ^ cflags[a].inverse) && !(d->flags & cflags[a].bitmask))) {
      if(!Quiet(player))
        notify(player, "Option already %sset.", inv?"un":"");
      continue;
    }

    if(inv ^ cflags[a].inverse)
      d->flags &= ~cflags[a].bitmask;
    else
      d->flags |= cflags[a].bitmask;

    if(!Quiet(player))
      notify(player, "Option %sset.", inv?"un":"");
  }
}


/* Text Emulation & Translation */

// Enable wordwrap around ansi characters.
//
static char *wordwrap(dbref player, char *str)
{
  static char buf[8192];
  char *r=buf, *p=NULL, *s=str;
  int wlen=0, llen=0, width=get_cols(NULL, player)-1;

  while(*s && r-buf < 8000) {
    if(*s == '\n') {	// Newline
      *r++=*s++;
      llen=wlen=0;
      p=NULL;
      continue;
    }

    if(*s == '\t') {    // Pseudo-tab
      if(llen < width)
        llen+=8-(llen % 8);
      if(llen >= width) {
        *r++='\n';
        s++;
        llen=wlen=0;
        p=NULL;
      } else
        *r++=*s++;
      continue;
    }

    if(*s == ' ') {	// Space
      if(llen == width) {
        *r++='\n';
        if(!wlen)
          *r++=*s;
        s++;
        llen=wlen=0;
        p=NULL;
      } else {
        p=r;
        *r++=*s++;
        llen++;
        wlen=0;
      }
      continue;
    }

    if(*s == '\e') {	// Escape character; copy ansi codes
      if(*(s+1)) {
        *r++=*s++;
        if(*s == '[') {  /* variable-length CSI code */
          *r++=*s++;
          while(*s && *s < 64 && r-buf < 7999)
            *r++=*s++;
        } else if(*s && *s < 48)  /* 2-byte code */
          *r++=*s++;
        if(*s)
          *r++=*s++;
      } else
        s++;
      continue;
    }

    if(llen == width) {
      if(p) {
        *p='\n';
        llen=wlen;
        p=NULL;
      } else {
        *r++='\n';
        p=NULL;
        llen=wlen=0;
      }
    }

    *r++=*s++;
    llen++;
    wlen++;
  }
  *r='\0';
  return buf;
}

// Filter output based on a player's +term configuration settings.
//
char *term_emulation(DESC *d, char *msg, int *len)
{
  static unsigned char buf[16384];
  unsigned char temp[8192], *s;
  int terminfo;
  
  terminfo=(game_online && !Invalid(d->player))?db[d->player].data->term:
           (TERM_BEEPS|TERM_ANSI|TERM_HIGHFONT);

  /* apply wordwrap to output string, except for lines with special codes */
  if((terminfo & TERM_WORDWRAP) && (msg[0] != '\e' || msg[1] == '['))
    msg=wordwrap(d->player, msg);

  /* strip ansi escape codes */
  if(!(terminfo & TERM_ANSI)) {
    s=temp;
    do {
      while(*msg == '\e')
        skip_ansi(msg);
      *s++=*msg;
    } while(*msg++);
    msg=temp;
  }

  /* format message for output */
  for(s=buf;*msg;msg++) {
    /* strip beeps */
    if(*msg == '\a' && !(terminfo & TERM_BEEPS))
      continue;

    /* transpose LF into CRLF */
    if(*msg == '\n')
      *s++='\r';
    *s++=*msg;
  }
  *s='\0';
  *len=s-buf;

  /* change 8-bit font to 7-bit for players */
  if(!(terminfo & TERM_HIGHFONT))
    for(s=buf;*s;s++)
      if(*s > 127) {
        if(*s == 255) {  /* Skip Telnet IAC */
          if(*(s+1)) s++;
          if(*(s+1)) s++;
        } else
          *s=highfont[(int)*s - 128];
      }

  return buf;
}


/* Color Manipulation */

int default_color=7;

// Return an integer color code based on a list of specified flags or numbers.
//
int color_code(int prev, char *s)
{
  static char flags[33]="h+f#u-i!*/InxrgybmvpcwNXRGYBMVPCW";
  static int attr[33]={8, 8, 128, 128, 256, 256, 512, 512, 1024, 2048, 4096,
                       -1, 0, 1,  2,  3,  4,  5,  5,  5,  6,  7,
                       -1, 0, 16, 32, 48, 64, 80, 80, 80, 96, 112};
  int a, color=prev;

  /* treat numbers as a base starting color */
  if(isdigit(*s)) {
    if((a=atoi(s)))
      color=a;
    while(isdigit(*++s));  // Skip pointer over digits to start of flags
  }

  /* parse flags string */
  for(;*s && *s != ',' && *s != ':';s++)
    for(a=0;a<33;a++)
      if(flags[a] == *s) {
        switch(a) {
          case 11:          color = prev; break;     // Default @color.
          case 22:          color = 7; break;        // All attributes off.
          case 12 ... 21:   color = (color & ~7) + attr[a]; break;
          case 23 ... 32:   color = (color & ~112) + attr[a]; break;
          default:          color |= attr[a];
        }
        break;
      }

  return color;
}

// Returns a color string containing flags used to compose color attributes.
//
char *unparse_color_code(int color)
{
  static char flags[13]="RGYBMCW#-!*/I";
  static char result[16];
  char *s=result;
  int i;

  s+=sprintf(s, "%d", color & 15);
  if(color & 112)
    *s++=flags[((color & 112) >> 4)-1];

  for(i=7;i<13;i++)
    if(color & (1 << i))
      *s++=flags[i];

  *s='\0';
  return result;
}

// Outputs an ANSI escape code to change the text color from <prev> to <color>.
// Can be called up to 4 times within one sprintf() function.
//
char *textcolor(int prev, int color)
{
  static char buf[4][24];
  static int cycle;
  char *result, *s;
  int set;

  if(!((prev^color) & (4096|2048|512|256|255)))  // No change?
    return "";
  if((color & ~1024) == 7)  // Default attributes
    return "\e[m";

  result=s=buf[cycle++ & 3];
  *s++='\e', *s++='[';
  /* Check if we're unsetting any attributes */
  if(prev & (~color & (4096|2048|512|256|128|8))) {
    *s++='0', *s++=';';
    prev=7;
  }

  set=color & ~prev;
  if(set & 4096) *s++='8', *s++=';';  // Invisible
  if(set & 2048) *s++='2', *s++=';';  // Half-intensity
  if(set &  512) *s++='7', *s++=';';  // Inverse
  if(set &  256) *s++='4', *s++=';';  // Underline/Italics
  if(set &  128) *s++='5', *s++=';';  // Blinking
  if(set &    8) *s++='1', *s++=';';  // Hilighted

  if((prev^color) & 7)
    *s++='3', *s++='0'|(color & 7), *s++=';';  // Foreground color changed
  if(((prev^color) >> 4) & 7)
    *s++='4', *s++='0'|((color>>4) & 7), *s++=';';  // Background color

  *(s-1)='m', *s='\0';  // The last semicolon gets replaced with an 'm'
  return result;
}

int prev_rainbow;  /* used to get the last color of the rainbow text */

char *rainbowmsg(char *s, int color, int size)
{
  static int seq[16]={4, 3, 6, 11, 5, 13, 14, 6, 6, 1, 2, 10, 4, 9, 12, 11};
  static char temp[8192];
  int prev=default_color;
  char *t=temp;

  /* If the first color is Normal, set it to Light Violet */
  if((color & 15) == 7)
    color=(color & ~15)|13;

  while(*s && t-temp < size-42) {
    strcpy(t, textcolor(prev, color));
    t+=strlen(t);
    *t++=*s++;
    if(*s != ' ') {
      prev=color;
      color=(color & ~15)|seq[color & 15];
    }
  }
  prev_rainbow=prev;
  strcpy(t, textcolor(prev, default_color));
  return temp;
}

// Adds color to string <s> based on the colors listed in <cname>. Colors are
// standard color codes (as listed in color_code() above) separated by commas,
// and each group may optionally be followed by a colon and a number specifying
// how many characters of <s> should have that color.
//
// Colors cycle endlessly to fill all remaining characters in <s> or until
// <size> bytes are recorded.
//
// Type=1: Strip annoying ANSI attributes (blink, etc.) for +com channel use.
//
char *color_by_cname(char *s, char *cname, int size, int type)
{
  static char temp[8192];
  char *t=temp, *r=cname;
  int color, count, prev=default_color;

  while(*s && t-temp < size-42) {
    /* Determine color of next character */
    color=color_code(7, r);
    if(!(color & 0x77))
      color |= 8;  /* Keep from becoming black */
    if(type)
      color &= ~0x1180;  /* Strip blink, underline, & invisible text */
    strcpy(t, textcolor(prev, color));
    prev=color;
    t+=strlen(t);

    /* Determine <count> characters using this color */
    for(;*r && *r != ':' && *r != ',';r++);
    count=1;
    if(*r == ':') {
      if(*++r == '*')
        count=8000;
      else if((count=atoi(r)) < 1)
        count=1;
      while(*r && *r != ',')
        r++;
      if(*r)
        r++;
    } else if(*r == ',')
      r++;
    if(!*r)
      r=cname;

    while(*s && count > 0) {
      if(*s != ' ')
        count--;
      *t++=*s++;
    }
  }
  strcpy(t, textcolor(prev, default_color));
  return temp;
}


/**** Icon Support ***********************************************************/

// Sends an icon definition in a base-128 encoded stream to the remote client.
// TinyMARE keeps a cache of which icons have been sent to each descriptor and
// only sends icon data that either is not already in the list or if the file
// timestamp changes.
//
// Icon definitions are sent via output queue #2 so that it is received by the
// client before any text using the icon is displayed.
//
static void send_icon_definition(DESC *d, char *file, unsigned int mtime,
                                 unsigned int id, int size)
{
  struct {
    char code[3];
    int param[3];
  } __attribute__((__packed__)) header={"\e]I", {htonl(mtime), htonl(id),
                                        htonl(size)}};

  unsigned char buf[4096];
  int fd, len, i=0;

  /* Check to see if player already has the icon with the right modtime */
  for(i=0;i < d->icon_size-1;i+=2)
    if(d->iconlist[i] == id) {
      if(d->iconlist[i+1] == mtime)
        return;
      break;
    }

  /* Open icon file */
  if((fd=open(file, O_RDONLY|O_BINARY)) == -1)
    return;

  /* Send ANSI header to remote terminal */
  output_binary(d, (char *)&header, sizeof(header));

  /* Send file to remote terminal */
  while((len=read(fd, buf, 4096)) > 0)
    output_binary(d, buf, len);

  close(fd);

  /* Add index to list of defined icons for that session */
  if(i >= d->icon_size) {
    if(!(d->icon_size & 31))
      d->iconlist=realloc(d->iconlist, (d->icon_size+32)*sizeof(int));
    d->icon_size+=2;
    d->iconlist[i]=id;
  }
  d->iconlist[i+1]=mtime;  /* Update modtime for icon */
}

// Displays an icon at the current cursor position on the remote terminal.
// Icons are referenced by filename, sent via unique CRC-32 IDs computed on the
// "directory/.../filename" string of the icon file. Files are relative to the
// icons/ directory.
//
// The icon ANSI display code is sent as follows, where each value is 0-65535:
//  ESC [ Type ; MSWcrc ; LSWcrc w
//
// The TinyMARE client is expected to cache icon files for the duration of the
// session. If the client directly connects to TinyMARE, it can supply the
// server with a list of icon/modtime pairs already known.
//
// Type=0: Icon is displayed above text on the screen.
//      1: Icon is displayed under text but above cell background color.
//      2: Icon is placed as a background image stretched to fit.
//      3: Icon is tiled as a background image.
//
// No two icons can occupy the same origin character-cell on the screen.
//
char *send_icon(dbref player, int type, char *icon)
{
  DESC *d;
  struct stat statbuf;
  static unsigned char buf[4][24];
  static int cycle;
  unsigned char file[strlen(icon)+11], *s;
  unsigned int crc, mtime;

  /* Make sure the player can view icons */
  if(!view_icons(player))
    return "";

  /* Determine if the file size is legal */
  sprintf(file, "icons/%s", icon);
  if(stat(file, &statbuf)) {
    strcpy(strnul(file), ".png");
    if(stat(file, &statbuf))
      return "";
  }
  if(statbuf.st_size < 1 || statbuf.st_size > 16777216)
    return "";

  /* Get the file timestamp */
  mtime=statbuf.st_mtime;

  /* Compute 32-bit crc of filename */
  crc=compute_crc(file+6);

  /* Send icon definition in base-128 */
  if((d=Desc(db[player].owner)))
    send_icon_definition(d, file, mtime, crc, statbuf.st_size);

  /* Build output buffer */
  s=buf[cycle++ & 3];
  sprintf(s, "\e[%d;%d;%dw", type & 7, crc >> 16, crc & 65535);

  return s;
}
