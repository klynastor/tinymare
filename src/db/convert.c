/* db/convert.c */
/* Utilities for converting other database formats into TinyMare format */

#include "externs.h"

extern dbref *zonelist;
extern dbref *parentlist;

extern int db_format;
extern int db_vers;
extern int db_init;
extern int db_flags;

/* 1=Use PennMush-style quoted strings and attribute values */
int db_quoted_strings;

/* Mush-style attribute definitions list */
int mush_atrdefs;
static struct mush_defattr {
  struct mush_defattr *next;
  int num;
  int flags;
  char name[0];
} *mushdefs;

static void read_attrs(dbref thing, FILE *f);
static int mush_atr_search(dbref thing, int atrnum);


/**** General Text Reading Section *******************************************/

// Read next line from a text-file, uncompressed.
//
static char *read_string(FILE *f)
{
  static char buf[8192];
  char *s=buf;

  /* Get PennMUSH-style quoted strings */
  if(db_quoted_strings) {
    if((*s=getc(f)) == '"') {
      while(s-buf < 7999 && (*s=getc(f)) != '"') {
        if(*s == '\\')
          *s=getc(f);
        if(*s != '\r')
          s++;
      }
      *s='\0';

      /* Scan to end of line */
      while(getc(f) != '\n');
      return buf;
    }
    ungetc(*s, f);
  }

  if(!ftext(buf, 8000, f))
    *buf='\0';
  else
    buf[8000]='\0';
  return buf;
}

// Read string-based object list from file.
//
static dbref *read_list(FILE *f)
{
  int a, b, *list=NULL;

  if(!(a=atoi(read_string(f))))
    return NULL;

  db_lists++;
  list=malloc((a+1)*sizeof(dbref));
  for(b=0;b<a;b++)
    list[b]=atoi(read_string(f));
  list[a]=NOTHING;
  return list;
}

// Inserts # characters in front of every dbref# in boolexp-type lock.
//
static char *convert_boolexp(char *lock)
{
  static char buf[8192];
  char *s=buf, *t=lock, prev=0;

  while(s-buf < 8000 && *t) {
    if(isdigit(*t) && prev != '-' && !isalnum(prev) && prev != ':' &&
       prev != '/' && prev != '#')
      *s++='#';
    *s++=prev=*t++;
  }
  *s='\0';
  return buf;
}


/**** Mud-Specific Section ***************************************************/

// Convert TinyMuse @color attributes.
//
static char *convert_color(char *text)
{
  static char *v15table[20]={
    "Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White",
    "BlackB","RedB","GreenB","YellowB","BlueB","MagentaB","CyanB","WhiteB",
    "Normal", "Bright", "Blink", "Reverse"};

  static char v11convert[35]="#+nnNnrrRrggGgyyYybbBbmmMmccCcwwWw";
  static char v14convert[41]="!+uub#r!NxRrGgYyBbMmCcWw0X1R2G3Y4B5M6C7W";
  static char v15convert[21]="xrgybmcwXRGYBMCWn+#!";

  // v11 color:  |codes|
  // v14 color:  |codes+text|
  // v15 color:  |comma separated words|

  static char buf[8192];
  char *r, *s=text, *t=buf;
  int a;

  /* Match TinyMuse color types by DB version */
  switch(db_vers) {
  case 11:
    while(*s) {
      for(a=0;a<34;a+=2)
        if(*s == v11convert[a])
          break;
      if(a == 34)
        return "";
      *t++=v11convert[a+1];
      s++;
    }
    *t='\0';
    return buf;

  case 14:
    while(*s && *s != '+') {
      for(a=0;a<40;a+=2)
        if(*s == v14convert[a])
          break;
      if(a == 40)
        return "";
      *t++=v14convert[a+1];
      s++;
    }
    if(*s != '+')
      return "";

    /* Add text */
    *t++='|';
    while(*++s)
      *t++=*s;

    /* End color */
    strcpy(t, "%|n");
    return buf;

  case 15:
    while((r=parse_up(&s,',')))
      for(a=0;a<20;a++)
        if(!strcasecmp(r, v15table[a])) {
          *t++=v15convert[a];
          break;
        }
    *t='\0';
    return buf;
  }

  return "";
}

// Convert color specifiers in TinyMuse attributes.
//
static char *color_attribute(char *text)
{
  static char buf[8192];
  char *r, *s=buf, *t=text, *str;

  while(s-buf < 8000 && *t) {
    if(*t == '|' && *(t+1) != '|' && *(t+1) != ' ' && (r=strchr(t+1, '|'))) {
      *r='\0';
      if(*(str=convert_color(t+1))) {
        *s++='%';
        *s++='|';
        s+=sprintf(s, "%s", str);
        t=r;
      }
      *r='|';
    }
    *s++=*t++;
  }

  *s='\0';
  return buf;
}

// Convert TinyMuse object flags.
//
static int convert_muse_flags(int old)
{
  int new;

  /* Convert old object type mask */
  new=old & 0xF;
  if(new >= 8)
    new=TYPE_PLAYER;
  else if(new == 3)
    new=TYPE_ZONE;
  else if(new > 3)
    new=TYPE_GARBAGE;

  /* Old TinyMuse v1 flag checking */
  if(db_format == F_MUSE && db_vers == 1)
    if(!(old & 0x40000) ^ !(old & 0x20))
      old ^= 0x40020;  /* Chown_Ok and Link_Ok are switched around. Toggle. */

  /* Remove unused flag bitmask */
  switch(new & 0x7) {
    case TYPE_ROOM:
      new |= (old & 0x00041200);  // Equivalent flags
      break;
    case TYPE_THING:
      if(old & 0x80)	// Light Flag
        new|=LIGHT;
      if(old & 0x80000)	// Enter_Ok
        new|=THING_ENTER_OK;
      break;
      new |= (old & 0x00000200);  // Equivalent flags
    case TYPE_EXIT:
      if(old & 0x10)  // Light Flag
        new|=LIGHT;
      break;
    case TYPE_PLAYER:
      new |= (old & 0x00400080);  // Equivalent flags
      break;
  }

  return new | (old & 0x09924460);  // Flags from any object type
}

