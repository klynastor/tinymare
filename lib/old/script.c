/* The MARE $program code is shown here in full. It is extremely buggy and is
   urged to be used, if at all, under restricted POW_SECURITY permissions.
   -- Copyright 1995, by Byron Stanoszek */


// Print Program Queue

  if(string_prefix("program", arg1) || !strcmp(arg1, "") ||
     !strcasecmp(arg1, "all")) {
    count=0;
    for(ptmp=qprog;ptmp;ptmp=ptmp->next)
    if (!Invalid(ptmp->uid) && (db[ptmp->uid].owner == db[player].owner ||
        power(player, POW_QUEUE)) && (thing == db[ptmp->uid].owner ||
        thing == ptmp->uid || thing == NOTHING)) {
      sprintf(buf,"%s:%s:%d:%s:%d:%d:%d:", unparse_object(player, ptmp->uid),
              ptmp->program, ptmp->proglin[ptmp->lnum-1].line, ptmp->status,
              ptmp->priority, ptmp->pid, ptmp->ppid);
      nthing=ptmp->uid;
      sprintf(buff, "%02d.", ptmp->cpu_time/60);
      if(ptmp->cpu_time%60 < 0)
        strcat(buff, "00");
      else
        strcat(buff, tprintf("%02d", ptmp->cpu_time%60));
      strcat(buf, tprintf("%d:%s  %s", ptmp->alarm_ticks, buff,
             progheader(ptmp->uid, ptmp->program)));
      if(!count)
        notify(player, "\e[1mProgram commands:\e[m");
      notify(player, buf);
      ++count; ++tcount;
    }
    if(count > 0)
      notify(player, tprintf("-- Total: %d Program --", count));
  }


// In cque.c, halt_object():

  /* remove program pid#s */
  for(ptmp=qprog;ptmp;ptmp=ptmp=pnext) {
    pnext=ptmp->next;
    if(ptmp->pid != 1 && (ptmp->uid == thing || db[ptmp->uid].owner == thing ||
        thing == AMBIGUOUS))
      remove_pid(ptmp->pid);
  }

// In ctrl.c, do_decompile():

  for(cur=db[thing].program;cur;cur=cur->next) {
    var=controls(player, thing, POW_MODIFY);
    if((var && !(cur->perms & PERM_UR)) || (!var && !(cur->perms & PERM_OR)))
      continue;
    notify(player, tprintf("$open %s:%s%s%s%s%s", objname, cur->name,
           (*cur->cmdstr || *cur->header)?"=":"", cur->cmdstr,
           (*cur->header)?":":"", cur->header));
    if(*cur->lock)
      notify(player, tprintf("$lock %s:%s=%s", objname, cur->name, cur->lock));
    notify(player, tprintf("$chmod %s:%s=%s", objname, cur->name,
           unparse_perms(cur->perms)));
    for(dts=cur->line;dts;dts=dts->next)
      notify(player, tprintf("$append %s:%s=%s", objname, cur->name,
             uncompress(dts->line)));
  }

// From db/load.c and db/save.c:

void write_program(f, ptr)
FILE *f;
struct progcon *ptr;
{
  struct progcon *cur;
  struct progdata *dts;

  for(cur=ptr;cur;cur=cur->next) {
    if(cur->perms & PERM_TMP)
      continue;
    putstring(f, cur->name);
    putstring(f, cur->cmdstr);
    putstring(f, cur->header);
    putstring(f, cur->lock);
    putnum(f, cur->perms, 16);
    putnum(f, cur->maxline, 16);
    for(dts=cur->line;dts;dts=dts->next)
      putstring(f, dts->line);
  }
  fputc(0, f);
}

