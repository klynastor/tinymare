/* prog/eval.c - Function Parser */

#include "externs.h"
#include <math.h>

int func_recur;  // Current depth level in nested functions.
int func_total;  // Number of total functions processed in command.

#define FUNCTION(x) static void fun_##x(char *buff, char **args, \
                                        dbref privs, dbref doer, int nargs)

#define INIT_MATCH(x,y)    dbref x=match_thing(privs, y, MATCH_EVERYTHING)
#define INIT_CMATCH(x,y,z) dbref x=match_controlled(privs, y, z)

#define ISTRUE(x) (*x && (*x != '#' || x[1] != '-') && (*x != '0' || atoi(x)))

#define ADDWORD(buff, s, format, args...) \
 ({  if(s != buff) { \
       if(s-buff > 7980) { strcpy(s, " ..."); return; } \
       *s++=' '; \
     } s += sprintf(s, format, ## args);  })

#define DELIMCH(x,y,ch)  char x=((nargs > y && (*args[y] == '\n' || \
                           (*args[y] > 32 && *args[y] < 127))) ? *args[y] : ch)
#define DELIM(x,y) DELIMCH(x,y,' ')

#define ERR_PERM	"#-1 Permission denied."
#define ERR_PLAYER	"#-1 No such player."
#define ERR_RANGE	"#-1 Out of range."
#define ERR_LOC		"#-1 Too far away to see."
#define ERR_CONN	"#-1 Not connected."
#define ERR_DATE	"#-1 Invalid date."
#define ERR_ENV		"#-1 Invalid global register."

/* Set 1: Basic Math *********************************************************/

FUNCTION(add) {
  int a, result=0;
  char *r, *s;

  for(a=0;a < nargs;a++) {
    s=args[a];
    while((r=strspc(&s)))
      result+=atoi(r);
  }
  sprintf(buff, "%d", result);
}

FUNCTION(fadd) {
  int a;
  double result=0;
  char *r, *s;

  for(a=0;a < nargs;a++) {
    s=args[a];
    while((r=strspc(&s)))
      result+=atof(r);
  }
  print_float(buff, result);
}

FUNCTION(mul) {
  int a, result=1;
  char *r, *s;

  for(a=0;a < nargs;a++) {
    s=args[a];
    while((r=strspc(&s)))
      if(*r)
        result*=atoi(r);
  }
  sprintf(buff, "%d", result);
}

FUNCTION(fmul) {
  int a;
  double result=1;
  char *r, *s;

  for(a=0;a < nargs;a++) {
    s=args[a];
    while((r=strspc(&s)))
      if(*r)
        result*=atof(r);
  }
  print_float(buff, result);
}

FUNCTION(sub) { sprintf(buff, "%d", atoi(args[0])-atoi(args[1])); }
FUNCTION(fsub) { print_float(buff, atof(args[0])-atof(args[1])); }
FUNCTION(div) { if(!atoi(args[1])) strcpy(buff, "inf");
                  else sprintf(buff, "%d", atoi(args[0])/atoi(args[1])); }
FUNCTION(fdiv) { if(!atof(args[1])) strcpy(buff, "inf");
                   else print_float(buff, atof(args[0])/atof(args[1])); }
FUNCTION(mod) { if(!atoi(args[1])) strcpy(buff, "0");
                  else sprintf(buff, "%d", atoi(args[0])%atoi(args[1])); }

FUNCTION(rdiv) {
  int result, x=atoi(args[0]), y=atoi(args[1]), neg=(x < 0)^(y < 0);

  if(!y)
    result=0;
  else {
    result=x/y;
    if(x < 0)
      x=-x;
    if(y < 0)
      y=-y;
    result+=(rand_num(y) < x % y)?(neg?-1:1):0;
  }   
  sprintf(buff, "%d", result);
}

FUNCTION(min) {
  int a, count=0;
  double result=0;
  char *r, *s;

  for(a=0;a < nargs;a++) {
    s=args[a];
    while((r=strspc(&s)))
      if(*r && (!count++ || atof(r) < result))
        result=atof(r);
  }
  print_float(buff, result);
}

FUNCTION(max) {
  int a, count=0;
  double result=0;
  char *r, *s;

  for(a=0;a < nargs;a++) {
    s=args[a];
    while((r=strspc(&s)))
      if(*r && (!count++ || atof(r) > result))
        result=atof(r);
  }
  print_float(buff, result);
}

FUNCTION(avg) {
  int a, count=0;
  double result=0;
  char *r, *s;

  for(a=0;a < nargs;a++) {
    s=args[a];
    while((r=strspc(&s)))
      if(*r) {
        result+=atof(r);
        count++;
      }
  }

  print_float(buff, count?(result/count):0);
}

FUNCTION(stddev) {
  int a, count=0;
  double avg=0, result=0;
  char *r, *s;

  /* Count and average without munging up the original args */
  for(a=0;a < nargs;a++) {
    s=args[a];
    while(1) {
      while(*s == ' ')
        s++;
      if(!*s)
        break;
      avg+=atof(s);
      count++;
      while(*s && *s != ' ')
        s++;
    }
  }

  if(count < 2) {
    strcpy(buff, "0");
    return;
  }

  avg/=count;

  /* Sum the squares of (value - average) */
  for(a=0;a < nargs;a++) {
    s=args[a];
    while((r=strspc(&s)))
      if(*r)
        result+=pow(atof(r)-avg,2);
  }

  print_float(buff, sqrt(result/count));
}

/* Set 2: Predicate Calculus / Logic *****************************************/

FUNCTION(gt)   { sprintf(buff, "%d", atof(args[0]) >  atof(args[1])); }
FUNCTION(gteq) { sprintf(buff, "%d", atof(args[0]) >= atof(args[1])); }
FUNCTION(lt)   { sprintf(buff, "%d", atof(args[0]) <  atof(args[1])); }
FUNCTION(lteq) { sprintf(buff, "%d", atof(args[0]) <= atof(args[1])); }
FUNCTION(eq)   { sprintf(buff, "%d", atof(args[0]) == atof(args[1])); }
FUNCTION(neq)  { sprintf(buff, "%d", atof(args[0]) != atof(args[1])); }

FUNCTION(ncomp) { sprintf(buff, "%d", atof(args[0]) < atof(args[1])?-1:
                                      atof(args[0]) > atof(args[1])?1:0); }

FUNCTION(truth) { sprintf(buff, "%d", ISTRUE(args[0])); }
FUNCTION(not) { sprintf(buff, "%d", !ISTRUE(args[0])); }
FUNCTION(if) { strcpy(buff, ISTRUE(args[0])?args[1]:""); }
FUNCTION(ifelse) { strcpy(buff, ISTRUE(args[0])?args[1]:args[2]); }
FUNCTION(iftrue) { strcpy(buff, ISTRUE(args[0])?args[0]:args[1]); }
FUNCTION(between) { sprintf(buff, "%d", (atof(args[0])>=atof(args[1]) &&
                                         atof(args[0])<=atof(args[2]))); }

FUNCTION(band) { sprintf(buff, "%d", atoi(args[0]) & atoi(args[1])); }
FUNCTION(bnand) { sprintf(buff, "%d", atoi(args[0]) & ~atoi(args[1])); }
FUNCTION(bor) { sprintf(buff, "%d", atoi(args[0]) | atoi(args[1])); }
FUNCTION(bxor) { sprintf(buff, "%d", atoi(args[0]) ^ atoi(args[1])); }
FUNCTION(lxor) { sprintf(buff, "%d", ISTRUE(args[0]) ^ ISTRUE(args[1])); }

FUNCTION(shl) { sprintf(buff, "%d", atoi(args[0]) << atoi(args[1])); }
FUNCTION(shr) { sprintf(buff, "%d", atoi(args[0]) >> atoi(args[1])); }
FUNCTION(ffs) { sprintf(buff, "%d", ffs(atoi(args[0]))); }

FUNCTION(bitcount) { sprintf(buff, "%ld", popcount(atoi(args[0]))); }

FUNCTION(land) {
  int i;

  for(i=0;i < nargs && ISTRUE(args[i]);i++);
  sprintf(buff, "%d", (i == nargs));
}

FUNCTION(lor) {
  int i;

  for(i=0;i < nargs && !ISTRUE(args[i]);i++);
  sprintf(buff, "%d", (i != nargs));
}

/* Set 3: Math Library *******************************************************/

FUNCTION(e) { strcpy(buff, "2.718281828"); }
FUNCTION(pi) { strcpy(buff, "3.141592654"); }
FUNCTION(sin) { print_float(buff, sin(atof(args[0]))); }
FUNCTION(cos) { print_float(buff, cos(atof(args[0]))); }
FUNCTION(tan) { print_float(buff, tan(atof(args[0]))); }
FUNCTION(asin) { if(fabs(atof(args[0])) > 1) strcpy(buff, "inf");
                   else print_float(buff, asin(atof(args[0]))); }
FUNCTION(acos) { if(fabs(atof(args[0])) > 1) strcpy(buff, "inf");
                   else print_float(buff, acos(atof(args[0]))); }
FUNCTION(atan) { print_float(buff, atan(atof(args[0]))); }

FUNCTION(exp) { print_float(buff, exp(atof(args[0]))); }
FUNCTION(pow) { print_float(buff, pow(atof(args[0]),atof(args[1]))); }
FUNCTION(sqrt) { if(atof(args[0]) < 0) strcpy(buff, "nan");
                   else print_float(buff, sqrt(atof(args[0]))); }
FUNCTION(ln) { if(atof(args[0]) <= 0) strcpy(buff, "nan");
                 else print_float(buff, log(atof(args[0]))); }

FUNCTION(neg) { print_float(buff, atof(args[0])?-atof(args[0]):0); }
FUNCTION(abs) { print_float(buff, fabs(atof(args[0]))); }
FUNCTION(sign) { sprintf(buff, "%d", atof(args[0])>0?1:atof(args[0])<0?-1:0); }

FUNCTION(int) { sprintf(buff, "%d", atoi(args[0])); }
FUNCTION(ceil) { sprintf(buff, "%.0f", ceil(atof(args[0]))); }
FUNCTION(floor) { sprintf(buff, "%.0f", floor(atof(args[0]))); }

FUNCTION(degrees) { print_float(buff, degrees(atof(args[0]),atof(args[1]))); }
FUNCTION(dist2d) { print_float(buff, sqrt(pow(atof(args[0])-atof(args[2]),2)+
                        pow(atof(args[1])-atof(args[3]),2))); }
FUNCTION(dist3d) { print_float(buff, sqrt(pow(atof(args[0])-atof(args[3]),2)+
                        pow(atof(args[1])-atof(args[4]),2)+
                        pow(atof(args[2])-atof(args[5]),2))); }

FUNCTION(round) {
  int range=(nargs==2)?atoi(args[1]):0;
  sprintf(buff, "%.*f", range<0?0:range>9?9:range, atof(args[0]));
}

FUNCTION(range) {
  double value[3]={atof(args[0]), atof(args[1]), atof(args[2])};

  if(value[1] > value[2])
    strcpy(buff, "#-1 Invalid range.");
  else
    print_float(buff, value[0] < value[1]?value[1]:
                      value[0] > value[2]?value[2]:value[0]);
}

FUNCTION(log) {
  double base=(nargs==2)?atof(args[1]):10.0;

  if(atof(args[0]) <= 0 || base <= 0)
    strcpy(buff, "nan");
  else
    print_float(buff, log(atof(args[0]))/log(base));
}

FUNCTION(factor) {
  int count=0, d=3, q, n=atoi(args[0]);
  char *s=buff;

  if(n >= -1 && n <= 1) {
    sprintf(buff, "%d", n);
    return;
  } if(n < 0) {
    *s++='-';
    n=-n;
  }

  /* knock off all 2's */
  while(!(n & 1) && n) {
    if(count++)
      *s++=' ';
    *s++='2';
    n >>= 1;
  }
  *s='\0';

  /* find primes starting at 3 and increment by 2 */
  do {
    while(n == (q=n/d) * d) {
      sprintf(s, "%s%d", count++?" ":"", d);
      s+=strlen(s);
      n = q;
    }
  } while((d += 2) <= q);

  /* a prime number remains */
  if(n > 1)
    sprintf(s, "%s%d", count++?" ":"", n);
}

/* Set 4: Game Runtime Configuration *****************************************/

FUNCTION(mudname)  { strcpy(buff, MUD_NAME); }
FUNCTION(version)  { strcpy(buff, mud_version); }
FUNCTION(config)   { get_config(privs, buff, args[0]); }
FUNCTION(uptime)   { sprintf(buff, "%ld", now-mud_up_time); }
FUNCTION(restarts) { sprintf(buff, "%d", nstat[NS_EXEC]-1); }
FUNCTION(dbtop)    { sprintf(buff, "#%d", db_top-1); }
FUNCTION(upfront)  { sprintf(buff, "#%d", get_upfront()); }
FUNCTION(cpu)      { sprintf(buff, "%.3f", (double)cpu_percent/1000); }
FUNCTION(cputime)  { sprintf(buff, "%.2f", (double)get_cputime()/1000); }

FUNCTION(netstat) {
  static const char *ns[NET_STATS]={
    "FILES", "OUTPUT", "INPUT", "EXEC", "CRASHES", "DBSAVE", "LOGINS",
    "CONNECTS", "FAILED", "NEWCR", "GUESTS", "NCMDS", "NPCMDS", "NQCMDS",
    "NFCMDS", "QUEUE", "OVERRUNS", "MAIL", "EMAIL", "ERECV", "QUERY", "RESOLV",
  };

  int a;

  for(a=0;a < NET_STATS && ns[a] && strcasecmp(ns[a], args[0]);a++);
  if(a == NET_STATS)
    strcpy(buff, "#-1 No such variable.");
  else
    sprintf(buff, "%d", nstat[a]);
}

/* Set 5: Global Weather Almanac *********************************************/

FUNCTION(wtime) { strcpy(buff, wtime(nargs?atol(args[0]):-1, 0)); }
FUNCTION(wdate) { strcpy(buff, wtime(nargs?atol(args[0]):-1, 1)); }
FUNCTION(timeofday) { sprintf(buff, "%d", TIMEOFDAY); }