// Convert TinyMush standard flags.
//
static int convert_mush_flags(FILE *f, int *royalty)
{
  /* 64 bitflags */
  static int conv_table[][3]={
    {3, TYPE_EXIT, EXIT_TRANSPARENT},
    {5, TYPE_ROOM, ROOM_LINK_OK},
    {6, NOTYPE, DARK},
    {7, TYPE_ROOM, ROOM_JUMP_OK},
    {9, TYPE_THING, THING_DESTROY_OK},
    {11, NOTYPE, QUIET},
    {12, NOTYPE, HAVEN},
    {14, NOTYPE, GOING},
    {16, TYPE_PLAYER, PUPPET},  /* Myopic */
    {17, NOTYPE, PUPPET},
    {18, NOTYPE, CHOWN_OK},
    {19, TYPE_THING, THING_ENTER_OK},
    {20, NOTYPE, VISIBLE},
    {23, NOTYPE, OPAQUE},
    {24, NOTYPE, VERBOSE},
    {31, TYPE_PLAYER, PLAYER_TERSE},

    /* 2nd word */
    {33, TYPE_ROOM, ROOM_ABODE},
    {34, TYPE_ROOM, ROOM_FLOATING},
    {35, TYPE_PLAYER, DARK},  /* Unfindable */
    {36, NOTYPE, BEARING},
    {37, NOTYPE, LIGHT},
    {45, TYPE_ZONE, ZONE_UNIVERSAL},
    {60, TYPE_PLAYER, PLAYER_NOTICE},
    {63, TYPE_PLAYER, PLAYER_SLAVE},
  };

  int i, j, new, flags1, flags2=0;

  /* Read in flagsets */
  flags1=atoi(read_string(f));
  if(db_flags & MUSH_XFLAGS)
    flags2=atoi(read_string(f));
  if(db_flags & MUSH_3FLAGS)
    read_string(f);  /* 3rd flag word is currently reserved for softcode */

  /* Convert old object type mask */
  new=flags1 & 0x7;
  if(new >= 5)
    new=TYPE_GARBAGE;

  /* Convert only the flags we use */

  for(i=0;i < NELEM(conv_table);i++) {
    j=conv_table[i][0];
    if(((j < 32 && (flags1 & (1 << j))) ||
        (j >= 32 && (flags2 & (1 << (j-32))))) &&
       (conv_table[i][1] == NOTYPE || conv_table[i][1] == (new & 7)))
      new |= conv_table[i][2];
  }

  if(flags1 & 0x10)
    *royalty=1;
  else if(flags1 & 0x20000000)
    *royalty=2;
  else
    *royalty=0;

  return new;
}

// Convert PennMush old-style flags.
//
static int convert_penn_old_flags(FILE *f, int *royalty)
{
  /* flags1 bits */
  static int conv_table[][3]={
    {5, TYPE_ROOM, ROOM_LINK_OK},
    {6, NOTYPE, DARK},
    {7, NOTYPE, VERBOSE},
    {11, NOTYPE, QUIET},
    {12, NOTYPE, HAVEN},
    {14, NOTYPE, GOING},
    {18, NOTYPE, CHOWN_OK},
    {19, TYPE_THING, THING_ENTER_OK},
    {20, NOTYPE, VISIBLE},
    {21, NOTYPE, LIGHT},
    {23, NOTYPE, OPAQUE},
  };

  /* flags2 bits, based on object type */
  static int objtype_table[][3]={
    {3, TYPE_PLAYER, PLAYER_TERSE},
    {3, TYPE_ROOM, ROOM_FLOATING},
    {4, TYPE_PLAYER, PUPPET},  /* Myopic */
    {4, TYPE_ROOM, ROOM_ABODE},
    {5, TYPE_ROOM, ROOM_JUMP_OK},
    {9, TYPE_EXIT, EXIT_TRANSPARENT},
  };

  int i, new, flags1, flags2;

  /* Read in flagsets */
  flags1=atoi(read_string(f));
  flags2=atoi(read_string(f));

  /* Convert old object type mask */
  new=flags1 & 0x7;
  if(new >= 5)
    new=TYPE_GARBAGE;

  /* Convert only the flags we use */

  for(i=0;i < NELEM(conv_table);i++)
    if((conv_table[i][1] == NOTYPE || conv_table[i][1] == (new & 7)) &&
       (flags1 & (1 << conv_table[i][0])))
      new |= conv_table[i][2];

  for(i=0;i < NELEM(objtype_table);i++)
    if(objtype_table[i][1] == (new & 7) &&
       (flags2 & (1 << objtype_table[i][0])))
      new |= objtype_table[i][2];

  if(flags1 & 0x10)
    *royalty=1;
  else if(flags1 & 0x400000)
    *royalty=2;
  else
    *royalty=0;

  return new;
}

// Convert PennMush new-style flags.
//
static int convert_penn_new_flags(FILE *f, int *royalty)
{
  /* List of flag words */
  static struct {
    char *name;
    int type;
    int flag;
  } conv_table[]={
    {"ABODE", TYPE_ROOM, ROOM_ABODE},
    {"CHOWN_OK", NOTYPE, CHOWN_OK},
    {"DARK", NOTYPE, DARK},
    {"ENTER_OK", TYPE_THING, THING_ENTER_OK},
    {"FLOATING", TYPE_ROOM, ROOM_FLOATING},
    {"GOING", NOTYPE, GOING},
    {"HAVEN", NOTYPE, HAVEN},
    {"JUMP_OK", TYPE_ROOM, ROOM_JUMP_OK},
    {"LIGHT", NOTYPE, LIGHT},
    {"LINK_OK", TYPE_ROOM, ROOM_LINK_OK},
    {"MYOPIC", TYPE_PLAYER, PUPPET},
    {"OPAQUE", NOTYPE, OPAQUE},
    {"QUIET", NOTYPE, QUIET},
    {"TERSE", TYPE_PLAYER, PLAYER_TERSE},
    {"TRANSPARENT", TYPE_EXIT, EXIT_TRANSPARENT},
    {"VERBOSE", NOTYPE, VERBOSE},
    {"VISUAL", NOTYPE, VISIBLE},
  };

  char *r, *s;
  int i, j, new;

  *royalty=0;

  /* Read object type */
  i=atoi(read_string(f));
  new=(i & 1)?TYPE_ROOM:(i & 2)?TYPE_THING:(i & 4)?TYPE_EXIT:
      (i & 8)?TYPE_PLAYER:TYPE_GARBAGE;

  /* Read flag list */
  s=read_string(f);
  while((r=strspc(&s))) {
    if(!strcmp("WIZARD", r))
      *royalty=1;
    else if(!strcmp("ROYALTY", r)) {
      if(!*royalty)
        *royalty=2;
    } else {
      for(i=0;i < NELEM(conv_table);i++)
        if(conv_table[i].type == NOTYPE || conv_table[i].type == (new & 7)) {
          if((j=strcmp(conv_table[i].name, r)) < 0)
            continue;
          if(!j)
            new |= conv_table[i].flag;
          break;
        }
    }
  }

  return new;
}