/* loads compressed programs from db file */
int read_program(f, thing)
FILE *f;
dbref thing;
{
  struct progcon *curr, *new;
  struct progdata *cur2=NULL;
  char *s;
  int a;

  db[thing].program=NULL;
  while(1) {
    s=getstring_noalloc(f);
    if(!*s) /* blank line = no more programs to read */
      return 1;
    new=malloc(sizeof(struct progcon)); /* allocate the program data */
    curr=db[thing].program;
    if(!db[thing].program)
      db[thing].program=new;
    else while(curr) {
      if(!curr->next) {
	curr->next=new;
	break;
      }
      curr=curr->next;
    }

    /* set program data */
    new->next = NULL;
    SPTR(new->name, s);
    getstring(f, new->cmdstr);
    getstring(f, new->header);
    getstring(f, new->lock);
    new->perms = getnum(f, 16);
    new->maxline = getnum(f, 16);
    new->line = NULL;

    /* set data lines within */
    for(a=0;a<new->maxline;a++) {
      if(!a) {
        new->line=malloc(sizeof(struct progdata));
        getstring(f, new->line->line);
        new->line->next = NULL;
	cur2=new->line;
      } else {
	cur2->next=malloc(sizeof(struct progdata));
        getstring(f, cur2->next->line);
	cur2->next->next = NULL;
	cur2=cur2->next;
      }
    }
  }
}


// From game/predicates.c, objmem_usage():

  struct progcon *con;
  struct progdata *dts;

  /* program memory */
  for(con=db[thing].program;con;con=con->next) {
    k += sizeof(struct progcon)+4;
    k += strlen(con->name)+4+1;
    k += strlen(con->cmdstr)+4+1;
    k += strlen(con->header)+4+1;
    k += strlen(con->lock)+4+1;
    for(dts=con->line;dts;dts=dts->next) {
      k += sizeof(struct progdata)+4;
      k += strlen(dts->line)+4+1;
    }
  }



// Beginning of code.

/* prog/script.c */
/* commands that deal with the editing of programs */

#include "externs.h"

int skiperrstr=0;

/* scan all programs */
struct all_prog_list *all_programs_internal(thing, myop, dep)
dbref thing;
struct all_prog_list *myop;
int dep;
{
  struct progcon *con;
  dbref *ptr;

  for (con=db[thing].program;con;con=con->next)
    if (!dep || con->perms & PERM_INH) {
      if (!myop) {
	myop = malloc(sizeof(struct all_prog_list));
	myop->next = NULL;
      } else {
	struct all_prog_list *foo;
	foo = malloc(sizeof(struct all_prog_list));
	foo->next = myop;
	myop = foo;
      }
      myop->type = con;
      myop->owner = thing;
    }

  for(ptr=db[thing].parents;ptr && *ptr != NOTHING;ptr++)
    myop = all_programs_internal(*ptr, myop, dep+1);
  return myop;
}

struct all_prog_list *all_programs(thing)
dbref thing;
{
  return all_programs_internal(thing, NULL, 0);
}

/* display program list in unix-style format */
int list_programs(player, thing)
dbref player, thing;
{
  struct all_prog_list *con, *cnext;
  struct progdata *dts;
  char buf[1050];
  int c=0, size;

  for(con=all_programs(thing);con;con=cnext) {
    cnext=con->next;
    if(!c)
      notify(player, "\e[1mPrograms:\e[m");
    sprintf(buf, "#%d:%s", con->owner, con->type->name);

    /* calculate #bytes used */
    size=sizeof(struct progcon);
    size += strlen(con->type->name)+1;
    size += strlen(con->type->cmdstr)+1;
    size += strlen(con->type->header)+1;
    size += strlen(con->type->lock)+1;
    for(dts=con->type->line;dts;dts=dts->next) {
      size += sizeof(struct progdata);
      size += strlen(dts->line)+1;
    }

    notify(player, tprintf(" %-20.20s \e[1m%-15.15s\e[m %s%9d %s", buf,
	   con->type->cmdstr, unparse_perms(con->type->perms), size,
	   con->type->header));
    c++;
    free(con);
  }
  return c;
}