FUNCTION(gameage) {
  if(BEGINTIME > now)
    BEGINTIME=now;
  sprintf(buff, "%ld", now-BEGINTIME);
}

/* Set 6: Time and Session Management ****************************************/

FUNCTION(time) {
  long tt=nargs?get_date(args[0], privs, 0):now;
  strcpy(buff, (tt == -1)?ERR_DATE:mktm(privs, tt));
}

FUNCTION(date) {
  long tt=nargs?get_date(args[0], privs, 0):now;
  strcpy(buff, (tt == -1)?ERR_DATE:mkdate(mktm(privs, tt)));
}

FUNCTION(rtime) {
  long tt=nargs?get_date(args[0], privs, 0):now;

  if(tt == -1)
    strcpy(buff, ERR_DATE);
  else {
    char *s=mktm(privs, tt);

    sprintf(buff, "%02d%-6.6s %cM", (atoi(s+11)+11)%12+1, s+13,
            (atoi(s+11) > 11)?'P':'A');
  }
}

FUNCTION(xtime) {
  if(!nargs) {
    struct timeval current;

    gettimeofday(&current, NULL);
    sprintf(buff, "%f", (double)current.tv_sec+
            ((double)current.tv_usec/1000000));
  } else {
    long tt=get_date(args[0], privs, 0);

    if(tt == -1)
      strcpy(buff, ERR_DATE);
    else
      sprintf(buff, "%ld", tt);
  }
}

FUNCTION(timefmt) {
  struct timeval current;
  int usec=0, mul=100000;
  long tt;
  char *s;

  if(nargs == 1) {
    gettimeofday(&current, NULL);
    tt=current.tv_sec;
    usec=current.tv_usec;
  } else {
    tt=atol(args[1]);
    if((s=strchr(args[1], '.')))
      for(++s;mul && isdigit(*s);s++,mul /= 10)
        usec+=(*s-'0')*mul;
  }

  timefmt(buff, args[0], tt, usec);
}
    
FUNCTION(laston) {
  dbref thing=lookup_player(args[0]);

  if(thing == AMBIGUOUS)
    thing=privs;
  if(thing == NOTHING || Typeof(thing) != TYPE_PLAYER)
    strcpy(buff, ERR_PLAYER);
  else
    sprintf(buff, "%ld", db[thing].data->last);
}

FUNCTION(lastoff) {
  dbref thing=lookup_player(args[0]);

  if(thing == AMBIGUOUS)
    thing=privs;
  if(thing == NOTHING || Typeof(thing) != TYPE_PLAYER)
    strcpy(buff, ERR_PLAYER);
  else if(!controls(privs, thing, POW_WHO))
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%ld", db[thing].data->lastoff);
}

FUNCTION(tml) { strcpy(buff, time_format(TML, atol(args[0]))); }
FUNCTION(tms) { strcpy(buff, time_format(TMS, atol(args[0]))); }
FUNCTION(tmf) { strcpy(buff, time_format(TMF, atol(args[0]))); }
FUNCTION(tma) { strcpy(buff, time_format(TMA, atol(args[0]))); }

static void io_list(char *buff, dbref privs, char *name, int type)
{
  DESC *d;
  dbref who;

  if((who=lookup_player(name)) == AMBIGUOUS)
    who=privs;
  if(who == NOTHING || Typeof(who) != TYPE_PLAYER)
    strcpy(buff, ERR_PLAYER);
  else if(!controls(privs, who, POW_WHO) && (type > 2 ||
          !could_doit(privs, who, A_HLOCK)))
    strcpy(buff, ERR_PERM);
  else if(!(d=Desc(who)))
    strcpy(buff, ERR_CONN);
  else switch(type) {
    case 1: sprintf(buff, "%ld", now - d->last_time); return;
    case 2: sprintf(buff, "%ld", now - d->login_time); return;
    case 3: strcpy(buff, d->addr); return;
    case 4: strcpy(buff, ipaddr(d->ipaddr)); return;
    case 5: sprintf(buff, "%d", d->concid); return;
    case 6: sprintf(buff, "%d", d->port); return;
    case 7: sprintf(buff, "%d", d->cmds); return;
    case 8: sprintf(buff, "%d", d->input); return;
    case 9: sprintf(buff, "%d", d->output); return;
  }
}

FUNCTION(idle)   { io_list(buff, privs, args[0], 1); }
FUNCTION(onfor)  { io_list(buff, privs, args[0], 2); }
FUNCTION(host)   { io_list(buff, privs, args[0], 3); }
FUNCTION(ipaddr) { io_list(buff, privs, args[0], 4); }
FUNCTION(concid) { io_list(buff, privs, args[0], 5); }
FUNCTION(port)   { io_list(buff, privs, args[0], 6); }
FUNCTION(cmds)   { io_list(buff, privs, args[0], 7); }
FUNCTION(input)  { io_list(buff, privs, args[0], 8); }
FUNCTION(output) { io_list(buff, privs, args[0], 9); }

FUNCTION(lwho) {
  DESC *d;
  char *s=buff;
 
  for(d=descriptor_list;d;d=d->next)
    if((d->flags & C_CONNECTED) && (controls(privs, d->player, POW_WHO) ||
       could_doit(privs, d->player, A_HLOCK)))
      ADDWORD(buff, s, "#%d", d->player);
}

FUNCTION(cwho) {
  DESC *d;
  char *s=buff;

  if(!power(privs, POW_SECURITY) && !is_on_channel(db[privs].owner, args[0]))
    strcpy(buff, "#-1 Not on channel.");
  else {
    for(d=descriptor_list;d;d=d->next)
      if((d->flags & C_CONNECTED) && is_on_channel(d->player, args[0]) &&
         (controls(privs, d->player, POW_WHO) ||
         could_doit(privs, d->player, A_HLOCK)))
        ADDWORD(buff, s, "#%d", d->player);
  }
}

/* Set 7: Database Values ****************************************************/

FUNCTION(num) {
  sprintf(buff, "#%d", match_thing(privs, args[0], MATCH_EVERYTHING));
}

FUNCTION(pnum) {
  dbref thing=lookup_player(args[0]);
  sprintf(buff, "#%d", (thing == AMBIGUOUS) ? NOTHING : thing);
}

FUNCTION(name) {
  INIT_MATCH(it, args[0]);
  strcpy(buff, it==NOTHING?"":db[it].name);
}

FUNCTION(fullname) {
  INIT_MATCH(it, args[0]);
  char *r, *s;

  if(it == NOTHING)
    return;

  strcpy(buff, db[it].name);
  if(Typeof(it) == TYPE_EXIT && *(s=atr_get(it, A_ALIAS))) {
    r=strnul(buff);
    *r++=';';
    strmaxcpy(r, s, 8000-(r-buff));
  }
}

FUNCTION(flags) {
  INIT_MATCH(it, args[0]);
  strcpy(buff, it==NOTHING?"#-1":unparse_flags(privs, it, 0));
}

FUNCTION(unparse) {
  INIT_MATCH(thing, args[0]);

  if(thing == NOTHING && *args[0] == '#')
    thing=atoi(args[0]+1);
  strcpy(buff, unparse_object(doer, thing));
}

FUNCTION(owner) {
  ATTR *attr;
  dbref thing;
  char *s;

  /* return owner of object */
  if(nargs == 1) {
    if(!(s=strchr(args[0], '/'))) {
      if((thing=match_thing(privs, args[0], MATCH_EVERYTHING)) != NOTHING) {
        if(Typeof(thing) == TYPE_PLAYER || !HIDDEN_ADMIN ||
           class(db[thing].owner) != CLASS_WIZARD ||
           controls(privs, thing, POW_FUNCTIONS))
          thing=db[thing].owner;
        else
          thing=NOTHING;
      }
      sprintf(buff, "#%d", thing);
      return;
    }
    *s++='\0';
  } else
    s=args[1];

  /* Find object to which an attribute is defined upon.
   * A #-1 means it is a builtin attribute. */

  if((thing=match_thing(privs, args[0], MATCH_EVERYTHING)) == NOTHING ||
     !(attr=atr_str(thing, s)))
    strcpy(buff, "#-1");
  else if(can_see_atr(privs, thing, attr))
    sprintf(buff, "#%d", attr->obj);  /* note: not owner of object! */
  else
    strcpy(buff, ERR_PERM);
}

static void session_data(char *buff, dbref privs, char *name, int type)
{
  INIT_CMATCH(thing, name, POW_FUNCTIONS);

  if(thing == NOTHING)
    strcpy(buff, ERR_PERM);
  else if(Typeof(thing) != TYPE_PLAYER)
    strcpy(buff, ERR_PLAYER);
  else
    sprintf(buff, "%d", !type ? db[thing].data->sessions:
            type==1?db[thing].data->steps:db[thing].data->age);
}

FUNCTION(sessions) { session_data(buff, privs, args[0], 0); }
FUNCTION(steps)    { session_data(buff, privs, args[0], 1); }
FUNCTION(age)      { session_data(buff, privs, args[0], 2); }

FUNCTION(cols) {
  INIT_MATCH(player, args[0]);
  
  if(player == NOTHING)
    strcpy(buff, "#-1");
  else
    sprintf(buff, "%d", get_cols(NULL, player));
}

FUNCTION(rows) {
  INIT_MATCH(player, args[0]);
  
  if(player == NOTHING)
    strcpy(buff, "#-1");
  else
    sprintf(buff, "%d", get_rows(NULL, player));
}

FUNCTION(term) {
  INIT_MATCH(player, args[0]);

  if(player == NOTHING)
    strcpy(buff, "#-1");
  else
    eval_term_config(buff, db[player].owner, args[1]);
}

FUNCTION(type) {
  INIT_MATCH(thing, args[0]);
  strcpy(buff, (thing == NOTHING)?"#-1":typenames[Typeof(thing)]);
}

FUNCTION(class) {
  INIT_MATCH(thing, args[0]);
  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else if(Typeof(thing) == TYPE_PLAYER) {
    if(HIDDEN_ADMIN && !Immortal(privs))
      strcpy(buff, ERR_PERM);
    else
      strcpy(buff, classnames[class(thing)]);
  } else
    strcpy(buff, typenames[Typeof(thing)]);
}

FUNCTION(rank) {
  INIT_MATCH(thing, args[0]);
  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else if(HIDDEN_ADMIN && !Immortal(privs))
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%d", class(db[thing].owner));
}

FUNCTION(immortal) {
  INIT_MATCH(thing, args[0]);
  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else
    sprintf(buff, "%d", Immortal(thing));
}

FUNCTION(gender) {
  static const char *types[4]={"Male", "Female", "Neuter", "Plural"};

  INIT_MATCH(it, args[0]);

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, it, POW_FUNCTIONS) && !nearby(privs, it))
    strcpy(buff, ERR_LOC);
  else
    strcpy(buff, types[get_gender(it)]);
}

static void gender_substitute(char *buff, dbref privs, char *name, int type)
{
  INIT_MATCH(it, name);

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, it, POW_FUNCTIONS) && !nearby(privs, it))
    strcpy(buff, ERR_LOC);
  else
    strcpy(buff, gender_subst(it, type));
}

FUNCTION(subj)  { gender_substitute(buff, privs, args[0], SUBJ); }
FUNCTION(objn)  { gender_substitute(buff, privs, args[0], OBJN); }
FUNCTION(poss)  { gender_substitute(buff, privs, args[0], POSS); }
FUNCTION(aposs) { gender_substitute(buff, privs, args[0], APOSS); }

FUNCTION(rmatch) {
  INIT_CMATCH(who, args[0], POW_FUNCTIONS);
  if(who == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "#%d", match_thing(who, args[1], MATCH_EVERYTHING));
}

FUNCTION(nearby) {
  INIT_MATCH(obj1, args[0]);
  INIT_MATCH(obj2, args[1]);
  sprintf(buff, "%d", (locatable(privs, obj1) && locatable(privs, obj2) &&
          nearby(obj1, obj2)));
}

FUNCTION(sees) {
  INIT_MATCH(obj1, args[0]);
  INIT_MATCH(obj2, args[1]);

  if(obj1 == NOTHING || obj2 == NOTHING)
    strcpy(buff, "#-1");
  else if((!controls(privs, obj1, POW_FUNCTIONS) && !nearby(privs, obj1)) ||
          (!controls(privs, obj2, POW_FUNCTIONS) && !nearby(privs, obj2)))
    strcpy(buff, ERR_LOC);
  else
    sprintf(buff, "%d", can_see(obj1, obj2)?1:0);
}

FUNCTION(loc) {
  INIT_MATCH(it, args[0]);
  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!locatable(privs, it))
    strcpy(buff, ERR_LOC);
  else
    sprintf(buff, "#%d", db[it].location);
}

FUNCTION(room) {
  INIT_MATCH(it, args[0]);
  int dep, var=it;

  if(it == NOTHING) {
    strcpy(buff, "#-1");
    return;
  } if(!locatable(privs, it)) {
    strcpy(buff, ERR_LOC);
    return;
  }

  for(dep=0;dep < 100 && var != db[var].location;dep++) {
    var=db[var].location;
    if(Invalid(var))
      break;
  }

  /* No room found? */
  if(!Invalid(var) && Typeof(var) != TYPE_ROOM)
    var=NOTHING;

  sprintf(buff, "#%d", var);
}

FUNCTION(rloc) {
  INIT_MATCH(it, args[0]);
  int dep, var=it, limit=atoi(args[1]);

  if(it == NOTHING) {
    strcpy(buff, "#-1");
    return;
  } if(!locatable(privs, it)) {
    strcpy(buff, ERR_LOC);
    return;
  }

  /* set a reasonable limit to the recursion */
  if(limit > 100)
    limit=100;

  for(dep=0;dep < limit && var != db[var].location;dep++) {
    var=db[var].location;
    if(Invalid(var))
      break;
  }

  sprintf(buff, "#%d", var);
}