// Convert TinyMuse builtin-attribute indexes.
//
static int convert_muse_attr(dbref thing, int attr, char *text)
{
  static int conv_table[]={
    59, A_EMAIL, 135, A_OCONN, 136, A_ACONN, 137, A_ODISC, 138, A_ADISC,
    142, A_APAGE, 155, A_USE, 156, A_OUSE, 157, A_AUSE, 158, A_HLOCK,
    159, A_PLOCK, 161, A_LLOCK, 162, A_LFAIL, 163, A_OLFAIL, 164, A_ALFAIL,
    165, A_SLOCK, 166, A_SFAIL, 168, A_ASFAIL, 169, A_DOING, 178, A_PLAN,
    182, A_AIDESC, 183, A_OIDESC, 184, A_CAPTION, 196, A_KILL, 197, A_AKILL,
    198, A_OKILL, 201, A_COMTITLE};

  static int removed_atr[]={10, 15, 16, 17, 18, 27, 28, 31, 38, 41, 46, 51, 53};

  int i;

  switch(attr) {
  case 5:
    if(Typeof(thing) != TYPE_PLAYER)
      return 0;
    if(strlen(text) != 13)
      crypt_pass(thing, text, 0);
    else {
      unsigned char *x=text, *r=db[thing].data->pass;
      int a, b=0;

      for(a=0;a<13;a++)
        x[a]=x[a]<58?x[a]-46:x[a]<91?x[a]-53:(x[a]-59);
      for(a=0;a<7;b+=4) {
        r[a++]=(x[b]<<2)+(x[b+1]>>4);
        r[a++]=(x[b+1]<<4)+(x[b+2]>>2);
        r[a++]=(x[b+2]<<6)+(x[b+3]);
      }
      r[a]=x[b]<<2;
    }
    return 0;
  case 7:
    if(Typeof(thing) == TYPE_PLAYER) {
      if((*text|32) == 'f' || (*text|32) == 'w')
        db[thing].data->gender=1;
      return 0;
    }
    return 5;
  case 25:
    if(Typeof(thing) == TYPE_PLAYER)
      db[thing].data->lastoff=atol(text);
    return 0;
  case 30:
    if(Typeof(thing) == TYPE_PLAYER)
      db[thing].data->last=atol(text);
    return 0;
  case 52:
    if(Typeof(thing) == TYPE_PLAYER)
      db[thing].pennies=atoi(text);
    return 0;
  case 57:
    if(Typeof(thing) == TYPE_PLAYER) {
      char *s=strrchr(text, ':');

      db[thing].data->tz=atoi(text);
      if(s)
        db[thing].data->tzdst=(*(s+1) == 'y' || *(s+1) == 'Y');
    }
    return 0;
  case 10: case 11: case 15:
    if(db_vers < 13)  /* These attrs remain unchanged up to v12 */
      return attr;
    if(attr == 11 && db_vers == 15) {  /* v15 @color attribute */
      strcpy(text, convert_color(text));
      if(*text)
        return A_COLOR;
    }
    return 0;
  case 175:  /* 175 and 176 are only valid up to v14 databases */
    if(db_vers < 15)
      return A_AIDESC;
    return 0;
  case 176:
    if(db_vers < 15)
      return A_OIDESC;
    return 0;
  case 179:
    if(db_vers == 15)
      return A_RLNAME;
    return 0;
  case 180:
    if(Typeof(thing) == TYPE_PLAYER)
      db[thing].data->sessions=atoi(text);
    return 0;
  }

  /* change attributes via convert table */
  for(i=0;i < NELEM(conv_table);i+=2)
    if(conv_table[i] == attr)
      return conv_table[i+1];

  /* Delete these attributes */
  for(i=0;i < NELEM(removed_atr);i++)
    if(removed_atr[i] == attr)
      return 0;

  /* Special cases for deletion */
  if((attr >= 55 && attr <= 99) || attr > 134)
    return 0;

  /* Incompatible channel string. Remove them. */
  if(db_vers > 12 && attr == A_CHANNEL)
    return 0;

  /* Else retain the current attribute value */
  return attr;
}

// Converts TinyMush builtin-attribute numbers to new format.
//
static int convert_mush_attr(dbref thing, int attr, char **string)
{
  static int conv_table[]={
    16, A_AUSE, 39, A_ACONN, 40, A_ADISC, 45, A_USE, 46, A_OUSE, 50, A_LEAVE,
    51, A_OLEAVE, 52, A_ALEAVE, 55, A_MOVE, 56, A_OMOVE, 57, A_AMOVE,
    58, A_ALIAS, 59, A_ELOCK, 60, A_LLOCK, 61, A_PLOCK, 62, A_ULOCK,
    63, A_LGIVE, 64, A_EALIAS, 65, A_LALIAS, 66, A_EFAIL, 67, A_OEFAIL,
    68, A_AEFAIL, 69, A_LFAIL, 70, A_OLFAIL, 71, A_ALFAIL, 72, A_HAVEN,
    73, A_AWAY, 74, A_IDLE, 75, A_UFAIL, 76, A_OUFAIL, 77, A_AUFAIL,
    87, A_LGIVE, 88, A_LASTFROM, 93, A_LLINK, 98, A_LPARENT, 129, A_GFAIL,
    130, A_OGFAIL, 131, A_AGFAIL, 209, A_SLOCK, 216, A_VLINK, 221, A_HTML};

  char *text=*string, *s;
  int i;

  switch(attr) {
  case 5:
    if(Typeof(thing) != TYPE_PLAYER)
      return 0;
    if(strlen(text) != 13)
      crypt_pass(thing, text, 0);
    else {
      unsigned char *x=text, *r=db[thing].data->pass;
      int a, b=0;

      for(a=0;a<13;a++)
        x[a]=x[a]<58?x[a]-46:x[a]<91?x[a]-53:(x[a]-59);
      for(a=0;a<7;b+=4) {
        r[a++]=(x[b]<<2)+(x[b+1]>>4);
        r[a++]=(x[b+1]<<4)+(x[b+2]>>2);
        r[a++]=(x[b+2]<<6)+(x[b+3]);
      }
      r[a]=x[b]<<2;
    }
    return 0;
  case 7:
    if(Typeof(thing) == TYPE_PLAYER) {
      if((*text|32) == 'f' || (*text|32) == 'w')
        db[thing].data->gender=1;
      return 0;
    }
    return 5;
  case 25:
    if(Typeof(thing) == TYPE_PLAYER)
      db[thing].pennies=atoi(text);
    return 0;
  case 30:
    if(Typeof(thing) == TYPE_PLAYER)
      db[thing].data->last=atol(text);
    return 0;
  case 42:
    *string=convert_boolexp(text);
    return A_LOCK;
  case 43:
    if(!(db_flags & MUSH_ATRNAME))
      return 0;

    /* Set object name with alias parsing for exits */
    if((s=strchr(text, ';')))
      *s++='\0';
    WPTR(db[thing].name, secure_string(text));

    /* Add exit aliases */
    if(Typeof(thing) == TYPE_EXIT && *s) {
      *string=s;
      return A_ALIAS;
    }
    return 0;

  /* Delete these attributes */
  case 17: case 18: case 27: case 28: case 31:
    return 0;
  }

  /* Change attributes via convert table */
  for(i=0;i < NELEM(conv_table);i+=2)
    if(conv_table[i] == attr)
      return conv_table[i+1];

  /* Special cases for deletion */
  if((attr >= 38 && attr <= 99) || attr >= 126)
    return 0;

  /* Else retain the current attribute value */
  return attr;
}

