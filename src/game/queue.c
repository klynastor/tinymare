/* game/queue.c */
/* command queue processing */

#include "externs.h"

int immediate_mode;

int queue_size[NUM_QUEUES];
int queue_overrun;

static struct cmdque *qfirst, *qlast;
static struct cmdque *qwait, *qwaitlast;

static long next_wait;
static char *sema_id;


// Immediately executes a list of ;-separated commands with player=%! and
// thing=%#.
//
void immediate_que(dbref player, char *cmd, dbref thing)
{
  char buf[8192], buff[strlen(cmd)+1], *r, *s=buff;
  char *sptr[10];
  int a, len[10];
  dbref temp;

  /* Check that the player can do a queue command */
  if((db[player].flags & (GOING|HAVEN)) || Typeof(player) == TYPE_GARBAGE)
    return;

  temp=speaker;  /* Save speaker */

  /* Preserve environment %0 to %9 */
  for(a=0;a<10;a++) {
    len[a]=strlen(env[a])+1;
    memcpy(sptr[a]=alloca(len[a]), env[a], len[a]);
  }

  strcpy(buff, cmd);
  while((r=ansi_parse_up(&s, ';'))) {
    func_zerolev();
    pronoun_substitute(buf, player, thing, r);

    immediate_mode=1;
    if(*buf)
      process_command(player, buf, thing, 0);
    immediate_mode=0;

    /* Restore local registers after each command */
    for(a=0;a<10;a++)
      memcpy(env[a], sptr[a], len[a]);

    /* Exit if object destroyed itself */
    if(Typeof(player) == TYPE_GARBAGE)
      break;
  }

  speaker=temp;
}

// Enters a list of ;-separated commands into the command queue, one entry
// per command.
//
void parse_que(dbref player, char *text, dbref cause)
{
  struct cmdque *new;
  char buff[strlen(text)+1], *cmd, *s=buff;
  int a=0, len, pow_queue=power(player, POW_QUEUE);

  if((db[player].flags & (GOING|HAVEN)) || Typeof(player) == TYPE_GARBAGE)
    return;

  strcpy(buff, text);

  /* Add commands to queue */
  while((cmd=ansi_parse_up(&s, ';'))) {
    /* Make sure player can afford to do it */
    if(!pow_queue) {
      if(db[player].queue >= QUEUE_QUOTA) {
        db[player].flags |= HAVEN;
        notify(db[player].owner, "Run away object: %s",
               unparse_object(db[player].owner, player));
        return;
      } if(!db[db[player].owner].pennies &&
           !charge_money(player, rand_num(QUEUE_COST)?0:1, 0)) {
        notify(db[player].owner, "Not enough money to queue command on %s.",
               unparse_object(db[player].owner, player));
        return;
      }
    }

    if(queue_size[Q_COMMAND] >= (pow_queue?MAX_WIZARD_CMDS:MAX_PLAYER_CMDS)) {
      notify(db[player].owner, "%s: Too many queued commands in progress.",
             unparse_object(db[player].owner, player));
      return;
    }

    /* Create pointer reference */
    len=strlen(cmd)+1;
    new=malloc(sizeof(struct cmdque)-sizeof(long)+len);
    new->next=NULL;
    new->player=player;
    new->cause=cause;
    if(sema_id)
      SPTR(new->id, sema_id);
    else
      new->id=NULL;
    for(a=0;a<10;a++) {
      if(*env[a])
        SPTR(new->env[a], env[a]);
      else
        new->env[a]=NULL;
    }
    memcpy(new->cmd, cmd, len);

    /* Add to the queue list */
    if(qlast) {
      qlast->next=new;
      qlast=new; 
    } else
      qlast=qfirst=new;

    db[player].queue++;
    queue_size[Q_COMMAND]++;
  }
}