FUNCTION(con) {
  INIT_MATCH(it, args[0]);
  int pow_plane=power(privs, POW_PLANE);

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, it, POW_FUNCTIONS) && !nearby(privs, it))
    strcpy(buff, ERR_LOC);
  else {
    DOLIST(it, db[it].contents)
      if((controls(privs, it, POW_FUNCTIONS) || can_see(privs, it)) &&
         (!IS(it, TYPE_THING, THING_MONSTER) || power(privs, POW_COMBAT)) &&
         (pow_plane || match_plane(privs, it)))
        break;
    sprintf(buff, "#%d", it);
  }
}

FUNCTION(lcon) {
  INIT_MATCH(it, args[0]);
  char *s=buff;
  int plane=(Typeof(privs)==TYPE_ROOM || Typeof(privs)==TYPE_ZONE)?0:
            db[privs].plane;

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, it, POW_FUNCTIONS) && !nearby(privs, it))
    strcpy(buff, ERR_LOC);
  else if(nargs == 2 && !power(privs, POW_PLANE))
    strcpy(buff, ERR_PERM);
  else {
    if(nargs == 2)
      plane=atoi(args[1]);

    DOLIST(it, db[it].contents)
      if((controls(privs, it, POW_FUNCTIONS) || can_see(privs, it)) &&
         (!IS(it, TYPE_THING, THING_MONSTER) || power(privs, POW_COMBAT)) &&
         (plane == -1 || db[it].plane == -1 || db[it].plane == plane ||
          Typeof(db[it].location) != TYPE_ROOM))
        ADDWORD(buff, s, "#%d", it);
  }
}

FUNCTION(plcon) {
  INIT_MATCH(it, args[0]);
  char *s=buff;
  int plane=(Typeof(privs)==TYPE_ROOM || Typeof(privs)==TYPE_ZONE)?0:
            db[privs].plane;

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, it, POW_FUNCTIONS) && !nearby(privs, it))
    strcpy(buff, ERR_LOC);
  else if(nargs == 2 && !power(privs, POW_PLANE))
    strcpy(buff, ERR_PERM);
  else {
    if(nargs == 2)
      plane=atoi(args[1]);

    DOLIST(it, db[it].contents)
      if(Typeof(it) == TYPE_PLAYER &&
         (controls(privs, it, POW_FUNCTIONS) || can_see(privs, it)) &&
         (plane == -1 || db[it].plane == -1 || db[it].plane == plane ||
          Typeof(db[it].location) != TYPE_ROOM))
        ADDWORD(buff, s, "#%d", it);
  }
}

FUNCTION(exit) {
  INIT_MATCH(it, args[0]);
  int pow_plane=power(privs, POW_PLANE);

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, it, POW_FUNCTIONS) && !nearby(privs, it))
    strcpy(buff, ERR_LOC);
  else {
    DOLIST(it, db[it].exits)
      if((controls(privs, it, POW_FUNCTIONS) || can_see(privs, it)) &&
         (pow_plane || match_plane(privs, it)))
        break;
    sprintf(buff, "#%d", it);
  }
}

FUNCTION(lexits) {
  INIT_MATCH(it, args[0]);
  char *s=buff;
  int plane=(Typeof(privs)==TYPE_ROOM || Typeof(privs)==TYPE_ZONE)?0:
            db[privs].plane;

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, it, POW_FUNCTIONS) && !nearby(privs, it))
    strcpy(buff, ERR_LOC);
  else if(nargs == 2 && !power(privs, POW_PLANE))
    strcpy(buff, ERR_PERM);
  else {
    if(nargs == 2)
      plane=atoi(args[1]);

    DOLIST(it, db[it].exits)
      if((controls(privs, it, POW_FUNCTIONS) || can_see(privs, it)) &&
         (plane == -1 || db[it].plane == -1 || db[it].plane == plane ||
          Typeof(db[it].location) != TYPE_ROOM))
        ADDWORD(buff, s, "#%d", it);
  }
}

FUNCTION(randexit) {
  INIT_CMATCH(thing, args[0], POW_FUNCTIONS);
  if(thing == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "#%d", random_exit(thing, NOTHING));
}

FUNCTION(next) {
  INIT_MATCH(it, args[0]);
  int pow_plane=power(privs, POW_PLANE);

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if((!controls(privs, it, POW_FUNCTIONS) &&
     !controls(privs, db[it].location, POW_FUNCTIONS) &&
     !nearby(privs, db[it].location)))
    strcpy(buff, ERR_LOC);
  else {
    DOLIST(it, db[it].next)
      if((controls(privs, it, POW_FUNCTIONS) || can_see(privs, it)) &&
         (!IS(it, TYPE_THING, THING_MONSTER) || power(privs, POW_COMBAT)) &&
         (pow_plane || match_plane(privs, it)))
        break;
    sprintf(buff, "#%d", it);
  }
}

FUNCTION(objlist) {
  INIT_MATCH(it, args[0]);
  int pow_plane=power(privs, POW_PLANE);
  char *s=buff;

  if(it == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, it, POW_FUNCTIONS) &&
     !controls(privs, db[it].location, POW_FUNCTIONS) &&
     !nearby(privs, db[it].location))
    strcpy(buff, ERR_LOC);
  else {
    DOLIST(it, it)
      if((controls(privs, it, POW_FUNCTIONS) || can_see(privs, it)) &&
         (!IS(it, TYPE_THING, THING_MONSTER) || power(privs, POW_COMBAT)) &&
         (pow_plane || match_plane(privs, it)))
        ADDWORD(buff, s, "#%d", it);
  }
}

FUNCTION(link) {
  INIT_CMATCH(it, args[0], POW_FUNCTIONS);
  if(it == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "#%d", db[it].link);
}

FUNCTION(linkup) {
  INIT_MATCH(thing, args[0]);
  char *s=buff;
  int haspow;
  dbref i;

  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else {
    haspow=controls(privs, thing, POW_FUNCTIONS);
    for(i=0;i<db_top;i++)
      if(db[i].link == thing && (haspow || controls(privs, i, POW_FUNCTIONS)))
        ADDWORD(buff, s, "#%d", i);
  }
}

FUNCTION(entrances) {
  INIT_MATCH(thing, args[0]);
  char *s=buff, *t;
  int haspow;
  dbref i;

  if(thing == NOTHING) {
    strcpy(buff, "#-1");
    return;
  }
  if(Typeof(thing) != TYPE_ROOM)
    return;

  haspow=controls(privs, thing, POW_FUNCTIONS);

  for(i=0;i<db_top;i++) {
    if(Typeof(i) != TYPE_EXIT)
      continue;
    if((db[i].link != thing && db[i].link != AMBIGUOUS) ||
       (!haspow && !controls(privs, i, POW_FUNCTIONS)))
      continue;

    if(db[i].link == AMBIGUOUS) {
      t=atr_get(i, A_VLINK);
      while((t=strchr(t, '#')))
        if(isdigit(*++t) && atoi(t) == thing) {
          if(can_variable_link(i, thing))
            ADDWORD(buff, s, "#%d", i);
          break;
        }
    } else
      ADDWORD(buff, s, "#%d", i);
  }
}

FUNCTION(oexit) {
  INIT_MATCH(thing, args[0]);
  char *s=buff;
  dbref it;

  if(thing == NOTHING || Typeof(thing) != TYPE_EXIT)
    strcpy(buff, "#-1");
  else if(!controls(privs, thing, POW_FUNCTIONS))
    strcpy(buff, ERR_PERM);
  else {
    it=(db[thing].link == HOME) ? db[privs].link : db[thing].link;
    if(it == AMBIGUOUS)
      it=resolve_link(doer, thing, NULL);

    if(it != NOTHING)
      DOLIST(it, db[it].exits)
        if((db[it].link == db[thing].location || (db[it].link == AMBIGUOUS &&
            resolve_link(doer, it, NULL) == db[thing].location)) &&
           (controls(privs, it, POW_FUNCTIONS) || can_see(privs, it)))
          ADDWORD(buff, s, "#%d", it);
  }
}

FUNCTION(path) {
  INIT_MATCH(player, args[0]);
  INIT_MATCH(thing, args[1]);
  int depth=atoi(args[2]);
  dbref *list, *ptr;
  char *s=buff;

  if(player == NOTHING || thing == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, player, POW_FUNCTIONS) ||
          !controls(privs, db[thing].location, POW_FUNCTIONS))
    strcpy(buff, ERR_PERM);
  else if((Typeof(player) != TYPE_PLAYER && Typeof(player) != TYPE_THING) ||
          Typeof(thing) == TYPE_ZONE)
    strcpy(buff, "#-1 Invalid object type.");
  else if(depth < 1 || depth > 100)
    strcpy(buff, ERR_RANGE);
  else {
    list=find_path(player, thing, depth, privs);
    for(ptr=list;ptr && *ptr != NOTHING;ptr++)
      ADDWORD(buff, s, "#%d", *ptr);
    free(list);
  }
}

FUNCTION(powers) {
  INIT_MATCH(player, args[0]);

  if(player == NOTHING)
    strcpy(buff, "#-1");
  else if(RESTRICT_HASPOW && !controls(privs, player, POW_EXAMINE))
    strcpy(buff, ERR_PERM);
  else
    disp_powers(buff, db[player].owner);
}

FUNCTION(controls) {
  INIT_MATCH(player, args[0]);
  INIT_MATCH(object, args[1]);
  int k;

  /* Determine power to check */
  if(nargs == 3) {
    for(k=0;k<NUM_POWS;k++)
      if(!strcasecmp(powers[k].name, args[2]))
        break;
  } else
    k=POW_MODIFY;

  if(player == NOTHING || object == NOTHING)
    strcpy(buff, "#-1");
  else if(RESTRICT_HASPOW && !controls(privs, player, POW_EXAMINE))
    strcpy(buff, ERR_PERM);
  else if(k == NUM_POWS)
    strcpy(buff, "#-1 No such power.");
  else
    sprintf(buff, "%d", controls(player, object, k));
}

FUNCTION(haspow) {
  INIT_MATCH(player, args[0]);
  int k;

  for(k=0;k<NUM_POWS;k++)
    if(!strcasecmp(powers[k].name, args[1]))
      break;

  if(player == NOTHING)
    strcpy(buff, "#-1");
  else if(k == NUM_POWS)
    strcpy(buff, "#-1 No such power.");
  else if(RESTRICT_HASPOW && !controls(privs, player, POW_EXAMINE))
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%d", power(player, k));
}

FUNCTION(parents) {
  INIT_MATCH(thing, args[0]);
  char *s=buff;
  dbref *ptr;
  int haspow;

  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else {
    haspow=controls(privs, thing, POW_FUNCTIONS);
    for(ptr=db[thing].parents;ptr && *ptr != NOTHING;ptr++)
      if(haspow || controls(privs, *ptr, POW_FUNCTIONS))
        ADDWORD(buff, s, "#%d", *ptr);
  }
}

FUNCTION(children) {
  INIT_MATCH(thing, args[0]);
  char *s=buff;
  dbref *ptr;
  int haspow;

  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else {
    haspow=controls(privs, thing, POW_FUNCTIONS);
    for(ptr=db[thing].children;ptr && *ptr != NOTHING;ptr++)
      if(haspow || controls(privs, *ptr, POW_FUNCTIONS))
        ADDWORD(buff, s, "#%d", *ptr);
  }
}

FUNCTION(parents_r) {
  INIT_STACK;
  INIT_MATCH(thing, args[0]);
  char *s=buff;
  dbref *p;
  int haspow;

  if(thing == NOTHING) {
    strcpy(buff, "#-1");
    return;
  }

  haspow=controls(privs, thing, POW_FUNCTIONS);

  while(1) {
    /* Add parents to the stack */
    if(db[thing].parents)
      PUSH(db[thing].parents);

    /* Get the next object from the stack */
    if(STACKTOP())
      break;
    POP(p);
    if(haspow || controls(privs, *p, POW_FUNCTIONS))
      ADDWORD(buff, s, "#%d", *p);
    thing=*p++;
    if(*p != NOTHING)
      PUSH(p);
  }
}

FUNCTION(children_r) {
  INIT_STACK;
  INIT_MATCH(thing, args[0]);
  char *s=buff;
  dbref *p;
  int haspow;

  if(thing == NOTHING) {
    strcpy(buff, "#-1");
    return;
  }

  haspow=controls(privs, thing, POW_FUNCTIONS);

  while(1) {
    /* Add children to the stack */
    if(db[thing].children)
      PUSH(db[thing].children);

    /* Get the next object from the stack */
    if(STACKTOP())
      break;
    POP(p);
    if(haspow || controls(privs, *p, POW_FUNCTIONS))
      ADDWORD(buff, s, "#%d", *p);
    thing=*p++;
    if(*p != NOTHING)
      PUSH(p);
  }
}

FUNCTION(is_a) {
  INIT_MATCH(thing, args[0]);
  INIT_MATCH(parent, args[1]);

  if(thing == NOTHING || parent == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, thing, POW_FUNCTIONS) && !controls(privs, parent,
          POW_FUNCTIONS))
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%d", is_a(thing, parent));
}

FUNCTION(has_a) {
  INIT_MATCH(room, args[0]);
  INIT_MATCH(parent, args[1]);
  char *s=buff;
  dbref i, *p;

  if(room == NOTHING || parent == NOTHING)
    strcpy(buff, "#-1");
  else if(!controls(privs, room, POW_FUNCTIONS) && Typeof(room) != TYPE_ZONE &&
          !nearby(privs, room))
    strcpy(buff, ERR_LOC);
  else if(Typeof(room) == TYPE_ZONE) {
    for(p=db[room].zone;p && *p != NOTHING;p++)
      DOLIST(i, db[*p].contents)
        if(is_a(i, parent) && (controls(privs, i, POW_FUNCTIONS) ||
           controls(privs, parent, POW_FUNCTIONS)))
          ADDWORD(buff, s, "#%d", i);
  } else {
    DOLIST(i, db[room].contents)
      if(is_a(i, parent) && (controls(privs, i, POW_FUNCTIONS) ||
         controls(privs, parent, POW_FUNCTIONS)))
        ADDWORD(buff, s, "#%d", i);
  }
}

