/* The MARE $program code is shown here in full. It is extremely buggy and is
   urged to be used, if at all, under restricted POW_SECURITY permissions.
   -- Copyright 1995, by Byron Stanoszek */


/* prog/execution.c */
/* command program queue */

#include "externs.h"

int pqueue_top=0;
int prog_haltflag=0;
int current_process_id=0;
int mud_errno=0;
int depth_progvar;
struct progcon *progfind();
int bgmode=1;
int prog_exec=0;
char search_progvar[8192];
char switch_match[8192];

PROGQUE *qprog=NULL;


int list_pid(type)
int type;
{
  PROGQUE *tmp;
  int a=0;

  if(type == 0) {
    for(tmp=qprog;tmp;tmp=tmp->next)
      a++;
    return a;
  }
  if(type == 1)
    return pqueue_top;
  return 0;
}

int create_pid(player,thing,program,lnum)
dbref player, thing;
char *program;
int lnum;
{
  static struct all_prog_list *rslt=NULL;
  struct progltype *new, *old;
  struct progcon *con;
  struct progdata *dts;
  PROGQUE *tmp, *bkg;
  char buf[8192];
  char buff[8192];
  char buf2[8192];
  dbref nthing=thing;
  int a, b, c, depth=0, siz=0;
  int priorities[NUM_CLASSES-1] = { 5, 10, 15, 20, 30 };
  char *r, *s;

  if(db[thing].queue >= QUEUE_QUOTA && !power(thing, POW_SECURITY)) {
    mud_errno = ERR_FORK;
    return 0;
  }

  /* initialize ptr */
  if(!rslt)
    rslt=malloc(sizeof(struct all_prog_list));

  rslt->owner=thing;
  if(!progfind(rslt, program)) {
    mud_errno = ERR_NOPROG;
    return 0;
  }
  con=rslt->type;
  if(con->line <= 0) {
    mud_errno = ERR_NOPROG;
    return 0;
  }

  while(1) {
    ++pqueue_top;
    if(pqueue_top > MAX_PROCESSES)
      pqueue_top = 1;
    a=0;
    for(tmp=qprog;tmp;tmp=tmp->next) {
      if (tmp->pid == pqueue_top) {
	a=1; ++depth; break;
      }
    }
    if(depth >= MAX_PROCESSES) {
      mud_errno = ERR_FORK;
      return 0;
    }
    if(a) continue;
    break;
  }

  /* we got the new pid, pqueue_top, so initiate the process */
  db[thing].queue++;

  tmp=malloc(sizeof(PROGQUE));
  tmp->pid = pqueue_top;
  tmp->ppid = current_process_id;
  if(!tmp->ppid)
    tmp->ppid = 1;
  tmp->uid = thing;
  tmp->oid = player;
  SPTR(tmp->program, program);
  tmp->pmode = con->perms;
  tmp->lnum = 0; /* initialize */
  tmp->mlin = 0;
  tmp->bkgnd = bgmode;
  if(!bgmode) {
    prog_haltflag = 1;
    for(bkg=qprog;bkg;bkg=bkg->next) {
      if (tmp->ppid == bkg->pid) {
	strcpy(bkg->status, "W");
	break;
      }
    }
  }
  strcpy(tmp->status, "I");
  if(con->perms & PERM_STK)
    db[thing].flags |= MARKED;
  tmp->priority = priorities[class(thing)-1];
  tmp->alarm_ticks = 0;
  tmp->cpu_time = 0;
  tmp->depth = 0;
  tmp->console = 0;
  tmp->prog_return[0] = 0;
  for(b=1;b<MAX_DEPTH;b++)
    for(a=0;a<10;a++)
      SPTR(tmp->qargs[b][a], "");
  for(a=0;a<10;a++)
    SPTR(tmp->qargs[0][a], wptr[a]);
  tmp->sig_pending[0] = -1;
  tmp->state[0] = 0;
  tmp->current_sig = 0;
  tmp->vlist = NULL;
  for(a=0;a < MAX_DEPTH;a++) {
    tmp->loop_count[a] = 0;
    SPTR(tmp->loop_pure[a], "");
    SPTR(tmp->result_arg[a], "");
  }

  a=1; b=0; old=NULL;
  nthing=thing;
  for(dts=con->line;dts;dts=dts->next) {
    strcpy(buf, uncompress(dts->line));
    s=buf;
    while((r=(char *)parse_up(&s,';'))) {
      if(b+1 >= siz) {
        siz+=100;
        new = malloc(siz*sizeof(struct progltype));
        if(old) {
	  memcpy(new,old,(siz-100)*sizeof(struct progltype));
	  free(old);
        }
        old = new;
      }
      strcpy(buf2, r);
      c=0;
      while(isspace(buf2[c])) c++;
      strcpy(buff, buf2+c);
      if(!strcmp(buff, "") || buff[0] == '^' ||
	 (buff[0] == '@' && buff[1] == '@'))
	continue;
      old[b].line = a;
      SPTR(old[b].str, buff);
      if(!tmp->lnum && a >= lnum)
	tmp->lnum = b;
      b++;
    }
    a++;
  }
  /* set end of program */
  if(!b)
    old = malloc(sizeof(struct progltype));
  old[b].line = NOTHING;
  old[b].str = NULL;
  tmp->mlin = b;
  tmp->proglin = old;

  tmp->next=qprog;
  qprog=tmp;
  return tmp->pid;
}