// Converts PennMush builtin-attribute names to TinyMare numeric format.
//
static int convert_penn_attr(dbref thing, char *name, char *text)
{
  static char *special_attrs[]={
    "Last", "Lastlogout", "Pennies", "Sex", "Xyxxy"};

  static char *str_types[]={
    "Aahear", "Aclone", "Aconnect", "Adeath", "Adescribe", "Adestroy",
    "Adisconnect", "Adrop", "Aefail", "Aenter", "Afailure", "Afollow", "Agive",
    "Ahear", "Aidescribe", "Aleave", "Alfail", "Alias", "Amail", "Amhear",
    "Amove", "Apayment", "Areceive", "Asuccess", "Atport", "Aufail",
    "Aunfollow", "Ause", "Away", "Azenter", "Azleave", "Charges", "Comment",
    "Conformat", "Cost", "Death", "Debugforwardlist", "Descformat", "Describe",
    "Drop", "Ealias", "Efail", "Enter", "Exitformat", "Exitto", "Failure",
    "Filter", "Follow", "Following", "Followers", "Forwardlist", "Give",
    "Haven", "Idescformat", "Idescribe", "Idle", "Infilter", "Inprefix",
    "Lalias", "LastIP", "Lastfailed", "Lastpaged", "Lastsite", "Leave",
    "Lfail", "Listen", "Mailcurf", "Mailfilters", "Mailfolders",
    "Mailsignature", "Move", "Nameaccent", "Nameformat", "Odeath", "Odescribe",
    "Odrop", "Oefail", "Oenter", "Ofailure", "Ofollow", "Ogive", "Oidescribe",
    "Oleave", "Olfail", "Omove", "Opayment", "Oreceive", "Oxmove", "Osuccess",
    "Otport", "Oufail", "Ounfollow", "Ouse", "Oxenter", "Oxleave", "Oxtport",
    "Ozenter", "Ozleave", "Payment", "Prefix", "Receive", "Queue",
    "Registered_Email", "Rquota", "Runout", "Semaphore", "Startup", "Success",
    "Tfprefix", "Tport", "Ufail", "Unfollow", "Use", "Va", "Vb", "Vc", "Vd",
    "Ve", "Vf", "Vg", "Vh", "Vi", "Vj", "Vk", "Vl", "Vm", "Vn", "Vo", "Vp",
    "Vq", "Vr", "Vrml_url", "Vs", "Vt", "Vu", "Vv", "Vw", "Vx", "Vy", "Vz",
    "Zenter", "Zleave"};

  static unsigned char str_attrs[]={
    0, A_ACLONE, A_ACONN, A_AKILL, A_ADESC, A_ADISC, 0, A_ADROP, A_AEFAIL,
    A_AENTER, A_AFAIL, 0, A_AGIVE, A_AHEAR, A_AIDESC, A_ALEAVE, A_ALFAIL,
    A_ALIAS, 0, 0, A_AMOVE, A_APAY, 0, A_ASUCC, 0, A_AUFAIL, 0, A_AUSE, A_AWAY,
    0, 0, 0, 0, 0, A_COST, A_KILL, 0, 0, A_DESC, A_DROP, A_EALIAS, A_EFAIL,
    A_ENTER, 0, 0, A_FAIL, 0, 0, 0, 0, 0, A_GIVE, A_HAVEN, 0, A_IDESC, A_IDLE,
    0, 0, A_LALIAS, 0, 0, 0, A_LASTFROM, A_LEAVE, A_LFAIL, A_LISTEN, 0, 0, 0,
    0, A_MOVE, 0, 0, A_OKILL, A_ODESC, A_ODROP, A_OEFAIL, A_OENTER, A_OFAIL, 0,
    A_OGIVE, A_OIDESC, A_OLEAVE, A_OLFAIL, A_OMOVE, A_OPAY, 0, 0, A_OSUCC, 0,
    A_OUFAIL, 0, A_OUSE, 0, 0, 0, 0, 0, A_PAY, A_PREFIX, 0, 0, 0, 0, 0, 0,
    A_STARTUP, A_SUCC, 0, 0, A_UFAIL, 0, A_USE, 100, 101, 102, 103, 104, 105,
    106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 0, 118, 119,
    120, 121, 122, 123, 124, 125, 0, 0};

  int i, j;

  /* Scan list of attrs with special meaning first */
  for(i=0;i < NELEM(special_attrs);i++) {
    if(!(j=strcasecmp(special_attrs[i], name))) {
      switch(i) {
      case 0:  // Last
        if(Typeof(thing) == TYPE_PLAYER)
          db[thing].data->last=atol(text);
        return 0;
      case 1:  // Lastlogout
        if(Typeof(thing) == TYPE_PLAYER)
          db[thing].data->lastoff=atol(text);
        return 0;
      case 2:  // Pennies
        if(Typeof(thing) == TYPE_PLAYER)
          db[i].pennies=atoi(text);
        return 0;
      case 3:  // Sex
        if(Typeof(thing) == TYPE_PLAYER) {
          if((*text|32) == 'f' || (*text|32) == 'w')
            db[thing].data->gender=1;
          return 0;
        }
        return A_SEX;
      case 4:  // Xyxxy
        if(Typeof(thing) != TYPE_PLAYER)
          return 0;

        /* Set a default pass until SHA Encryption is implemented in Mare */
        crypt_pass(thing, "pennmush", 0);
        return 0;
      }
    }
    if(j > 0)
      break;
  }

  /* Scan attribute names by string */
  for(i=0;i < NELEM(str_types);i++) {
    if(!(j=strcasecmp(str_types[i], name)))
      return str_attrs[i];
    if(j > 0)
      break;
  }

  return -1;  /* User-defined attribute */
}

// Load TinyMuse string-based attribute definitions.
//
static void muse_get_atrdefs(FILE *f, dbref thing)
{
  ATRDEF *new, *last=NULL;
  int flags, obj, num=0;
  char *s;
  
  while(*(s=read_string(f))) {
    if(*s == '\\')
      return;

    /* Convert attribute flags */
    flags=atoi(s+1);
    obj=atoi(read_string(f));
    s=read_string(f);
    if(++num > 255)  /* Maximum of 255 attribute definitions on object */
      continue;

    new=malloc(sizeof(ATRDEF));
    new->next=NULL;
    new->atr.flags=(flags & 0x01FF) | ((flags & 0x4000) ? AF_HAVEN:0);
    new->atr.obj=obj;
    new->atr.num=num;
    SPTR(new->atr.name, s);

    if(last)
      last->next=new;
    else
      db[thing].atrdefs=new;
    last=new;
  }
}

// Load TinyMush global attribute definitions (found in header).
//
void mush_get_atrdefs(FILE *f)
{
  struct mush_defattr *atr;
  char buf[1024], *r, *s;
  int num, flags, len;

  /* Get attribute number */
  num=atoi(fgets(buf, 1024, f));
  if(num < 256)
    return;

  /* Get attribute name/flags */
  s=read_string(f);
  if(!(r=strchr(s, ':')) || !*++r)
    return;
  len=strlen(r)+1;

  /* Create new mush atrdef entry */
  atr=malloc(sizeof(struct mush_defattr)+len);
  atr->next=mushdefs;
  mushdefs=atr;
  atr->num=num;
  atr->flags=0;
  memcpy(atr->name, r, len);
  mush_atrdefs++;

  /* Convert attribute flags */
  flags=atoi(s);
  if(flags & 0x000A)	atr->flags |= AF_DARK;
  if(flags & 0x000C)	atr->flags |= AF_WIZ;
  if(flags & 0x0100)	atr->flags |= HAVEN;
  if(flags & 0x0210)	atr->flags |= AF_HIDDEN;
  if(flags & 0x0400)	atr->flags |= AF_LOCK;
  if(flags & 0x0800)	atr->flags |= AF_OSEE;
  if(!(flags & 0x1000))	atr->flags |= INH;
}