FUNCTION(zone) {
  INIT_MATCH(thing, args[0]);
  char *s=buff;
  dbref *ptr;

  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else
    for(ptr=Zone(thing);ptr && *ptr != NOTHING;ptr++)
      ADDWORD(buff, s, "#%d", *ptr);
}

FUNCTION(inzone) {
  INIT_CMATCH(it, args[0], POW_FUNCTIONS);
  char *s=buff;
  dbref *p;

  if(it == NOTHING)
    strcpy(buff, ERR_PERM);
  else if(Typeof(it) != TYPE_ZONE)
    strcpy(buff, "#-1");
  else
    for(p=db[it].zone;p && *p != NOTHING;p++)
      ADDWORD(buff, s, "#%d", *p);
}

FUNCTION(zwho) {
  INIT_CMATCH(it, args[0], POW_FUNCTIONS);
  char *s=buff;
  dbref i, *p;

  if(it == NOTHING)
    strcpy(buff, ERR_PERM);
  else if(Typeof(it) != TYPE_ZONE)
    strcpy(buff, "#-1");
  else {
    for(p=db[it].zone;p && *p != NOTHING;p++)
      DOLIST(i, db[*p].contents)
        if(Typeof(i) == TYPE_PLAYER)
          ADDWORD(buff, s, "#%d", i);
  }
}

FUNCTION(plane) {
  INIT_MATCH(thing, args[0]);

  if(thing == NOTHING || !power(privs, POW_PLANE))
    strcpy(buff, ERR_PERM);
  else if(Typeof(thing) == TYPE_ROOM || Typeof(thing) == TYPE_ZONE)
    strcpy(buff, "#-1");
  else
    sprintf(buff, "%d", db[thing].plane);
}

FUNCTION(waittime) {
  INIT_CMATCH(thing, args[0], POW_QUEUE);
  long next;

  if(thing == NOTHING)
    strcpy(buff, ERR_PERM);
  else {
    next=next_semaid(thing, args[1]);
    if(next == -1)
      strcpy(buff, "#-1 No such ID.");
    else
      sprintf(buff, "%ld", next);
  }
}