void do_wait(player, arg1, command, argv, pure, cause)
dbref player, cause;
char *arg1, *command, **argv, *pure;
{
  struct cmdque *new;
  int a, len, wait=atoi(arg1), pow_queue=power(player, POW_QUEUE);
  char *id, *s;

  if(db[player].flags & (GOING|HAVEN))
    return;

  /* Find @wait ID, stripping surrounding spaces */
  if((id=strchr(arg1, '/'))) {
    while(*++id && isspace(*id));
    if(!*id)
      id=NULL;
    else {
      for(s=strnul(id);isspace(*(s-1));s--);
      *s='\0';
      for(s=id;*s >= 33 && *s <= 126;s++);
      if(*s) {
        notify(player, "Invalid semaphore ID.");
        return;
      }
      halt_object(player, id, 0);
      if(wait < 1)
        sema_id=id;
    }
  }

  /* Execute command immediately if no time is specified */
  if(wait < 1) {
    parse_que(player, command, cause);
    sema_id=NULL;
    return;
  } else if(wait > 31536000)  /* 1 year maximum */
    wait=31536000;

  /* Make sure player can afford to do it */
  if(!pow_queue) {
    if(db[player].queue >= QUEUE_QUOTA) {
      db[player].flags |= HAVEN;
      notify(db[player].owner, "Run away object: %s",
             unparse_object(db[player].owner, player));
      return;
    } if(!db[db[player].owner].pennies) {
      notify(db[player].owner, "Not enough money to queue @wait on %s.",
             unparse_object(db[player].owner, player));
      return;
    }
  }

  if(queue_size[Q_WAIT] >= (pow_queue?MAX_WIZARD_WAIT:MAX_PLAYER_WAIT)) {
    notify(db[player].owner, "%s: Too many @wait commands in progress.",
           unparse_object(db[player].owner, player));
    return;
  }

  /* Create pointer reference */
  len=strlen(command)+1;
  new=malloc(sizeof(struct cmdque)+len);
  new->next=NULL;
  new->player=player;
  new->cause=cause;
  new->wait=now+wait;
  if(id)
    SPTR(new->id, id);
  else
    new->id=NULL;
  for(a=0;a<10;a++) {
    if(*env[a])
      SPTR(new->env[a], env[a]);
    else
      new->env[a]=NULL;
  }
  memcpy(new->waitcmd, command, len);

  /* Add to the @wait queue */
  if(qwaitlast) {
    qwaitlast->next=new;
    qwaitlast=new; 
  } else
    qwaitlast=qwait=new;

  /* New value for @wait queue to wake up on */
  if(!next_wait || new->wait < next_wait)
    next_wait=new->wait;

  db[player].queue++;
  queue_size[Q_WAIT]++;
}

// Execute up to <num> queue commands or until the time-slice has expired.
//
void flush_queue(int num)
{
  struct cmdque *ptr;
  char buff[8192];
  int a, i;

  if(!num)
    return;

  queue_overrun=0;
  nstat[NS_QUEUE]++;
  for(i=0;i<num && qfirst;i++) {
    /* Check time slice */
    if(game_online && get_time() > time_slice) {
      nstat[NS_OVERRUNS]++;
      queue_overrun=num-i;
      break;
    }

    /* Remove entry from queue */
    ptr=qfirst;
    if(!(qfirst=ptr->next))
      qlast=NULL;
    queue_size[Q_COMMAND]--;

    /* Process command entry */
    db[ptr->player].queue--;
    if(!(db[ptr->player].flags & (GOING|HAVEN))) {
      for(a=0;a<10;a++)
        strcpy(env[a], ptr->env[a]?:"");
      func_zerolev();
      pronoun_substitute(buff, ptr->player, ptr->cause, ptr->cmd);
      if(*buff)
        process_command(ptr->player, buff, ptr->cause, 0);
    }

    /* Remove pointers */
    free(ptr->id);
    for(a=0;a<10;a++)
      free(ptr->env[a]);
    free(ptr);
  }

  for(a=0;a<10;a++)
    *env[a]='\0';
}