// Free global attributes after database is loaded.
//
void mush_free_atrdefs()
{
  struct mush_defattr *ptr, *next;

  for(ptr=mushdefs;ptr;ptr=next) {
    next=ptr->next;
    free(ptr);
  }
  mushdefs=NULL;
  mush_atrdefs=0;
}


/**** Load Database Object Section *******************************************/

// Loads objects in string-based, non-compressed form.
//
int muse_db_read_object(dbref i, FILE *f)
{
  char alias[256], buf[8192], *r, *s;
  int a, money=0;
  dbref newobj;

  /* Read next object number */
  while(fgets(buf, 8192, f)) {
    if(*buf == '&' || *buf == '!')
      break;
    if(*buf == '*')  /* End of database */
      return NOTHING;
    if(*buf == '@') {
      /* Load TinyMuse Configuration Variables */
      for(a=0; fgets(buf, 8192, f); a++) {
        if(*buf == '~')  /* End of config list */
          break;
        switch(a) {
          case 0: USER_LIMIT=atoi(buf); break;     // Mortal User Limit
          case 1: WIZLOCK_LEVEL=atoi(buf); break;  // Restrict Connect Class
        }
      }
      if(a) {
        log_main("Loaded %d configuration setting%s.", a, a==1?"":"s");
        log_main("%s", "");
      }
    }
  }

  /* Read in object */
  speaker=newobj=atoi(buf+1);

  if(newobj < 0 || newobj >= db_init) /* Invalid object# */
    db_error(tprintf("Invalid database object: #%d (previous #%d)",
             newobj, i-1));
  if(newobj < i) /* Object# is lower than previous. oops. */
    db_error(tprintf("Object order mismatch: #%d to #%d.", i-1, newobj));

  /* We're skipping objects here, fill in the ones we missed */
  if(newobj > i)
    for(a=i;a<newobj;a++)
      new_object();

  /* Read it in */
  new_object();

  /* Set object name with alias parsing for exits */
  s=read_string(f);
  *alias=0;
  if((r=parse_up(&s, ';'))) {
    strmaxcpy(alias, s, 256);
    if(strlen(r) > 50)
      r[50]='\0';
    s=secure_string(r);
  }
  WPTR(db[newobj].name, s);

  if(db_vers == 14)
    read_string(f);  /* Ignore @cname string */

  db[newobj].location=atoi(read_string(f));

  /* Temporarily store object zone */
  if(db_format == F_MUSE_ZONED)
    zonelist[newobj]=atoi(read_string(f));

  db[newobj].contents=atoi(read_string(f));
  db[newobj].exits=atoi(read_string(f));
  if(db_vers > 4)
    db[newobj].link=atoi(read_string(f));
  db[newobj].next=atoi(read_string(f));
  if(db_vers < 9)
    atr_add(newobj, A_LOCK, convert_boolexp(read_string(f)));
  db[newobj].owner=atoi(read_string(f));
  if(db_vers < 4)
    money=atoi(read_string(f));
  db[newobj].flags=convert_muse_flags(atoi(read_string(f)));

  /* Convert old links */
  if(db_vers < 5) {
    if(Typeof(newobj) == TYPE_ROOM || Typeof(newobj) == TYPE_EXIT) {
      db[newobj].link = db[newobj].location;
      db[newobj].location = newobj;
    } else {
      db[newobj].link = db[newobj].exits;
      db[newobj].exits = NOTHING;
    }
  }

  if(db_vers > 9) {
    db[newobj].create_time=atol(read_string(f));
    db[newobj].mod_time=atol(read_string(f));
  } else if(Typeof(newobj) == TYPE_PLAYER)
    db[newobj].create_time=now;

  if(Typeof(newobj) == TYPE_PLAYER) {
    if(FORCE_UC_NAMES) {
      /* Capitalize player's name */
      *db[newobj].name=toupper(*db[newobj].name);
    }

    /* Players Must be owned by themselves */
    db[newobj].owner=newobj;

    /* Allocate space for extra player data */
    db[newobj].data=calloc(1, sizeof(struct playerdata));

    /* Set default timezone */
    db[newobj].data->tz=TIMEZONE;
    db[newobj].data->tzdst=HAVE_DST;

    if(money > 0)
      db[newobj].pennies=money;

    if(db_vers >= 6) {
      /* TinyMuse has a very loose class structure */
      static char powlvl[10]={1, 1, 1, 1, 2, 2, 3, 3, 4, 5};

      a=atoi(read_string(f))-1;
      initialize_class(newobj, (a < 0)?CLASS_PLAYER : (a > 10)?CLASS_WIZARD :
                       powlvl[a]);
    } else
      initialize_class(newobj, (newobj == GOD)?CLASS_WIZARD:CLASS_PLAYER);
  } else if(Typeof(newobj) == TYPE_EXIT)
    db[newobj].plane=-1;  /* Exits can be seen in any plane by default */

  /* Read attributes, parents, atrdefs */
  read_attrs(newobj, f);
  if(db_vers > 7) {
    db[newobj].parents=read_list(f);
    db[newobj].children=read_list(f);
    muse_get_atrdefs(f, newobj);
  }

  /* Misc variables */
  if(Typeof(newobj) == TYPE_PLAYER) {
    while((s=strchr(db[newobj].name,' ')))  /* Playernames can't have spaces */
      *s='_';
    if(strlen(db[newobj].name) > 16)
      db[newobj].name[16]='\0';   /* Strip long player names to 16 chars */
    add_player(newobj);
  }

  /* Set exit alias */
  if(Typeof(newobj) == TYPE_EXIT && *alias)
    atr_add(newobj, A_ALIAS, alias);

  return newobj;
}

// Read string-based attribute list.
//
static void read_attrs(dbref thing, FILE *f)
{
  ALIST *new, *last;
  char *s, *cmp;
  int attr, len;
  dbref obj;
  
  for(last=db[thing].attrs;last && last->next;last=last->next);

  while(*(s=read_string(f))) {
    if(*s == '<')  /* End of attr list */
      return;
    if(*s != '>')  /* TinyMush Attribute with %R is truncated */
      continue;
    attr=atoi(s+1);  /* Get attribute number */

    /* Read object attribute is defined on */
    if(db_format == F_MUSE_ZONED && db_vers >= 8)
      obj=atoi(read_string(f));
    else
      obj=NOTHING;

    /* Read attribute data */
    s=read_string(f);

    /* TinyMush Attribute Ownership/Locks (ignored) */
    if(*s == 1) {
      if(!(s=strchr(s, ':')))
        continue;
      if(!(s=strchr(++s, ':')))
        continue;
      s++;
    }

    /* Convert attributes to Mare format */
    if(obj == NOTHING) {
      if(db_format == F_MUSH || db_format == F_MUSH3) {
        if(attr > 255) {
          attr=mush_atr_search(thing, attr);
          obj=thing;
        } else
          attr=convert_mush_attr(thing, attr, &s);
      } else
        attr=convert_muse_attr(thing, attr, s);
      if(!attr)
        continue;
    } else if(obj != NOTHING)
      attr++;  /* Increment 1 for user-defined attributes */

    /* Exceed max #atrdefs on object, or is a global Mush user-defined attr */
    if(attr > 255)
      continue;

    if(db_format == F_MUSE_ZONED && db_vers >= 11)
      cmp=compress(color_attribute(s));
    else
      cmp=compress(s);
    len=strlen(cmp)+1;

    db_attrs++;
    new=malloc(sizeof(ALIST)+len);
    new->next=NULL;
    new->num=attr;
    new->obj=obj;
    memcpy(new->text, cmp, len);
    if(last)
      last->next=new;
    else
      db[thing].attrs=new;
    last=new;
  }
}