int copy_pid(pid,forka,forkb)
int pid;
char *forka, *forkb;
{
  PROGQUE *cbg, *tmp;
  VLIST *vars, *old;
  int a, b, depth=0;

  for(cbg=qprog;cbg;cbg=cbg->next)
    if(cbg->pid == pid) break;
  if(!cbg)
    return 0;

  if(db[cbg->uid].queue >= QUEUE_QUOTA && !power(db[cbg->uid].owner,
     POW_SECURITY)) {
    mud_errno = ERR_FORK;
    return 0;
  }

  while(1) {
    ++pqueue_top;
    if(pqueue_top > MAX_PROCESSES)
      pqueue_top = 1;
    a=0;
    for(tmp=qprog;tmp;tmp=tmp->next) {
      if (tmp->pid == pqueue_top) {
	a=1; ++depth; break;
      }
    }
    if(depth == MAX_PROCESSES) {
      mud_errno = ERR_FORK;
      return 0;
    }
    if(a) continue;
    break;
  }

  /* we got the new pid, pqueue_top, so initiate the process */
  db[cbg->uid].queue++;

  tmp=malloc(sizeof(PROGQUE));
  memcpy(tmp,cbg,sizeof(PROGQUE));
  tmp->pid = pqueue_top;
  strcpy(tmp->status, "I");
  tmp->cpu_time = 0;
  SPTR(tmp->program,cbg->program);

  /* copy environment variables */
  for(b=0;b<MAX_DEPTH;b++) {
    for(a=0;a<10;a++)
      SPTR(tmp->qargs[b][a], cbg->qargs[b][a]);
    SPTR(tmp->result_arg[b], cbg->result_arg[b]);
    SPTR(tmp->loop_pure[b], cbg->loop_pure[b]);
  }
  WPTR(cbg->result_arg[cbg->depth], forka);
  WPTR(tmp->result_arg[tmp->depth], forkb);

  tmp->vlist=NULL;
  for(vars=cbg->vlist;vars;vars=vars->next) {
    old = malloc(sizeof(VLIST));
    SPTR(old->vname, vars->vname);
    SPTR(old->str, vars->str);
    old->next = tmp->vlist;
    tmp->vlist = old;
  }   
  tmp->lnum++;

  tmp->proglin = malloc((tmp->mlin+1)*sizeof(struct progltype));
  for(a=0;a < tmp->mlin;a++) {
    tmp->proglin[a].line = cbg->proglin[a].line;
    SPTR(tmp->proglin[a].str, cbg->proglin[a].str);
  }
  tmp->proglin[tmp->mlin].line = NOTHING;
  tmp->proglin[tmp->mlin].str = NULL;

  tmp->next=qprog;
  qprog=tmp;
  return tmp->pid;
}

void mud_perror(player)
dbref player;
{
  PROGQUE *tmp;
  char buf[1024];

  char *errorstr[] = {"Ok", "Fork: Too many processes", "No such program",
    "Syntax error", "Parse error: 'endswitch' outside of loop",
    "Too many signals", "Illegal range value",
    "Parse error: 'endsig' not found",
    "Parse error: 'endsig' outside of loop", "Parse error: 'endfunc' not found",
    "Parse error: 'endfunc' outside of loop", "Maximum depth exceeded",
    "Parse error: Unknown depth", "Parse error: 'return' outside of loop",
    "Function not found", "Unknown Argument",
    "Parse error: 'done' outside of loop", "Parse error: 'done' not found",
    "Parse error: 'break' outside of loop",
    "Parse error: 'endfor' outside of loop", "Undefined error",
    "Parse error: 'endswitch' not found", "Parse error: 'endif' not found",
    "Marker not found", "Parse error: 'continue' outside of loop",
    "Out of memory"};

  if (mud_errno > TOP_ERRORMSG || mud_errno < 0)
    mud_errno = ERR_UNDEF;
  if (!current_process_id) {
    notify(player, tprintf("\a\e[1mParse Error\e[m> %s", errorstr[mud_errno]));
    mud_errno = ERR_OK;
    return;
  }

  for(tmp=qprog;tmp;tmp=tmp->next) {
    if(tmp->pid == current_process_id) {
      if (tmp->lnum > 0 && tmp->lnum <= tmp->mlin)
        sprintf(buf, "Line \e[1m%s\e[m", tprintf("%d",
                tmp->proglin[tmp->lnum-1].line));
      else
	strcpy(buf, "\e[1mEnd of Program\e[m");
      notify(tmp->oid, tprintf("\a\e[1m#%d:%s\e[m> %s: %s", tmp->uid,
             tmp->program, buf, errorstr[mud_errno]));
      mud_errno = ERR_OK;
      return;
    }
  }
}

void remove_pid(pid)
int pid;
{
  PROGQUE *trail=NULL,*point,*next;
  VLIST *vars, *old;
  int a, b, ppid, bkg=0;

  ppid=1;
  for(point=qprog;point;point=next)
    if (point->pid == pid) {
      ppid = point->ppid;
      bkg=point->bkgnd;
      db[point->uid].queue--;
      if(point->pmode & PERM_STK)
	db[point->uid].flags &= ~MARKED;
      if (trail) {
	trail->next=next=point->next;
      } else {
	qprog=next=point->next;
      }
      for(a=0;a <= point->mlin;a++)
	free(point->proglin[a].str);
      free(point->proglin);
      for(vars=point->vlist;vars;vars=old) {
	old=vars->next;
	free(vars->vname);
	free(vars->str);
	free(vars);
      }
      free(point->program);
      for(a=0;a<MAX_DEPTH;a++) {
	for(b=0;b<10;b++)
          free(point->qargs[a][b]);
	free(point->result_arg[a]);
	free(point->loop_pure[a]);
      }
      free(point);
    } else {
      next=(trail=point)->next;
    }

  for(point=qprog;point;point=point->next) {
    if(point->ppid == pid) {
      point->ppid = 1;
      if(point->current_sig >= MAX_DEPTH) {
	mud_errno = ERR_SIGDEPTH;
	a = current_process_id;
	current_process_id = point->pid;
	mud_perror(point->oid);
	current_process_id = a;
      }
      point->sig_pending[point->current_sig] = PSIG_PARENT;
      point->current_sig++;
    }
  }

  for(point=qprog;point;point=point->next) {
    if(point->pid == ppid) {
      if(!bkg && point->status[0] == 'W')
	strcpy(point->status, "R");
      if(point->current_sig >= MAX_DEPTH) {
	mud_errno = ERR_SIGDEPTH;
	a = current_process_id;
	current_process_id = point->pid;
	mud_perror(point->oid);
	current_process_id = a;
      } else {
        point->sig_pending[point->current_sig] = PSIG_CHILD;
        point->current_sig++;
      }
    }
  }
}