/* scan an object to find program matching name */
int progfind(prog, name)
struct all_prog_list *prog;
char *name;
{
  struct all_prog_list *con, *cnext;
  int c=0, thing=prog->owner;

  for(con=all_programs(thing);con;con=cnext) {
    cnext=con->next;
    if(!c && !strcasecmp(con->type->name, name)) {
      prog->type=con->type;
      prog->owner=con->owner;
      c=1;
    }
    free(con);
  }
  return c;
}

struct all_prog_list *exist_new_program(player, arg, program)
dbref player;
char *arg, *program;
{
  static struct all_prog_list *rslt=NULL;
  char argument[8192];
  char *r, *s;

  /* initialize ptr */
  if(!rslt)
    rslt=malloc(sizeof(struct all_prog_list));

  /* initialize settings */
  rslt->line=0;
  rslt->type=NULL;
  rslt->thing=NOTHING;
  *program=0;

  /* grab object number */
  if(!*arg) {
    notify(player, "Object name missing.");
    return NULL;
  }
  strcpy(argument, arg);
  s=argument;
  r=parse_up(&s,':');
  rslt->owner=match_thing(player, r, MATCH_EVERYTHING);
  rslt->thing=rslt->owner;
  if(rslt->owner == NOTHING)
    return NULL;

  if(!*s) {
    notify(player, "Program name missing.");
    return NULL;
  }
  r=parse_up(&s,',');
  if(!ok_attribute_name(r)) {
    notify(player, "Illegal program name.");
    return NULL;
  }
  if(!progfind(rslt, r)) {
    strncpy(program, r, 511);
    program[511]='\0';
    return rslt;
  }
  notify(player, tprintf("Program \"\e[1m%s\e[m\" already exists.", r));
  return NULL;
}

struct all_prog_list *unparse_programcmd(player, arg)
dbref player;
char *arg;
{
  static struct all_prog_list *rslt=NULL;
  char argument[8192];
  char *r, *s;

  /* initialize ptr */
  if(!rslt)
    rslt=malloc(sizeof(struct all_prog_list));

  /* initialize settings */
  rslt->line=0;
  rslt->type=NULL;
  rslt->thing=NOTHING;

  /* grab object number */
  if(!*arg)
    strcpy(argument, "me");
  else
    strcpy(argument, arg);
  s=argument;
  r=parse_up(&s,':');
  rslt->owner=match_thing(player, r, MATCH_EVERYTHING);
  rslt->thing=rslt->owner;
  if(rslt->owner == NOTHING)
    return NULL;

  if(!*s)
    return rslt;

  /* find desired program */
  r=parse_up(&s,',');
  if(!ok_attribute_name(r)) {
    notify(player, "Illegal program name.");
    return NULL;
  }
  if(!progfind(rslt, r)) {
    notify(player, tprintf("No such program \"\e[1m%s\e[m\" on %s", r,
           unparse_object(player, rslt->owner)));
    return NULL;
  }

  /* parse line number */
  rslt->line=atoi(s);

  return rslt;
}

