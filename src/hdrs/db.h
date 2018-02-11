/* hdrs/db.h */

extern dbref speaker;
extern dbref notify_location;

extern dbref atime_obj;
extern dbref wander_obj;
extern int wander_count;

extern int dberrcheck;
extern int boot_flags;
extern int mare_test_db;
extern int db_forceload;
extern int db_readonly;

extern int default_color;

extern int ndisrooms;
extern int treedepth;

extern dbref run_exit;
extern int walk_verb;
extern int enter_plane;

extern FILE *db_read_file;
extern char *db_file;
extern int dbref_len;
extern int db_attrs;
extern int db_lists;

extern int disp_hidden;

extern char env[10][8192];
extern char log_text[8192];
extern dbref log_dbref;

extern int func_recur;
extern int func_total;


/* Object type definitions (0-7) */

#define TYPE_ROOM 	0
#define TYPE_THING 	1
#define TYPE_EXIT 	2
#define TYPE_PLAYER	3
#define TYPE_ZONE	4
#define TYPE_GARBAGE	7

#define NOTYPE		-1	// Matches any object type

#define Typeof(x) (db[x].flags & 0x7)
#define IS(thing,type,flag) ((Typeof(thing)==type) && (db[thing].flags&(flag)))

/* Miscellaneous Flags (Any object type) */
#define CHOWN_OK	BV5
#define DARK		BV6
#define VERBOSE		BV8
#define HAVEN		BV10
#define GOING		BV14
#define PUPPET		BV17
#define VISIBLE		BV20
#define OPAQUE		BV23
#define QUIET		BV24
#define LIGHT		BV26
#define BEARING		BV27

/* The above flags OR'd together */
#define NOTYPE_FLAGMASK	\
  (BV5|BV6|BV8|BV10|BV14|BV17|BV20|BV23|BV24|BV26|BV27)

/* Object flags */
#define THING_MOUNT        BV3
#define THING_UNSAVED      BV7
#define THING_DESTROY_OK   BV9
#define THING_WANDER       BV11
#define THING_ENTER_OK     BV15
#define THING_MONSTER      BV22
#define THING_GRAB_OK      BV25

/* Exit flags */
#define EXIT_TRANSPARENT   BV3
#define EXIT_DOOR          BV4

/* Player flags */
#define PLAYER_BUILD       BV4
#define PLAYER_SLAVE       BV7
#define PLAYER_CONNECT     BV9
#define PLAYER_INACTIVE_OK BV11
#define PLAYER_TERSE       BV22
#define PLAYER_NOTICE      BV25

/* Room flags */
#define ROOM_XZONE	BV3
#define ROOM_OCEANIC	BV4
#define ROOM_JUMP_OK	BV9
#define ROOM_HEALING	BV11
#define ROOM_FLOATING	BV12
#define ROOM_ILLNESS	BV13
#define ROOM_NEST	BV15
#define ROOM_FOG	BV16
#define ROOM_LINK_OK	BV18
#define ROOM_ARENA	BV19
#define ROOM_SANCTUARY	BV21
#define ROOM_SHAFT	BV22
#define ROOM_ABODE	BV25

/* Zone flags */
#define ZONE_UNIVERSAL	BV4
#define ZONE_RESTRICTED	BV7

/* Internal (unsaved) flags */
#define ROOM_MARKED	BV31	// Check for disconnected rooms


/* Special dbref's */
#define GOD		 1	// Ultimate wizard character
#define NOTHING		-1	// Null dbref
#define AMBIGUOUS	-2	// Multiple possibilities, for matchers
#define HOME		-3	// Virtual room, represents mover's home

/* Structure for defining flags in game/unparse.c */
extern struct flagstr {
  char *name;
  char flag;
  char objtype;
  int bits;
} flaglist[];

/* Definitions used for <trig> type in global_trigger(): */
#define TRIG_PLAYER	BV0
#define TRIG_ROOM	BV1
#define TRIG_CONTENTS	BV2
#define TRIG_INVENTORY	BV3
#define TRIG_ZONES	BV4
#define TRIG_UZO	BV5

