/* db/attrib.c */
/* Database attribute maintainence procedures */

#include "externs.h"

static ATTR attr[]={
#define DOATTR(num, var, name, flags) {name, NOTHING, num, flags},
#define VREGS
#include "attrib.h"
#undef VREGS
#undef DOATTR
};

int db_attrs;  /* Keeps count of the number of attributes stored */


/* lists all attributes to player */
void list_attributes(dbref player, char *name)
{
  int a, b;

  if(*name) {
    b=atoi(name);
    for(a=0;a < NELEM(attr);a++)
      if(attr[a].num == b || !strcasecmp(attr[a].name, name)) {
        notify(player, "%4d: %-15.15s %s", attr[a].num,
               attr[a].name, unparse_attrflags(attr[a].flags));
        return;
      }
    notify(player, "No such built-in attribute '%s'.", name);
    return;
  }

  notify(player, "Numerical list of attributes and their hardcoded options:");
  notify(player, "%s", "");
  for(a=0;a < NELEM(attr);a++)
    notify(player, "%4d: %-15.15s %s", attr[a].num,
           attr[a].name, unparse_attrflags(attr[a].flags));
}


/* pointer-returning functions */

/* returns the pointer to a builtin attribute given its name */
ATTR *builtin_atr_str(char *str)
{
  static hash *attrhash;

  if(!attrhash)
    attrhash=make_hashtab(attr);
  return lookup_hash(attrhash, str);
}

/* look up an attr name from the list of defattrs on <obj> */
ATTR *def_atr_str(dbref obj, char *str)
{
  ATRDEF *k;
  
  for(k=db[obj].atrdefs;k;k=k->next)
    if(!strcasecmp(k->atr.name, str))
      return &k->atr;
  return NULL;
}

/* given an attribute name <str>, looks up an attr set on <obj> */
ATTR *atr_str(dbref obj, char *str)
{
  INIT_STACK;
  dbref *p;
  ATRDEF *k;

  /* Search for defined attribute on <obj> or <obj>'s parents (recursively) */
  while(1) {
    for(k=db[obj].atrdefs;k;k=k->next)
      if(!strcasecmp(k->atr.name, str))
        return &k->atr;

    /* Add parents to the stack */
    if(db[obj].parents)
      PUSH(db[obj].parents);

    /* Get the next object from the stack */
    if(STACKTOP())
      break;
    POP(p);
    obj=*p++;
    if(*p != NOTHING)
      PUSH(p);
  }

  /* If nothing was found, scan the builtin attribute list */
  return builtin_atr_str(str);
}

/* get the attribute definition for #obj.num */
ATTR *atr_num(dbref obj, unsigned char num)
{
  static ATTR *attrs[256];
  static int init=0;
  ATRDEF *k;
  int a;

  /* Built-in Attribute */
  if(obj == NOTHING) {
    /* allocate index on first run */
    if(!init) {
      for(a=0;a < NELEM(attr);a++)
        attrs[attr[a].num]=&attr[a];
      init=1;
    }
    return attrs[num];
  }

  /* User-defined Attribute */
  for(k=db[obj].atrdefs;k;k=k->next)
    if(k->atr.num == num)
      return &k->atr;
  return NULL;
}


/* routines to handle object attribute lists */

/* adds defined attr <num> on <obj> to <thing> */
void atr_add_obj(dbref thing, dbref obj, unsigned char num, char *str)
{
  ALIST *ptr, *last=NULL;
  int len;

  db[thing].mod_time=now;
  str=compress(str);
  len=strlen(str)+1;

  for(ptr=db[thing].attrs;ptr;ptr=ptr->next) {
    if(ptr->obj == obj && ptr->num == num) {
      /* erase attribute? */
      if(!*str) {
        if(last)
          last->next=ptr->next;
        else
          db[thing].attrs=ptr->next;
        free(ptr);
        db_attrs--;
        return;
      }

      /* otherwise set new value on attribute */
      ptr=realloc(ptr, sizeof(ALIST)+len);
      if(last)
        last->next=ptr;
      else
        db[thing].attrs=ptr;

      memcpy(ptr->text, str, len);
      return;
    }
    last=ptr;
  }

  if(!*str)  /* nothing to clear? */
    return;

  /* add attribute */
  db_attrs++;
  ptr=malloc(sizeof(ALIST)+len);
  ptr->next=NULL;
  ptr->obj=obj;
  ptr->num=num;
  memcpy(ptr->text, str, len);
  if(last)
    last->next=ptr;
  else
    db[thing].attrs=ptr;
}