void do_progcmd_list(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con;
  struct progdata *dts;
  char linestr[50];
  char buf[50];
  int a, b, size, permloc=1, minline=0, maxline=0, curline;
  int count=0;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    if(!list_programs(player, rslt->owner))
      notify(player, "No programs found.");
    return;
  }

  notify(player, tprintf("#%d:%s \"\e[1m%s\e[m\"  %s", rslt->owner, con->name,
	 con->cmdstr, con->header));

  /* show program headers */
  if(controls(player, rslt->owner, POW_MODIFY))
    permloc=PERM_UR;
  else
    permloc=PERM_OR;
  if(!strcmp(arg2, "") || !(con->perms & permloc) || !strcmp(arg2, "0")) {
    notify(player,tprintf("\e[1mOwner:\e[m%s", unparse_object(player,
	   db[rslt->owner].owner)));
    notify(player,tprintf("\e[1mPerms:\e[m%s", unparse_perms(con->perms)));
    notify(player,tprintf("\e[1m Lock:\e[m%s", unparse_dbref(player,
           con->lock, 0)));
    size=sizeof(struct progcon);
    size += strlen(con->name)+1;
    size += strlen(con->cmdstr)+1;
    size += strlen(con->header)+1;
    size += strlen(con->lock)+1;
    for(dts=con->line;dts;dts=dts->next) {
      size += sizeof(struct progdata);
      size += strlen(dts->line)+1;
    }
    notify(player,tprintf("\e[1mBytes:\e[m%d", size));
    if (!(con->perms & permloc) || *arg2 == '0')
      return;
    notify(player, "\e[1m-------\e[m");
  }

  /* calculate range */
  if(strcmp(arg2, "")) {
    strncpy(linestr, arg2, 40);
    linestr[40]='\0';
    strcpy(buf, "");
    b=0;
    for(a=0;a<strlen(linestr);a++) {
      if(linestr[a] == '-') {
	buf[b]='\0';
	minline=atoi(buf);
	maxline=atoi(linestr+a+1);
	break;
      }
      buf[b] = linestr[a];
      if(a == strlen(linestr)-1) {
	buf[b+1]='\0';
	minline=atoi(buf);
        maxline=atoi(buf);
	break;
      }
      b++;
    }
  }
  if(minline <= 0)
    minline=1;
  if(maxline <= 0)
    maxline=0;
  if(maxline && maxline < minline) {
    notify(player, tprintf("Line mismatch: %s", arg2));
    return;
  }

  /* display program data according to range */
  curline=1;
  for(dts=con->line;dts;dts=dts->next,curline++) {
    if(curline < minline)
      continue;
    if(maxline && curline > maxline)
      break;
    notify(player, tprintf("\e[1m%5d:\e[m%s", curline, uncompress(dts->line)));
    ++count;
  }

  /* show results */
  if(!count) {
    if(!maxline)
      notify(player, "No lines in program.");
    else
      notify(player, "No lines match.");
  }
}

void do_progcmd_cmdstr(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  }

  /* check permissions */
  if(strchr(arg2, ';') || strchr(arg2, '{') || strchr(arg2, '}')) {
    notify(player, "That is not a good name for a command string.");
    return;
  }
  if(!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* set command string */
  WPTR(con->cmdstr, arg2);
  notify(player, tprintf("#%d:%s - Set.", rslt->owner, con->name));
}

void do_progcmd_comment(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  }

  /* check permissions */
  if(!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* set program header */
  WPTR(con->header, arg2);
  notify(player, tprintf("#%d:%s - Set.", rslt->owner, con->name));
}

void do_progcmd_lock(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  }

  /* check permissions */
  if(!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* set lock */
  WPTR(con->lock, process_lock(player, arg2));
  notify(player, tprintf("#%d:%s - %s.", rslt->owner, con->name,
         *con->lock?"Locked":"Unlocked"));
}

