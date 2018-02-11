/* prog/boolexp.c */
/* utilities for parsing the @lock mechanism */

#include "externs.h"

static int boolexp(char *key);
static int boolexp_OPER(char **str);
static int boolexp_REF(char **str);

static dbref parse_player;
static dbref parse_object;

static int lock_recursion;  /* Protection from recursion */

// @Lock Accepts the following keys:
//
// Key Type    Form in @Lock Command
// ----------  ------------------------------
// Normal      <object>
// Is          =<object>
// Carry       +<object>
// Ownership   $<object>
// Indirect    @<object>
//             @<object>/<lock-attribute>
// Attribute   <attribute>:<wildcard-pattern>
//             =<attribute>:<wildcard-pattern>
//             +<attribute>:<wildcard-pattern>
//             <object>/<attribute>:<wildcard-pattern>
// Evaluation  [function()]
// Compound    <key> | <key>
//             <key> & <key>
//             !<key>
//             ( <key> )


/* Grabs the next literal word from the string.
   Returns 1 if the word is a function call. */
static int get_word(char *d, char **s, int type)
{
  char *start=d;
  int func=0, dep=0;

  skip_whitespace(*s);

  while(**s && **s != '&' && **s != '|' && **s != ')' &&
        (!type || (**s != ':' && **s != '/'))) {
    /* Copy text literally between [ ] in functions */
    if(**s == '[') {
      func=1;
      while(**s) {
        if(**s == '[')
          dep++;
        else if(**s == ']')
          if(!--dep)
            break;
        *d++=*(*s)++;
      }
      if(!**s)
        break;
    }
    *d++=*(*s)++;
  }

  while(d > start && *(d-1) == ' ')	/* eat trailing whitespace */
    d--;
  *d='\0';
  return func;
}

/* Process an inputted lock string for placement into an attribute */
char *process_lock(dbref player, dbref object, char *arg)
{
  ATTR *attr;
  static char buff[8192];
  char type, *r=buff, *s=arg;
  int func, dep=0;
  dbref thing;

  while(*s) {
    /* Copy prefix delimiters */
    while(*s && (*s == '!' || *s == '(' || *s == ' ')) {
      if(*s != ' ')
        *r++=*s;
      if(*s++ == '(')
        ++dep;
    }

    if(*s == '$' || *s == '+' || *s == '=' || *s == '@')
      type=*r++=*s++;
    else
      type=0;

    /* Get the next literal */
    func=get_word(r, &s, 1);

    switch(*s) {
    case '/':  /* An indirect attribute */
      if(type && type != '@') {
        notify(player, "Invalid key for '%c'-type lock.", type);
        return NULL;
      }
      if((thing=match_thing(player, r, MATCH_EVERYTHING)) == NOTHING)
        return NULL;
      sprintf(r, "#%d", thing);
      r+=strlen(r);
      *r++=*s++;

      /* Get the attribute portion */
      func=get_word(r, &s, 1);

      if(*s == '/' || (type == '@' && *s == ':')) {
        notify(player, "Invalid key for '%c'-type lock.", type);
        return NULL;
      } if(!func) {
        if(!(attr=atr_str(thing, r))) {
          notify(player, "No such attribute '%s' on %s.", r,
                 unparse_object(player, thing));
          return NULL;
        } if(!can_see_atr(player, thing, attr)) {
          notify(player, "Cannot see attribute '%s' on %s.", attr->name,
                 unparse_object(player, thing));
          return NULL;
        } if(type == '@' && !(attr->flags & AF_LOCK)) {
          notify(player, "Attribute '%s' is not a lock.", attr->name);
          return NULL;
        } if(!can_see_atr(object, thing, attr)) {
          /* Wizard is setting a non-wizard lock */
          notify(player, "Warning: %s cannot see attribute '%s' on %s.",
                 unparse_object(player, object), attr->name,
                 unparse_object(player, thing));
        }
      }
      r+=strlen(r);
      *r++=*s++;
      get_word(r, &s, 0);
      break;

    case ':':  /* Built-in attribute */
      if(type == '$' || type == '@') {
        notify(player, "Invalid key for '%c'-type lock.", type);
        return NULL;
      }
      if(!func && !(attr=builtin_atr_str(r)) && type != '@')
        notify(player, "Warning: No such built-in attribute '%s'.", r);
      r+=strlen(r);
      *r++=*s++;
      get_word(r, &s, 0);
      break;

    default:
      if(func)
        break;
      if((thing=match_thing(player, r, MATCH_EVERYTHING)) == NOTHING)
        return NULL;
      sprintf(r, "#%d", thing);

      /* Check permissions on Indirect locks */
      if(type == '@') {
        if(!(attr=atr_num(NOTHING, A_LOCK)) || !can_see_atr(player, thing,
           attr)) {
          notify(player, "Cannot see default lock on %s.",
                 unparse_object(player, thing));
          return NULL;
        } if(!can_see_atr(object, thing, attr)) {
          /* Wizard is setting a non-wizard lock */
          notify(player, "Warning: %s cannot see default lock on %s.",
                 unparse_object(player, object),
                 unparse_object(player, thing));
        }
      }
    }

    r+=strlen(r);
    while(*s && *s != '&' && *s != '|') {
      if((*s != ')' && *s != ' ') || (*s == ')' && --dep < 0)) {
        notify(player, "I don't understand that key.");
        return NULL;
      }
      if(*s == ')')
        *r++=*s;
      s++;
    }
    if(*s)
      *r++=*s++;
  }

  /* Mismatched ()'s */
  if(dep) {
    notify(player, "I don't understand that key.");
    return NULL;
  }

  *r='\0';
  return buff;
}


