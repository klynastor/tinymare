/* mare/timer.c */
/* Functions for calculating resource/cpu time used by TinyMARE.
   All `quad' values are in milliseconds unless otherwise stated. */

#include "externs.h"
#include <sys/resource.h>

dbref atime_obj;
dbref wander_obj;

int wander_count;
long dump_interval;  // Seconds (ticks) since last dump.
int cpu_percent;     // CPU Usage in the last 60 seconds (95000 = 95%).

double cmdfp[2];     // Running #Cmds/sec averages.
int cmdav[2];        // #Cmds at last checkpoint.


/* Get current system time (resolution in milliseconds) */
quad get_time()
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return (quad)tv.tv_sec*1000 + tv.tv_usec/1000;
}

/* Returns the amount of CPU time used */
quad get_cputime()
{
  struct rusage usage;

  getrusage(RUSAGE_SELF, &usage);
  return (quad)(usage.ru_utime.tv_sec+usage.ru_stime.tv_sec)*1000 +
          (usage.ru_utime.tv_usec+usage.ru_stime.tv_usec)/1000;
}

/* Called by the timer every 60 seconds to update the global 'cpu_percent' */
void update_cpu_percent()
{
  static quad last_cpu, last_time;
  quad cur_cpu, cur_time;

  cur_time=get_time();
  cur_cpu=get_cputime();

  /* Record the last minute's CPU% usage */
  if(last_time && last_time < cur_time)
    cpu_percent=(cur_cpu-last_cpu)*100000/(cur_time-last_time);
  else
    cpu_percent=0;

  last_time=cur_time;
  last_cpu=cur_cpu;
}

static void next_atime(int ticks)
{
  static int atime_ticks;
  char *s;
  int a, check=1;
  dbref i, top;

  if(ATIME_INTERVAL < 1)
    return;

  /* Update @atime counter */
  if((atime_ticks+=ticks) >= ATIME_INTERVAL*10)
    atime_ticks %= ATIME_INTERVAL*10;
  if((top=db_top*(quad)atime_ticks/(ATIME_INTERVAL*10)) < atime_obj)
    top=db_top;

  /* Loop for all objects to be processed this time-slice */
  for(;atime_obj < top;atime_obj++) {
    /* Stop when end of time slice is reached */
    if(check) {
      if(get_time() > time_slice)
        break;
      check=0;
    }

    i=atime_obj;

    /* Skip over destroyed objects or disconnected players */
    if(Typeof(i) == TYPE_GARBAGE || (Typeof(i) == TYPE_PLAYER &&
       !(db[i].flags & PLAYER_CONNECT)))
      continue;

    /* Set %# for messages */
    speaker=i;

    /* Execute @atime */
    if(!(db[i].flags & (GOING|HAVEN)) && *(s=atr_get(i, A_ATIME))) {
      immediate_que(i, s, i);
      for(a=0;a<10;a++)
        *env[a]='\0';
      check=1;
    }

  }

  /* When @atime cycle is complete, increment loop counters */
  if(atime_obj == db_top) {
    atime_ticks=0;
    atime_obj=0;
  }
}

static void next_wander(int ticks)
{
  static int num_wander, wander_loop;

  int n, top=wander_count;
  dbref i, exit;

  if(WANDER_INTERVAL < 1 || !top)
    return;
  if(WANDER_RATE < 1)
    WANDER_RATE=1;
  if(wander_loop >= WANDER_RATE)
    wander_loop=(wander_obj % WANDER_RATE);

  /* Update Wander counter */
  num_wander += (quad)ticks*top*100000/(WANDER_INTERVAL*10);
  if(num_wander < 0)
    num_wander=2147483647;

  /* Check time-slice before processing any movements */
  if(get_time() > time_slice)
    return;

  /* Loop for all objects to be processed this time-slice */
  for(n=num_wander/100000;n > 0;wander_obj+=WANDER_RATE) {
    if(wander_obj >= db_top) {
      wander_loop=(wander_loop+1) % WANDER_RATE;
      wander_obj=wander_loop;
    }
    i=wander_obj;

    if(!IS(i, TYPE_THING, THING_WANDER))
      continue;
    n--;

    if((db[i].flags & (GOING|HAVEN)) ||
       (exit=random_exit(i, NOTHING)) == NOTHING)
      continue;
    speaker=i;

    run_exit=exit, walk_verb=1;  /* @lock already checked by random_exit() */
    do_move(i, db[exit].name);
    run_exit=NOTHING, walk_verb=0;

    /* Stop when end of time slice is reached */
    if(get_time() > time_slice) {
      wander_obj+=WANDER_RATE;
      break;
    }
  }

  num_wander=(num_wander % 100000)+n;
}