void do_progcmd_chmod(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con;
  char pmode[10];
  char perms[10];
  int a;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  }
  strncpy(pmode, arg2, 9);
  pmode[9]='\0';

  if(!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }
  strcpy(perms, "---------");
  if (!strcmp(pmode,"")) {
    strcpy(perms, unparse_perms(con->perms));
    notify(player, tprintf("\e[1m#%d:%s->\e[m %s",rslt->owner,con->name,perms));
    return;
  }
  if (isdigit(pmode[0])) {
    if(strlen(pmode) != 3) {
      notify(player, "Unknown mode.");
      return;
    }
    for(a=0;a<3;a++) {
      if(pmode[a] > '7') {
	notify(player, tprintf("Unknown mode character: %c", pmode[a]));
	return;
      }
    }
    if((pmode[0]-48) & 4) perms[0] = 't';
    if((pmode[0]-48) & 2) perms[1] = 's';
    if((pmode[0]-48) & 1) perms[2] = 'i';
    if((pmode[1]-48) & 4) perms[3] = 'r';
    if((pmode[1]-48) & 2) perms[4] = 'w';
    if((pmode[1]-48) & 1) perms[5] = 'x';
    if((pmode[2]-48) & 4) perms[6] = 'r';
    if((pmode[2]-48) & 2) perms[7] = 'w';
    if((pmode[2]-48) & 1) perms[8] = 'x';
  } else {
    if(strlen(pmode) != 9) {
      notify(player, "Unknown mode.");
      return;
    }
    for(a=0;a<9;a++) {
      if(pmode[a] != '-' && ((!a && pmode[a] != 't') ||
         (a == 1 && pmode[a] != 's') ||
	 (a == 2 && pmode[a] != 'i') ||
	 (a>2 && a%3 == 0 && pmode[a] != 'r') ||
	 (a>2 && a%3 == 1 && pmode[a] != 'w') ||
	 (a>2 && a%3 == 2 && pmode[a] != 'x'))) {
	notify(player, tprintf("Unknown mode character: %c", pmode[a]));
	return;
      }
      perms[a] = pmode[a];
    }
  }

  con->perms = 0;
  if(perms[0] == 't') con->perms |= PERM_TMP;
  if(perms[1] == 's') con->perms |= PERM_STK;
  if(perms[2] == 'i') con->perms |= PERM_INH;
  if(perms[3] == 'r') con->perms |= PERM_UR;
  if(perms[4] == 'w') con->perms |= PERM_UW;
  if(perms[5] == 'x') con->perms |= PERM_UX;
  if(perms[6] == 'r') con->perms |= PERM_OR;
  if(perms[7] == 'w') con->perms |= PERM_OW;
  if(perms[8] == 'x') con->perms |= PERM_OX;
  notify(player, tprintf("#%d:%s - Mode changed to \e[1m%s\e[m", rslt->owner,
         con->name, perms));
}

void do_progcmd_open(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con, *dcon;
  char program[512];
  char cmdstr[512];
  char *r, *s;

  /* make sure program name matches nothing */
  if(!(rslt=exist_new_program(player, arg1, program)))
    return;

  /* check permissions */
  if(!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  strncpy(cmdstr, arg2, 511);
  cmdstr[511]='\0';
  s=cmdstr;
  r=parse_up(&s,':');

  if(r && (strchr(r, ';') || strchr(r, '{') || strchr(r, '}'))) {
    notify(player, "That is not a good name for a command string.");
    return;
  }
  con=malloc(sizeof(struct progcon));
  dcon=db[rslt->owner].program;
  if(!dcon)
    db[rslt->owner].program = con;
  else while(dcon) {
    if(!dcon->next) {
      dcon->next=con;
      break;
    }
    dcon=dcon->next;
  }

  con->next=NULL;
  con->line=NULL;
  SPTR(con->name, program);
  SPTR(con->cmdstr, r?r:"");
  SPTR(con->header, s?s:"");
  SPTR(con->lock, "");
  con->perms=PERM_UR|PERM_UW|PERM_UX|PERM_OR|PERM_OX;
  con->maxline=0;
  notify(player, tprintf("#%d:%s - Opened.", rslt->owner, program));
}

void do_progcmd_replace(player, arg1, arg2)
dbref player;
char *arg1, *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con;
  struct progdata *dts;
  int b=1;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  } else if(rslt->line <= 0) {
    notify(player, "Line number must be positive.");
    return;
  }

  /* check permissions */
  if (!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }
  for(dts=con->line;dts;dts=dts->next,b++) {
    if(b == rslt->line) {
      WPTR(dts->line, compress(arg2));
      notify(player, tprintf("Line \e[1m%d\e[m of program #%d:%s - Set.",
	     rslt->line, rslt->owner, con->name));
      return;
    }
  }
  notify(player, tprintf("Line \e[1m%d\e[m of program #%d:%s doesn't exist.",
	 rslt->line, rslt->owner, con->name));
}