void do_priority(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  PROGQUE *tmp;

  if(atoi(arg1) < 1 || atoi(arg1) > MAX_PROCESSES) {
    notify(player, "Invalid PID#.");
    return;
  }
  if(atoi(arg2) < 0 || atoi(arg2) > 100) {
    notify(player, "Invalid priority, must be in the range of 0 to 100.");
    return;
  }

  for(tmp=qprog;tmp;tmp=tmp->next) {
    if(tmp->pid == atoi(arg1)) {
      tmp->priority = atoi(arg2);
      notify(player, "Priority set.");
      return;
    }
  }
  notify(player, "No such PID#.");
}

void do_terminate(player, arg1, arg2)
dbref player;
char *arg1;
char *arg2;
{
  static char *signal_list[6]={"Kill", "Child", "Alarm",
                               "Parent", "Reattach", "Hangup"};
  unsigned char sig=0;
  PROGQUE *tmp;
  int a=0;

  if(atoi(arg1) < 1 || atoi(arg1) > MAX_PROCESSES) {
    notify(player, "Invalid PID#.");
    return;
  }
  sig=atoi(arg2);
  if(*arg2 && !isdigit(*arg2)) {
    for(a=0;a<6;a++)
      if(string_prefix(signal_list[a], arg2)) {
        sig=a;
        break;
      }
  }
  if(a == 6) {
    notify(player, "Invalid Signal.");
    return;
  }
  if(current_process_id) {
    parse_que(player, tprintf("@terminate %d=%d", atoi(arg1), sig), player);
    return;
  }
  for(tmp=qprog;tmp;tmp=tmp->next) {
    if(tmp->pid == atoi(arg1)) {
      if(!controls(player, tmp->uid, POW_MODIFY)) {
	if(!sig) {
	  notify(player, "Permission denied.");
	  return;
        }
	if(tmp->current_sig > MAX_DEPTH) {
	  notify(player, tprintf("\a\e[1m#%d:%s\e[m> Too Many Signals",
		 tmp->uid, tmp->program));
	  return;
	}
	if(controls(player, tmp->uid, POW_MODIFY) || tmp->pmode & PERM_OW) {
	  tmp->sig_pending[tmp->current_sig] = sig;
	  ++tmp->current_sig;
	  if(!(db[player].flags & QUIET))
	    notify(player, "Ok.");
	  return;
        } else {
	  notify(player, "Permission denied.");
	  return;
	}
      } else {
	if(tmp->current_sig == MAX_DEPTH && sig) {
	  notify(player, "Warning: Too many signals.");
	  return;
        }
	if(!sig) {
	  remove_pid(tmp->pid);
	  if(!(db[player].flags & QUIET))
	    notify(player, "Terminated.");
	  return;
	}
	if(tmp->current_sig > MAX_DEPTH) {
	  notify(player, tprintf("#%d:%s> Too Many Signals", tmp->uid,
		 tmp->program));
	  return;
	}
	tmp->sig_pending[tmp->current_sig] = sig;
	++tmp->current_sig;
	if(!(db[player].flags & QUIET))
	  notify(player, "Ok.");
	return;
      }
    }
  }
  notify(player, "No such PID#.");
}

void raise_psig(pid, sig)
int pid, sig;
{
  PROGQUE *tmp;

  for(tmp=qprog;tmp;tmp=tmp->next)
    if(tmp->pid == pid) {
      if(!sig) {
        remove_pid(pid);
        return;
      }
      if(tmp->current_sig >= MAX_DEPTH)
        return;
      tmp->sig_pending[tmp->current_sig++] = sig;
      return;
    }
}

void pque_second(ticks)
int ticks;
{
  PROGQUE *tmp;  /* Counts down the number of ticks remaining in alarms */
  int a;

  if(ticks < 1)
    return;
  for(tmp=qprog;tmp;tmp=tmp->next) {
    if(tmp->status[0] == 'H') {
      a = atoi(tmp->status+1)-ticks;
      if(a < 1) a=0;
      sprintf(tmp->status, "H%d", a);
    }
    if(tmp->alarm_ticks > 0) {
      a = tmp->alarm_ticks-ticks;
      if(a < 1) {
	a = 0;
        if(tmp->current_sig > 19) {
	  mud_errno = ERR_SIGDEPTH;
	  mud_perror(tmp->oid);
	  strcpy(tmp->status, "Z");
	  tmp->alarm_ticks = 0;
	  parse_que(GOD, tprintf("@terminate %d", tmp->pid), GOD);
	  continue;
        }
	tmp->sig_pending[tmp->current_sig] = PSIG_ALARM;
	tmp->current_sig++;
      }
      tmp->alarm_ticks = a;
    }
  }
}