/* gets defined attr <num> on <obj> from <thing> */
char *atr_get_obj(dbref thing, dbref obj, unsigned char num)
{
  INIT_STACK;
  ALIST *ptr;
  ATTR *a;
  dbref *p;

  while(1) {
    /* Scan attribute list for matches */
    for(ptr=db[thing].attrs;ptr;ptr=ptr->next)
      if(ptr->num == num && ptr->obj == obj)
        return uncompress(ptr->text);

    /* Check (only once) that the attribute is Inheritable */
    if(STACKTOP() && (!(a=atr_num(obj, num)) || !(a->flags & INH)))
      break;

    /* Add parents to the stack */
    if(thing != obj && db[thing].parents)
      PUSH(db[thing].parents);

    /* Get the next object from the stack */
    if(STACKTOP())
      break;
    POP(p);
    thing=*p++;
    if(*p != NOTHING)
      PUSH(p);
  }

  return "";
}

// When destroying <obj> or delparenting <thing> from <obj>, remove all
// attributes that have references to <obj> on its children (nonrecursively).
//
// If <num> is not -1, then only remove that specific attribute. Used during an
// @undefattr from a parent.
//
void atr_clean(dbref thing, dbref obj, int num)
{
  INIT_STACK;
  ALIST *ptr, *last, *next;
  dbref *p;

  while(1) {
    /* Wipe all attributes on <thing> belonging to <obj> */
    for(last=NULL,ptr=db[thing].attrs;ptr;ptr=next) {
      next=ptr->next;
      if(ptr->obj == obj && (num == -1 || ptr->num == num)) {
        if(last)
          last->next=next;
        else
          db[thing].attrs=next;
        free(ptr);
        db_attrs--;

        if(num != -1)
          break;
      } else
        last=ptr;
    }

    /* Add children to the stack */
    if(db[thing].children)
      PUSH(db[thing].children);

    /* Get the next object from the stack */
    if(STACKTOP())
      break;
    POP(p);
    thing=*p++;
    if(*p != NOTHING)
      PUSH(p);
  }
}

/* copy attributes from source to dest;
   should only be used on a newly created object */
void atr_cpy(dbref dest, dbref source)
{                 
  ALIST *ptr, *new, *last=NULL;
  ATRDEF *k, *newk, *lastk=NULL;
  int len;
  
  /* copy attribute definitions */
  for(k=db[source].atrdefs;k;k=k->next) {
    newk=malloc(sizeof(ATRDEF));
    newk->next=NULL;
    newk->atr.obj=dest;
    newk->atr.num=k->atr.num;
    newk->atr.flags=k->atr.flags;
    SPTR(newk->atr.name, k->atr.name);
    if(lastk)
      lastk->next=newk;
    else
      db[dest].atrdefs=newk;
    lastk=newk;
  }

  /* copy attribute data */
  for(ptr=db[source].attrs;ptr;ptr=ptr->next) {
    if(ptr->obj != NOTHING && source != ptr->obj && !is_a(dest, ptr->obj))
      continue;
    db_attrs++;
    len=strlen(ptr->text)+1;
    new=malloc(sizeof(ALIST)+len);
    new->next=NULL;
    new->obj=(ptr->obj == source)?dest:ptr->obj;
    new->num=ptr->num;
    memcpy(new->text, ptr->text, len);
    if(last)
      last->next=new;
    else
      db[dest].attrs=new;
    last=new;
  }
}

// Returns a allocated list of all attributes (local or inherited) that
// are defined on an object. Results MUST be freed when done using them.
//
struct all_atr_list *all_attributes(dbref thing)
{
  INIT_STACK;
  struct all_atr_list *list=NULL;
  int i, count=0, dep=0;
  ATTR *atr;
  ALIST *a;
  dbref *ptr;

  while(1) {
    for(a=db[thing].attrs;a;a=a->next) {
      if(!(atr=atr_num(a->obj, a->num)) || (dep && !(atr->flags & INH)))
        continue;

      /* Check to see if there's already something with this type */
      for(i=0;i < count && list[i].type != atr;i++);
      if(i < count)
        continue;

      /* Increase the number of attributes returned by 32 (+1 for EOL) */
      if(!(count++ & 31))
        list=realloc(list, (count+32)*sizeof(struct all_atr_list));
      list[i].type=atr;
      list[i].value=a->text;
      list[i].numinherit=dep;
    }

    /* Add parents to the stack */
    if(db[thing].parents) {
      PUSH(db[thing].parents);
      PUSH(dep+1);
    }

    /* Get the next object from the stack */
    if(STACKTOP())
      break;

    POP(dep);
    POP(ptr);
    thing=*ptr++;
    if(*ptr != NOTHING) {
      PUSH(ptr);
      PUSH(dep);
    }
  }

  /* Mark end of list by setting all fields to NULL */
  if(list)
    memset(&list[count], 0, sizeof(struct all_atr_list));

  return list;
}