// Load TinyMush string-based attribute definitions.
//
static int mush_atr_search(dbref thing, int atrnum)
{
  struct mush_defattr *atr;
  ATRDEF *k, *last=NULL;
  int num=0;

  /* Scan for attribute in global list */
  for(atr=mushdefs;atr;atr=atr->next)
    if(atr->num == atrnum)
      break;
  if(!atr)
    return 0;  /* Attribute not found */

  /* Count number of existing atrdefs on <thing> */
  for(k=db[thing].atrdefs;k;k=k->next,num++)
    last=k;
  if(++num > 255)
    return 0;  /* Too many attrs defined */

  /* Define the attribute */
  k=malloc(sizeof(ATRDEF));
  k->next=NULL;
  k->atr.flags=atr->flags;
  k->atr.obj=thing;
  k->atr.num=num;
  SPTR(k->atr.name, atr->name);

  if(last)
    last->next=k;
  else
    db[thing].atrdefs=k;

  return num;
}

// Loads objects in string-based, non-compressed form.
//
int mush_db_read_object(dbref i, FILE *f)
{
  char alias[256], buf[8192], *r, *s;
  int a, money=0, royalty;
  dbref newobj;

  /* Read next string header */
  while(fgets(buf, 8192, f) && *buf != '!')
    if(*buf == '*')  /* End of database */
      return NOTHING;

  /* Read in object */
  speaker=newobj=atoi(buf+1);

  if(newobj < 0 || newobj >= db_init) /* Invalid object# */
    db_error(tprintf("Invalid database object: #%d (previous #%d)",
             newobj, i-1));
  if(newobj < i) /* Object# is lower than previous. oops. */
    db_error(tprintf("Object order mismatch: #%d to #%d.", i-1, newobj));

  /* We're skipping objects here, fill in the ones we missed */
  if(newobj > i)
    for(a=i;a<newobj;a++)
      new_object();

  /* Read it in */
  new_object();

  /* Set object name with alias parsing for exits */
  *alias=0;
  if(!(db_flags & MUSH_ATRNAME)) {
    s=read_string(f);
    if((r=parse_up(&s, ';'))) {
      strmaxcpy(alias, s, 256);
      if(strlen(r) > 50)
        r[50]='\0';
      s=secure_string(r);
    }
    WPTR(db[newobj].name, s);
  }

  db[newobj].location=atoi(read_string(f));

  /* Tempoarily store object's Zone */
  if(db_flags & MUSH_ZONE)
    zonelist[newobj]=atoi(read_string(f));

  db[newobj].contents=atoi(read_string(f));
  db[newobj].exits=atoi(read_string(f));
  if(db_flags & MUSH_LINK)
    db[newobj].link=atoi(read_string(f));
  db[newobj].next=atoi(read_string(f));
  if(!(db_flags & MUSH_ATRKEY))
    atr_add(newobj, A_LOCK, convert_boolexp(read_string(f)));
  db[newobj].owner=atoi(read_string(f));

  /* Tempoarily store object's @parent */
  if(db_flags & MUSH_PARENT)
    parentlist[newobj]=atoi(read_string(f));

  if(!(db_flags & MUSH_ATRMONEY))
    money=atoi(read_string(f));
  db[newobj].flags=convert_mush_flags(f, &royalty);

  /* Ignore powers list */
  if(db_flags & MUSH_POWERS) {
    read_string(f);
    read_string(f);
  }

  if(Typeof(newobj) == TYPE_PLAYER)
    db[newobj].create_time=now;

  /* Swap attribute meanings around */
  switch(Typeof(newobj)) {
  case TYPE_EXIT:
    db[newobj].link=db[newobj].location;
    db[newobj].location=db[newobj].exits;
    db[newobj].exits=NOTHING;
    db[newobj].plane=-1;  /* Exits can be seen in any plane by default */
    break;
  case TYPE_ROOM:
    db[newobj].link=db[newobj].location;
    db[newobj].location=newobj;
    break;
  case TYPE_THING:
    if(!(db_flags & MUSH_LINK)) {
      db[newobj].link=db[newobj].exits;
      db[newobj].exits=NOTHING;
    }
    break;
  case TYPE_PLAYER:
    if(!(db_flags & MUSH_LINK)) {
      db[newobj].link=db[newobj].exits;
      db[newobj].exits=NOTHING;
    }

    /* Owner on players means something entirely different in Mush */
    db[newobj].owner=newobj;

    if(FORCE_UC_NAMES) {
      /* Capitalize player's name */
      *db[newobj].name=toupper(*db[newobj].name);
    }

    /* Allocate space for extra player data */
    db[newobj].data=calloc(1, sizeof(struct playerdata));

    /* Set default timezone */
    db[newobj].data->tz=TIMEZONE;
    db[newobj].data->tzdst=HAVE_DST;

    if(money > 0)
      db[newobj].pennies=money;

    initialize_class(newobj, (newobj == GOD || royalty == 1)?CLASS_WIZARD:
                     (royalty == 2)?CLASS_WIZARD-1:CLASS_PLAYER);
  }

  /* Read attribute list */
  read_attrs(newobj, f);

  /* Misc variables */
  if(Typeof(newobj) == TYPE_PLAYER) {
    while((s=strchr(db[newobj].name,' ')))  /* Playernames can't have spaces */
      *s='_';
    if(strlen(db[newobj].name) > 16)
      db[newobj].name[16]='\0';   /* Strip long player names to 16 chars */
    add_player(newobj);
  }

  /* Set exit alias */
  if(Typeof(newobj) == TYPE_EXIT && *alias)
    atr_add(newobj, A_ALIAS, alias);

  return newobj;
}

// Reads the Spiffy Lock format in the latest Pennmush databases. Each lock
// is saved in the database like the following:
//
// lockcount #      - #=The number of lock structures to read in.
//
//  type "Type"     - Type=A lock type from 'lock_types[]' below.
//  creator #       - #=Object dbref# who set the lock.
//  flags #         - #=Lock flags; ignored during conversion.
//  key "Value"     - Value=The actual text stored in the attribute.
//
static void read_penn_locks_v3(dbref newobj, FILE *f)
{
  static char *lock_types[]={
    "Basic", "Enter", "Use", "Page", "Speech",
    "Parent", "Link", "Leave", "Give"};

  static unsigned char lock_attrs[]={
    A_LOCK, A_ELOCK, A_ULOCK, A_PLOCK, A_SLOCK,
    A_LPARENT, A_LLINK, A_LLOCK, A_LGIVE};

  char buf[32], *s;
  int i, j;

  /* Make sure the "lockcount" keyword exists */
  fgets(buf, 32, f);
  if(strncmp(buf, "lockcount ", 10))
    db_error(tprintf("Invalid lock count for object: #%d.", newobj));
  i=atoi(buf+10);

  /* Read and validate at most <i> lock structures */
  while(i-- > 0) {
    fread(buf, 5, 1, f);
    if(strncmp(buf, "type ", 5))
      db_error(tprintf("Invalid lock for object: #%d.", newobj));
    s=read_string(f);
    for(j=0;j < NELEM(lock_types);j++)
      if(!strcmp(lock_types[j], s))
        break;
    read_string(f);
    read_string(f);
    if(j == NELEM(lock_types))
      read_string(f);
    else {
      fread(buf, 4, 1, f);
      if(strncmp(buf, "key ", 4))
        db_error(tprintf("Invalid lock key for object: #%d.", newobj));

      /* Add the lock */
      atr_add(newobj, lock_attrs[j], read_string(f));
    }
  }
}