/* Timer Functions */

// do_decisecond(): Triggers every 0.1 seconds.
//  ticks := Number of 0.1-second intervals elapsed since last call.
//
static void do_decisecond(int ticks)
{
}

// do_second(): Triggers once per second.
//  ticks := Number of seconds elapsed since last call.
//
static void do_second(int ticks)
{
  static int cmdav_loop=0;
  DESC *d;

  /* Update players' online status */
  for(d=descriptor_list;d;d=d->next) {
    if((d->flags & C_CONNECTED) && !IS(db[d->player].location, TYPE_ROOM,
       ROOM_XZONE))
      db[d->player].data->age+=ticks;
  }

  /* Other stuff to do every second */

#ifndef __CYGWIN__
  dns_events();			// Keep track of expired dns cache records
#endif

  update_io();			// Updates login prompt and input quotas
  purge_going_objs();		// Purge any objects scheduled for destruction
  wait_second();		// Process queue of @waited commands

  /* Update running command averages */
  if((cmdav_loop += ticks) >= 5) {
    cmdfp[0]+=(((double)(nstat[NS_NPCMDS]-cmdav[0])/cmdav_loop)-cmdfp[0])/3;
    cmdfp[1]+=(((double)(nstat[NS_NQCMDS]-cmdav[1])/cmdav_loop)-cmdfp[1])/3;
    cmdav[0]=nstat[NS_NPCMDS];
    cmdav[1]=nstat[NS_NQCMDS];
    cmdav_loop=0;
  }

  /* Automatic Database Saving */
  if(!halt_dump && !db_readonly) {
    dump_interval += ticks;
    if(dump_interval > DUMP_INTERVAL) {
      dump_interval=0;
      fork_and_dump();
    }
  }
}

// do_minute(): Triggers each minute on the minute.
//
static void do_minute()
{
  DESC *d;
  static int last_weekday=-1, report_loop;
  int weekday;

  /* Loop for all connected sockets */
  for(d=descriptor_list;d;d=d->next) {
    /* Ping concentrators every minute */
    if(d->flags & C_PARENT)
      queue_output(d, 1, "\0\0\0\0\0\001\004", 7);

    /* Ping remotelink services */
    else if((d->flags & C_SERVICE) && d->state == 33) {
      DESC *k;
      int a;

      for(a=0,k=descriptor_list;k;k=k->next)
        if((k->flags & C_CONNECTED) && could_doit(0, k->player, A_HLOCK))
          a++;
      output2(d, tprintf("$20 %d\n", a));
    }

    /* Process session inactivity timeout */
    else if(!(d->flags & (C_OUTBOUND|C_SOCKET|C_DNS|C_PARENT|C_SERVICE)))
      check_timeout(d);
  }

  /* Report disconnected rooms every 10 minutes */
  if(!(++report_loop % 10))
    report_disconnected_rooms(NOTHING);

  /* Other stuff to do every minute... */

  update_cpu_percent();		// Calculate CPU% Usage used in past 60sec

  /* Set weekday to 0..6, 0 being Sunday */
  weekday=(((now+(TIMEZONE*3600))/86400)-3) % 7;

  /* The following executes daily at midnight */
  if(last_weekday == -1)
    last_weekday=weekday;
  else if(last_weekday != weekday) {
    last_weekday=weekday;

    // Remove old players from the game every Sunday at midnight.
    if(INACTIVENUKE > 0 && weekday == 0) {
      log_main("Invoking inactivity deletion...");
      inactive_nuke();
    }
  }
}


/* Main Timer Loop */

// Processes special timer events every 0.1, 1, or 60 seconds.
//
void dispatch()
{
  static quad deci, second, minute;  /* Holds tick differences */
  quad timer=get_time();
  int ticks, secs;

  /* Deciseconds */
  timer/=100;
  if(!(ticks=timer-deci))
    return;
  deci=timer;
  if(ticks < 0 || ticks > 200)
    ticks=1;
  do_decisecond(ticks);

  /* Seconds */
  timer/=10;
  secs=timer-second;
  second=timer;
  if(secs > 0 && secs < 21)
    do_second(secs);

  /* Minutes */
  timer/=60;
  if(timer-minute == 1)
    do_minute();
  minute=timer;


  /**** Queue Processing ****/

  /* First process left-over queue commands from the last run */
  if(queue_overrun)
    flush_queue(queue_overrun);

  /* Execute the next round of @atimes, as time-slice permits */
  next_atime(ticks);

  /* Execute the next round of Wander moves */
  next_wander(ticks);

  /* Run only the current number of commands in the queue */
  flush_queue(queue_size[Q_COMMAND]);
}