char *prog_function(pid, report, d)
int pid;
char *report;
int d;
{
  PROGQUE *tmp, *otmp;
  static char buff[8192];
  int a;

  for(tmp=qprog;tmp;tmp=tmp->next)
    if(tmp->pid == pid) break;
  if(!tmp)
    return "PID NOT FOUND";

  if(!strcasecmp("PID", report) || !strcmp(report, ""))
    sprintf(buff, "%d", tmp->pid);
  else if(!strcasecmp("PPID", report))
    sprintf(buff, "%d", tmp->ppid);
  else if(!strcasecmp("CPID", report)) {
    strcpy(buff, "");
    for(otmp=qprog;otmp;otmp=otmp->next)
      if(otmp->ppid == tmp->pid)
	strcat(buff, tprintf(" %d", otmp->pid));
    buff[strlen(buff)+1]='\0';
    sprintf(buff, "%s", buff+1);
  } else if(!strcasecmp("UID", report))
    sprintf(buff, "%d", tmp->uid);
  else if(!strcasecmp("OID", report))
    sprintf(buff, "%d", tmp->oid);
  else if(!strcasecmp("PROG", report))
    sprintf(buff, "%s", tmp->program);
  else if(!strcasecmp("PERMS", report))
    sprintf(buff, "%d", tmp->pmode);
  else if(!strcasecmp("LINE", report))
    sprintf(buff, "%d", tmp->proglin[tmp->lnum-1].line);
  else if(!strcasecmp("LNUM", report))
    sprintf(buff, "%d", tmp->lnum);
  else if(!strcasecmp("MAXLINE", report))
    sprintf(buff, "%d", tmp->mlin);
  else if(!strcasecmp("STATUS", report))
    sprintf(buff, "%s", tmp->status);
  else if(!strcasecmp("PRIORITY", report))
    sprintf(buff, "%d", tmp->priority);
  else if(!strcasecmp("ALARM", report))
    sprintf(buff, "%d", tmp->alarm_ticks);
  else if(!strcasecmp("CPU", report))
    sprintf(buff, "%d", tmp->cpu_time);
  else if(!strcasecmp("DEPTH", report))
    sprintf(buff, "%d", tmp->depth);
  else if(!strcasecmp("SIGNAL", report)) {
    strcpy(buff, "");
    for(a=0;(a<tmp->current_sig);a++)
      strcat(buff, tprintf(" %d", tmp->sig_pending[a]));
    buff[strlen(buff)+1] = '\0';
    sprintf(buff, "%s", buff+1);
  } else if(tmp->depth-d < 0)
    return "UNKNOWN DEPTH";
  else if(!strcasecmp("RETURN", report))
    sprintf(buff, "%d", tmp->prog_return[tmp->depth-d]);
  else if(!strcasecmp("LOOP", report)) {
    if(tmp->depth-d-1 < 0)
      return "UNKNOWN DEPTH";
    sprintf(buff, "%d", tmp->loop_count[tmp->depth-d-1]);
  } else if(!strcasecmp("A", report))
    sprintf(buff, "%s", tmp->result_arg[tmp->depth-d]);
  else if(isdigit(report[0]))
    sprintf(buff, "%s", tmp->qargs[tmp->depth-d][report[0]-48]);

  return buff;
}

/* The overall main loop of the Central Processing System */
/* Gets called between each select() to activate each PID on the list */