// Read string-based attribute list.
//
static void read_penn_attrs(dbref thing, FILE *f)
{
  ALIST *new, *last;
  ATRDEF *newdef, *lastdef=NULL;
  char buf[1024], *s, *t, *cmp;
  int flags, attr, len, numdef=0;
  dbref obj;

  for(last=db[thing].attrs;last && last->next;last=last->next);

  while(fgets(buf, 1024, f)) {
    if(buf[0] == '<')  /* End of attr list */
      return;
    if(buf[0] != ']' || buf[1] == '^' || !buf[1])
      db_error(tprintf("Error reading attributes on object: #%d.", thing));

    s=buf+1;
    parse_up(&s, '^');
    parse_up(&s, '^');
    flags=atoi(s);

    /* Read attribute data */
    t=read_string(f);

    /* Convert attributes to Mare format */
    if(!(attr=convert_penn_attr(thing, buf+1, t)))
      continue;
    if(attr == -1) {  /* User-defined attribute */
      if(++numdef > 255)  /* Maximum of 255 attribute definitions on object */
        continue;

      /* Add attribute definition */
      newdef=malloc(sizeof(ATRDEF));
      newdef->next=NULL;
      newdef->atr.flags=0;
      newdef->atr.obj=thing;
      newdef->atr.num=numdef;
      SPTR(newdef->atr.name, buf+1);

      /* Convert attribute flags */
      if(flags & 0x0002)    newdef->atr.flags |= AF_HIDDEN;
      if(flags & 0x0004)    newdef->atr.flags |= AF_WIZ;
      if(flags & 0x0040)    newdef->atr.flags |= AF_DARK;
      if(!(flags & 0x0080)) newdef->atr.flags |= INH;
      if(flags & 0x0200)    newdef->atr.flags |= AF_OSEE;
      if(flags & 0x80000)   newdef->atr.flags |= AF_UNIMP;

      if(!(db_flags & PENN_AF_VISUAL) && !(flags & 0x0001))
        newdef->atr.flags |= AF_OSEE;

      if(lastdef)
        lastdef->next=newdef;
      else
        db[thing].atrdefs=newdef;
      lastdef=newdef;

      obj=thing;
      attr=numdef;
    } else
      obj=NOTHING;  /* Built-in attribute */

    if(!*t)
      continue;

    /* Add the attribute */
    cmp=compress(t);
    len=strlen(cmp)+1;

    db_attrs++;
    new=malloc(sizeof(ALIST)+len);
    new->next=NULL;
    new->num=attr;
    new->obj=obj;
    memcpy(new->text, cmp, len);
    if(last)
      last->next=new;
    else
      db[thing].attrs=new;
    last=new;
  }
}

// Loads Pennmush-style objects in string-based, non-compressed form.
//
int penn_db_read_object(dbref i, FILE *f)
{
  char alias[256], buf[8192], *r, *s;
  int a, money=0, royalty=0;
  dbref newobj;

  /* Read next string header */
  while(fgets(buf, 8192, f) && *buf != '!')
    if(*buf == '*')  /* End of database */
      return NOTHING;

  /* Read in object */
  speaker=newobj=atoi(buf+1);

  if(newobj < 0 || newobj >= db_init) /* Invalid object# */
    db_error(tprintf("Invalid database object: #%d (previous #%d)",
             newobj, i-1));
  if(newobj < i) /* Object# is lower than previous. oops. */
    db_error(tprintf("Object order mismatch: #%d to #%d.", i-1, newobj));

  /* We're skipping objects here, fill in the ones we missed */
  if(newobj > i)
    for(a=i;a<newobj;a++)
      new_object();

  /* Read it in */
  new_object();

  /* Set object name with alias parsing for exits */
  *alias=0;
  s=read_string(f);
  if((r=parse_up(&s, ';'))) {
    strmaxcpy(alias, s, 256);
    if(strlen(r) > 50)
      r[50]='\0';
    s=secure_string(r);
  }
  WPTR(db[newobj].name, s);

  db[newobj].location=atoi(read_string(f));
  db[newobj].contents=atoi(read_string(f));
  db[newobj].exits=atoi(read_string(f));
  db[newobj].next=atoi(read_string(f));
  parentlist[newobj]=atoi(read_string(f));  /* Temporary object list */

  /* Read locks */
  if(db_flags & PENN_LOCKS_V3)
    read_penn_locks_v3(newobj, f);
  else
    db_error(tprintf("Cannot read object #%d with v%d Locks.", newobj,
             (db_flags & PENN_LOCKS_V2)?2:1));

  db[newobj].owner=atoi(read_string(f));
  zonelist[newobj]=atoi(read_string(f));  /* Temporary object list */
  money=atoi(read_string(f));

  if(!(db_flags & PENN_NEW_FLAGS))
    db[newobj].flags=convert_penn_old_flags(f, &royalty);
  else
    db[newobj].flags=convert_penn_new_flags(f, &royalty);

  /* Read miscellaneous attributes */
  if(!(db_flags & PENN_NO_POWERS)) {
    a=atoi(read_string(f));
    if(newobj != GOD && !royalty && Typeof(newobj)==TYPE_PLAYER && (a & 0x10))
      db[newobj].flags |= PLAYER_BUILD;
  }
  if(!(db_flags & PENN_NO_CHAT))
    read_string(f);
  if(db_flags & PENN_WARNINGS)
    read_string(f);

  /* Read creation time */
  if(db_flags & PENN_CREATION) {
    db[newobj].create_time=atoll(read_string(f));
    db[newobj].mod_time=atoll(read_string(f));
  } else if(Typeof(newobj) == TYPE_PLAYER)
    db[newobj].create_time=now;

  /* Swap attribute meanings around */
  switch(Typeof(newobj)) {
  case TYPE_EXIT:
    db[newobj].link=db[newobj].location;
    db[newobj].location=db[newobj].exits;
    db[newobj].exits=NOTHING;
    db[newobj].plane=-1;  /* Exits can be seen in any plane by default */
    break;
  case TYPE_ROOM:
    db[newobj].link=db[newobj].location;
    db[newobj].location=newobj;
    break;
  case TYPE_THING:
    db[newobj].link=db[newobj].exits;
    db[newobj].exits=NOTHING;
    break;
  case TYPE_PLAYER:
    db[newobj].link=db[newobj].exits;
    db[newobj].exits=NOTHING;

    /* Owner on players means something entirely different in Mush */
    db[newobj].owner=newobj;

    if(FORCE_UC_NAMES) {
      /* Capitalize player's name */
      *db[newobj].name=toupper(*db[newobj].name);
    }

    /* Allocate space for extra player data */
    db[newobj].data=calloc(1, sizeof(struct playerdata));

    /* Set default timezone */
    db[newobj].data->tz=TIMEZONE;
    db[newobj].data->tzdst=HAVE_DST;

    if(money > 0)
      db[newobj].pennies=money;

    initialize_class(newobj, (newobj == GOD || royalty == 1)?CLASS_WIZARD:
                     (royalty == 2)?CLASS_WIZARD-1:CLASS_PLAYER);
  }

  /* Read attribute list */
  read_penn_attrs(newobj, f);

  /* Misc variables */
  if(Typeof(newobj) == TYPE_PLAYER) {
    while((s=strchr(db[newobj].name,' ')))  /* Playernames can't have spaces */
      *s='_';
    if(strlen(db[newobj].name) > 16)
      db[newobj].name[16]='\0';   /* Strip long player names to 16 chars */
    add_player(newobj);
  }

  /* Set exit alias */
  if(Typeof(newobj) == TYPE_EXIT && *alias)
    atr_add(newobj, A_ALIAS, alias);

  return newobj;
}