/* Process a locked attribute - Entry point */
int eval_boolexp(dbref player, dbref object, char *key)
{
  /* Save old values on the stack to make this function multi-callable */
  dbref pp=parse_player, po=parse_object;
  int old_recursion=lock_recursion;
  int result;

  lock_recursion=0;
  parse_player=player;
  parse_object=object;

  result=boolexp(key);

  lock_recursion=old_recursion;
  parse_player=pp;
  parse_object=po;

  return result;
}

/* Recursive internal expression evaluation */
static int boolexp(char *key)
{
  int len=strlen(key)+1;
  char buf[len], *s=buf;

  if(len == 1)
    return 1;
  if(++lock_recursion == LOCK_LIMIT) {
    log_main("Boolean Expr.: Recursion detected in %s(#%d) lock.",
             db[parse_object].name, parse_object);
    notify(db[parse_object].owner, "Warning: Recursion detected in %s lock.",
           unparse_object(db[parse_object].owner, parse_object));
  }
  if(lock_recursion >= LOCK_LIMIT)
    return 0;

  memcpy(buf, key, len);
  return boolexp_OPER(&s);
}

/* Perform boolean operations */
static int boolexp_OPER(char **buf)
{
  int lhs, true=1;

  /* Unary negation */
  while(**buf == ' ' || **buf == '!') {
    if(**buf == '!')
      true^=1;
    (*buf)++;
  }

  /* Nested parentheses */
  if(**buf == '(') {
    (*buf)++;
    lhs=boolexp_OPER(buf) ? true : !true;
    skip_whitespace(*buf);
    if(**buf == ')')
      (*buf)++;
  } else
    lhs=boolexp_REF(buf) ? true : !true;

  skip_whitespace(*buf);

  if(**buf == '&') {
    (*buf)++;
    return (boolexp_OPER(buf) && lhs);
  }

  if(**buf == '|') {
    (*buf)++;
    return (boolexp_OPER(buf) || lhs);
  }

  return lhs;
}