FUNCTION(lsema) {
  INIT_CMATCH(thing, args[0], POW_QUEUE);

  if(thing == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    semalist(buff, thing);
}


/* Set 8: Database Attributes ************************************************/

FUNCTION(v) {
  if(strlen(args[0]) > 1) {   /* not a null or 1-character string */
    ATTR *attr;
    char *s;
    dbref match;
    
    /* Match #num.attr format */
    if(*args[0] == '#' && isdigit(*(args[0]+1)) && (s=strchr(args[0], '.')) &&
       !Invalid((match=atoi(args[0]+1))))
      attr=atr_str(match, s+1);
    else
      attr=atr_str(privs, args[0]);

    if(!attr) {
    } else if(can_see_atr(privs, privs, attr))
      strcpy(buff, atr_get_obj(privs, attr->obj, attr->num));
    return;
  }

  switch(tolower(*args[0])) {
    case '0' ... '9': strcpy(buff, env[*args[0]-'0']); return;
    case 'n': strcpy(buff, db[doer].name); return;
    case '#': sprintf(buff, "#%d", doer); return;
    case '!': sprintf(buff, "#%d", privs); return;
    case 'u': strcpy(buff, db[privs].name); return;
    case 'l': sprintf(buff, "#%d", locatable(privs, doer) ?
                      db[doer].location:NOTHING); return;
  }
}

FUNCTION(get) {
  ATTR *attr;
  dbref thing, match;
  char *s, *t;

  /* Backward compatibility: get(obj/attr) */
  if(nargs == 1) {
    if(!(s=strchr(args[0], '/'))) {
      strcpy(buff, "#-1 No attribute name specified.");
      return;
    }
    *s++='\0';
  } else
    s=args[1];

  if((thing=match_thing(privs, args[0], MATCH_EVERYTHING)) == NOTHING) {
    strcpy(buff, "#-1");
    return;
  }

  /* Match #num.attr format */
  if(*s == '#' && isdigit(*(s+1)) && (t=strchr(s, '.')) && *(t+1) &&
     !Invalid((match=atoi(s+1))))
    attr=def_atr_str(match, t+1);
  else
    attr=atr_str(thing, s);

  if(!attr)
    return;
    
  if(can_see_atr(privs, thing, attr))
    strcpy(buff, atr_get_obj(thing, attr->obj, attr->num));
  else
    strcpy(buff, ERR_PERM);
}

FUNCTION(zoneattr) {
  ATTR *attr=NULL;
  dbref thing, match, *zone;
  int defattr=0;
  char *s, *t;

  /* Also allow zoneattr(obj/attr) */
  if(nargs == 1) {
    if(!(s=strchr(args[0], '/'))) {
      strcpy(buff, "#-1 No attribute name specified.");
      return;
    }
    *s++='\0';
  } else
    s=args[1];

  if((thing=match_thing(privs, args[0], MATCH_EVERYTHING)) == NOTHING) {
    strcpy(buff, "#-1");
    return;
  }

  /* Match #num.attr format */
  if(*s == '#' && isdigit(*(s+1)) && (t=strchr(s, '.')) && *(t+1) &&
     !Invalid((match=atoi(s+1)))) {
    if(!(attr=def_atr_str(match, t+1)))
      return;
    defattr=1;
  }
    
  /* We only check zones that we can see attributes on */
  for(zone=Zone(thing);zone && *zone != NOTHING;zone++)
    if((defattr || (attr=atr_str(*zone, s))) &&
       can_see_atr(privs, *zone, attr) &&
       *(t=atr_get_obj(*zone, attr->obj, attr->num))) {
      strcpy(buff, t);
      return;
    }
}

FUNCTION(isattr) {
  INIT_MATCH(thing, args[0]);
  ATTR *attr;

  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else
    sprintf(buff, "%d", (attr=atr_str(thing, args[1])) &&
                         can_see_atr(privs, thing, attr));
}

FUNCTION(hasattr) {
  INIT_MATCH(thing, args[0]);
  ATTR *attr;

  if(thing == NOTHING)
    strcpy(buff, "#-1");
  else
    sprintf(buff, "%d", (attr=atr_str(thing, args[1])) &&
            can_see_atr(privs, thing, attr) &&
            *(atr_get_obj(thing, attr->obj, attr->num)));
}

FUNCTION(attropts) {
  ATTR *attr;
  dbref thing;
  char *s;

  if(nargs == 1) {
    if(!(s=strchr(args[0], '/'))) {
      strcpy(buff, "#-1 No attribute name specified.");
      return;
    }
    *s++='\0';
  } else
    s=args[1];

  if((thing=match_thing(privs, args[0], MATCH_EVERYTHING)) == NOTHING)
    strcpy(buff, "#-1");
  else if((attr=atr_str(thing, s))) {
    if(can_see_atr(privs, thing, attr))
      strcpy(buff, unparse_attrflags(attr->flags));
    else
      strcpy(buff, ERR_PERM);
  }
}

FUNCTION(hasflag) {
  ATTR *attr;
  char *s=strchr(args[0], '/');
  dbref thing;
  int a;

  if((nargs == 2 && s) || nargs == 3) {
    if(nargs == 2)
      *s++='\0';
    else
      s=args[1];
  } else
    s=NULL;

  if((thing=match_thing(privs, args[0], MATCH_EVERYTHING)) == NOTHING) {
    strcpy(buff, "#-1");
    return;
  }

  /* search for a flag on a particular attribute */
  if(s) {
    for(a=0;a<16;a++)
      if(string_prefix(attr_options[a], args[nargs-1]))
        break;
    if(a == 16)
      strcpy(buff, "#-1 No such attribute option.");
    else if(!(attr=atr_str(thing, s)) || !can_see_atr(privs, thing, attr))
      strcpy(buff, "0");
    else
      sprintf(buff, "%d", (attr->flags & (1 << a)) ? 1:0);
    return;
  }

  /* search for a flag on an object */

  for(a=0;flaglist[a].name;a++)
    if((strlen(args[1]) == 1 && flaglist[a].flag == *args[1]) ||
       (strlen(args[1]) > 1 && string_prefix(flaglist[a].name, args[1])))
      break;
  if(!flaglist[a].name)
    strcpy(buff, "#-1 No such flag.");
  else
    /* Don't show 'Connected' flag if player is hidden! */
    if(flaglist[a].objtype == TYPE_PLAYER && flaglist[a].bits ==
       PLAYER_CONNECT && !(could_doit(privs, thing, A_HLOCK) ||
       controls(privs, thing, POW_WHO)))
      strcpy(buff, "0");
  else
    sprintf(buff, "%d",
      (!flaglist[a].bits && Typeof(thing) == flaglist[a].objtype) ||
      (flaglist[a].bits && (flaglist[a].objtype == NOTYPE ||
       flaglist[a].objtype == Typeof(thing)) &&
       (db[thing].flags & flaglist[a].bits)));
}

FUNCTION(lattr) {
  INIT_MATCH(obj, args[0]);
  char *s=buff;
  ALIST *a;
  ATTR *atr;

  if(obj == NOTHING)
    return;

  for(a=db[obj].attrs;a;a=a->next)
    if((atr=atr_num(a->obj, a->num)) && can_see_atr(privs, obj, atr)) {
      if(s-buff > 7930) {
        strcpy(s, " ...");
        return;
      }
      if(s != buff)
        *s++=' ';
      strcpy(s, atr->name);
      s+=strlen(s);
    }
}

FUNCTION(lattrdef) {
  INIT_MATCH(it, args[0]);
  char *s=buff;
  ATRDEF *k;

  if(it == NOTHING)
    return;

  if((db[it].flags & VISIBLE) || controls(privs, it, POW_EXAMINE))
    for(k=db[it].atrdefs;k;k=k->next) {
      if(s-buff > 7930) {
        strcpy(s, " ...");
        return;
      }
      if(s != buff)
        *s++=' ';
      strcpy(s, k->atr.name);
      s+=strlen(s);
    }
}

FUNCTION(createtime) {
  INIT_CMATCH(it, args[0], POW_FUNCTIONS);
  if(it == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%ld", db[it].create_time);
}

FUNCTION(modtime) {
  INIT_CMATCH(it, args[0], POW_FUNCTIONS);
  if(it == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%ld", db[it].mod_time);
}

FUNCTION(pennies) {
  INIT_CMATCH(who, args[0], POW_MONEY);
  if(who == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%d", db[who].pennies);
}

FUNCTION(stats) {
  dbref owner=AMBIGUOUS;
  int i, total, obj[8], pla[NUM_CLASSES];
  char *s=buff;
  
  if(nargs) {
    if((owner=lookup_player(args[0])) == NOTHING) {
      strcpy(buff, ERR_PLAYER);
      return;
    }

    if(owner == AMBIGUOUS)
      owner=db[privs].owner;
    else if(!controls(privs, owner, POW_STATS)) {
      strcpy(buff, ERR_PERM);
      return;
    }
  }

  calc_stats(owner, &total, obj, pla);
  s+=sprintf(s, "%d %d", db_top, total);
  for(i=0;i < 8;i++)
    s+=sprintf(s, " %d", obj[i]);
}

FUNCTION(objmem) {
  INIT_CMATCH(thing, args[0], POW_STATS);
  if(thing == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%d", mem_usage(thing));
}

FUNCTION(playmem) {
  INIT_CMATCH(thing, args[0], POW_STATS);
  if(thing == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "%d", playmem_usage(db[thing].owner));
}

FUNCTION(setq) {
  int a=atoi(args[0]);

  if(a < 0 || a > 9)
    strcpy(buff, ERR_ENV);
  else
    strcpy(env[a], args[1]);
}

FUNCTION(setr) {
  int a=atoi(args[0]);

  if(a < 0 || a > 9)
    strcpy(buff, ERR_ENV);
  else {
    strcpy(env[a], args[1]);
    strcpy(buff, args[1]);
  }
}

FUNCTION(s) { pronoun_substitute(buff, privs, doer, args[0]); }

FUNCTION(s_as) {
  INIT_CMATCH(newdoer, args[1], POW_MODIFY);
  INIT_CMATCH(newprivs, args[2], POW_MODIFY);

  if(newdoer == NOTHING || newprivs == NOTHING)
    strcpy(buff, ERR_PERM);
  else
    pronoun_substitute(buff, newprivs, newdoer, args[0]);
}

FUNCTION(s_as_with) {
  INIT_CMATCH(newdoer, args[1], POW_MODIFY);
  INIT_CMATCH(newprivs, args[2], POW_MODIFY);

  char *sptr[10];
  int a, len[10];

  if(newdoer == NOTHING || newprivs == NOTHING) {
    strcpy(buff, ERR_PERM);
    return;
  }

  /* Preserve local registers */
  for(a=0;a<10;a++) {
    len[a]=strlen(env[a])+1;
    memcpy(sptr[a]=alloca(len[a]), env[a], len[a]);
    strcpy(env[a], (a < nargs-3)?args[a+3]:"");
  }

  pronoun_substitute(buff, newprivs, newdoer, args[0]);

  /* Restore local registers */
  for(a=0;a<10;a++)
    memcpy(env[a], sptr[a], len[a]);
}

FUNCTION(localize) {
  char *sptr[10];
  int a, len[10];

  /* Preserve local registers */
  for(a=0;a<10;a++) {
    len[a]=strlen(env[a])+1;
    memcpy(sptr[a]=alloca(len[a]), env[a], len[a]);
  }

  pronoun_substitute(buff, privs, doer, args[0]);

  /* Restore local registers */
  for(a=0;a<10;a++)
    memcpy(env[a], sptr[a], len[a]);
}

FUNCTION(localize_as) {
  INIT_CMATCH(newdoer, args[1], POW_MODIFY);
  INIT_CMATCH(newprivs, args[2], POW_MODIFY);

  char *sptr[10];
  int a, len[10];

  if(newdoer == NOTHING || newprivs == NOTHING) {
    strcpy(buff, ERR_PERM);
    return;
  }

  /* Preserve local registers */
  for(a=0;a<10;a++) {
    len[a]=strlen(env[a])+1;
    memcpy(sptr[a]=alloca(len[a]), env[a], len[a]);
  }

  pronoun_substitute(buff, newprivs, newdoer, args[0]);

  /* Restore local registers */
  for(a=0;a<10;a++)
    memcpy(env[a], sptr[a], len[a]);
}

FUNCTION(parse_lock) {
  INIT_MATCH(player, args[0]);
  INIT_CMATCH(thing, args[1], POW_EXAMINE);

  char *sptr[10];
  int a, len[10];

  if(player == NOTHING || thing == NOTHING) {
    strcpy(buff, ERR_PERM);
    return;
  }

  /* Preserve local registers */
  for(a=0;a<10;a++) {
    len[a]=strlen(env[a])+1;
    memcpy(sptr[a]=alloca(len[a]), env[a], len[a]);
  }

  sprintf(buff, "%d", eval_boolexp(player, thing, args[2]));

  /* Restore local registers */
  for(a=0;a<10;a++)
    memcpy(env[a], sptr[a], len[a]);
}

FUNCTION(valid) {
  static const char *titles[]={
    "Name", "Playername", "Alias", "Attribute", "Itemname"};

  int i;

  for(i=0;i < NELEM(titles) && !string_prefix(titles[i], args[0]);i++);
  if(i == NELEM(titles)) {
    strcpy(buff, "#-1 No such category.");
    return;
  }

  switch(i) {
    case 0: i=ok_name(args[1]); break;
    case 1: i=ok_player_name(args[1], 0); break;
    case 2: i=ok_player_name(args[1], 1); break;
    case 3: i=ok_attribute_name(args[1]); break;
    case 4: i=ok_itemname(args[1]); break;
  }

  sprintf(buff, "%d", i);
}

/* Set 9: String Utilities ***************************************************/

FUNCTION(strlen) { sprintf(buff, "%d", (int)strlen(args[0])); }
FUNCTION(ascii)  { sprintf(buff, "%d", (unsigned char)*args[0]); }
FUNCTION(edit)   { edit_string(buff, args[0], args[1], args[2]); }
FUNCTION(secure) { strcpy(buff, secure_string(args[0])); }

FUNCTION(match)  {
  sprintf(buff, "%d", wild_match(args[0], args[1], WILD_BOOLEAN));
}

FUNCTION(comp) {
  int i=strcmp(args[0], args[1]);
  sprintf(buff, "%d", (i>0)?1 : (i<0)?-1 : 0);
}

FUNCTION(strcat) {
  char *s=buff;
  int i;

  for(i=0;i<nargs && (s-buff < 8000);i++)
    strmaxcpy(s+=strlen(s), args[i], 8001-(s-buff));
}

FUNCTION(wmatch) {
  DELIM(delim, 2);
  char *r, *t=args[0];
  int wcount=1;

  while((r=parse_up(&t, delim))) {
    if(!*r && delim == ' ')
      continue;
    if(wild_match(r, args[1], WILD_BOOLEAN)) {
      sprintf(buff, "%d", wcount);
      return;
    }
    wcount++;
  }
  strcpy(buff, "0");
}

FUNCTION(matchall) {
  DELIM(delim, 2);
  char *r, *s=buff, *t=args[0];
  int wcount=1;

  while((r=parse_up(&t, delim))) {
    if(!*r && delim == ' ')
      continue;
    if(wild_match(r, args[1], WILD_BOOLEAN))
      ADDWORD(buff, s, "%d", wcount);
    wcount++;
  }
}
  
FUNCTION(regmatch) {
  char *sptr[10];
  int a=(nargs==3)?atoi(args[2]):0, b, result;

  if(a < 0 || a > 9) {
    strcpy(buff, ERR_ENV);
    return;
  }

  /* preserve copy of old environment */
  for(b=a;b<10;b++)
    APTR(sptr[b], env[b]);

  if(!(result=wild_match(args[0], args[1], a+1)))
    for(b=a;b<10;b++)
      strcpy(env[b], sptr[b]);  /* restore old environment if case failed */

  sprintf(buff, "%d", result);
}

FUNCTION(capstr) {
  *buff=toupper(*args[0]);
  if(*args[0])
    strcpy(buff+1, args[0]+1);
}

FUNCTION(ucstr) {
  char *s=buff, *t=args[0];

  while(*t)
    *s++=toupper(*t++);
  *s='\0';
}

FUNCTION(lcstr) {
  char *s=buff, *t=args[0];

  while(*t)
    *s++=tolower(*t++);
  *s='\0';
}

FUNCTION(pos) {
  char *s=strstr(args[1], args[0]);
  sprintf(buff, "%d", s?(int)(s-args[1]+1):0);
}

FUNCTION(lpos) {
  DELIM(ch, 1);
  char *r=args[0]-1, *s=buff;

  while((r=strchr(r+1, ch)))
    ADDWORD(buff, s, "%d", (int)(r-args[0]));
}

FUNCTION(mid) {
  int l=atoi(args[1]), len=atoi(args[2]);

  if(l < 0 || len < 0)
    strcpy(buff, ERR_RANGE);
  else if(l < strlen(args[0])) {
    if(len > 8000)
      len=8000;
    strmaxcpy(buff, args[0]+l, len+1);  /* no buffer overrun */
  }
}

FUNCTION(delete) {
  int len=atoi(args[1]), l=atoi(args[2]);

  if(len < 0 || l < 0)
    strcpy(buff, ERR_RANGE);
  else {
    if(len > 8000)
      len=8000;
    strmaxcpy(buff, args[0], len+1);
    if(len+l < strlen(args[0]))
      strcpy(buff+len, args[0]+len+l);
  }
}

FUNCTION(first) {
  DELIM(delim, 1);
  char *r, *s=args[0];

  if(delim == ' ')
    while(*s && *s == ' ')
      s++;
  if((r=strchr(s, delim)))
    *r='\0';
  strcpy(buff, s);
}

FUNCTION(rest) {
  DELIM(delim, 1);
  char *s=args[0];

  if(delim == ' ')
    while(*s && *s == ' ')
      s++;
  if((s=strchr(s, delim))) {
    if(delim == ' ')
      while(*(s+1) == ' ')
        s++;
    strcpy(buff, s+1);
  }
}

FUNCTION(last) {
  DELIM(delim, 1);
  char *s=strnul(args[0]), *end=s;

  if(delim == ' ') {
    while(s > args[0] && *(s-1) == ' ')
      s--;
    end=s;
  }
  while(s > args[0] && *(s-1) != delim)
    s--;

  memcpy(buff, s, end-s);
  buff[end-s]='\0';
}

FUNCTION(extract) {
  DELIM(delim, 3);
  char *r, *s=args[0];
  int first=atoi(args[1]), last=first+atoi(args[2])-1, wcount=1;

  /* check length specified */
  if(last < first-1) {
    strcpy(buff, ERR_RANGE);
    return;
  } if(last < 1 || last < first)
    return;

  if(delim == ' ')
    trim_spaces(&s);
  r=s;

  while((r=strchr(r, delim))) {
    if(wcount >= last)
      break;
    r++;
    if(delim == ' ')
      while(*r == delim)
        r++;
    if(!*r || ++wcount == first)
      s=r;
  }

  if(wcount >= first) {
    if(r)
      *r='\0';
    strcpy(buff, s);
  }
}

FUNCTION(remove) {
  DELIM(delim, 3);
  int first=atoi(args[1]), last=first+atoi(args[2]), wcount=1;
  char *r, *s=args[0];

  /* check length specified */
  if(last < first) {
    strcpy(buff, ERR_RANGE);
    return;
  }

  if(delim == ' ')
    trim_spaces(&s);

  if(last <= 1 || last <= first) {
    strcpy(buff, s);
    return;
  }

  r=s;
  while((s=strchr(s, delim))) {
    if(++wcount == first)
      strmaxcpy(buff, r, s-r+1);
    s++;
    if(delim == ' ')
      while(*s == ' ')
        s++;
    if(wcount == last) {
      if((r=strnul(buff)) > buff)
        *r++=delim;
      strcpy(r, s);
      return;
    }
  }

  if(wcount < first)
    strcpy(buff, r);
}

FUNCTION(insert) {
  DELIM(delim, 3);
  char *p=buff, *r, *s=args[0];
  int first=atoi(args[1]), wcount=1;

  if(delim == ' ')
    trim_spaces(&s);
  r=s;

  /* scan for location to begin insert */
  if(first > 1) {
    while((s=strchr(s, delim))) {
      if(delim == ' ')
        while(*(s+1) == ' ')
          s++;
      if(++wcount == first) {
        *s++='\0';
        break;
      }
      s++;
    }

    strcpy(buff, r);
    p+=strlen(p);
    *p++=delim;
  }

  strmaxcpy(p, args[2], 8001-(p-buff));
  p+=strlen(p);

  /* copy remainder of string to result buffer */
  if(s && *s && p-buff < 8000) {
    *p++=delim;
    strmaxcpy(p, s, 8001-(p-buff));
  }
}

FUNCTION(replace) {
  DELIM(delim, 3);
  char *p=buff, *r, *s=args[0];
  int replace=atoi(args[1]), wcount=1;

  if(delim == ' ')
    trim_spaces(&s);
  r=s;

  /* scan for word to replace */
  if(replace > 1) {
    while((s=strchr(s, delim))) {
      if(delim == ' ')
        while(*(s+1) == ' ')
          s++;
      if(++wcount == replace) {
        *s++='\0';
        break;
      }
      s++;
    }

    strcpy(buff, r);
    p+=strlen(p);
    *p++=delim;
  }

  strmaxcpy(p, args[2], 8001-(p-buff));
  p+=strlen(p);

  /* copy remainder of string to result buffer */
  if(s && *s && p-buff < 8000) {
    if(replace < 1)
      *p++=delim;
    else
      s=strchr(s, delim);

    if(s)
      strmaxcpy(p, s, 8001-(p-buff));
  }
}

FUNCTION(swap) {
  DELIM(delim, 3);
  char *r, *s=args[0], *t;
  char *p1=NULL, *p2=NULL, *end1=NULL;
  int first=atoi(args[1]), second=atoi(args[2]), wcount;

  if(delim == ' ')
    trim_spaces(&s);

  /* check range */
  if(first == second || (first <= 1 && second <= 1)) {
    strcpy(buff, s);  /* nothing to do */
    return;
  } if(first > second) {  /* force first < second */
    int tmp=first;
    first=second;
    second=tmp;
  }

  /* Find both strings to replace. p1=str1, p2=str2, end1=end of str1. */
  for(r=s,wcount=1;*r;wcount++) {
    /* beginning of next word */
    if(wcount >= first && first > 0 && !p1)
      p1=r;
    else if(wcount == second)
      p2=r;

    while(*r && *r != delim)
      r++;
    if(p1 && !end1)
      end1=r;
    if(p2 || !*r)  /* at end of second word */
      break;

    r++;
    if(delim == ' ')
      while(*r == ' ')
        r++;
  }
  if(!p2 && wcount == second)  /* blank word at end of list */
    p2=r;

  /* Do nothing if 1st word wasn't found yet, or both words are out of range */
  if(first >= wcount || (!p1 && !p2)) {
    strcpy(buff, s);
    return;
  }

  /* copy up to string 1 & replace */
  t=buff;
  if(p1) {
    memcpy(t, s, p1-s);
    t+=p1-s;
    if(p2) {
      memcpy(t, p2, r-p2);
      t+=r-p2;
    } else {
      /* 2nd word did not exist. Advance end1 past delimiter. */
      if(*end1)
        end1++;
      if(delim == ' ')
        while(*end1 == ' ')
          end1++;
    }
  } else {
    /* prepend word */
    memcpy(t, p2, r-p2);
    t+=r-p2;
    *t++=delim;
  }

  /* if first word doesn't exist, set end1 to beginning of string */
  if(!end1)
    end1=s;

  /* move word to end if second word wasn't found */
  if(!p2) {
    strcpy(t, end1);
    t+=strlen(t);

    /* append word */
    *t++=delim;
    memcpy(t, p1, end1-p1);
    t+=end1-p1;

    /* trim delimiter */
    if(*(t-1) == delim)
      t--;
    if(delim == ' ')
      while(*(t-1) == ' ')
        t--;
    *t='\0';
    return;
  }

  /* else copy up to string 2 & replace */
  memcpy(t, end1, p2-end1);
  t+=p2-end1;
  if(!p1 && !*r) {
    /* back out delimiter */
    t--;
    if(delim == ' ')
      while(*(t-1) == ' ')
        t--;
  } else if(!p1) {
    /* 1st word did not exist. Advance 'r' (end2) past delimiter. */
    r++;
    if(*r == ' ')
      while(*r == ' ')
        r++;
  } else {
    /* insert word */
    memcpy(t, p1, end1-p1);
    t+=end1-p1;
  }

  /* copy end of string */
  strcpy(t, r);
}

FUNCTION(nearest) {
  int a, num, list[2000], top=0, how=0, index=-1;
  char *r, *s=args[0];
  
  /* Read in number list using insert-sort */
  while((r=strspc(&s)))
    if(*r) {
      /* Scan to insert number in ascending order */
      num=atoi(r);
      for(a=top;a > 0;a--)
        if(list[a-1] <= num)
          break;
      if(a > 0 && list[a-1] == num)
        continue;

      /* Insert new number */
      if(top-a)
        memmove(list+a+1, list+a, (top-a)*sizeof(int));
      list[a]=num;
      top++;
    }

  /* Determine if we only check Less-than, Greater-than, or Both */
  if(nargs > 2)
    how=((*args[2]|32) == 'l')?1:((*args[2]|32) == 'g')?2:0;
  num=atoi(args[1]);

  /* Scan list until we're at the pivot point */
  for(a=0;a < top && num > list[a];a++);

  if(a < top && (num == list[a] || how == 1))  /* Exact match */
    index=a;
  else if(a == top || how == 2)  /* Nearest greater-than match */
    index=(how == 1)?-1:(a-1);
  else if(!a)
    index=0;
  else
    index=(list[a]-num < num-list[a-1])?a:(a-1);

  if(index > -1)
    sprintf(buff, "%d", list[index]);
}

FUNCTION(sort) {
  DELIM(delim, 2);
  DELIMCH(odelim, 3, delim);
  char *words[4096], *r, *s=args[0], type=(nargs > 1?*args[1]:0)|32;
  int i, j, len, wcount=0;

  if(type == 32) {
    /* Auto-detect sort type */
    type='n';
    for(r=args[0];*r;r++) {
      if(*r != delim) {
        if(*r == '#' && *(r+1) != delim && isdigit(*(r+1)))
          type='d';
        else if(!isdigit(*r) && *r != '-') {
          type='i';
          break;
        }
      }
      while(*r && *r != delim)
        if(*r++ == '.' && type == 'n')
          type='f';
      if(!*r)
        break;
    }
  } else if(!strchr("adfin", type)) {
    strcpy(buff, "#-1 Bad sort type.");
    return;
  }

  if(delim == ' ')
    trim_spaces(&s);

  /* Go through word by word and do insertion-sort */
  while((r=parse_up(&s, delim))) {
    if(!*r && delim == ' ')
      continue;
    for(i=0;i < wcount;i++)
      if((type == 'a' && strcmp(r, words[i]) < 0) ||
         (type == 'i' && strcasecmp(r, words[i]) < 0) ||
         (type == 'n' && atoi(r) < atoi(words[i])) ||
         (type == 'f' && atof(r) < atof(words[i])) ||
         (type == 'd' && *words[i] == '#' && (*r != '#' ||
                          (atoi(r+1) < atoi(words[i]+1)))))
        break;
    for(j=wcount++;j > i;j--)
      words[j]=words[j-1];
    words[i]=r;
  }

  /* Build result string */
  s=buff;
  for(i=0;i < wcount;i++) {
    if(s != buff)
      *s++=odelim;
    len=strlen(words[i]);
    memcpy(s, words[i], len);
    s+=len;
  }
  *s='\0';
}

FUNCTION(setdiff) {
  DELIM(delim, 2);
  DELIMCH(odelim, 3, delim);
  char *words[4096], *r, *s=args[0];
  int i, len, wcount=0;

  if(delim == ' ')
    trim_spaces(&s);

  /* Count words in <list1> */
  while((r=parse_up(&s, delim)))
    if(*r || delim != ' ')
      words[wcount++]=r;

  /* Subtract words from those in <list2> */
  s=args[1];
  while((r=parse_up(&s, delim)))
    if(*r || delim != ' ')
      for(i=0;i < wcount;i++)
        if(words[i] && !strcasecmp(words[i], r))
          words[i]=NULL;

  /* Build result string */
  s=buff;
  for(i=0;i < wcount;i++)
    if(words[i]) {
      if(s != buff)
        *s++=odelim;
      len=strlen(words[i]);
      memcpy(s, words[i], len);
      s+=len;
    }
  *s='\0';
}

FUNCTION(setinter) {
  DELIM(delim, 2);
  DELIMCH(odelim, 3, delim);
  char *words[4096], flags[4096], *r, *s=args[0];
  int i, len, wcount=0;

  if(delim == ' ')
    trim_spaces(&s);

  /* Count only unique words in <list1> */
  while((r=parse_up(&s, delim)))
    if(*r || delim != ' ') {
      for(i=0;i < wcount && strcasecmp(words[i], r);i++);
      if(i == wcount) {
        flags[wcount]=0;
        words[wcount++]=r;
      }
    }

  /* Loop for each word in <list2> to see if it exists in <list1> */
  s=args[1];
  while((r=parse_up(&s, delim)))
    if(*r || delim != ' ') {
      for(i=0;i < wcount && (flags[i] || strcasecmp(words[i], r));i++);
      if(i != wcount)
        flags[i]=1;
    }

  /* Build result string of all words flagged */
  s=buff;
  for(i=0;i < wcount;i++)
    if(flags[i]) {
      if(s != buff)
        *s++=odelim;
      len=strlen(words[i]);
      memcpy(s, words[i], len);
      s+=len;
    }
  *s='\0';
}

FUNCTION(setunion) {
  DELIM(delim, 2);
  DELIMCH(odelim, 3, delim);
  char *words[8192], *r, *s=args[0];
  int i, len, wcount=0;

  if(delim == ' ')
    trim_spaces(&s);

  /* Count only unique words in <list1> */
  while((r=parse_up(&s, delim)))
    if(*r || delim != ' ') {
      for(i=0;i < wcount && strcasecmp(words[i], r);i++);
      if(i == wcount)
        words[wcount++]=r;
    }

  /* Add unique words from <list2> */
  s=args[1];
  while((r=parse_up(&s, delim)))
    if(*r || delim != ' ') {
      for(i=0;i < wcount && strcasecmp(words[i], r);i++);
      if(i == wcount)
        words[wcount++]=r;
    }

  /* Build result string */
  s=buff;
  for(i=0;i < wcount;i++) {
    len=strlen(words[i]);
    if(s-buff+len+(s != buff) > 8000)
      break;
    if(s != buff)
      *s++=odelim;
    memcpy(s, words[i], len);
    s+=len;
  }
  *s='\0';
}

FUNCTION(wcount) {
  DELIM(delim, 1);
  char *s=args[0];
  int wcount=1;
  
  if(!*args[0]) {
    strcpy(buff, "0");
    return;
  }

  if(delim == ' ')
    trim_spaces(&s);

  while((s=strchr(s, delim))) {
    wcount++;
    s++;
    if(delim == ' ')
      while(*s == ' ')
        s++;
  }

  sprintf(buff, "%d", wcount);
}

FUNCTION(flip) {
  char *s=strnul(args[0]), *r=buff;

  while(s > args[0])
    *r++=*--s;
  *r='\0';
}

FUNCTION(wordflip) {
  DELIM(delim, 1);
  char *r=buff, *s=args[0], *t=strnul(args[0]);

  /* remove leading and following spaces */
  if(delim == ' ') {
    while(*s && *s == ' ')
      s++;
    while(t > s && *(t-1) == ' ')
      t--;
  }

  while(t >= s) {
    *t--='\0';

    while(t >= s && *t != delim)
      t--;
    if(*buff)
      *r++=delim;
    strcpy(r, t+1);
    r+=strlen(r);
  }
}

FUNCTION(scramble) {
  int i, j, n=strlen(args[0]);

  strcpy(buff, args[0]);

  for(i=0;i<n;i++) {
    j=rand_num(n);
    XCHG(buff[i], buff[j]);
  }
}

FUNCTION(shuffle) {
  DELIM(delim, 1);
  DELIMCH(odelim, 2, delim);
  char *words[4096], *r, *s=args[0];
  int i, j, len, wcount=0;

  if(delim == ' ')
    trim_spaces(&s);

  /* Record pointers to every word in <word list> */
  while((r=parse_up(&s, delim))) {
    if(!*r && delim == ' ')
      continue;
    words[wcount++]=r;
  }

  /* Shuffle words */
  for(i=0;i < wcount;i++) {
    j=rand_num(wcount);
    XCHG(words[i], words[j]);
  }

  /* Build result string */
  s=buff;
  for(i=0;i < wcount;i++) {
    if(s != buff)
      *s++=odelim;
    len=strlen(words[i]);
    memcpy(s, words[i], len);
    s+=len;
  }
  *s='\0';
}

FUNCTION(rand) { sprintf(buff, "%d", rand_num(atoi(args[0]))); }

FUNCTION(srand) {
  int modulo=atoi(args[0]), n=atoi(args[2]), count=(nargs > 3)?atoi(args[3]):1;

  if(modulo < 1)
    modulo=1;
  if(count < 1)
    count=1;
  if(n < 0 || n+count > 10000 || count > 1000)
    strcpy(buff, ERR_RANGE);
  else
    mt_repeatable_rand(buff, modulo, atoi(args[1]), n, count);
}

FUNCTION(dice) {
  int total, count, n=atoi(args[0]), die=atoi(args[1]);

  if(n < 0 || n > 1000)
    strcpy(buff, ERR_RANGE);
  else {
    for(total=0,count=0;count < n;count++)
      total += rand_num(die);
    sprintf(buff, "%d", total+n);
  }
}

FUNCTION(randword) {
  DELIM(delim, 1);
  char *s=args[0], *t;
  int wcount=1, num;
  
  if(delim == ' ')
    trim_spaces(&s);

  if(!*s)
    return;

  t=s;
  while((t=strchr(t, delim))) {
    wcount++;
    t++;
    if(delim == ' ')
      while(*t == ' ')
        t++;
  }

  num=rand_num(wcount);
  wcount=0;
  t=s;

  while((t=strchr(t, delim))) {
    if(wcount >= num)
      break;
    t++;
    if(delim == ' ')
      while(*t == delim)
        t++;
    if(!*t || ++wcount == num)
      s=t;
  }

  if(t)
    *t='\0';
  strcpy(buff, s);
}

FUNCTION(base) {
  int digit, neg=0, oldbase=atoi(args[1]), newbase=atoi(args[2]);
  char temp[66], *r=args[0], *s;
  unsigned quad decimal=0;

  if(oldbase < 2 || oldbase > 36 || newbase < 2 || newbase > 36) {
    strcpy(buff, "#-1 Bases must be between 2 and 36.");
    return;
  }
  
  /* convert from oldbase */
  while(*r == ' ')
    r++;
  if(*r == '-') {
    neg=1;
    r++;
  } else if(*r == '+')
    r++;

  while(*r && *r != ' ') {
    digit=(*r >= '0' && *r <= '9')?(*r-48):((*r|32) >= 'a' && (*r|32) <= 'z')?
           ((*r|32)-87):36;
    if(digit >= oldbase) {
      strcpy(buff, "#-1 Illegal digit.");
      return;
    }
    decimal*=oldbase;
    decimal+=digit;
    r++;
  }

  if(!decimal) {
    strcpy(buff, "0");
    return;
  }

  /* convert to newbase (backwards) */
  s=temp+65;
  *s='\0';

  while(decimal > 0) {
    digit = decimal % newbase;
    decimal /= newbase;
    *--s = digit+(digit < 10?48:87);
  }

  if(neg)
    *--s='-';
  strcpy(buff, s);
}

FUNCTION(chr) {
  int chr=atoi(args[0]);

  if(!power(privs, POW_ANIMATION) && (chr < 32 || chr >= 127))
    chr=' ';
  buff[0]=chr;
  buff[1]='\0';
}

FUNCTION(esc) {
  if(!power(privs, POW_ANIMATION))
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "\e[%s", (nargs == 1)?args[0]:"");
}

FUNCTION(csi) {
  if(!power(privs, POW_ANIMATION))
    strcpy(buff, ERR_PERM);
  else
    sprintf(buff, "\e%s", (nargs == 1)?args[0]:"");
}

FUNCTION(ansi) {
  int color=color_code(7, args[0]);

  if(color & 1024)
    strcpy(buff, rainbowmsg(args[1], color, 8000));
  else
    sprintf(buff, "%s%s%s", textcolor(default_color, color), args[1],
            textcolor(color, default_color));
}

FUNCTION(cname) {
  INIT_MATCH(it, args[0]);
  strcpy(buff, it==NOTHING?"":colorname(it));
}

FUNCTION(ctext) { strcpy(buff, color_by_cname(args[0], args[1], 8000, 0)); }

FUNCTION(strip) { strcpy(buff, args[0]); }  // Stripped by F_STRIP flag.

FUNCTION(art) { strcpy(buff, article(args[0])); }
FUNCTION(pluralize) { strcpy(buff, pluralize(args[0])); }

FUNCTION(isdbref) {
  sprintf(buff, "%d", (*args[0] == '#' && isnumber(args[0]+1) &&
    !Invalid(atoi(args[0]+1)) && Typeof(atoi(args[0]+1)) != TYPE_GARBAGE));
}

FUNCTION(isnum) {
  char *s=args[0];
  int decimal=0;

  if(*s == '-' || *s == '+')
    s++;
  if(!*s) {
    strcpy(buff, "0");
    return;
  }

  for(;*s;s++) {
    if(*s == '.') {
      if(decimal || s == args[0] || !isdigit(*(s-1)) || !isdigit(*(s+1)))
        break;
      decimal=1;
    } else if(!isdigit(*s))
      break;
  }

  sprintf(buff, "%d", *s?0:1);
}

FUNCTION(isword) {
  char *s=args[0];

  if(!*s)
    strcpy(buff, "0");
  else {
    while(isalpha(*s))
      s++;
    sprintf(buff, "%d", *s?0:1);
  }
}

static void increment_string(char *buff, char *str, int inc)
{
  char *s;

  if(!*str) {
    sprintf(buff, "%d", inc);
    return;
  }
  s=strnul(str)-1;
  if(!isdigit(*s)) {
    strcpy(buff, "#-1 String must end in an integer.");
    return;
  }

  while(s > str && (isdigit(*(s-1)) || *(s-1) == '-')) {
    s--;
    if(*s == '-')
      break;
  }

  memcpy(buff, str, s-str);
  sprintf(buff+(s-str), "%d", atoi(s)+inc);
}

FUNCTION(inc) { increment_string(buff, args[0], 1); }
FUNCTION(dec) { increment_string(buff, args[0], -1); }

FUNCTION(spc) {
  int len=atoi(args[0]);

  if(len > 0) {
    if(len > 8000)
      len=8000;
    memset(buff, ' ', len);
    buff[len]='\0';
  }
}

FUNCTION(lnum) {
  double i=(nargs > 1)?atof(args[1]):0, j=(nargs > 2)?atof(args[2]):1;
  int a, x=atoi(args[0]), len;
  char temp[8192], *s=buff;

  for(a=0;a < x;a++) {
    print_float(temp, i+a*j);
    len=strlen(temp);
    if(s-buff+len >= 8000)
      return;
    if(s != buff)
      *s++=' ';
    memcpy(s, temp, len+1);
    s+=len;
  }
}

FUNCTION(left) {
  int len=atoi(args[1]);

  if(len > 7999)
    strcpy(buff, args[0]);
  else if(len > 0)
    strmaxcpy(buff, args[0], len+1);
}

FUNCTION(right) {
  int len=atoi(args[1]);

  if(len > 0) {
    if((len=strlen(args[0])-len) < 0)
      len=0;
    strcpy(buff, args[0]+len);
  }
}

FUNCTION(ljust) {
  DELIM(fill, 2);
  int i=strlen(args[0]), len=atoi(args[1]);

  if(len < 0 || len > 8000) {
    strcpy(buff, ERR_RANGE);
    return;
  }

  /* if string is smaller than length, pad with fill character */
  if(len > i) {
    memcpy(buff, args[0], i);
    memset(buff+i, fill, len-i);
    buff[len]='\0';
  } else
    strmaxcpy(buff, args[0], len+1);
}

FUNCTION(rjust) {
  DELIM(fill, 2);
  int i=strlen(args[0]), len=atoi(args[1]);

  if(len < 0 || len > 8000) {
    strcpy(buff, ERR_RANGE);
    return;
  }

  if(len > i) {
    memset(buff, fill, len-i);
    strcpy(buff+(len-i), args[0]);
  } else
    strcpy(buff, args[0]+i-len);
}

FUNCTION(center) {
  DELIM(fill, 2);
  int len=atoi(args[1]);

  if(len < 0 || len > 8000) {
    strcpy(buff, ERR_RANGE);
    return;
  }

  strcpy(buff, center(args[0], len, fill));
}

FUNCTION(trim) {
  DELIM(trim_char, 2);
  char *str=args[0], *end;

  /* trim left side of string */
  if(nargs < 2 || tolower(*args[1]) != 'r')
    while(*str && *str == trim_char)
      str++;

  /* trim right side */
  end=strnul(str);
  if(nargs < 2 || tolower(*args[1]) != 'l')
    while(end > str && *(end-1) == trim_char)
      end--;

  /* copy into result buffer */
  memcpy(buff, str, end-str);
  buff[end-str]='\0';
}

FUNCTION(justify) {
  int count=0, len=atoi(args[1]), line=atoi(args[2]);
  char *r, *s=args[0];

  if(len < 1 || len > 8000) {
    strcpy(buff, ERR_RANGE);
    return;
  } if(line < 0) {
    strcpy(buff, "#-1 Bad line number.");
    return;
  }

  /* If line=0, count number of lines for <len> justification,
     otherwise print the appropriate line requested. */

  while((r=eval_justify(&s, len)))
    if(++count == line)
      break;
  if(!line)
    sprintf(buff, "%d", count);
  else
    strcpy(buff, r?:"");
}

FUNCTION(switch) {
  int i;

  for(i=1;i+1 < nargs;i+=2)
    if(wild_match(args[0], args[i], WILD_BOOLEAN)) {
      strcpy(buff, args[i+1]);
      return;
    }

  if(i < nargs)
    strcpy(buff, args[i]);
}

FUNCTION(repeat) {
  char *s=buff;
  int a=atoi(args[1]);

  if(a < 0) {
    strcpy(buff, ERR_RANGE);
    return;
  }

  if(!*args[0])
    return;

  while(--a >= 0 && (s-buff) < 8000)
    strmaxcpy(s+=strlen(s), args[0], 8001-(s-buff));
}

FUNCTION(foreach) {
  DELIM(delim, 2);
  DELIMCH(odelim, 3, delim);
  char *p=buff, *r, *s=args[0], *sptr, temp[8192];
  int len;

  if(!*args[0])
    return;

  /* Save environment %0 */
  len=strlen(env[0])+1;
  memcpy(sptr=alloca(len), env[0], len);

  /* Loop for all words in <arg1> */
  while(func_total <= MAX_FUNCTIONS && (r=parse_perfect(&s, delim))) {
    strcpy(env[0], r);
    pronoun_substitute(temp, privs, doer, args[1]);
    if(!*temp)
      continue;
    if(p != buff) {
      if(p-buff+strlen(temp) > 8000)
        break;
      *p++=odelim;
    }
    strcpy(p, temp);
    p+=strlen(p);
  }

  /* Restore %0 */
  memcpy(env[0], sptr, len);

  if(func_total > MAX_FUNCTIONS)
    strcpy(buff, "#-1 Too many functions.");
}

FUNCTION(oper) {
  DELIM(delim, 3);
  char *r, *s=args[0], *sptr[2];

  if(!*args[0]) {
    strcpy(buff, args[1]);
    return;
  }

  /* Save environment %0 and %1 */
  APTR(sptr[0], env[0]);
  APTR(sptr[1], env[1]);

  /* Set initial %1 */
  strcpy(env[1], args[1]);

  while(func_total < MAX_FUNCTIONS && (r=parse_perfect(&s, delim))) {
    strcpy(env[0], r);
    pronoun_substitute(buff, privs, doer, args[2]);
    strcpy(env[1], buff);
  }

  /* Restore %0 and %1 */
  strcpy(env[0], sptr[0]);
  strcpy(env[1], sptr[1]);

  if(func_total > MAX_FUNCTIONS)
    strcpy(buff, "#-1 Too many functions.");
}


/* Function flags */
#define F_EVAL	0x1	// Function arguments are automatically evaluated.
#define F_STRIP	0x2	// Strip ansi escape codes from arguments.

static struct funclist {
  char *name;
  void (*fun)();
  int flags:16;		// Function flags (listed above).
  unsigned int loarg:8;	// Minimum #args
  unsigned int hiarg:8;	// Maximum #args (100 = Unlimited)
} flist[]={
  {"ABS",	fun_abs,	0, 1},
  {"ACOS",	fun_acos,	0, 1},
  {"ADD",	fun_add,	0, 1,100},
  {"AGE",	fun_age,	0, 1},
  {"ANSI",	fun_ansi,	2, 2},
  {"APOSS",	fun_aposs,	0, 1},
  {"ART",	fun_art,	0, 1},
  {"ASCII",	fun_ascii,	0, 1},
  {"ASIN",	fun_asin,	0, 1},
  {"ATAN",	fun_atan,	0, 1},
  {"ATTROPTS",	fun_attropts,	0, 1,2},
  {"AVG",	fun_avg,	0, 1,100},
  {"BAND",	fun_band,	0, 2},
  {"BASE",	fun_base,	2, 3},
  {"BETWEEN",	fun_between,	0, 3},
  {"BITCOUNT",	fun_bitcount,	0, 1},
  {"BNAND",	fun_bnand,	0, 2},
  {"BOR",	fun_bor,	0, 2},
  {"BXOR",	fun_bxor,	0, 2},
  {"CAPSTR",	fun_capstr,	2, 1},
  {"CEIL",	fun_ceil,	0, 1},
  {"CENTER",	fun_center,	2, 2,3},
  {"CHILDREN",	fun_children,	0, 1},
  {"CHILDREN_R",fun_children_r,	0, 1},
  {"CHR",	fun_chr,	0, 1},
  {"CLASS",	fun_class,	0, 1},
  {"CMDS",	fun_cmds,	0, 1},
  {"CNAME",     fun_cname,      0, 1},
  {"COLS",	fun_cols,	0, 1},
  {"COMP",	fun_comp,	2, 2},
  {"CON",	fun_con,	0, 1},
  {"CONCID",	fun_concid,	0, 1},
  {"CONFIG",	fun_config,	0, 1},
  {"CONTROLS",	fun_controls,	0, 2,3},
  {"COS",	fun_cos,	0, 1},
  {"CPU",	fun_cpu,	0, 0},
  {"CPUTIME",	fun_cputime,	0, 0},
  {"CREATETIME",fun_createtime,	0, 1},
  {"CREDITS",	fun_pennies,	0, 1},
  {"CSI",	fun_csi,	0, 0,1},
  {"CTEXT",	fun_ctext,	2, 2},
  {"CWHO",	fun_cwho,	0, 1},
  {"DATE",	fun_date,	0, 0,1},
  {"DBTOP",	fun_dbtop,	0, 0},
  {"DEC",	fun_dec,	2, 1},
  {"DEGREES",	fun_degrees,	0, 2},
  {"DELETE",	fun_delete,	2, 3},
  {"DICE",	fun_dice,	0, 2},
  {"DIST2D",	fun_dist2d,	0, 4},
  {"DIST3D",	fun_dist3d,	0, 6},
  {"DIV",	fun_div,	0, 2},
  {"E",		fun_e,		0, 0},
  {"EDIT",	fun_edit,	2, 3},
  {"ENTRANCES",	fun_entrances,	0, 1},
  {"ESC",	fun_esc,	0, 0,1},
  {"EQ",	fun_eq,		0, 2},
  {"EXIT",	fun_exit,	0, 1},
  {"EXP",	fun_exp,	0, 1},
  {"EXTRACT",	fun_extract,	2, 3,4},
  {"FACTOR",	fun_factor,	0, 1},
  {"FADD",	fun_fadd,	0, 1,100},
  {"FDIV",	fun_fdiv,	0, 2},
  {"FFS",	fun_ffs,	0, 1},
  {"FIRST",	fun_first,	2, 1,2},
  {"FLAGS",	fun_flags,	0, 1},
  {"FLIP",	fun_flip,	2, 1},
  {"FLOOR",	fun_floor,	0, 1},
  {"FMUL",	fun_fmul,	0, 1,100},
  {"FOREACH",	fun_foreach,	2, 2,4},
  {"FSUB",	fun_fsub,	0, 2},
  {"FULLNAME",	fun_fullname,	0, 1},
  {"GAMEAGE",	fun_gameage,	0, 0},
  {"GENDER",	fun_gender,	0, 1},
  {"GET",	fun_get,	0, 1,2},
  {"GT",	fun_gt,		0, 2},
  {"GTEQ",	fun_gteq,	0, 2},
  {"HASATTR",	fun_hasattr,	0, 2},
  {"HASFLAG",	fun_hasflag,	0, 2,3},
  {"HASPOW",	fun_haspow,	0, 2},
  {"HAS_A",	fun_has_a,	0, 2},
  {"HOME",	fun_link,	0, 1},
  {"HOST",	fun_host,	0, 1},
  {"IDLE",	fun_idle,	0, 1},
  {"IF",	fun_if,		0, 2},
  {"IFELSE",	fun_ifelse,	0, 3},
  {"IFTRUE",	fun_iftrue,	0, 2},
  {"IMMORTAL",	fun_immortal,	0, 1},
  {"INC",	fun_inc,	2, 1},
  {"INPUT",	fun_input,	0, 1},
  {"INSERT",	fun_insert,	2, 3,4},
  {"INT",	fun_int,	0, 1},
  {"INZONE",	fun_inzone,	0, 1},
  {"IPADDR",	fun_ipaddr,	0, 1},
  {"ISATTR",	fun_isattr,	0, 2},
  {"ISDBREF",	fun_isdbref,	0, 1},
  {"ISNUM",	fun_isnum,	0, 1},
  {"ISWORD",	fun_isword,	0, 1},
  {"IS_A",	fun_is_a,	0, 2},
  {"JUSTIFY",	fun_justify,	2, 3},
  {"LAND",	fun_land,	0, 2,100},
  {"LAST",	fun_last,	2, 1,2},
  {"LASTON",	fun_laston,	0, 1},
  {"LASTOFF",	fun_lastoff,	0, 1},
  {"LATTR",	fun_lattr,	0, 1},
  {"LATTRDEF",	fun_lattrdef,	0, 1},
  {"LCON",	fun_lcon,	0, 1,2},
  {"LCSTR",	fun_lcstr,	2, 1},
  {"LEFT",	fun_left,	2, 2},
  {"LEXITS",	fun_lexits,	0, 1,2},
  {"LINK",	fun_link,	0, 1},
  {"LINKUP",	fun_linkup,	0, 1},
  {"LJUST",	fun_ljust,	2, 2,3},
  {"LN",	fun_ln,		0, 1},
  {"LNUM",	fun_lnum,	0, 1,3},
  {"LOC",	fun_loc,	0, 1},
  {"LOCALIZE",	fun_localize,	0, 1},
  {"LOCALIZE_AS",fun_localize_as,0,3},
  {"LOG",	fun_log,	0, 1,2},
  {"LOR",	fun_lor,	0, 2,100},
  {"LPOS",	fun_lpos,	2, 2},
  {"LSEMA",	fun_lsema,	0, 1},
  {"LT",	fun_lt,		0, 2},
  {"LTEQ",	fun_lteq,	0, 2},
  {"LWHO",	fun_lwho,	0, 0},
  {"LXOR",	fun_lxor,	0, 2},
  {"MATCH",	fun_match,	2, 2},
  {"MATCHALL",	fun_matchall,	2, 2,3},
  {"MAX",	fun_max,	0, 1,100},
  {"MID",	fun_mid,	2, 3},
  {"MIN",	fun_min,	0, 1,100},
  {"MOD",	fun_mod,	0, 2},
  {"MODTIME",	fun_modtime,	0, 1},
  {"MONEY",	fun_pennies,	0, 1},
  {"MUDNAME",	fun_mudname,	0, 0},
  {"MUL",	fun_mul,	0, 1,100},
  {"NAME",	fun_name,	0, 1},
  {"NCOMP",	fun_ncomp,	0, 2},
  {"NEARBY",	fun_nearby,	0, 2},
  {"NEAREST",	fun_nearest,	0, 2,3},
  {"NEG",	fun_neg,	0, 1},
  {"NEQ",	fun_neq,	0, 2},
  {"NETSTAT",	fun_netstat,	0, 1},
  {"NEXT",	fun_next,	0, 1},
  {"NOT",	fun_not,	0, 1},
  {"NUM",	fun_num,	0, 1},
  {"OBJLIST",	fun_objlist,	0, 1},
  {"OBJMEM",	fun_objmem,	0, 1},
  {"OBJN",	fun_objn,	0, 1},
  {"OEXIT",	fun_oexit,	0, 1},
  {"ONFOR",	fun_onfor,	0, 1},
  {"OPER",	fun_oper,	2, 3,4},
  {"OUTPUT",	fun_output,	0, 1},
  {"OWNER",	fun_owner,	0, 1,2},
  {"PARENTS",	fun_parents,	0, 1},
  {"PARENTS_R",	fun_parents_r,	0, 1},
  {"PARSE_LOCK",fun_parse_lock,	0, 3},
  {"PATH",	fun_path,	0, 3},
  {"PENNIES",	fun_pennies,	0, 1},
  {"PI",	fun_pi,		0, 0},
  {"PLANE",	fun_plane,	0, 1},
  {"PLAYMEM",	fun_playmem,	0, 1},
  {"PLCON",	fun_plcon,	0, 1,2},
  {"PLURALIZE",	fun_pluralize,	0, 1},
  {"PNUM",	fun_pnum,	0, 1},
  {"PORT",	fun_port,	0, 1},
  {"POS",	fun_pos,	2, 2},
  {"POSS",	fun_poss,	0, 1},
  {"POW",	fun_pow,	0, 2},
  {"POWERS",	fun_powers,	0, 1},
  {"RAND",	fun_rand,	0, 1},
  {"RANDEXIT",	fun_randexit,	0, 1},
  {"RANDWORD",	fun_randword,	2, 1,2},
  {"RANGE",	fun_range,	0, 3},
  {"RANK",	fun_rank,	0, 1},
  {"RDIV",	fun_rdiv,	0, 2},
  {"REGMATCH",	fun_regmatch,	2, 2,3},
  {"REMOVE",	fun_remove,	2, 3,4},
  {"REPEAT",	fun_repeat,	0, 2},
  {"REPLACE",	fun_replace,	2, 3,4},
  {"REST",	fun_rest,	2, 1,2},
  {"RESTARTS",	fun_restarts,	0, 0},
  {"RIGHT",	fun_right,	2, 2},
  {"RJUST",	fun_rjust,	2, 2,3},
  {"RLOC",	fun_rloc,	0, 2},
  {"RMATCH",	fun_rmatch,	0, 2},
  {"ROOM",	fun_room,	0, 1},
  {"ROUND",	fun_round,	0, 1,2},
  {"ROWS",	fun_rows,	0, 1},
  {"RTIME",	fun_rtime,	0, 0,1},
  {"S",		fun_s,		0, 1},
  {"SCRAMBLE",	fun_scramble,	2, 1},
  {"SECURE",	fun_secure,	2, 1},
  {"SEES",	fun_sees,	0, 2},
  {"SESSIONS",	fun_sessions,	0, 1},
  {"SETDIFF",	fun_setdiff,	2, 2,4},
  {"SETINTER",	fun_setinter,	2, 2,4},
  {"SETUNION",	fun_setunion,	2, 2,4},
  {"SETQ",	fun_setq,	0, 2},
  {"SETR",	fun_setr,	0, 2},
  {"SHL",	fun_shl,	0, 2},
  {"SHR",	fun_shr,	0, 2},
  {"SHUFFLE",	fun_shuffle,	2, 1,3},
  {"SIGN",	fun_sign,	0, 1},
  {"SIN",	fun_sin,	0, 1},
  {"SORT",	fun_sort,	2, 1,4},
  {"SPC",	fun_spc,	0, 1},
  {"SQRT",	fun_sqrt,	0, 1},
  {"SRAND",	fun_srand,	0, 3,4},
  {"STATS",	fun_stats,	0, 0,1},
  {"STDDEV",	fun_stddev,	0, 1,100},
  {"STEPS",	fun_steps,	0, 1},
  {"STRCAT",	fun_strcat,	0, 2,100},
  {"STRIP",	fun_strip,	2, 1},
  {"STRLEN",	fun_strlen,	2, 1},
  {"SUB",	fun_sub,	0, 2},
  {"SUBJ",	fun_subj,	0, 1},
  {"SWAP",	fun_swap,	2, 3,4},
  {"SWITCH",	fun_switch,	0, 2,100},
  {"S_AS",	fun_s_as,	0, 3},
  {"S_AS_WITH",	fun_s_as_with,	0, 3,13},
  {"TAN",	fun_tan,	0, 1},
  {"TERM",	fun_term,	0, 2},
  {"TIME",	fun_time,	0, 0,1},
  {"TIMEFMT",	fun_timefmt,	0, 1,2},
  {"TIMEOFDAY",	fun_timeofday,	0, 0},
  {"TMA",	fun_tma,	0, 1},
  {"TMF",	fun_tmf,	0, 1},
  {"TML",	fun_tml,	0, 1},
  {"TMS",	fun_tms,	0, 1},
  {"TRIM",	fun_trim,	2, 1,3},
  {"TRUTH",	fun_truth,	0, 1},
  {"TYPE",	fun_type,	0, 1},
  {"UCSTR",	fun_ucstr,	2, 1},
  {"UNPARSE",	fun_unparse,	0, 1},
  {"UPFRONT",	fun_upfront,	0, 0},
  {"UPTIME",	fun_uptime,	0, 0},
  {"V",		fun_v,		0, 1},
  {"VALID",	fun_valid,	0, 2},
  {"VERSION",	fun_version,	0, 0},
  {"WAITTIME",	fun_waittime,	0, 2},
  {"WCOUNT",	fun_wcount,	2, 1,2},
  {"WDATE",	fun_wdate,	0, 0,1},
  {"WMATCH",	fun_wmatch,	2, 2,3},
  {"WORDFLIP",	fun_wordflip,	2, 1,2},
  {"WTIME",	fun_wtime,	0, 0,1},
  {"XTIME",	fun_xtime,	0, 0,1},
  {"ZONE",	fun_zone,	0, 1},
  {"ZONEATTR",	fun_zoneattr,	0, 1,2},
  {"ZWHO",	fun_zwho,	0, 1},

};

void list_functions(dbref player)
{
  char buf[512];
  int a, col=get_cols(NULL, player);

  notify(player, "List of hardcoded functions in alphabetical order:");
  notify(player, "%s", "");
  strcpy(buf, "");
  for(a=0;a < NELEM(flist);a++) {
    if(*buf && strlen(buf)+strlen(flist[a].name) >= col-2) {
      notify(player, "%s", buf);
      strcpy(buf, "");
    }
    if(*buf)
      strcat(buf, ", ");
    strcat(buf, flist[a].name);
  }
  if(*buf)
    notify(player, "%s", buf);
  notify(player, "%s", "");
}

// Search for a user-defined function.
//
static int user_defined(char **str, char *buff, dbref privs, dbref doer)
{
  ATTR *attr;
  char temp[8192], *saveptr[10], *writeptr[10], *s=buff;
  dbref thing=NOTHING, *ptr;
  int a, b;

  if(*s == '!')
    s++;

  /* Check for function defined on a specific object.  e.g. #123.func() */
  if(*s == '#') {
    thing=atoi(s+1);
    if(Invalid(thing) || !(s=strchr(s, '.')))
      return 0;
    if(!(attr=def_atr_str(thing, s+1)) || !(attr->flags & AF_FUNC) ||
       !can_see_atr(privs, thing, attr))
      return 0;
  } else if((attr=atr_str(privs, s)) && (attr->flags & AF_FUNC)) {
    thing=privs;
  } else {
    /* Scan zones for matching function */
    for(ptr=Zone(privs);ptr && *ptr != NOTHING;ptr++)
      if((attr=atr_str(*ptr, s)) && (attr->flags & AF_FUNC)) {
        thing=*ptr;
        break;
      }

    /* If still nothing, try universal zones */
    if(thing == NOTHING)
      for(ptr=uzo;ptr && *ptr != NOTHING;ptr++)
        if((attr=atr_str(*ptr, s)) && (attr->flags & AF_FUNC)) {
          thing=*ptr;
          break;
        }
  }

  /* Nothing found. */
  if(thing == NOTHING)
    return 0;

  /* Get the 10 arguments to the function */
  for(a=0;**str;a++) {
    if(a >= 10) {  /* Skip over remaining arguments */
      while(**str && **str != ')' && **str != ']')
        (*str)++;
      break;
    }
    if(**str == ')' || **str == ']') {
      APTR(writeptr[a], "");
      continue;
    }
    evaluate(str, buff, privs, doer, 1);
    APTR(writeptr[a], buff);
    if(**str == ',')
      (*str)++;
  }
  for(b=0;b<10;b++) {
    APTR(saveptr[b], env[b]);
    strcpy(env[b], b<a?writeptr[b]:"");
  }

  if(**str)
    (*str)++;
  strcpy(temp, atr_get_obj(thing, attr->obj, attr->num));
  pronoun_substitute(buff, (attr->flags & AF_PRIVS)?attr->obj:privs,doer,temp);

  for(b=0;b<10;b++)
    strcpy(env[b], saveptr[b]);
  return 1;
}


/* Exported functions */

int function(char **str, char *buff, dbref privs, dbref doer)
{
  static hash *funchash;
  struct funclist *fp;
  char **args=NULL, *s;
  int argc=0, inv=0, old_quiet;

  /* function names cannot have spaces or symbols in them */
  if(!ok_attribute_name(buff))
    return 0;

  if(++func_recur > FUNC_RECURSION) {
    strcpy(buff, "#-1 Too many levels of recursion.");
    func_recur--;
    return 1;
  } if(++func_total > MAX_FUNCTIONS) {
    strcpy(buff, "#-1 Too many functions.");
    func_recur--;
    return 1;
  }

  /* determine the function name we wish to find */
  s=buff;
  if(*s == '!') {
    inv=1;
    s++;
  }
  if(!funchash)
    funchash=make_hashtab(flist);
  fp=lookup_hash(funchash, s);

  /* Not a built-in function */
  if(!fp) {
    if(user_defined(str, buff, privs, doer)) {  // User-defined function found
      if(inv)
        sprintf(buff, "%d", !ISTRUE(buff));
      func_recur--;
      return 1;
    }
    func_recur--;
    return 0;
  }

  /* get the arguments to the function */
  if(**str != ')' && **str != ']')
    while(**str) {
      evaluate(str, buff, privs, doer, 1);

      if(!(argc%10))
        args=realloc(args, (argc+10)*sizeof(char *));  /* resize arg table */
      if(!args) {
        perror("argument allocation");
        argc=0;
        break;
      }

      APTR(args[argc], buff);

      /* strip ansi escape codes from argument */
      if(fp->flags & F_STRIP)
        strip_ansi(args[argc]);

      /* check argument delimiters or end of function */
      argc++;
      if(**str == ',')
        (*str)++;
      else if(**str == ')' || **str == ']')
        break;
    }

  if(**str)
    (*str)++;

  if(argc < fp->loarg || (fp->hiarg < 100 && argc > (fp->hiarg?:fp->loarg))) {
    s=buff+sprintf(buff, "#-1 Function %s requires %s", fp->name,
                   fp->hiarg >= 100?"at least ":"");
    if(!fp->hiarg || fp->hiarg >= 100)
      sprintf(s, "%d argument%s.", fp->loarg, fp->loarg==1?"":"s");
    else if(fp->loarg == fp->hiarg-1)
      sprintf(s, "%d or %d arguments.", fp->loarg, fp->hiarg);
    else
      sprintf(s, "between %d and %d arguments.", fp->loarg, fp->hiarg);
  } else {
    old_quiet=match_quiet;
    match_quiet=1;
    *buff='\0';
    fp->fun(buff, args, privs, doer, argc);
    match_quiet=old_quiet;
    if(inv)
      sprintf(buff, "%d", !ISTRUE(buff));
  }

  free(args);
  func_recur--;
  return 1;
}

// Evaluates a function and returns its result.
//
// type=Delimiter character to end subcommand processing.
// type=1: Special; called from above to parse individual function arguments.
//
void evaluate(char **str, char *buff, dbref privs, dbref doer, char type)
{
  char *s=*str, *e=buff, *end=buff, temp[8192];
  int depth, func=0;

  /* Skip preceding spaces */
  while(*s && *s == ' ')
    s++;

  while(*s) {
    if(*s == '{') {
      /* If this is first character, eliminate surrounding braces */
      if(e == buff) {
        char *u=buff, *t=s+1;

        depth=1;
        while(*t) {
          if(*t == '{')
            depth++;
          else if(*t == '}') {
            if(!--depth) {
              t++;
              break;
            }
          }
          *u++=*t++;
        }
        while(isspace(*t))
          t++;
        if(!*t || (type == 1 && (*t == ']' || *t == ')' || *t == ',')) ||
           (type != 1 && (*t == type))) {
          e=end=u;
          s=t;
          continue;
        }
      }

      /* Else, copy all characters until the last } is reached */
      *e++=*s++;
      depth=1;
      while(depth && *s) {
        if(*s == '{')
          depth++;
        else if(*s == '}')
          depth--;
        *e++=*s++;
      }
      continue;
    }

    if(*s == '\e' && *(s+1) == '[') {  /* Copy single [ symbol after escape */
      *e++=*s++;
      *e++=*s++;
      continue;
    }
      
    if(*s == '[' && type == 1) {  /* Skip over nested function calls */
      *e++=*s++;
      depth=1;
      while(depth && *s) {
        if(*s == '\e' && *(s+1) == '[')
          *e++=*s++;
        else if(*s == '[')
          depth++;
        else if(*s == ']')
          depth--;
        *e++=*s++;
      }
      continue;
    }

    if(*s == '(') {  /* Function Header */
      if(!func) {
        *e='\0';
        *str=s+1;

        if(function(str, buff, privs, doer)) {
          e=end=temp;  /* all future writes will be ignored */
          s=*str;
          func=2;
          continue;
        }
        func=1;
      }

      /* Copy text in parentheses literally */
      *e++=*s++;
      depth=1;
      while(depth && *s) {
        if(*s == '(')
          depth++;
        else if(*s == ')')
          depth--;
        *e++=*s++;
      }
      continue;
    }

    /* Check end of function scope */
    if(type == 1 && (*s == ']' || *s == ')' || *s == ','))
      break;

    /* Check delimiter in command-parsing */
    if(type != 1 && *s == type) {
      s++;
      break;
    }

    if(*s)
      *e++=*s++;
  }

  /* Remove following spaces (only if there wasn't an ending brace) */
  if(func != 2)
    while(e > end && *(e-1) == ' ')
      e--;

  *e='\0';
  *str=s;
}