#define TRIG_ALL	(~0)	// All trig types included


/* Macros for gender substitutions */
enum gender_types { MALE, FEMALE, NEUTER, PLURAL };
enum pronoun_types { SUBJ=0, OBJN=4, POSS=8, APOSS=12 };

/* Returns a random number from 0 to num-1 */
#define rand_num(num) ({ int _n=num; \
                         (_n < 2)?0:((mt_rand() & 0x7fffffff) % _n); })

/* Randomized division with floating point effect */
#define rdiv(x,y)     ({ typeof(x) _x=x; int _y=y; \
                         (!_y)?0:((_x / _y)+(rand_num(_y) < _x % _y)); })

/* Get size of file */
#define fsize(file)   ({ struct stat st; \
                         (long)(stat(file, &st)?-1:st.st_size); })

/* Modification time of file */
#define fdate(file)   ({ struct stat st; \
                         stat(file, &st)?-1:st.st_mtime; })


/* Read/write strings/integers to a file in big-endian format: */

/* Strings */
#define getstring(x,p) SPTR(p, getstring_noalloc(x))

/* 8-bit */
#define putchr(f, num) fputc(num, f)

/* 16-bit */
#define putshort(f, num)  ({ unsigned short _x=htons(num); \
			     fwrite(&_x, 2, 1, f); })

#define getshort(f)       ({ unsigned short _x; \
                             fread(&_x, 2, 1, f); ntohs(_x); })

/* 32-bit */
#define putnum(f, num)  ({ int _x=htonl(num); fwrite(&_x, 4, 1, f); })

#define getnum(f)       ({ int _x; fread(&_x, 4, 1, f); ntohl(_x); })


/* 64-bit */
#define putlong(f, num)  ({ quad _x=htonll(num); fwrite(&_x, 8, 1, f); })

#define getlong(f)       ({ quad _x; fread(&_x, 8, 1, f); ntohll(_x); })


#define atr_get(thing, num) atr_get_obj(thing, NOTHING, num)
#define atr_add(thing, num, str) atr_add_obj(thing, NOTHING, num, str)

#define class(x) (db[x].data->class)
#define terminfo(x,y) (db[db[x].owner].data->term & (y))

#define view_icons(x) (IS(x, TYPE_PLAYER, PLAYER_CONNECT) && \
                       (db[x].data->term & TERM_ICONS))

#define power(x,pow) controls(x, NOTHING, pow)
#define could_doit(x,y,attr) ({ dbref _tr=y; char *_s=atr_get(_tr, attr); \
                                *_s?eval_boolexp(x, _tr, _s):1; })
#define notify_except(loc,thing,fmt,args...) \
                         notify_room(loc, thing, NOTHING, fmt, ## args)
#define atr_parse(x,y,attr) parse_function(x, y, atr_get(x, attr))
#define get_zone(player) ({ dbref *_zone=Zone(player); \
                            _zone ? *_zone : NOTHING; })

#define Invalid(x) ({ dbref _obj=x; (_obj < 0 || _obj >= db_top); })
#define Immortal(x) (db[db[x].owner].data->class >= CLASS_IMMORTAL)
#define Guest(x) (db[db[x].owner].data->class == CLASS_GUEST)
#define Builder(x) (IS(db[x].owner, TYPE_PLAYER, PLAYER_BUILD) || \
                    power(x, POW_BUILD))
#define Quiet(x) (db[x].flags & QUIET)
#define Zone(player) ({ dbref _loc=mainroom(player); \
                        (Typeof(_loc) == TYPE_ROOM)?db[_loc].zone:NULL; })

#define Desc(x) (db[x].data->desc)
#define ipaddr(x) ({ struct in_addr _addr; _addr.s_addr=x; inet_ntoa(_addr); })

#define func_zerolev() ({ func_recur=0; func_total=0; })

#define getidletime(x) ({ DESC *_desc; \
			  (Typeof(x) != TYPE_PLAYER || !(_desc=Desc(x)))?-1: \
			  (now - _desc->last_time); })