// This function is called once every second to check and process @wait
// queue commands.
//
void wait_second()
{
  struct cmdque *ptr, *next, *last=NULL;
  int a;

  if(!next_wait || next_wait > now)  /* Nothing to do yet? */
    return;
  next_wait=0;

  for(ptr=qwait;ptr;ptr=next) {
    next=ptr->next;
    if(ptr->wait > now && !(db[ptr->player].flags & (GOING|HAVEN))) {
      last=ptr;
      if(!next_wait || ptr->wait < next_wait)
        next_wait=ptr->wait;
      continue;
    }

    /* Remove from queue */
    if(last)
      last->next=ptr->next;
    else
      qwait=ptr->next;
    queue_size[Q_WAIT]--;

    /* Process @wait command */
    db[ptr->player].queue--;
    if(!(db[ptr->player].flags & (GOING|HAVEN))) {
      for(a=0;a<10;a++)
        strcpy(env[a], ptr->env[a]?:"");
      parse_que(ptr->player, ptr->waitcmd, ptr->cause);
    }

    /* Remove pointers */
    free(ptr->id);
    for(a=0;a<10;a++)
      free(ptr->env[a]);
    free(ptr);
  }
  qwaitlast=last;
}

// Display the commands pending in the queue.
//
void do_ps(dbref player, char *arg1, char *arg2)
{
  const char *options[]={"All", "Immediate", "Waited", 0};
  struct cmdque *tmp;
  dbref thing=power(player, POW_QUEUE)?AMBIGUOUS:player;
  int count[NUM_QUEUES], i, type=0, tcount=0;
  
  /* Select a type of viewing */
  if(*arg1) {
    for(type=0;options[type];type++)
      if(string_prefix(options[type], arg1))
        break;
    if(!options[type]) {
      notify(player, "No such queue '%s'.", arg1);
      return;
    }
  }

  /* If present, only view the queue on <arg2> */
  if(*arg2) {
    if((thing=match_thing(player, arg2, MATCH_EVERYTHING)) == NOTHING)
      return;
    if(!controls(player, thing, POW_QUEUE)) {
      notify(player, "Permission denied.");
      return;
    }
  }

  /* Only display what's in the queue before we send any messages */
  for(i=0;i < NUM_QUEUES;i++) {
    count[i]=queue_size[i];
    if((!type || type == i+1) && count[i])
      tcount=1;
  }
  if(!tcount) {
    notify(player, "Nothing in the queue.");
    return;
  }

  /* Immediate commands */
  if(type < 2) {
    for(tcount=0,i=0,tmp=qfirst;tmp && i < count[Q_COMMAND];tmp=tmp->next,i++)
      if((thing == AMBIGUOUS || thing == tmp->player ||
          thing == db[tmp->player].owner) &&
         controls(player, tmp->player, POW_QUEUE)) {
        if(!i)
          notify(player, "\e[1mImmediate commands:\e[m");
        if(tmp->id)
          notify(player, "\e[1;36m[\e[37m%s\e[36m]\e[m%s:%s", tmp->id,
                 unparse_object(player, tmp->player), tmp->cmd);
        else
          notify(player, "%s:%s", unparse_object(player, tmp->player),
                 tmp->cmd);
        tcount++;
      }
    if(tcount > 0)
      notify(player, "-- Total: %d Immediate --", tcount);
  }

  /* Waited Commands */
  if(!type || type == 2) {
    for(tcount=0,i=0,tmp=qwait;tmp && i < count[Q_WAIT];tmp=tmp->next,i++)
      if((thing == AMBIGUOUS || thing == tmp->player ||
         thing == db[tmp->player].owner) &&
         controls(player, tmp->player, POW_QUEUE)) {
        if(!i)
          notify(player, "\e[1m@waited commands:\e[m");
        notify(player, "\e[1;36m[\e[37m%ld%s%s\e[36m]\e[m%s:%s",
               max(tmp->wait-now, 0), tmp->id?"/":"", tmp->id?:"",
               unparse_object(player, tmp->player), tmp->waitcmd);
        tcount++;
      }
    if(tcount > 0)
      notify(player, "-- Total: %d @waited --", tcount);
  }

}