// Stack-based function to search all carried inventory objects for <thing>.
//
static int is_carrying(dbref player, dbref thing)
{
  INIT_STACK;
  dbref obj=db[player].contents;

  if(obj == NOTHING)
    return 0;

  while(1) {
    /* Test for a match */
    if(obj == thing)
      return 1;

    /* Descend into contents list, guarding against objects in themselves */
    if(obj != player && db[obj].contents != NOTHING) {
      if(db[obj].next != NOTHING)
        PUSH(db[obj].next);
      obj=db[obj].contents;
      continue;
    }

    /* Scan next object in list */
    if((obj=db[obj].next) == NOTHING) {
      /* Get the next object from the stack */
      if(STACKTOP())
        return 0;
      POP(obj);
    }
  }
}

/* Determine reference value of lock */
static int boolexp_REF(char **buf)
{
  ATTR *attr;
  char type=0, str[8192], key[8192], *s=str, *t;
  dbref thing, player;

  /* Retrieve entire key, then parse it into 'str' */
  get_word(key, buf, 0);
  pronoun_substitute(str, parse_object, parse_player, key);

  /* Numerical function result */
  if(isdigit(*str) || *str == '-')
    return atoi(str);

  /* Check Lock Type */

  /* Indirect-Type Lock '@' */
  if(*str == '@') {
    if((s=strchr(str, '/')))
      *s++='\0';

    /* get dbref number */
    for(t=str+1;*t == ' ';t++);
    if((thing=match_thing(parse_object, t, MATCH_EVERYTHING|MATCH_QUIET)) ==
       NOTHING)
      return 0;

    /* Attribute specified? */
    if(s)
      attr=atr_str(thing, s);
    else
      attr=atr_num(NOTHING, A_LOCK);

    if(!attr || !can_see_atr(parse_object, thing, attr) ||
       !(attr->flags & AF_LOCK))
      return 0;
    return boolexp(atr_get_obj(thing, attr->obj, attr->num));
  }

  /* Owner-Type Lock '$' */
  if(*str == '$') {
    for(s=str+1;*s == ' ';s++);
    thing=match_thing(parse_object, s, MATCH_EVERYTHING|MATCH_QUIET);
    return (thing != NOTHING && db[thing].owner == db[parse_player].owner);
  }

  /* Object/Attribute:Pattern Match */
  if((s=strchr(str, '/'))) {
    *s++='\0';

    if((thing=match_thing(parse_object, str, MATCH_EVERYTHING|MATCH_QUIET)) ==
       NOTHING)
    return 0;

    if((t=strchr(s, ':')))
      *t++='\0';
    else
      t="?*";  /* match a non-empty attribute */

    if(!(attr=atr_str(thing, s)) || !can_see_atr(parse_object, thing, attr))
      return 0;
    return wild_match(atr_get_obj(thing,attr->obj,attr->num), t, WILD_BOOLEAN);
  }

  /* Determine Lock Type: Const, Carry '+', Is '=' */

  s=str;
  if(*s == '+' || *s == '=') {
    type=*s++;
    skip_whitespace(s);
  }

  if((t=strchr(s, ':'))) {	/* Key = Attribute:Pattern */
    *t++='\0';
    thing=NOTHING;
  } else {			/* Key = Object */
    if((thing=match_thing(parse_object, s, MATCH_EVERYTHING|MATCH_QUIET)) ==
       NOTHING)
    return 0;
  }

  /* Check Player: Const-Type & Is-Type '=' */
  if(type != '+') {
    if(t) {
      if((attr=atr_str(parse_player, s)) &&
         can_see_atr(parse_object, parse_player, attr) &&
         wild_match(atr_get_obj(parse_player, attr->obj, attr->num), t,
                    WILD_BOOLEAN))
        return 1;
    } else if(parse_player == thing)
      return 1;
  }

  /* Check Contents: Const-Type & Carry-Type '+' */
  if(type != '=') {
    if(t) {
      DOLIST(player, db[parse_player].contents)
        if((attr=atr_str(player,s)) && can_see_atr(parse_object,player,attr) &&
           wild_match(atr_get_obj(player,attr->obj,attr->num),t,WILD_BOOLEAN))
          return 1;
    } else if((Typeof(thing) == TYPE_THING || Typeof(thing) == TYPE_PLAYER) &&
              is_carrying(parse_player, thing))
      return 1;
  }

  return 0;
}