void do_progcmd_append(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con;
  struct progdata *dts, *new;
  int a;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  }

  /* check permissions */
  if (!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  } if(con->maxline > 65534) {
    notify(player, "Program has too many lines.");
    return;
  }

  /* search program lines */
  con->maxline++;
  new=malloc(sizeof(struct progdata));
  dts=con->line;
  a=1;
  if(!dts)
    con->line = new;
  else {
    a++;
    while(dts) {
    if(!dts->next) {
      dts->next=new;
      break;
    }
    dts=dts->next;
    a++;
    }
  }

  /* set it */
  new->next = NULL;
  SPTR(new->line, compress(arg2));
  notify(player, tprintf("Line \e[1m%d\e[m of program #%d:%s - Set.", a,
         rslt->owner, con->name));
}

void do_progcmd_insert(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con;
  struct progdata *dts, *new;
  int b=1;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  } else if(rslt->line <= 0) {
    notify(player, "Line number must be positive.");
    return;
  }

  /* check permissions */
  if (!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* check lines */
  if(con->maxline < rslt->line) {
    notify(player, tprintf("Line \e[1m%d\e[m of program #%d:%s doesn't exist.",
	   rslt->line, rslt->owner, con->name));
    return;
  } if(con->maxline > 65534) {
    notify(player, "Program has too many lines.");
    return;
  }
  con->maxline++;
  new=malloc(sizeof(struct progdata));
  if(rslt->line == 1) {
    new->next = con->line;
    con->line = new;
  } else {
    for(dts=con->line;dts;dts=dts->next,b++) {
      if(b == rslt->line-1) {
	new->next = dts->next;
	dts->next = new;
	break;
      }
    }
  }

  SPTR(new->line, compress(arg2));
  notify(player, tprintf("Line \e[1m%d\e[m of program #%d:%s - Inserted.",
	 rslt->line, rslt->owner, con->name));
}

void do_progcmd_delete(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt;
  struct progcon *con, *dcon;
  struct progdata *dts, *new, *old=NULL;
  int a, b=0, curline, nlines, maxline=0, minline=0;
  char linestr[40];
  char buf[50];

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  }

  /* check permissions */
  if (!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  } else if (rslt->line) {
    notify(player, "Warning: The syntax to delete any line(s) of a file is:");
    notify(player, "$delete object:program=Line#");
    notify(player,"The way you used this command will delete the entire file.");
    return;
  } else if(!(con->perms & PERM_UW)) {
    notify(player, "Sorry, this program has been protected by erasure of any kind. See '$chmod' for details.");
    return;
  }

  /* scan for previous program in list */
  for(dcon=db[rslt->owner].program;dcon;dcon=dcon->next)
    if(dcon->next == con)
      break;

  if(!strcmp(arg2, "")) {
    for(dts=con->line;dts;dts=new) {
      new=dts->next;
      free(dts->line);
      free(dts);
    }
    notify(player, tprintf("Program \e[1m#%d:%s\e[m permanently removed.",
	    rslt->owner, con->name));
    free(con->name); free(con->cmdstr); free(con->header);
    free(con->lock);
    if(!dcon)
      db[rslt->owner].program = con->next;
    else
      dcon->next=con->next;
    free(con);
    return;
  }

  /* find line range */
  strncpy(linestr, arg2, 39);
  linestr[39]='\0';
  strcpy(buf, "");
  for(a=0;a<strlen(linestr);a++) {
    if(linestr[a] == '-') {
      buf[b]='\0';
      minline=atoi(buf);
      maxline=atoi(linestr+a+1);
      break;
    }
    buf[b] = linestr[a];
    if(a == strlen(linestr)-1) {
      buf[b+1]='\0';
      minline=atoi(buf);
      maxline=atoi(buf);
      break;
    }
    b++;
  }
  if(minline <= 0)
    minline=1;
  if(maxline <= 0)
    maxline=con->maxline;
  if(maxline < minline) {
    notify(player, tprintf("Line mismatch: %s", arg2));
    return;
  }
  nlines=(maxline+1)-minline;

  dts=con->line;
  for(b=2;dts && b < minline;dts=dts->next,b++);
  if(minline > 1)
    new=dts->next;
  else
    new=con->line;
  for(curline=0;new && (!nlines || curline < nlines);new=old) {
    old=new->next;
    free(new->line); free(new);
    curline++;
  }
  if(minline > 1)
    dts->next=old;
  else
    con->line=old;

  con->maxline -= curline;
  notify(player, tprintf("Line%s \e[1m%s\e[m of program #%d:%s deleted.",
	 nlines!=1?"s":"", arg2, rslt->owner, con->name));
}