// Internal function to remove all Command/@wait queue entries for <thing>.
// Removes the entire queue if <thing> = AMBIGUOUS.
//
int halt_object(dbref thing, char *id, int wild)
{
  struct cmdque *ptr, *next, *last;
  int a, queue, count=0;

  for(queue=0;queue < 2;queue++) {
    last=NULL;
    for(ptr=(queue == Q_COMMAND)?qfirst:qwait; ptr; ptr=next) {
      next=ptr->next;
      if((thing != AMBIGUOUS && ptr->player != thing &&
          db[ptr->player].owner != thing) ||
         (id && ((!wild && strcasecmp(ptr->id?:"", id)) ||
          (wild && !wild_match(ptr->id?:"", id, 0))))) {
        last=ptr;
        continue;
      }

      /* Remove entry from queue */
      if(last)
        last->next=ptr->next;
      else if(queue == Q_COMMAND)
        qfirst=ptr->next;
      else
        qwait=ptr->next;

      queue_size[queue]--;
      db[ptr->player].queue--;
      count++;

      /* Remove pointers */
      free(ptr->id);
      for(a=0;a<10;a++)
        free(ptr->env[a]);
      free(ptr);
    }

    /* Set the tail pointer */
    if(queue == Q_COMMAND)
      qlast=last;
    else
      qwaitlast=last;
  }

  return count;
}

// Removes all queued commands from a certain player.
// If target is specified as ALL, it will halt all objects in the entire game.
//
void do_halt(player, arg1, ncom, argv, pure, cause)
dbref player, cause;
char *arg1, *ncom, **argv, *pure;
{
  dbref thing=player;

  if(!strcmp(arg1, "ALL") && power(player, POW_SECURITY))
    thing=AMBIGUOUS;
  else if(*arg1) {
    if((thing=match_thing(player, arg1, MATCH_EVERYTHING)) == NOTHING)
      return;
    if(!controls(player, thing, POW_MODIFY)) {
      notify(player, "Permission denied.");
      return;
    }
  }

  if(!Quiet(player)) {
    if(thing == player)
      notify(player, "Everything halted.");
    else if(thing == AMBIGUOUS)
      notify(player, "Halted away every blessed thing.");
    else
      notify(player, "%s halted.", unparse_object(player, thing));
  }

  /* Remove the queue commands */
  halt_object(thing, NULL, 0);

  /* Execute extra command? */
  if(*ncom && !Invalid(thing))
    parse_que(thing,ncom,cause);
}

// Cancels all commands in the Command & @wait queues with ID matching <id>.
//
void do_cancel(dbref player, char *obj, char *id)
{
  dbref thing=match_thing(player, obj, MATCH_EVERYTHING);
  int count;

  if(thing == NOTHING)
    return;
  if(!controls(player, thing, POW_QUEUE)) {
    notify(player, "Permission denied.");
    return;
  }

  count=halt_object(thing, id, 1);
  if(!Quiet(player))
    notify(player, "%d command%s cancelled.", count, count==1?"":"s");
}

// Enters a mutually-exclusive command into the Command Queue with ID <id>.
//
void do_semaphore(player, id, cmd, argv, pure, cause)
dbref player, cause;
char *id, *cmd, **argv, *pure;
{
  if(!*id) {
    notify(player, "Usage: @sema <id>=<commands>");
    return;
  }

  halt_object(player, id, 0);
  if(*cmd) {
    sema_id=id;
    parse_que(player, cmd, cause);
    sema_id=NULL;
  }
}

