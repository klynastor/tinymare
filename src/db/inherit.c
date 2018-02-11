/* db/inherit.c */
/* Commands dealing with Object Lists on Parents/Children/Relays/Zones */

#include "externs.h"

const char *attr_options[16]={
  "osee", "dark", "wizard", "unsaved", "hidden", "date", "inherit", "lock",
  "function", "haven", "bitmap", "dbref", "combat", "privs", "", ""};


// Non-recursively checks children of <thing> to see if <attr> value was
// overridden. Returns the first object number with that attribute set.
//
static dbref check_attr_override(dbref thing, ATTR *attr)
{
  INIT_STACK;
  ALIST *ptr;
  dbref *p;

  while(1) {
    /* Add object's children to the stack */
    if(db[thing].children)
      PUSH(db[thing].children);

    /* Get the next object from the stack */
    if(STACKTOP())
      break;
    POP(p);
    thing=*p++;
    if(*p != NOTHING)
      PUSH(p);

    /* Scan attribute list for matches */
    for(ptr=db[thing].attrs;ptr;ptr=ptr->next)
      if(ptr->num == attr->num && ptr->obj == attr->obj)
        return thing;
  }

  return NOTHING;
}

/* Define and set options on a new object attribute */
void do_defattr(dbref player, char *arg1, char *arg2)
{
  ATRDEF *k, *new, *last=NULL;
  ATTR *attr;
  int i, flags=0, wildcard=0;
  dbref thing, obj;
  char *name, *r;

  if(!(name=strchr(arg1, '/'))) {
    notify(player, "You must specify an attribute name.");
    return;
  }
  *name++='\0';
  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* Setting flags on an already existing attribute */
  if((attr=atr_str(thing, name))) {
    if(Invalid(attr->obj)) {
      notify(player,
             "Sorry, there is already a built-in attribute with that name.");
      return;
    } if(attr->obj != thing) {
      notify(player,
             "Sorry, that attribute name is already defined on the parent %s.",
             unparse_object(player, attr->obj));
      return;
    } if((attr->flags & AF_HIDDEN) && player != GOD) {
      notify(player, "Permission denied.");
      return;
    }
  } else if(strchr(name, '*') || strchr(name, '?'))
    wildcard=1;
  else if(!ok_attribute_name(name)) {
    notify(player, "That's not a good name for an attribute.");
    return;
  }

  /* Read attribute options list */
  while((r=parse_up(&arg2, ' '))) {
    for(i=0;i<16;i++)
      if(string_prefix(attr_options[i], r) && (i != 4 || player == GOD)) {
        flags |= 1 << i;
        break;
      }
    if(i == 16)
      notify(player, "Unknown attribute option: %s", r);
  }
  
  if(attr) {
    if((flags & AF_PRIVS) && (obj=check_attr_override(thing, attr)) != NOTHING)
      notify(player, "Cannot set attribute option 'privs' because at least "
             "one child, %s, has this value overridden.",
             unparse_object(player, obj));
    else {
      attr->flags=flags;
      notify(player, "Options set.");
    }
    return;
  } else if(wildcard) {  /* Set flags on all matching attribute names */
    for(i=0,k=db[thing].atrdefs;k;k=k->next)
      if((!(k->atr.flags & AF_HIDDEN) || player == GOD) &&
         wild_match(k->atr.name, name, 0)) {
        if((flags & AF_PRIVS) &&
           (obj=check_attr_override(thing, &k->atr)) != NOTHING)
          notify(player, "Cannot set attribute option 'privs' on #%d/%s.",
                 thing, k->atr.name);
        else {
          k->atr.flags=flags;
          i++;
        }
      }

    if(!i)
      notify(player, "No attributes match.");
    else
      notify(player, "Options set on %d attribute%s.", i, i==1?"":"s");
    return;
  }

  i=1;
  for(k=db[thing].atrdefs;k;k=k->next) {
    if(k->atr.num > i)
      break;
    last=k;
    i++;
  }
  if(i > 255) {
    notify(player,
           "Sorry, you can't have that many defined attributes on an object.");
    return;
  }

  new=malloc(sizeof(ATRDEF));
  new->next=k;
  new->atr.obj=thing;
  new->atr.num=i;
  new->atr.flags=flags;
  SPTR(new->atr.name, name);

  if(last)
    last->next=new;
  else
    db[thing].atrdefs=new;
  notify(player, "Attribute defined%s.", flags?" and options set":"");
}