void do_progcmd_unlock(player, arg1)
dbref player;
char *arg1;
{
  do_progcmd_lock(player, arg1, "");
}

void do_progcmd_edit(player, arg1, arg2, argv)
dbref player;
char *arg1, *arg2;
char *argv[];
{
  struct all_prog_list *rslt;
  struct progcon *con;
  struct progdata *dts;
  int b=1, count=0;
  char temp[8192], *new;

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(con=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  } else if(rslt->line <= 0) {
    notify(player, "Line number must be positive.");
    return;
  }

  /* check permissions */
  if (!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  if(con->maxline < rslt->line) {
    notify(player, tprintf("Line \e[1m%d\e[m of program #%d:%s doesn't exist.",
	   rslt->line, rslt->owner, con->name));
    return;
  }
  if(!argv[1] || !*argv[1]) {
    notify(player, "Nothing to do.");
    return;
  }
  new=(argv[2]) ? argv[2]:"";

  for(dts=con->line;dts;dts=dts->next,b++)
    if(rslt->line == b)
      break;
  if(!dts)
    return;
  if((count=edit_string(temp, uncompress(dts->line), argv[1], new))) {
    WPTR(dts->line, compress(temp));
    notify(player, tprintf("\e[1m%5d:\e[m%s", rslt->line, temp));
    notify(player, tprintf("Line \e[1m%d\e[m of program #%d:%s - %d change%s made.",
	   rslt->line, rslt->owner, con->name, count, count==1?"":"s"));
  } else
    notify(player, "No matches found.");
}

void do_progcmd_copy(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt, *rslt2;
  struct progcon *con=NULL, *dcon, *oprg=NULL;
  struct progdata *dts, *new=NULL;
  int a, b=0;
  char program[512];

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(oprg=rslt->type)) {
    notify(player, "Program 1 name missing.");
    return;
  }

  if(!(rslt2=exist_new_program(player, arg2, program)))
    return;

  /* check permissions */
  if(!controls(player, rslt->owner, POW_MODIFY))
    a=PERM_OR;
  else
    a=PERM_UR;
  if(!(oprg->perms & a)) {
    notify(player, tprintf("Cannot read \e[1m#%d:%s\e[m", rslt->owner,
	   oprg->name));
    return;
  }

  con=malloc(sizeof(struct progcon));
  dcon=db[rslt2->owner].program;
  if(!dcon)
    db[rslt2->owner].program = con;
  else while(dcon) {
    if(!dcon->next) {
      dcon->next=con;
      break;
    }
    dcon=dcon->next;
  }
  memcpy(con,oprg,sizeof(struct progcon));

  con->next=NULL;
  con->line=NULL;
  SPTR(con->name, program);
  SPTR(con->header, oprg->header);
  SPTR(con->cmdstr, oprg->cmdstr);
  SPTR(con->lock, oprg->lock);
  b=0;
  for(dts=oprg->line;dts;dts=dts->next,b++) {
    if(!b) {
      con->line = malloc(sizeof(struct progdata));
      con->line->next = NULL;
      SPTR(con->line->line, dts->line);
      new=con->line;
    } else {
      new->next = malloc(sizeof(struct progdata));
      new->next->next = NULL;
      SPTR(new->next->line, dts->line);
      new=new->next;
    }
  }
  notify(player, tprintf("#%d:%s - Copied to \e[1m#%d:%s\e[m", rslt->owner,
         oprg->name, rslt2->owner, program));
}