// Returns the number of seconds remaining for an object's specific ID in
// either the Command or @wait queues. Returns -1 if <id> doesn't exist.
//
long next_semaid(dbref player, char *id)
{
  struct cmdque *ptr;
  int j;

  for(j=0;j < 2;j++)
    for(ptr=j?qwait:qfirst;ptr;ptr=ptr->next)
      if(ptr->player == player && ptr->id && !strcasecmp(ptr->id, id))
        return j?max(ptr->wait-now, 0):0;
  return -1;
}

// Lists all open semaphore IDs on <player>, in either the Command or @wait
// queues.
//
void semalist(char *buff, dbref player)
{
  struct cmdque *ptr;
  char *words[4000], *s=buff;
  int i, j, wcount=0;

  for(j=0;j < 2;j++)
    for(ptr=j?qwait:qfirst;ptr;ptr=ptr->next)
      if(ptr->player == player && ptr->id) {
        if(wcount == 4000)
          break;
        for(i=0;i<wcount;i++)
          if(!strcasecmp(words[i], ptr->id))
            break;
        if(i == wcount)
          words[wcount++]=ptr->id;
      }

  /* Store words found in buffer */
  *buff='\0';
  for(i=0;i < wcount && s-buff+strlen(words[i]) < 7999;i++)
    s+=sprintf(s, "%s%s", s == buff?"":" ", words[i]);
}


void save_queues(FILE *f)
{
  struct cmdque *ptr;
  int i, j;

  /* Save Command Queues */
  for(j=0;j<2;j++) {
    if((ptr=(j == Q_COMMAND)?qfirst:qwait)) {
      putchr(f, j+1);

      for(;ptr;ptr=ptr->next) {
        putref(f, ptr->player);
        putstring(f, (j == Q_COMMAND)?ptr->cmd:ptr->waitcmd);
        putref(f, ptr->cause);
        if(j == Q_WAIT)
          putlong(f, ptr->wait);
        putstring(f, ptr->id?:"");
        for(i=0;i<10;i++)
          putstring(f, ptr->env[i]?:"");
      }
      putnum(f, NOTHING);
    }
  }

  putchr(f, 0);
}

void load_queues(FILE *f)
{
  struct cmdque *new, *last;
  int i, len, count, queue;
  dbref obj;
  char *s;

  while((queue=fgetc(f)) && !feof(f)) {
    count=0;
    switch(--queue) {
    case Q_COMMAND:
    case Q_WAIT:
      last=NULL;
      while((obj=getref(f)) != NOTHING && !feof(f)) {
        s=getstring_noalloc(f);
        len=strlen(s)+1;
        new=malloc(sizeof(struct cmdque)+len -
                   (queue == Q_COMMAND?sizeof(long):0));
        new->next=NULL;
        new->player=obj;
        memcpy((queue == Q_COMMAND)?new->cmd:new->waitcmd, s, len);
        new->cause=getref(f);

        /* Set new value for @wait queue to wake up on */
        if(queue == Q_WAIT) {
          new->wait=getlong(f);
          if(!next_wait || new->wait < next_wait)
            next_wait=new->wait;
        }

        if(*(s=getstring_noalloc(f)))
          SPTR(new->id, s);
        else
          new->id=NULL;
        for(i=0;i<10;i++) {
          if(*(s=getstring_noalloc(f)))
            SPTR(new->env[i], s);
          else
            new->env[i]=NULL;
        }

        if(last)
          last->next=new;
        else if(queue == Q_COMMAND)
          qfirst=new;
        else
          qwait=new;
        last=new;
        count++;
      }

      /* Set the tail pointer */
      if(queue == Q_COMMAND)
        qlast=last;
      else
        qwaitlast=last;
      break;

    }

    queue_size[queue]=count;
  }
}

// Restores all players' queue quota counts from after a @reboot.
//
void restore_queue_quotas()
{
  struct cmdque *ptr;
  int queue;

  for(queue=0;queue<2;queue++)
    for(ptr=(queue == Q_COMMAND)?qfirst:qwait; ptr; ptr=ptr->next)
      db[ptr->player].queue++;
}
