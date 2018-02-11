/* mare/config.c */
/* TinyMARE Configuration */

#include "externs.h"

#define CONF(a,b,c,d,e,f) b c=d;
#include "config.h"
#undef CONF

/* configuration flags */
#define INT	1	// Integer format
#define DBREF	2	// #Dbref format
#define LONG	3	// Long format
#define CHAR	4	// String format
#define WIZ	0x10	// Wizard-viewable only
#define LBRK	0x20	// Line break on @list display
#define SAVE	0x40	// Save value regardless of default
#define ROOM	0x80	// #Dbref is room-only

static struct {
  char *name;
  int num;
  union { long *u64; int *u32; char **string; } var;
  union { long  u64; int  u32; char  *string; } def;
  int type;
  int fix;
} config_list[]={
#define CONF(a,b,c,d,e,f) {e, a, {(void *)&c}, {(long)d}, f, 0},
#include "config.h"
#undef CONF
};


void list_config(dbref player, char *arg1)
{
  int a, plvl=power(player, POW_SECURITY);
  char buf[1100];

  notify(player, "Environment configuration variables:");
  notify(player, "%s", "");
  notify(player, "Variable Name\304\304\304\304\304\304\304  "
                 "Default Value\304\304 Current Value\304\304");

  /* list the configuration variables */
  for(a=0;a < NELEM(config_list);a++) {
    if(*arg1 && strcasecmp(config_list[a].name, arg1))
      continue;
    if(((config_list[a].type & WIZ) || config_list[a].num >= 100) && !plvl)
      continue;
    if((config_list[a].type & 0xF) == CHAR)
      notify(player, "%20s: %s", config_list[a].name,
             *config_list[a].var.string);
    else {
      if(config_list[a].num < 80) {
        switch(config_list[a].type & 0xF) {
        case INT:
          sprintf(buf, "%-16d", config_list[a].def.u32);
          break;
        case DBREF:
          sprintf(buf, "#%-15d", config_list[a].def.u32);
          break;
        case LONG:
          if(config_list[a].def.u64)
            sprintf(buf, "%-16.16s", mktm(player, config_list[a].def.u64));
          else
            strcpy(buf, "(No date set)");
        }
      } else
        strcpy(buf, "");

      switch(config_list[a].type & 0xF) {
      case INT:
        notify(player, "%20s: %s%d", config_list[a].name, buf,
               *config_list[a].var.u32);
        break;
      case DBREF:
        notify(player, "%20s: %s%s", config_list[a].name,
               buf, unparse_object(player, *config_list[a].var.u32));
        break;
      case LONG:
        notify(player, "%20s: %s%s", config_list[a].name,
               buf, mktm(player, *config_list[a].var.u64));
      }
    }
    if(*arg1)
      return;
    if(config_list[a].type & LBRK)
      notify(player, "%s", "");
  }

  if(*arg1)
    notify(player, "Nothing found.");
}

void do_config(dbref player, char *name, char *value)
{
  dbref thing;
  int a;

  /* Scan configuration variables for proper setting */
   for(a=0;a < NELEM(config_list);a++)
     if(!strcasecmp(config_list[a].name, name))
       break;
   if(!config_list[a].name) {
    notify(player, "No such configuration variable found.");
    return;
  }

  /* Set configuration */

  switch(config_list[a].type & 0xF) {
  case INT:
    *config_list[a].var.u32 = atoi(value);
    break;
  case DBREF:
    if((thing=match_thing(player, value, MATCH_EVERYTHING)) == NOTHING)
      return;
    if((config_list[a].type & ROOM) && Typeof(thing) != TYPE_ROOM) {
      notify(player, "%s is not a room.", unparse_object(player, thing));
      return;
    }
    *config_list[a].var.u32 = thing;
    break;
  case LONG:
    *config_list[a].var.u64 = atol(value);
    break;
  case CHAR:
    /* allocate space for string */
    if(config_list[a].fix)
      free(*config_list[a].var.string);
    else
      config_list[a].fix=1;
    SPTR(*config_list[a].var.string, value);
  }

  if(!Quiet(player))
    notify(player, "Set.");
}

void get_config(dbref player, char *buff, char *name)
{
  int a;

  /* scan configuration variables for proper variable */
  *buff='\0';
  for(a=0;a < NELEM(config_list);a++)
    if(!strcasecmp(config_list[a].name, name))
      break;
  if(!config_list[a].name) {
    strcpy(buff, "#-1 No such config variable.");
    return;
  }

  if(((config_list[a].type & WIZ) || config_list[a].num >= 100) &&
     !power(player, POW_SECURITY)) {
    strcpy(buff, "#-1 Permission denied.");
    return;
  }

  switch(config_list[a].type & 0xF) {
    case INT:   sprintf(buff, "%d", *config_list[a].var.u32); return;
    case DBREF: sprintf(buff, "#%d", *config_list[a].var.u32); return;
    case LONG:  sprintf(buff, "%ld", *config_list[a].var.u64); return;
    case CHAR:  strcpy(buff, *config_list[a].var.string);
  }
}

int load_config(FILE *f)
{
  int a, type, num, count=0;

  while((type=fgetc(f))) {
    num=fgetc(f);

    /* scan config list */
    for(a=0;a < NELEM(config_list);a++)
      if(config_list[a].num == num)
        break;

    /* types don't match. read and disregard variable */
    if(!config_list[a].name || (type & 0xF) != (config_list[a].type & 0xF)) {
      switch(type & 0xF) {
        case INT: case DBREF: getnum(f); break;
        case LONG: getlong(f); break;
        case CHAR: getstring_noalloc(f);
      }
      continue;
    }

    /* store valid variable in configuration */
    switch(type & 0xF) {
      case INT: case DBREF:
        *config_list[a].var.u32=getnum(f); break;
      case LONG:
        *config_list[a].var.u64=getlong(f); break;
      case CHAR:
        /* allocate space for string */
        if(config_list[a].fix)
          free(*config_list[a].var.string);
        else
          config_list[a].fix=1;
        SPTR(*config_list[a].var.string, getstring_noalloc(f));
    }
    ++count;
  }

  return count;
}

void save_config(FILE *f)
{
  int a, type, count=0;

  for(a=0;a < NELEM(config_list);a++) {
    type=config_list[a].type & 0xF;

    /* determine which configuration variables to save */
    if(!(config_list[a].type & SAVE) && (!type || type > CHAR ||
       (type <= DBREF && config_list[a].def.u32 == *config_list[a].var.u32) ||
       (type == LONG && config_list[a].def.u64 == *config_list[a].var.u64) ||
       (type == CHAR && !strcasecmp(config_list[a].def.string,
        *config_list[a].var.string))))
      continue;

    /* save configuration to database file */
    count=1;
    putchr(f, type);
    putchr(f, config_list[a].num);
    switch(type) {
    case INT: case DBREF:
      putnum(f, *config_list[a].var.u32); break;
    case LONG:
      putlong(f, *config_list[a].var.u64); break;
    case CHAR:
      putstring(f, *config_list[a].var.string);
    }
  }

  if(count)
    putchr(f, 0);
}