void do_progcmd_name(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  struct all_prog_list *rslt, *rslt2;
  struct progcon *oprg=NULL;
  char program[512];
  char program2[512];

  /* find program name */
  if(!(rslt=unparse_programcmd(player, arg1)))
    return;
  if(!(oprg=rslt->type)) {
    notify(player, "Program name missing.");
    return;
  }

  sprintf(program, "#%d:", rslt->owner);
  strncat(program, arg2, 400);
  program[400]='\0';
  if(!(rslt2=exist_new_program(player, program, program2)))
    return;

  /* check permissions */
  if(!controls(player, rslt->owner, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  WPTR(oprg->name, program2);
  notify(player, tprintf("#%d:%s - Name set.", rslt->owner, program2));
}

char *progheader(thing, program)
dbref thing;
char *program;
{
  struct all_prog_list *con, *cnext;
  char *rslt=NULL;

  for(con=all_programs(thing);con;con=cnext) {
    cnext=con->next;
    if(!rslt && !strcasecmp(con->type->name, program))
      rslt=con->type->header;
    free(con);
  }

  if(!rslt)
    return "-deleted-";
  return rslt;
}

int prog_match(thing, player, str)
dbref thing, player;
char *str;
{
  struct all_prog_list *con, *cnext;
  int match=0, a;

  for(con=all_programs(thing);con;con=cnext) {
    cnext=con->next;
    if(wild_match(str,con->type->cmdstr,0,1)) {
      if(!eval_boolexp(player,thing,con->type->lock)) {
        free(con);
        continue;
      }
      if(controls(player, con->owner, POW_MODIFY))
        a=PERM_UX;
      else
        a=PERM_OX;
      if(!(con->type->perms & a) || !could_doit(player,thing,A_ULOCK) || 
         (db[thing].flags & MARKED) || (IS(thing, TYPE_THING, THING_KEY) &&
         Typeof(player) != TYPE_PLAYER && !power(player, POW_SECURITY)))
        ansi_did_it(player,thing,A_UFAIL,NULL,A_OUFAIL,NULL,A_AUFAIL);
      else if(!create_pid(player,thing,con->type->name,1))
        mud_perror(player);
      else
        match=1;
    }
    free(con);
  }

  return match;
}

char *unparse_perms(level)
int level;
{
  static char perms[10];

  strcpy(perms, "---------");
  if(level & PERM_TMP) perms[0] = 't';
  if(level & PERM_STK) perms[1] = 's';
  if(level & PERM_INH) perms[2] = 'i';
  if(level & PERM_UR)  perms[3] = 'r';
  if(level & PERM_UW)  perms[4] = 'w';
  if(level & PERM_UX)  perms[5] = 'x';
  if(level & PERM_OR)  perms[6] = 'r';
  if(level & PERM_OW)  perms[7] = 'w';
  if(level & PERM_OX)  perms[8] = 'x';

  return perms;
}

void clear_programs(thing)
dbref thing;
{
  struct progcon *con=NULL, *cnext;
  struct progdata *dts, *new=NULL;

  for(con=db[thing].program;con;con=cnext) {
    cnext=con->next;
    for(dts=con->line;dts;dts=new) {
      new=dts->next;
      free(dts->line);
      free(dts);
    }
    free(con->name); free(con->cmdstr); free(con->header);
    free(con->lock);
    free(con);
  }
  db[thing].program=NULL;
}