void do_program()
{
  PROGQUE *tmp;
  int a, d, cmdcount;
  dbref player, cause;
  char command[8192];
  char cmdbuf[8192];
  char cmdarg1[8192], cmdarg2[8192], cmdarg3[8192], cmdarg4[8192];
  char buff[8192];
  char *s, *r;

  bgmode=0;
  for(tmp=qprog;tmp;tmp=tmp->next) { /* go through Each pid available */
    current_process_id = tmp->pid;  /* set the current pid */
    if (db[tmp->uid].flags & GOING || db[tmp->uid].flags & HAVEN) {
      strcpy(tmp->status, "Z");    /* object is haven, destroy its process */
      parse_que(GOD, tprintf("@terminate %d",tmp->pid), GOD);
      continue;
    }
    if (tmp->priority == 0)  /* object is halted, no commands executed */
      continue;
    if (tmp->status[0] == 'Z' || tmp->status[0] == 'W')
      continue;    /* object is waiting for further instructions */
    if(!strcmp(tmp->status, "H0") || !strcmp(tmp->status, "P0"))
      strcpy(tmp->status, "R");
    if (tmp->status[0] == 'H')
      continue;         /* 'sleep' instruction hasn't ended yet */
    if (tmp->status[0] == 'R' && tmp->priority > 0)
      tmp->cpu_time++;   /* increment cpu_time if running */
    else if (tmp->status[0] == 'I')
      strcpy(tmp->status, "R");
    else if (tmp->status[0] == 'S') {
      if(prog_exec)
        strcpy(tmp->status, "R");
      else
        continue;
    }
    player = tmp->uid;  /* if it reaches the wait again, re-halt it */
    cause = tmp->oid; /* set the variables */
    strcpy(search_progvar, ""); /* we dont want to search for anything yet */
    cmdcount=0;

    if (tmp->current_sig > 0 && tmp->state[tmp->depth] != 1) {
      tmp->depth++; /* go into sub-process if there is a signal pending and */
      if(tmp->depth > MAX_DEPTH) { /* you're not in a signal command already */
	mud_errno = ERR_MAXDEPTH;
	mud_perror(player); /* print error */
	strcpy(tmp->status, "Z"); /* close process */
	parse_que(GOD, tprintf("@terminate %d", tmp->pid), GOD);
	continue; /* go back for another PID */
      }
      WPTR(tmp->result_arg[tmp->depth], tprintf("%d", tmp->sig_pending[0]));
      tmp->prog_return[tmp->depth] = tmp->lnum; /* init depth flags */
      tmp->current_sig--; /* decrement number of signals pending */
      tmp->state[tmp->depth] = 1; /* state that it is in a signal loop */
      for(a=0;a<tmp->current_sig;a++)
	tmp->sig_pending[a] = tmp->sig_pending[a+1];
      for(a=0;a<10;a++)
	WPTR(tmp->qargs[tmp->depth][a], "");
      tmp->lnum = 1;
      strcpy(search_progvar, "sig_caught"); /* search for the command */
    }
    depth_progvar = 0;
    mud_errno = ERR_OK;

    /* as long as we still have commands left to activate before the priority */
    /* runs out, lets activate them. */
    while(cmdcount < tmp->priority) {
      if(tmp->lnum < 1 || tmp->lnum > tmp->mlin) {
	strcpy(tmp->status, "Z");  /* end of program */
	parse_que(GOD, tprintf("@terminate %d",tmp->pid), GOD);
	break;
      }
      strcpy(command, tmp->proglin[tmp->lnum-1].str);
      if(command[0] == '&' && command[1] == ' ') {
	strcpy(buff, command+2);
	strcpy(command, buff);
	bgmode=1;
      }
      if(!strcmp(search_progvar, "")) {
	cmdcount++;
      } else {
	for(a=0;a<10;a++)
	  WPTR(wptr[a], tmp->qargs[tmp->depth][a]);
	make_quiet=1;
	pronoun_substitute(cmdbuf, tmp->uid, tmp->oid, command);
	make_quiet=0;
        s=cmdbuf;
	strcpy(cmdarg1, parse_up(&s,' '));
	if(!strcmp(search_progvar, "goto_element")) {
	  strcpy(cmdarg2, parse_up(&s,'='));
	  if(match_word("marker label", cmdarg1)) {
	    if(!strcasecmp(cmdarg2, switch_match)) {
	      if(depth_progvar > tmp->depth) {
		strcpy(search_progvar, "goto_fault");
		break;  /* analyze results */
	      }
	      while(tmp->depth > depth_progvar) {
		tmp->depth--;
		WPTR(tmp->result_arg[tmp->depth],tmp->result_arg[tmp->depth+1]);
	      }
	      depth_progvar=0;
	      strcpy(search_progvar, "");
	      tmp->lnum++;
	      continue;
	    }
          }
        }
	if(!depth_progvar) {
	  if(!strcmp(search_progvar, "if_element")) {
	    if(match_word("else endif", cmdarg1)) {
	      strcpy(search_progvar, "");
	      tmp->lnum++;
	      continue;
	    }
	    if(!strcasecmp(cmdarg1, "elseif")) {
	      d=0;
	      strcpy(cmdarg2, parse_up(&s,'='));
	      r=cmdarg2;
	      evaluate(&r, cmdarg4, tmp->uid, tmp->oid, 0);
	      if(!strcmp(s,"")) {
		if(ISTRUE(cmdarg4))
		  d=1;
		else
		  d=2;
	      }
	      while((r=(char *)parse_up(&s,',')) && !d) {
	        evaluate(&r, cmdarg3, tmp->uid, tmp->oid, 0);
		if(wild_match(cmdarg4, cmdarg3, 1, 1))
		  d=1;
	      }
	      if(d == 1) {
		strcpy(search_progvar, "");
		tmp->lnum++;
		continue;
	      }
	    }
	  }
	  if(!strcmp(search_progvar, "switch_element")) {
	    if(!strcasecmp(cmdarg1, "default")) {
	      strcpy(search_progvar, "");
	      tmp->lnum++;
	      continue;
	    }
	    if(!strcasecmp(cmdarg1, "case")) {
	      d=0;
	      while((r=(char *)parse_up(&s,',')) && !d) {
	        evaluate(&r, cmdarg3, tmp->uid, tmp->oid, 0);
	        if(wild_match(switch_match, cmdarg3, 1, 1))
		  d=1;
	      }
	      if(d) {
		strcpy(search_progvar, "");
		tmp->lnum++;
		continue;
	      }
	    }
	  }
	  if(*search_progvar == '9' && !strcasecmp(search_progvar+1,
	     command)) {
	    strcpy(search_progvar, "");
	    continue;
	  }
	  if(!strcasecmp(search_progvar, command)) {
	    strcpy(search_progvar, "");
	    tmp->lnum++;
	    continue;
	  }
	}
	if (match_word("while until for sig_caught function switch",
	    cmdarg1))
	  ++depth_progvar;
	if (match_word("done endfor endsig endfunc endswitch",
	    cmdarg1))
	  --depth_progvar;
	if(match_word("endif if_element", search_progvar)) {
	  if(!strcasecmp(cmdarg1, "if"))
	    ++depth_progvar;
	  if(!strcasecmp(cmdarg1, "endif"))
	    --depth_progvar;
	}
	if(depth_progvar < 0)
	  break;  /* end of program is reached, no match */

	tmp->lnum++;
        if(!strcmp(search_progvar, "sig_caught") && tmp->lnum == tmp->mlin) {
          tmp->lnum = tmp->prog_return[tmp->depth];
          tmp->depth--;
	  strcpy(search_progvar, "");
	}
	continue;
      }
      for(a=0;a<10;a++)
	WPTR(wptr[a], tmp->qargs[tmp->depth][a]);
      parse_immediate(player, command, cause);
      bgmode=0; /* unset background modifier */
      if (mud_errno) {
	mud_perror(player);
	strcpy(tmp->status, "Z");
	parse_que(GOD, tprintf("@terminate %d", tmp->pid), GOD);
	strcpy(search_progvar, "");
	break;
      }
      if (prog_haltflag == 2) {
	prog_haltflag=0;
	cmdcount--;
	tmp->lnum++;
	continue;
      }
      if (prog_haltflag == 1) {
	prog_haltflag=0;
	tmp->lnum++;
	break;
      }
      tmp->lnum++;
    }
    if(strcmp(search_progvar, "")) {
      strcpy(cmdbuf, search_progvar);
      if(*search_progvar == '9')
	strcpy(cmdbuf, search_progvar+1);
      s=cmdbuf;
      strcpy(cmdarg2, parse_up(&s,' '));
      if(!strcasecmp(cmdarg2, "endsig"))
	mud_errno = ERR_NOENDSIG;
      if(!strcasecmp(cmdarg2, "endfunc"))
	mud_errno = ERR_NOENDFUNC;
      if(match_word("sig_caught goto_fault", cmdarg2))
	mud_errno = ERR_NODEPTH;
      if(!strcasecmp(cmdarg2, "function"))
	mud_errno = ERR_NOFUNCTION;
      if(!strcasecmp(cmdarg2, "done"))
	mud_errno = ERR_NODONE;
      if(match_word("switch_element endswitch", cmdarg2))
	mud_errno = ERR_NOENDSWI;
      if(!strcasecmp(cmdarg2, "if_element"))
	mud_errno = ERR_NOENDIF;
      if(!strcasecmp(cmdarg2, "goto_element"))
	mud_errno = ERR_NOMARKER;
      mud_perror(player);
      strcpy(tmp->status, "Z");
      parse_que(GOD, tprintf("@terminate %d", tmp->pid), GOD);
      tmp->lnum = NOTHING;
    }
  } /* grab next PID and start loop all over again */
  bgmode=1;
  prog_exec=0;
  current_process_id = 0; /* we're done! yay! close up the pid and return to */
}                        /* the server */