/**** DB Conversion Fixup Routines *******************************************/

void install_zones()
{
  dbref i;

  if(!zonelist)
    return;

  /* Set universal zone object */
  if(zonelist[0] > 1 && Typeof(zonelist[0]) == TYPE_THING) {
    /* Change object type to zone + Universal flag */
    db[zonelist[0]].flags &= NOTYPE_FLAGMASK;
    db[zonelist[0]].flags |= TYPE_ZONE|ZONE_UNIVERSAL;
    db[db[zonelist[0]].location].contents = NOTHING;
    PUSH_L(uzo, zonelist[0]);
  } else
    zonelist[0]=NOTHING;

  for(i=2;i<db_top;i++) {
    if(Typeof(i) == TYPE_ROOM && zonelist[i] != zonelist[0] &&
       zonelist[i] > 1) {
      if(Typeof(zonelist[i]) == TYPE_THING) {
        /* Change object type to zone */
        db[zonelist[i]].flags &= NOTYPE_FLAGMASK;
        db[zonelist[i]].flags |= TYPE_ZONE;

        /* Clear zone's contents list */
        db[db[zonelist[i]].location].contents = NOTHING;
      }

      /* Add to room's zone list */
      if(Typeof(zonelist[i]) == TYPE_ZONE) {
        PUSH_L(db[i].zone, zonelist[i]);
        PUSH_L(db[zonelist[i]].zone, i);
      }
    }
  }

  free(zonelist);
  zonelist=NULL;
}

void install_parents()
{
  dbref i;

  if(!parentlist)
    return;

  for(i=0;i<db_top;i++)
    if(!Invalid(parentlist[i]) && !is_a(i, parentlist[i]) &&
       !is_a(parentlist[i], i)) {
      PUSH_L(db[i].parents, parentlist[i]);
      PUSH_L(db[parentlist[i]].children, i);
    }

  free(parentlist);
  parentlist=NULL;
}

void check_integrity()
{
  dbref i, j, next;

  /* Restore contents/exit locations */
  for(i=0;i<db_top;i++) {
    DOLIST(j, db[i].contents)
      db[j].location=i;
    DOLIST(j, db[i].exits)
      db[j].location=i;
  }

  /* Validate locations */
  for(i=0;i<db_top;i++) {
    /* Check for destroyed garbage objects */
    if(db[i].flags == (TYPE_THING|GOING) && db[i].location == NOTHING) {
      db[i].owner=GOD;
      db[i].flags=TYPE_GARBAGE;
      continue;
    }

    switch(Typeof(i)) {
    case TYPE_ZONE:
    case TYPE_GARBAGE:
      db[i].location=NOTHING;
      break;
    case TYPE_ROOM:
      db[i].location=i;
      break;
    default:
      if(db[i].location == NOTHING) {
        if(Typeof(i) != TYPE_PLAYER) {
          recycle(i);
          break;
        }
        db[i].location=db[i].link;
        ADDLIST(i, db[db[i].location].contents);
      }

      /* Make sure that object location is in contents/exits list */
      if(Typeof(i) == TYPE_EXIT)
        j=db[db[i].location].exits;
      else
        j=db[db[i].location].contents;
      DOLIST(j, j)
        if(j == i)
          break;
      if(j == NOTHING) {
        if(Typeof(i) == TYPE_EXIT)
          ADDLIST(i, db[db[i].location].exits);
        else
          ADDLIST(i, db[db[i].location].contents);
      }
    }
  }

  /* Check links */
  for(i=0;i<db_top;i++) {
    switch(Typeof(i)) { 
    case TYPE_GARBAGE:
      db[i].link=NOTHING;
      break;
    case TYPE_ZONE:
      if(!Invalid(db[i].link) && Typeof(db[i].link) != TYPE_ROOM)
        db[i].link=NOTHING;
      break;
    case TYPE_ROOM:
      if(db[i].link != HOME && (Invalid(db[i].link) ||
         (!Invalid(db[i].link) && Typeof(db[i].link) != TYPE_ROOM)))
        db[i].link=NOTHING;
      break;
    case TYPE_EXIT:
      if(db[i].link != HOME && (Invalid(db[i].link) ||
         (!Invalid(db[i].link) && Typeof(db[i].link) != TYPE_ROOM)))
        recycle(i);
      break;
    case TYPE_THING:
      if(Invalid(db[i].link) || Typeof(db[i].link) == TYPE_EXIT ||
         Typeof(db[i].link) == TYPE_ZONE || Typeof(db[i].link) == TYPE_GARBAGE)
        db[i].link=db[i].owner;
      break;
    case TYPE_PLAYER:
      if(Invalid(db[i].link) || Typeof(db[i].link) != TYPE_ROOM)
        db[i].link=PLAYER_START;
      break;
    }
  }

  /* Now check restrictions on location */
  for(i=0;i<db_top;i++) {
    /* Make sure zones contain no objects */
    if(Typeof(i) == TYPE_ZONE) {
      for(j=db[i].contents;j != NOTHING;j=next) {
        next=db[j].next;
        if(i != j)
          enter_room(j, HOME);
      }
      db[i].contents=NOTHING;
    }

    /* Remove all exits pointing to or from invalid rooms */
    if(Typeof(i) == TYPE_EXIT && (Invalid(db[i].location) ||
       db[i].link == NOTHING || Typeof(db[i].location) != TYPE_ROOM ||
       (db[i].link != HOME && Typeof(db[i].link) != TYPE_ROOM)))
      recycle(i);

    /* Players must be located in a room or an object */
    else if(Typeof(i) == TYPE_PLAYER && Typeof(db[i].location) != TYPE_ROOM &&
            Typeof(db[i].location) != TYPE_THING)
      enter_room(i, HOME);
  }

  validate_links();
}

void load_inzone_lists()
{
  dbref i, *p;

  for(i=0;i<db_top;i++)
    if(Typeof(i) == TYPE_ROOM)
      for(p=db[i].zone;p && *p != NOTHING;p++)
        PUSH_L(db[*p].zone, i);
}