/* Game startup flags */
#define BOOT_RESTART	BV0	// Hard reset the game during a reboot
#define BOOT_QUIET	BV1	// Suppress log_main from being displayed
#define BOOT_VALIDATE	BV2	// Do not save temporary structures over reboot

/* Database format versions */
#define F_MUSH		1	// TinyMush
#define F_MUSE		2	// TinyMuse
#define F_MUSE_ZONED	3	// TinyMuse Zoned
#define F_MARE		4	// TinyMare Compressed RAM
#define F_MUSH3		5	// TinyMush-3
#define F_PENNMUSH	6	// PennMush

/* Tinymare DB flags */
#define DB_LONGINT	BV0	// Time values stored as 8 bytes instead of 4
#define DB_COMBAT	BV1	// Combat data is stored for players

/* Tinymush DB flags */
#define MUSH_ZONE	BV0	// ZONE/DOMAIN field
#define MUSH_LINK	BV1	// LINK field (exits from objs)
#define MUSH_GDBM	BV2	// Attrs are in a GDBM DB, not here
#define MUSH_ATRNAME	BV3	// NAME is an attr, not in the hdr
#define MUSH_ATRKEY	BV4	// KEY is an attr, not in the hdr
#define MUSH_PARENT	BV5	// DB has the PARENT field
#define MUSH_ATRMONEY	BV7	// Money is kept in an attribute
#define MUSH_XFLAGS	BV8	// An extra word of flags
#define MUSH_POWERS	BV9	// Powers?
#define MUSH_3FLAGS	BV10	// Adding a 3rd flag word
#define MUSH_QUOTED	BV11	// Quoted strings, ala PennMUSH

/* Pennmush DB flags */
#define PENN_NO_CHAT	BV0	// No chat system stored in DB
#define PENN_WARNINGS	BV1	// No warnings stored in DB
#define PENN_CREATION	BV2	// Each DB object has a Creation Time
#define PENN_NO_POWERS	BV3	// No per-object powers stored in DB
#define PENN_LOCKS_V2	BV4	// DB contains v2 Locks
#define PENN_QUOTED	BV5	// Attribute values contain quoted strings
#define PENN_IMMORTAL	BV7	// Split Immortal?
#define PENN_NO_TEMPLE	BV8	// Bit for Temple flag changed
#define PENN_AF_VISUAL	BV10	// Attribute bit for Visual changed
#define PENN_LOCKS_V3	BV16	// DB contains v3 Locks
#define PENN_NEW_FLAGS	BV17	// DB contains flagnames in ASCII text


/* Attribute structure */
typedef struct {
  char *name;		  // Full name of attribute
  dbref obj;		  // Object in which attr is defined
  unsigned char num;	  // Attr number in cache (1-255)
  unsigned int flags:16;  // Any flags the attribute holds (listed below)
} ATTR;

/* Attribute flags */
#define AF_OSEE    BV0    // Players other than owner can see it
#define AF_DARK    BV1    // Wizards or owner of object defined on can see
#define AF_WIZ     BV2    // Only wizards can change it
#define AF_UNIMP   BV3    // Not important -- don't save value in the db
#define AF_HIDDEN  BV4    // Changed and viewed only by the high almighty
#define AF_DATE    BV5    // Date stored in universal longint form
#define INH        BV6    // Value inherited by children
#define AF_LOCK    BV7    // Interpreted as a boolean expression
#define AF_FUNC    BV8    // This is a user defined function
#define AF_HAVEN   BV9    // Can't execute commands on this attribute
#define AF_BITMAP  BV10   // An attribute expressed in hexadecimal form
#define AF_DBREF   BV11   // Calls unparse_object on #dbref numbers
#define AF_COMBAT  BV12   // Attribute editable only with pow_combat
#define AF_PRIVS   BV13   // Function executes with privs of object owner

extern const char *attr_options[16];

/* External attribute list */
enum attributes {
#define DOATTR(num, var, name, flags) var=num,
#include "attrib.h"
#undef DOATTR
};


/* Attribute list structure */
struct alist {
  struct alist *next;
  dbref obj;
  unsigned char num;
  char text[0];
};