/* Undefine a non-builtin attribute */
void do_undefattr(dbref player, char *arg1)
{
  ATTR *attr;
  ATRDEF *k, *last=NULL;
  dbref thing;
  char *name;
  
  if(!(name=strchr(arg1, '/'))) {
    notify(player, "You must specify an attribute name.");
    return;
  }
  *name++='\0';
  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  if(!(attr=atr_str(thing, name))) {
    notify(player, "No match.");
    return;
  } if(attr->obj == -1) {
    notify(player, "Only non-builtin attributes may be undefined.");
    return;
  } if(attr->obj != thing) {
    notify(player, "Sorry, that attribute is defined on the parent %s.",
           unparse_object(player, attr->obj));
    return;
  } if((attr->flags & AF_HIDDEN) && player != GOD) {
    notify(player, "Permission denied.");
    return;
  }

  for(k=db[thing].atrdefs;k;k=k->next) {
    if(&k->atr == attr) {
      atr_clean(thing, thing, k->atr.num);
      if(last)
        last->next=k->next;
      else
        db[thing].atrdefs=k->next;
      free(k->atr.name);
      free(k);
      notify(player, "Deleted.");
      return;
    }
    last=k;
  }
}

/* Rename a non-builtin attribute */
void do_redefattr(dbref player, char *arg1, char *arg2)
{
  ATTR *attr;
  char *name;
  dbref thing;
  
  if(!(name=strchr(arg1, '/'))) {
    notify(player, "You must specify an attribute name.");
    return;
  }
  *name++='\0';
  if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, thing, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  if(!(attr=atr_str(thing, name))) {
    notify(player, "No match.");
    return;
  } if(attr->obj == -1) {
    notify(player, "Only non-builtin attributes may be renamed.");
    return;
  } if(attr->obj != thing) {
    notify(player, "Sorry, that attribute is defined on the parent %s.",
           unparse_object(player, attr->obj));
    return;
  } if((attr->flags & AF_HIDDEN) && player != GOD) {
    notify(player, "Permission denied.");
    return;
  } if(!ok_attribute_name(arg2)) {
    notify(player, "That's not a good name for an attribute.");
    return;
  }

  /* Attribute can be easily renamed if letters have a capitalization change */
  if(!strcasecmp(attr->name, arg2)) {
    strcpy(attr->name, arg2);
    notify(player, "Renamed.");
    return;
  }

  /* Check if new attribute already exists */
  if(atr_str(thing, arg2)) {
    notify(player, "Sorry, that attribute name is already in use.");
    return;
  }

  WPTR(attr->name, arg2);
  notify(player, "Renamed.");
}

/* Display options of a defined attribute */
char *unparse_attrflags(int flags)
{
  static char buf[128];
  int a;

  *buf=0;
  for(a=0;a<16;a++)
    if(flags & (1 << a)) {
      if(*buf)
        strcat(buf, " ");
      strcat(buf, attr_options[a]);
    }

  return buf;
}


/* Determine if <parent> is an ancestor of <thing> */
int is_a(dbref thing, dbref parent)
{
  INIT_STACK;
  dbref *p;

  while(1) {
    if(thing == parent)
      return 1;

    /* Add parents to the stack */
    if(db[thing].parents)
      PUSH(db[thing].parents);

    /* Get the next object from the stack */
    if(STACKTOP())
      break;
    POP(p);
    thing=*p++;
    if(*p != NOTHING)
      PUSH(p);
  }

  return 0;
}

void do_addparent(dbref player, char *arg1, char *arg2)
{
  dbref thing, parent;

  if((thing=match_controlled(player, arg1, POW_MODIFY)) == NOTHING)
    return;
  if((parent=match_thing(player, arg2, MATCH_EVERYTHING)) == NOTHING)
    return;

  if(is_a(thing, parent)) {
    notify(player, "But it is already a parent of %s.",
           unparse_object(player, thing));
    return;
  } if(is_a(parent, thing)) {
    notify(player, "But it is a descendant of %s.",
           unparse_object(player, thing));
    return;
  } if((!(db[parent].flags&BEARING) && !controls(player,parent,POW_MODIFY)) ||
       ((db[parent].flags&BEARING) && !could_doit(player,parent,A_LPARENT))) {
    notify(player, "Sorry, you can't @addparent to that.");
    return;
  }
  
  PUSH_L(db[thing].parents, parent);
  PUSH_L(db[parent].children, thing);

  if(!Quiet(player))
    notify(player, "%s is now a parent of %s.", unparse_object(player, parent),
           unparse_object(player, thing));
}