char *get_result_arg()
{
  PROGQUE *tmp;

  for(tmp=qprog;tmp;tmp=tmp->next)
    if(tmp->pid == current_process_id) return tmp->result_arg[tmp->depth];
  return "";
}

int system_command(player,cause,command,arg1,arg2,argv,pure)
dbref player, cause;
char *command, *arg1, *arg2;
char *argv[];
char *pure;
{
  PROGQUE *tmp;
  char buff[8192];
  int a, b;

  for(tmp=qprog;tmp;tmp=tmp->next)
    if(tmp->pid == current_process_id) break;
  if (!tmp)
    return 0;

  if (match_word("end abort quit exit", command)) {
    strcpy(tmp->status, "Z");
    parse_que(GOD, tprintf("@terminate %d", tmp->pid), GOD);
    prog_haltflag = 1;
    return 1;
  }
  if (!strcasecmp(command, "sleep") || !strcasecmp(command, "halt")) {
    if(atoi(arg1) < 0 || atoi(arg1) > 65535) {
      mud_errno = ERR_ILLRANGE;
      return 1;
    }
    sprintf(tmp->status, "H%d", atoi(arg1));
    prog_haltflag = 1;
    return 1;
  }
  if (!strcasecmp(command, "queue")) {
    parse_que(player, pure, cause);
    return 1;
  }
  if (!strcasecmp(command, "wait")) {
    if(!strcmp(arg2,"")) {
      if(ISTRUE(arg1))
	return 1;
    } else {
      for(a=1;a<99 && argv[a];a++)
        if(wild_match(arg1, argv[a], 1, 1))
	  return 1;
    }
    strcpy(tmp->status, "S");
    tmp->lnum--;
    prog_haltflag = 1;
    return 1;
  }
  if (!strcasecmp(command, "alarm")) {
    if(atoi(arg1) < 0 || atoi(arg1) > 65535) {
      mud_errno = ERR_ILLRANGE;
      return 1;
    }
    tmp->alarm_ticks = atoi(arg1);
    return 1;
  }
  if (!strcasecmp(command, "setarg")) {
    VLIST *old;
    int memusg=0;

    if(!strcasecmp(arg1, "A"))
      WPTR(tmp->result_arg[tmp->depth], arg2);
    else if(isdigit(*arg1))
      WPTR(tmp->qargs[tmp->depth][arg1[0]-48], arg2);
    else if(strlen(arg1) > 1) {
      for(old=tmp->vlist;old;old=old->next) {
	if(!strcasecmp(old->vname, arg1)) {
	  WPTR(old->str, arg2);
	  return 1;
	}
	memusg+=strlen(old->str)+strlen(old->vname)+2;
      }
      memusg+=strlen(arg1)+strlen(arg2)+2;
      if(memusg >= 32768) {
	mud_errno = ERR_NOMEM;
	return 1;
      }
      old = malloc(sizeof(VLIST));
      SPTR(old->vname, arg1);
      SPTR(old->str, arg2);
      old->next = tmp->vlist;
      tmp->vlist = old;
    } else
      mud_errno = ERR_NOARG;
    return 1;
  }
  if (!strcasecmp(command, "sig_caught")) {
    strcpy(search_progvar, "endsig");
    prog_haltflag = 2;
    return 1;
  }
  if (!strcasecmp(command, "endsig")) {
    if (tmp->state[tmp->depth] != 1) {
      mud_errno = ERR_FENDSIG;
      return 1;
    }
    tmp->lnum = tmp->prog_return[tmp->depth]-1;
    tmp->depth--;
    if (tmp->depth < 0) {
      mud_errno = ERR_NODEPTH;
      return 1;
    }
    return 1;
  }
  if (!strcasecmp(command, "endswitch")) {
    if(tmp->state[tmp->depth] != 5) {
      mud_errno = ERR_FENDSWI;
      return 1;
    }
    tmp->depth--;
    if (tmp->depth < 0) {
      mud_errno = ERR_NODEPTH;
      return 1;
    }
    for(a=0;a<10;a++)
      WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth+1][a]);
    return 1;
  }
  if (!strcasecmp(command, "endfunc")) {
    if (tmp->state[tmp->depth] != 2) {
      mud_errno = ERR_FENDFUNC;
      return 1;
    }
    tmp->lnum = tmp->prog_return[tmp->depth];
    tmp->depth--;
    if (tmp->depth < 0) {
      mud_errno = ERR_NODEPTH;
      return 1;
    }
    return 1;
  }
  if (!strcasecmp(command, "return")) {
    while(tmp->state[tmp->depth] > 2) {
      tmp->loop_count[tmp->depth] = 0;
      tmp->depth--;
      if(tmp->depth < 0) {
	mud_errno = ERR_NODEPTH;
	return 1;
      }
    }
    if (tmp->state[tmp->depth] < 1 || tmp->state[tmp->depth] > 2) {
      mud_errno = ERR_FRETURN;
      return 1;
    }
    tmp->lnum = tmp->prog_return[tmp->depth];
    tmp->depth--;
    if (tmp->depth < 0) {
      mud_errno = ERR_NODEPTH;
      return 1;
    }
    if(tmp->state[tmp->depth+1] == 2)  /* return %A */
      WPTR(tmp->result_arg[tmp->depth], arg1);
    return 1;
  }
  if (!strcasecmp(command, "function")) {
    strcpy(search_progvar, "endfunc");
    prog_haltflag = 2;
    return 1;
  }
  if (match_word("marker label case default endif", command))
    return 1;
  if (!strcasecmp(command, "call")) {
    tmp->depth++;
    if(tmp->depth > MAX_DEPTH) {
      mud_errno = ERR_MAXDEPTH;
      return 1;
    }
    tmp->prog_return[tmp->depth] = tmp->lnum;
    tmp->state[tmp->depth] = 2;
    if(!strcmp(arg2, "")) {
      for(a=0;a<10;a++)
        WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth-1][a]);
    } else {
      for(a=0;a<10;a++)
        WPTR(tmp->qargs[tmp->depth][a], "");
      for(a=0;a<10 && argv[a+1];a++)
        WPTR(tmp->qargs[tmp->depth][a], argv[a+1]);
    }
    tmp->lnum = 0;
    sprintf(search_progvar, "function %s", arg1);
    prog_haltflag = 2;
    return 1;
  }
  if (!strcasecmp(command, "switch")) {
    tmp->depth++;
    if(tmp->depth > MAX_DEPTH) {
      mud_errno = ERR_MAXDEPTH;
      return 1;
    }
    tmp->prog_return[tmp->depth] = NOTHING;
    tmp->state[tmp->depth] = 5;
    for(a=0;a<10;a++)
      WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth-1][a]);
    strcpy(switch_match, arg1);
    strcpy(search_progvar, "switch_element");
    prog_haltflag = 2;
    return 1;
  }
  if (!strcasecmp(command, "while")) {
    b=0;
    if(!strcmp(arg2,"")) {
      if(ISTRUE(arg1))
	b=1;
    } else {
      for(a=1;a<99 && argv[a];a++)
        if(wild_match(arg1, argv[a], 1, 1))
	  b=1;
    }
    if(!b) {
      strcpy(search_progvar, "done");
      prog_haltflag = 2;
      return 1;
    }
    tmp->depth++;
    if(tmp->depth > MAX_DEPTH) {
      mud_errno = ERR_MAXDEPTH;
      return 1;
    }
    tmp->prog_return[tmp->depth] = tmp->lnum-1;
    tmp->state[tmp->depth] = 3;
    for(a=0;a<10;a++)
      WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth-1][a]);
    WPTR(tmp->result_arg[tmp->depth], tmp->result_arg[tmp->depth-1]);
    return 1;
  }
  if (!strcasecmp(command, "until")) {
    b=0;
    if(!strcmp(arg2,"")) {
      if(ISTRUE(arg1))
	b=1;
    } else {
      for(a=1;a<99 && argv[a];a++)
        if(wild_match(arg1, argv[a], 1, 1))
	  b=1;
    }
    if(b) {
      strcpy(search_progvar, "done");
      prog_haltflag = 2;
      return 1;
    }
    tmp->depth++;
    if(tmp->depth > MAX_DEPTH) {
      mud_errno = ERR_MAXDEPTH;
      return 1;
    }
    tmp->prog_return[tmp->depth] = tmp->lnum-1;
    tmp->state[tmp->depth] = 3;
    for(a=0;a<10;a++)
      WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth-1][a]);
    WPTR(tmp->result_arg[tmp->depth], tmp->result_arg[tmp->depth-1]);
    return 1;
  }
  if (!strcasecmp(command, "for")) {
    char *r, *s;

    tmp->loop_count[tmp->depth]++;
    tmp->depth++;
    if(tmp->depth > MAX_DEPTH) {
      mud_errno = ERR_MAXDEPTH;
      return 1;
    }
    tmp->prog_return[tmp->depth] = tmp->lnum-1;
    tmp->state[tmp->depth] = 4;
    for(a=0;a<10;a++)
      WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth-1][a]);

    a=0;
    if(!strlen(tmp->loop_pure[tmp->depth]))
      WPTR(tmp->loop_pure[tmp->depth], arg1);
    strcpy(buff, tmp->loop_pure[tmp->depth]);

    s=buff;
    while((r=(char *)parse_up(&s,' '))) {
      ++a; if(tmp->loop_count[tmp->depth-1] == a) break;
    }
    if(tmp->loop_count[tmp->depth-1] > a) {
      WPTR(tmp->loop_pure[tmp->depth], "");
      tmp->depth--;
      tmp->loop_count[tmp->depth] = 0;
      strcpy(search_progvar, "endfor");
      prog_haltflag = 2;
      return 1;
    }
    WPTR(tmp->result_arg[tmp->depth], r);
    return 1;
  }
  if (!strcasecmp(command, "done")) {
    if (tmp->state[tmp->depth] != 3) {
      mud_errno = ERR_FDONE;
      return 1;
    }
    tmp->lnum = tmp->prog_return[tmp->depth];
    tmp->depth--;
    if (tmp->depth < 0) {
      mud_errno = ERR_NODEPTH;
      return 1;
    }
    for(a=0;a<10;a++)
      WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth+1][a]);
    WPTR(tmp->result_arg[tmp->depth], tmp->result_arg[tmp->depth+1]);
    return 1;
  }
  if (!strcasecmp(command, "endfor")) {
    if (tmp->state[tmp->depth] != 4) {
      mud_errno = ERR_FENDFOR;
      return 1;
    }
    tmp->lnum = tmp->prog_return[tmp->depth];
    tmp->depth--;
    if (tmp->depth < 0) {
      mud_errno = ERR_NODEPTH;
      return 1;
    }
    for(a=0;a<10;a++)
      WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth+1][a]);
    return 1;
  }
  if (!strcasecmp(command, "break")) {
    if (tmp->state[tmp->depth] == 3) {
      tmp->depth--;
      if (tmp->depth < 0) {
	mud_errno = ERR_NODEPTH;
	return 1;
      }
      for(a=0;a<10;a++)
        WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth+1][a]);
      WPTR(tmp->result_arg[tmp->depth], tmp->result_arg[tmp->depth+1]);
      strcpy(search_progvar, "done");
      prog_haltflag = 2;
      return 1;
    }
    if(tmp->state[tmp->depth] == 4) {
      WPTR(tmp->loop_pure[tmp->depth], "");
      tmp->depth--;
      if (tmp->depth < 0) {
	mud_errno = ERR_NODEPTH;
	return 1;
      }
      for(a=0;a<10;a++)
        WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth+1][a]);
      tmp->loop_count[tmp->depth] = 0;
      strcpy(search_progvar, "endfor");
      prog_haltflag = 2;
      return 1;
    }
    if (tmp->state[tmp->depth] == 5) {
      tmp->depth--;
      if (tmp->depth < 0) {
	mud_errno = ERR_NODEPTH;
	return 1;
      }
      for(a=0;a<10;a++)
        WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth+1][a]);
      WPTR(tmp->result_arg[tmp->depth], tmp->result_arg[tmp->depth+1]);
      strcpy(search_progvar, "endswitch");
      prog_haltflag = 2;
      return 1;
    }
    mud_errno = ERR_FBREAK;
    return 1;
  }
  if (!strcasecmp(command, "continue")) {
    if (tmp->state[tmp->depth] == 3) {
      strcpy(search_progvar, "9done");
      prog_haltflag = 2;
      return 1;
    }
    if (tmp->state[tmp->depth] == 4) {
      strcpy(search_progvar, "9endfor");
      prog_haltflag = 2;
      return 1;
    }
    if (tmp->state[tmp->depth] == 5) {
      tmp->depth--;
      if (tmp->depth < 0) {
	mud_errno = ERR_NODEPTH;
	return 1;
      }
      for(a=0;a<10;a++)
        WPTR(tmp->qargs[tmp->depth][a], tmp->qargs[tmp->depth+1][a]);
      WPTR(tmp->result_arg[tmp->depth], tmp->result_arg[tmp->depth+1]);
      strcpy(search_progvar, "endswitch");
      prog_haltflag = 2;
      return 1;
    }
    mud_errno = ERR_FCONT;
    return 1;
  }
  if(!strcasecmp(command, "fork")) {
    char *buf,*s;

    s=arg1;
    buf=(char *)parse_up(&s, ',');
    copy_pid(tmp->pid, buf, s);
    return 1;
  }
  if(!strcasecmp(command, "print")) {
    notify(tmp->oid, pure);
    return 1;
  }
  if(match_word("else elseif", command)) {
    strcpy(search_progvar, "endif");
    prog_haltflag = 2;
    return 1;
  }
  if(!strcasecmp(command, "if")) {
    b=0;
    if(!strcmp(arg2,"")) {
      if(ISTRUE(arg1))
	b=1;
    } else {
      for(a=1;a<99 && argv[a];a++)
        if(wild_match(arg1, argv[a], 1, 1))
	  b=1;
    }
    if(!b) {
      strcpy(search_progvar, "if_element");
      prog_haltflag = 2;
    }
    return 1;
  }
  if(!strcasecmp(command, "jump")) {
    tmp->lnum = 0;
    strcpy(switch_match, arg1);
    strcpy(search_progvar, "goto_element");
    prog_haltflag = 2;
    return 1;
  }
  return 0;
}

char *program_arg(pid, name)
int pid;
char *name;
{
  PROGQUE *tmp;
  VLIST *old;

  for(tmp=qprog;tmp;tmp=tmp->next)
    if(tmp->pid == pid)
      for(old=tmp->vlist;old;old=old->next)
	if(!strcasecmp(old->vname, name))
	  return old->str;
  return "";
}