struct atrdef {
  struct atrdef *next;
  ATTR atr;
};

typedef struct atrdef ATRDEF;
typedef struct alist ALIST;


/* Database structures: */

struct playerdata {
  DESC *desc;

  unsigned int class:8;
  unsigned int rows:8;
  unsigned int cols:8;

  int tz:5;
  unsigned int tzdst:1;
  unsigned int gender:1;
  unsigned int passtype:1;

  unsigned int term:16;
  int steps;
  int sessions;
  int age;

  long last;
  long lastoff;

  unsigned char pass[20];
  unsigned char powers[(NUM_POWS+3)/4];
};

/* Database reference object data */
struct object {
  char *name;			// Name of object
  dbref location;		// Pointer to container
  dbref contents;		// Pointer to first item
  dbref exits;			// Pointer to first exit for rooms
  dbref link;			// Pointer to home/destination
  dbref next;			// Pointer to next in contents/exits chain
  dbref owner;			// Who controls this object

  unsigned int flags;		// Object flag list
  int plane;			// Dimension plane setting
  int pennies;			// Money value

  int queue;			// Number of commands in queue on object
  ALIST *attrs;			// Attribute data list
  ATRDEF *atrdefs;		// User-defined attribute list

  dbref *parents;		// Parents list
  dbref *children;		// Children list
  dbref *zone;			// Zone list on Rooms, Inzone list on Zones

  struct playerdata *data;	// Player data (see structure above)

  long mod_time;		// Date of modification
  long create_time;		// Date of creation
};

extern struct object *db;
extern dbref db_top;
extern dbref *uzo;

/* Macros for contents/exits lists */

#define DOLIST(var, first) \
  for(var=first; var != NOTHING; var=db[var].next)
#define ADDLIST(thing, locative) \
  (db[thing].next=locative, locative=thing)

/* Macros for object lists */

#define PUSH_F(list,value)	push_first(&list,value)
#define PUSH_L(list,value)	push_last(&list,value)
#define PULL(list,value)	pull_list(&list,value)

/* Returns true if <match> is in <list> */
#define inlist(list,match) \
  ({ \
    dbref *_l=list; \
    while(_l && *_l != NOTHING && *_l != (match)) _l++; \
    (!_l || *_l == NOTHING)?NULL:_l; \
  })

/* Counts elements in <list> */
#define list_size(list)  ({ int _i; dbref *_l=list; \
                            for(_i=0;_l && _l[_i] != NOTHING;_i++); _i; })

/* Erases contents of <list> */
#define CLEAR(list)  ({ dbref **_l=&(list); \
                        if(*_l) { free(*_l); *_l=NULL; db_lists--; } })

/* Increases the size of an array by 1 at <index> to <total> elements */
#define insert_array(orig, index, total, elem_size) ({ \
  int _i=index, _total=total, _size=elem_size; \
  void *_new=realloc(orig, _total*_size); \
  memmove(_new+(_i+1)*_size, _new+_i*_size, (_total-_i-1)*_size); _new; })

/* Shrinks the size of an array by 1 at <index> to <total> elements */
#define shrink_array(orig, index, total, elem_size) ({ \
  int _i=index, _total=total, _size=elem_size; \
  void *_orig=orig; \
  memmove(_orig+_i*_size, _orig+(_i+1)*_size, (_total-_i)*_size); \
  realloc(_orig, _total*_size); })

/* Attribute list structure */
struct all_atr_list {
  ATTR *type;
  char *value;
  unsigned int numinherit;
};

/* TinyMare-specific db configuration rules */
extern struct db_rule {
  int type;
  char *text;
  int (*load)();
  void (*save)();
} db_rule[];

/* +Com System lock table */
extern struct comlock {
  struct comlock *next;
  char *channel;
  dbref *list;
} *comlock_list;

/* Hash table manipulation */
#define make_hashtab(list) make_hash_table(list, sizeof(list), sizeof(*list))

/* Hash structure */
typedef struct hashent {
  struct hashent *ptr;
} hash;