void do_delparent(dbref player, char *arg1, char *arg2)
{
  dbref thing, parent;

  if((thing=match_controlled(player, arg1, POW_MODIFY)) == NOTHING)
    return;
  if((parent=match_thing(player, arg2, MATCH_EVERYTHING)) == NOTHING)
    return;
      
  if(!inlist(db[thing].parents, parent)) {
    notify(player, "Sorry, it doesn't have that as its parent.");
    return;
  }
  if(!(db[parent].flags & BEARING) && !controls(player, parent, POW_MODIFY)) {
    notify(player, "Sorry, you can't unparent from that.");
    return;
  }

  PULL(db[thing].parents, parent);
  PULL(db[parent].children, thing);
  atr_clean(thing, parent, -1);

  if(!Quiet(player))
    notify(player, "%s is no longer a parent of %s.",
           unparse_object(player, parent),
           unparse_object(player, thing));
}

/* Adds a zone to a room */
void do_addzone(dbref player, char *arg1, char *arg2)
{
  dbref thing, thing2;

  if((thing=match_controlled(player, arg1, POW_MODIFY)) == NOTHING)
    return;
  if((thing2=match_controlled(player, arg2, POW_MODIFY)) == NOTHING)
    return;
  if(Typeof(thing) != TYPE_ROOM) {
    notify(player, "Only rooms may be assigned to zones.");
    return;
  } if(Typeof(thing2) != TYPE_ZONE) {
    notify(player, "That is not a zone object.");
    return;
  }

  if(inlist(db[thing].zone, thing2)) {
    notify(player, "It already has that zone linked.");
    return;
  }

  PUSH_L(db[thing].zone, thing2);
  PUSH_L(db[thing2].zone, thing);
  if(!Quiet(player))
    notify(player, "Zone added.");
}

/* Remove zone from room */
void do_delzone(dbref player, char *arg1, char *arg2)
{
  dbref thing, thing2;

  if((thing=match_controlled(player, arg1, POW_MODIFY)) == NOTHING)
    return;
  if((thing2=match_controlled(player, arg2, POW_MODIFY)) == NOTHING)
    return;

  if(Typeof(thing) != TYPE_ROOM) {
    notify(player, "You can only remove zones from rooms.");
    return;
  } if(!inlist(db[thing].zone, thing2)) {
    notify(player, "That zone is not linked to that room.");
    return;
  }

  PULL(db[thing].zone, thing2);
  PULL(db[thing2].zone, thing);
  if(!Quiet(player))
    notify(player, "Zone removed.");
}


/* Object List Manipulation */

int db_lists=0;  /* Keeps a record of the number of lists defined */


/* Adds <thing> to front of <list> */
void push_first(dbref **list, dbref thing)
{
  int a;

  if(thing == NOTHING)
    return;

  /* Create new list */
  if(!*list) {
    *list=malloc(2*sizeof(dbref));
    (*list)[0]=thing;
    (*list)[1]=NOTHING;
    db_lists++;
    return;
  }

  /* Otherwise increment list size by 1 integer */
  for(a=0;(*list)[a] != NOTHING;a++);
  *list=realloc(*list, (a+2)*sizeof(dbref));

  /* Move array values up one */
  memmove(*list+1, *list, (a+1)*sizeof(dbref));

  **list=thing;  /* Insert item */
}

/* Adds <thing> to end of <list> */
void push_last(dbref **list, dbref thing)
{
  int a;

  if(thing == NOTHING)
    return;

  /* Create new list */
  if(!*list) {
    *list=malloc(2*sizeof(dbref));
    (*list)[0]=thing;
    (*list)[1]=NOTHING;
    db_lists++;
    return;
  }

  /* Otherwise increment list size by 1 integer and add new object to end */
  for(a=0;(*list)[a] != NOTHING;a++);

  *list=realloc(*list, (a+2)*sizeof(dbref));
  (*list)[a]=thing;
  (*list)[a+1]=NOTHING;
}

/* Removes <thing> from <list> */
void pull_list(dbref **list, dbref thing)
{
  dbref *ptr=*list;
  int a;

  if(thing == NOTHING || !ptr)
    return;

  for(a=0;ptr;ptr++) {
    if(*ptr == thing)
      continue;
    (*list)[a++]=*ptr;
    if(*ptr == NOTHING)
      break;
  }

  /* An empty list gets freed */
  if(a < 2) {
    free(*list);
    *list=NULL;
    db_lists--;
    return;
  }

  /* Free unused portion of list */
  *list=realloc(*list, a*sizeof(dbref));
}

/* Makes a copy of <list> in memory */
dbref *copy_list(dbref *list)
{
  dbref *ptr;
  int size;
  
  if(!list)
    return NULL;

  size=(list_size(list)+1)*sizeof(dbref);
  ptr=malloc(size);
  memcpy(ptr, list, size);
  db_lists++;

  return ptr;
}
