/* util/wd.c */

/**************************************************************
 *  Copyright 2001 by the authors and developers of TinyMare  *
 * See the file COPYRIGHT for more details regarding TinyMare *
 **************************************************************/

#define _GNU_SOURCE	// Use the GNU C Library Extensions

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

#ifdef __CYGWIN__
# include <sys/cygwin.h>
# define __GLIBC__ 2
#endif

#define MUD_PROGRAM	"../bin/netmare"

/* choose implementation of strsignal() for your system */
#ifndef __GLIBC__
# if defined(__hpux__)
#  define strsignal(x) ""
# elif defined(__osf__)
#  define strsignal(x) __sys_siglist[x]
# else
#  define strsignal(x) sys_siglist[x]
# endif
#endif


static void init_io() {
  int fd;
  
  /* jump into background */
  if(fork())
    exit(0);

  /* disassociate from controlling terminal */
  setsid();

  /* close standard input */
  fclose(stdin);
  
  /* open a link to the log file */
  if((fd=open("logs/wd.log", O_WRONLY|O_CREAT|O_APPEND, 0644)) < 0) {
    perror("logs/wd.log");
    exit(1);
  }
  
  /* convert standard output to logfile */
  close(fileno(stdout));
  dup2(fd, fileno(stdout));
  setbuf(stdout, NULL);
  
  /* convert standard error to logfile */
  close(fileno(stderr));
  dup2(fd, fileno(stderr));
  setbuf(stderr, NULL);
  
  /* this logfile reference is no longer needed */
  close(fd);
}

static void copy_logs() {
  FILE *f, *g;
  char buf[512];

  /* open files for reading and writing */
  if(!(f=fopen("logs/commands~", "r")))
    return;
  if(!(g=fopen("cmd_crash", "w"))) {
    fclose(f);
    return;
  }

  /* copy logs/commands~ */
  while(fgets(buf, 512, f))
    fputs(buf, g);
  fclose(f);

  if(!(f=fopen("logs/commands", "r"))) {
    fclose(g);
    return;
  }

  /* copy logs/commands */
  while(fgets(buf, 512, f))
    fputs(buf, g);

  /* clean up */
  fclose(g);
  fclose(f);
}


int main(int argc, char **argv) {
  int pid, status, mud_pid=0;
  time_t tt=time(NULL);

  init_io();

  printf("Watchdog (C) 2001 by the developers of TinyMare.\n"
         "Booting up.     PID%6d, %s  ---\n", getpid(), ctime(&tt));

  /* Main Loop */

  while(1) {
    copy_logs();
    
    /* Spawn a new process */
    if((mud_pid=fork()) < 0) {
      perror(MUD_PROGRAM);
      exit(1);
    }
    
    /* Execute mud program in child process: */
    if(!mud_pid) {
      unlink("logs/socket_table");
      setenv("WATCHDOG", "1", 1);
      argv[0]=MUD_PROGRAM;
      if(execvp(MUD_PROGRAM, argv) < 0)
        perror(MUD_PROGRAM);
      exit(0);
    }

    /* Parent continues here when mud program terminates */

    tt=time(NULL);
    printf("Game Online:    PID%6d, %s", mud_pid, ctime(&tt));

    pid=wait(&status);
    tt=time(NULL);

    if(WIFSTOPPED(status)) {
      printf("Game Suspended! PID%6d, %s", mud_pid, ctime(&tt));
      break;
    } else if(WIFSIGNALED(status)) {
      printf("Mud Crashed:  ! PID%6d, %24.24s -- %s (%d)\n", mud_pid,
             ctime(&tt), strsignal(WTERMSIG(status)), WTERMSIG(status));
    } else if(WEXITSTATUS(status) == 0) {
      printf("Shutdown At:    PID%6d, %s", mud_pid, ctime(&tt));
      break;
    } else if(WEXITSTATUS(status) == 1) {
      printf("Game Abort:     PID%6d, %s", mud_pid, ctime(&tt));
      break;
    } else
      printf("Game Reboot:    PID%6d, %s", mud_pid, ctime(&tt));
  }

  printf("\n--Watchdog Exiting.\n");
  return 0;
}
