/* db/load.c */
/* Contains routines to load most types of mud databases */

#include "externs.h"

#ifdef linux
# include <sys/mman.h>
#endif

struct object *db;
dbref *uzo;
dbref db_top;
dbref db_size;
dbref db_init;                    
dbref db_tech;

int db_format;
int db_vers;
int db_combat_vers;

int dbref_len=1;
int db_pows=NUM_POWS;
int db_flags;

extern int mush_atrdefs;
extern int db_quoted_strings;

dbref *zonelist;
dbref *parentlist;

static void load_db_header(FILE *f);
static int db_read_object(dbref i, FILE *f);
static void get_attrs(FILE *f, dbref thing);


// Allocate memory for a new database object.
//
dbref new_object()
{
  struct object *new_array;
  dbref i;
  
  /* If there are no destroyed objects to use, increase the size of db array */
  if(!game_online || (i=free_get()) == NOTHING) {
    if(game_online && DB_LIMIT > 1 && db_top >= DB_LIMIT)
      return NOTHING;
    i=db_top++;

    /* When creating first object, allocate entire size of db */
    if(!i)
      db_size=db_init-(db_init % 1000);

    if(!i || db_top > db_size) {
      db_size += 1000;
      if(game_online)
        log_main("DB: Increasing Database Array Size to %d Objects", db_size);

#ifdef linux
      /* Temporary measure: Use mmap() to cause SIGSEGV on accesses to #-1 */
      if(!db)
        new_array=mmap((void *)0x9000000, db_size*sizeof(struct object),
                       PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
      else
        new_array=mremap(db, (db_size-1000)*sizeof(struct object),
                         db_size*sizeof(struct object), MREMAP_MAYMOVE);

      if(new_array == MAP_FAILED)
        new_array=NULL;
#else
      new_array=realloc(db, db_size*sizeof(struct object));
#endif
      if(!new_array) {
        if(!game_online) {
          perror("Database Allocation");
          exit(1);
        }
        perror("Database Resize");
        db_size -= 1000;
        db_top--;
        return NOTHING;
      }
      db=new_array;
    }
  }

  /* Zero out the new object */
  memset(db+i, 0, sizeof(struct object));

  /* Set default values */
  SPTR(db[i].name, "");
  db[i].location=NOTHING;
  db[i].contents=NOTHING;
  db[i].exits=NOTHING;
  db[i].link=NOTHING;
  db[i].next=NOTHING;
  db[i].owner=GOD;
  db[i].flags=TYPE_GARBAGE;
  
  return i;
}

// Read a compressed DBREF# from a database file, optimized based on dbref_len.
//
int getref(FILE *f)
{
  int i;

  if((i=getc(f)) == 0xFF)
    return NOTHING;

  if(dbref_len >= 2) {
    i=(i<<8)|getc(f);
    if(dbref_len >= 3) {
      i=(i<<8)|getc(f);
      if(dbref_len == 4)
        i=(i<<8)|getc(f);
    }
  }

  return i;
}

// Reads string with terminating null byte from file.
//
char *getstring_noalloc(FILE *f)
{
  static char buf[8192];
  char *s=buf;
  int ch=0;
  
  while(s-buf < 8000 && (ch=getc(f)) > 0)
    *s++=ch;
  *s='\0';

  while(ch > 0)  /* Read remainder of line */
    ch=getc(f);

  return buf;
}

// Load an object list from the database.
//
dbref *getlist(FILE *f)
{
  dbref a, b, *list=NULL;

  if(!(a=getref(f)+1))
    return NULL;

  db_lists++;
  list=malloc((a+1)*sizeof(dbref));
  for(b=0;b<a;b++)
    list[b]=getref(f);
  list[a]=NOTHING;
  return list;
}


FILE *db_read_file;

static struct {
  char *type;	// Name of database format
  int vers;	// Highest version number we know how to load
} dbtypes[]={
  {"Unknown", 0},
  {"TinyMush", 10},
  {"TinyMuse", 3},
  {"TinyMuse Zoned", 15},
  {"TinyMare Compressed RAM", 2},
  {"TinyMush-3", 1},
  {"PennMush", 1},
  {0}};

// When to execute runlevel (for each pass below):
//  0: always
//  1: zonelist != NULL
//  2: parentlist != NULL
//  3: db_format != F_MARE
//  4: !disable_startups && (Not @rebooting || BOOT_VALIDATE set)
// -N: Only execute when upgrading from TinyMARE DB version N

/* Database validation routines */
static struct {
  char *msg;
  int runlevel;
  void (*func)();
} passes[]={
  {"Installing zone hierarchy",		1, install_zones},
  {"Linking object parents",		2, install_parents},
  {"Checking database integrity",	3, check_integrity},
  {"Loading inzone lists",	       -1, load_inzone_lists},
  {"Recycling old objects",		0, destroy_old_objs},
  {"Activating startups",		4, start_game},
  {"Flushing queue",			4, flush_queue},
  {0}};

/* TinyMare-specific configuration rules */
struct db_rule db_rule[]={
  { 1, "configuration setting",	load_config, save_config},
//{ 2, "accounting detail",     load_accounting, save_accounting},
  { 3, "site lockout",          load_sitelocks, save_sitelocks},
  { 4, "channel lock",		load_comlocks, save_comlocks},
  { 0}};

//{11, "tracking object",       load_tracking_objs, save_tracking_objs},
// 11: List of ordinary objects available to track with the Tracking technique.
//     The argument can be a function that resolves either to a room# or an obj#
//     in a specific room. : plane ?

// Compose a human-readable database type and version string.
//
char *get_db_version()
{
  static char buf[50];

  strcpy(buf, dbtypes[db_format].type);
  if(db_format && db_vers)
    sprintf(buf+strlen(buf), " v%d", db_vers);
  return buf;
}

// Print an error message to the screen and abort the program.
//
__attribute__((__noreturn__))
void db_error(char *message)
{
  log_main("--");
  log_main("netmare: Database Load Error...");
  log_main("netmare: %s", message);
  exit(1);
}

// Create a new database from scratch.
//
void create_db()
{
  dbref i;

  /* Set database version */
  db_format=F_MARE;

  /* Create initial room */
  i=new_object();
  WPTR(db[i].name, "Limbo");
  db[i].flags=TYPE_ROOM;
  db[i].location=i;
  db[i].create_time=now;

  /* Create a placeholder for the GOD character */
  recycle(new_object());

}


// Non-blocking load portion of the database file.
// Returns 0 if there is still more to load, 1 when finished.
//
int load_database()
{
  static int state=0;  /* 0=load db header, 1=load objects, 2=do passes */
  static int i, old=0;
  int j;

  /* State 2: DB is loaded; continue execution of next pass. */
  if(state == 2) {
    static int pass=0, index=0;

    /* End of passes? Finalize game. */
    if(!passes[index].msg) {
      if(disable_startups)
        halt_object(AMBIGUOUS, NULL, 0);  /* Clear command queue */
      return 1;
    }

    /* Check if we are to do this pass */
    if(!passes[index].runlevel ||
       (passes[index].runlevel == 1 && zonelist) ||
       (passes[index].runlevel == 2 && parentlist) ||
       (passes[index].runlevel == 3 && db_format != F_MARE) ||
       (passes[index].runlevel == 4 && !disable_startups &&
        (nstat[NS_EXEC] == 1 || (boot_flags & BOOT_VALIDATE))) ||
       (passes[index].runlevel < 0 && db_format == F_MARE &&
        db_vers <= -passes[index].runlevel)) {

      log_main("Pass %2d: %s.", ++pass, passes[index].msg);
      if(passes[index].func == flush_queue)
        flush_queue(queue_size[Q_COMMAND]);
      else
        passes[index].func();
    }

    index++;
    return 0;
  }

  /* State 0: Load database header */
  if(state == 0) {
    load_db_header(db_read_file);

    /* Determine string type for database conversions */
    if(((db_format == F_MUSH || db_format == F_MUSH3) &&
        (db_flags & MUSH_QUOTED)) ||
       (db_format == F_PENNMUSH && (db_flags & PENN_QUOTED)))
      db_quoted_strings=1;

    log_main(" Type: %s", get_db_version());
    if(db_init)
      log_main(" Objs: %d", db_init);
    log_main("%s", "");

    /* Tinymush global attribute definitions in header */
    if(mush_atrdefs) {
      log_main("Loaded %d attribute definition%s.", mush_atrdefs,
               mush_atrdefs==1?"":"s");
      log_main("%s", "");
    }

    /* Make sure we have a valid db version */
    if(!db_format) {
      log_main("netmare: Unknown database file format. Aborting...");
      exit(1);
    } if(db_format && db_vers > dbtypes[db_format].vers) {
      log_main("netmare: Cannot load database: Unknown DB version.");
      log_main("netmare: I only know about %s versions up to %d.",
               dbtypes[db_format].type, dbtypes[db_format].vers);
      log_main("netmare: Please upgrade to a later release of TinyMARE.");
      exit(1);
    } if((db_format == F_MUSH || db_format == F_MUSH3) &&
         (db_flags & MUSH_GDBM)) {
      log_main("netmare: Cannot load database: Not a flat file format.");
      log_main("netmare: Please export your TinyMUSH database properly.");
      exit(1);
    }

    /* Select dbref byte-length */
    dbref_len=(db_init+1 < 0xFF)?1:(db_init+1 < 0xFF00)?2:
               (db_init+1 < 0xFF0000)?3:4;

    /* Initialize zone conversion */
    if(db_format == F_MUSE_ZONED || db_format == F_MUSH3 ||
       db_format == F_PENNMUSH)
      zonelist=calloc(db_init, sizeof(dbref));

    /* Initialize parent conversion */
    if(db_format == F_MUSH || db_format == F_MUSH3 ||
       db_format == F_PENNMUSH) {
      parentlist=malloc(db_init*sizeof(dbref));
      memset(parentlist, 0xff, db_init*sizeof(dbref));
    }

    /* Read TinyMare configuration */
    if(db_format == F_MARE) {
      int a, b, count;
      long length=0;

      while((a=fgetc(db_read_file)) > 0) {
        for(b=0;db_rule[b].type;b++)
          if(a == db_rule[b].type)
            break;
        if(db_vers > 1)
          length=getnum(db_read_file);
        if(!db_rule[b].type) {
          if(db_vers == 1)
            db_error(tprintf("Unrecognized configuration rule: %d", a));
          else
            log_main("Ignoring unknown configuration rule: %d", a);
          fseek(db_read_file, length, SEEK_CUR);
          continue;
        }
        count=db_rule[b].load(db_read_file);
        if(count == -1)
          log_main("Loaded %s.", db_rule[b].text);
        else if(count)
          log_main("Loaded %d %s.", count, count==1?db_rule[b].text:
                   pluralize(db_rule[b].text));
      }
      log_main("%s", "");
    }

    /* Initialize wtime(), weather, database accounting */
    if(!mare_test_db)
      init_config();

    state=1;
    return 0;
  }

  /* State 1: Load 1000 more DB Objects */

  do {
    i = db_read_object(i, db_read_file)+1;  /* returns 0 if last object read */
    if((i-1)/1000 > old)
      while((i-1)/1000 > old) {
        old++;
        if(db_init < 10000 || !(old % 10))
          log_main("Reading database object #%d000...", old);
      }
  } while(i % 1000);  /* Preempt loading every 1000 objects */

  if(i)
    return 0;  /* database not finished loading yet */

  /* Finish database object loading and run startup procedures */
  if(Typeof(GOD) != TYPE_PLAYER)
    db_error(tprintf("God account %s(#%d) is not a Player. Aborting...",
             db[GOD].name, GOD));

  fclose(db_read_file);
  log_main("%s", "");
  log_main("Database successfully loaded.");
  log_main("%s", "");

  /* Free temporary space used during conversion */
  if(mush_atrdefs)
    mush_free_atrdefs();

  /* Make sure GOD is still a wizard */
  if(class(GOD) != CLASS_WIZARD)
    initialize_class(GOD, CLASS_WIZARD);

  /* Maintain counts for queue commands over a @reboot */
  restore_queue_quotas();

  /* Disconnect all previously connected players only on first startup */
  if(nstat[NS_EXEC] == 1)
    for(j=0;j<db_top;j++)
      if(IS(j, TYPE_PLAYER, PLAYER_CONNECT))
        announce_disconnect(j);

  /* Move on to db passes */
  state=2;
  return 0;
}

// Determine database type by reading header (first few lines).
//
static void load_db_header(FILE *f)
{
  char buf[1024];

  while(1) {
    switch(getc(f)) {
    case EOF:
      return;  /* No valid header found! */
    case '+':
      switch(getc(f)) {
      case 'A':
        mush_get_atrdefs(f);  /* Attribute Definition */
        continue;
      case 'F':
        /* Skip over Pennmush flags table */
        fgets(buf, 1024, f);
        if(!strncmp(buf, "LAGS LIST", 9))
          while(fgets(buf, 1024, f))
            if(!strncmp(buf, "\"END OF FLAGS\"", 14))
              break;
        continue;
      case 'S':
        fgets(buf, 1024, f);
        db_init=atoi(buf);  /* DB Size */
        continue;
      case 'T':
        db_format=F_MUSH3; /* TinyMush v3.0 Header Format */
        fgets(buf, 1024, f);
        db_vers=atoi(buf) & 0xff;
        db_flags=atoi(buf) >> 8;
        continue;
      case 'V':
        db_format=F_MUSH; /* TinyMush v2.0 Header Format */
        fgets(buf, 1024, f);
        db_vers=atoi(buf) & 0xff;
        db_flags=atoi(buf) >> 8;
        continue;
      case 'X':
        db_format=0; /* TinyMux Header Format - unsupported */
        fgets(buf, 1024, f);
        db_vers=atoi(buf) & 0xff;
        db_flags=atoi(buf) >> 8;
        continue;
      default:
        fgets(buf, 1024, f);
        continue;
      }
    case '-':
      switch(getc(f)) {
      case 'R':
        /* record #players online (currently unused) */
      default:
        fgets(buf, 1024, f);
        continue;
      }
    case '@':
      db_format=F_MUSE;  /* TinyMuse Format */
      fgets(buf, 1024, f);
      db_vers=atoi(buf) & 0xff;
      continue;
    case '~':
      fgets(buf, 1024, f);
      db_init=atoi(buf);  /* DB Size */
      if(db_format == F_MUSH) {
        db_format=F_PENNMUSH;  /* PennMush Header Format */
        db_flags-=5;
        db_vers=1;
      }
      continue;
    case '!':
      ungetc('!', f);
      return;
    case '&':
      if(db_format == F_MUSE) {
        db_format=F_MUSE_ZONED;  /* Change to TinyMuse Zoned */
        rewind(f);  /* get TinyMuse configuration parameters */
        return;
      }
      /* Flow through */
    case '#':
      log_main("%s", "");
      log_main("Old-style mud database detected. Unable to load file.");
      exit(1);
    case 'M':
      fread(buf, 3, 1, f);
      if(strncmp(buf, "ARE", 3)) {
        fgets(buf, 1024, f);
        continue;
      }
      db_format=F_MARE;  /* TinyMare Compressed RAM */
      db_vers=fgetc(f);
      db_init=getnum(f);
      db_flags=getnum(f);
      db_pows=fgetc(f);
      if(db_flags & DB_COMBAT)
        db_combat_vers=fgetc(f);

      if(db_vers > 1 && (db_flags & DB_COMBAT) && !db_forceload) {
        log_main("%s", "");
        log_main("This MARE server has not been compiled with Combat support "
                 "and the database file contains Combat structures. TinyMARE "
                 "will ignore all combat settings when loading this database "
                 "and overwrite them during the first save.");
        log_main("If this is what you really want, make a backup of the "
                 "database file and run 'netmare' with the -f option.");
        exit(1);
      }
      return;
    default:
      fgets(buf, 1024, f);
    }
  }
}

// Read next dbref# object from the database.
//
static int db_read_object(dbref i, FILE *f)
{
  ATRDEF *new, *last=NULL;
  dbref newobj;
  int a;

  /* Old database versions */
  switch(db_format) {
  case F_MUSH: case F_MUSH3:
    return mush_db_read_object(i, f);
  case F_MUSE: case F_MUSE_ZONED:
    return muse_db_read_object(i, f);
  case F_PENNMUSH:
    return penn_db_read_object(i, f);
  }

  /* Read in object */
  speaker=newobj=getref(f);
  if(newobj == NOTHING)  /* End of database reached? */
    return NOTHING;

  if(newobj < i || newobj >= db_init || (!i && newobj)) {
    /* Something went wrong on last obj# */
    if(i > 0)
      db_error(tprintf("Error loading object: #%d.", i-1));
    else
      db_error("Cannot find first record on database.");
  }

  /* We're skipping objects here, fill in the ones we missed */
  if(newobj > i)
    for(a=i;a < newobj;a++)
      new_object();

  /* Allocate memory for another object */
  new_object();

  /* Read it in */
  getstring(f, db[newobj].name);
  db[newobj].location=getref(f);
  db[newobj].contents=getref(f);
  db[newobj].exits=getref(f);
  db[newobj].link=getref(f);
  db[newobj].next=getref(f);
  db[newobj].owner=getref(f);
  db[newobj].flags=getnum(f);

  if(db[newobj].link >= db_init)
    db[newobj].link=(db[newobj].link == db_init)?HOME:AMBIGUOUS;

  if(db_vers > 1)
    db[newobj].plane=getnum(f);
  else if(Typeof(newobj) == TYPE_EXIT)
    db[newobj].plane=-1;  /* Exits can be seen in any plane by default */

  db[newobj].pennies=getnum(f);

  if(db_flags & DB_LONGINT) {
    db[newobj].create_time=getlong(f);
    db[newobj].mod_time=getlong(f);
  } else {
    db[newobj].create_time=(unsigned)getnum(f);
    db[newobj].mod_time=(unsigned)getnum(f);
  }

  /* Read in object lists */
  db[newobj].parents=getlist(f);
  db[newobj].children=getlist(f);
  if(db_vers == 1)
    if((db[newobj].zone=getlist(f)))  /* Ignore old relay data */
      CLEAR(db[newobj].zone);
  if(db_vers == 1 ||
     (Typeof(newobj) == TYPE_ROOM || Typeof(newobj) == TYPE_ZONE))
    db[newobj].zone=getlist(f);  /* Room's zone-list or Zone's inzone-list */

  /* Read in player-specific data structure */
  if(Typeof(newobj) == TYPE_PLAYER) {
    db[newobj].data=malloc(sizeof(struct playerdata));
    db[newobj].data->desc=NULL;
    db[newobj].data->class=fgetc(f)+1;
    db[newobj].data->rows=fgetc(f);
    db[newobj].data->cols=fgetc(f);

    a=fgetc(f);
    db[newobj].data->passtype=a >> 7;
    db[newobj].data->tzdst=a >> 6;
    db[newobj].data->tz=a >> 1;
    db[newobj].data->gender=a;

    if(db_vers > 1 && (db_flags & DB_COMBAT))
      db[newobj].data->term=getnum(f);
    else
      db[newobj].data->term=getshort(f);
    db[newobj].data->steps=getnum(f);
    db[newobj].data->sessions=getnum(f);
    db[newobj].data->age=getnum(f);

    if(db_flags & DB_LONGINT) {
      db[newobj].data->last=getlong(f);
      db[newobj].data->lastoff=getlong(f);
    } else {
      db[newobj].data->last=(unsigned)getnum(f);
      db[newobj].data->lastoff=(unsigned)getnum(f);
    }

    /* Read in password, which can be 10 or 20 characters long */
    fread(db[newobj].data->pass, db[newobj].data->passtype?20:10, 1, f);

    /* Read in powers. If database has a diff number of powers, reinit class */
    if(db_pows != NUM_POWS || db[newobj].data->class >= NUM_CLASSES) {
      fseek(f, (db_pows+3)/4, SEEK_CUR);
      initialize_class(newobj, db[newobj].data->class);
    } else
      fread(db[newobj].data->powers, (NUM_POWS+3)/4, 1, f);
  }

  /* Manage flag counts */
  if(IS(newobj, TYPE_THING, THING_WANDER))
    wander_count++;
  /* Read unused values from old db version */
  if(db_vers == 1) {
    getshort(f);
    fgetc(f);
  }

  /* Skip over combat structures */
  if(db_vers > 1 && (db_flags & DB_COMBAT)) {
    long pos=fgetc(f);

    if(pos != 0xFF) {
      pos=(pos << 24)|(fgetc(f) << 16);
      pos|=getshort(f);
      fseek(f, pos, SEEK_CUR);
    }
  }
  /* Read attributes */ 
  get_attrs(f, newobj);

  /* Load attribute definitions */
  while((a=fgetc(f))) {
    new=malloc(sizeof(ATRDEF));
    new->next=NULL;
    new->atr.obj=newobj;
    new->atr.num=a;
    new->atr.flags=getshort(f);
    getstring(f, new->atr.name);

    if(last)
      last->next=new;
    else
      db[newobj].atrdefs=new;
    last=new;
  }

  /* Misc variables */
  if(Typeof(newobj) == TYPE_PLAYER)
    add_player(newobj);
  if(IS(newobj, TYPE_ZONE, ZONE_UNIVERSAL))
    PUSH_L(uzo, newobj);

  return newobj;
}

// Read in the attribute list for an object.
//
static void get_attrs(FILE *f, dbref thing)
{
  ALIST *new, *last=NULL;
  int num, obj, len;
  char *s;
  
  while((num=fgetc(f))) {
    db_attrs++;
    obj=getref(f);
    s=getstring_noalloc(f);
    len=strlen(s)+1;
    new=malloc(sizeof(ALIST)+len);
    new->next=NULL;
    new->num=num;
    new->obj=obj;
    memcpy(new->text, s, len);

    if(last)
      last->next=new;
    else
      db[thing].attrs=new;
    last=new;
  }
}
