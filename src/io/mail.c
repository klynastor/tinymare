/* io/mail.c */
/* TinyMare Mail System Version 2.2.2 - October 13, 2000 */

#include "externs.h"
#include <sys/socket.h>
#include <netdb.h>

#define MAIL_VERSION	"2.2.2"
#define MAILFILE	"mailfile"


static struct mail_cmds {
  char *name;
  int num;
} mailcmd[]={
  {"List",	0},	// List messages in current folder
  {"Read",	1},	// Read a specific message number
  {"Send",	2},	// Send someone a message
  {"Check",	3},	// Check for new messages
  {"Next",	4},	// Read next unread message
  {"Clear",	5},	// Instant message wipeout, # or all
  {"Write",	6},	// Rewrite mailbox, remove deleted msgs
  {"Rewrite",	6},
  {"Purge",	6},
  {"Kill",	7},	// Mark a message for deletion
  {"Del",	7},
  {"Delete",	7},
  {"Undel",	8},	// Undelete a previously marked message
  {"Undelete",	8},
  {"Protect",	9},	// Protect a message from erasure
  {"Unprotect",	10},	// Unprotect that message
  {"Reply",	11},	// Reply to sender of message#
  {"Forward",	12},	// Forward message to another person
  {"Version",	13},	// Print version of mail system
  {"Scan",	14},	// View any message (Wizard Command)
  {"Remove",	15},	// Remove mail from a player (Wizard Command)
  {"Reset",	16},	// Clear all mail files (Wizard Command)
  {0}};

static int send_email(dbref, char *, char *);


/* initializes mail database with 0 messages */
static int init_mailfile()
{
  FILE *f;
  int i;

  if(!(f=fopen(MAILFILE, "wb"))) {
    perror("Mail Folder");
    return 0;
  }
  for(i=0;i<8;i++)
    fputc(0, f);
  fclose(f);
  return 1;
}

/* opens or initializes mailfile for read/write */
static FILE *open_mailfile(dbref player)
{
  FILE *f;

  if(!(f=fopen(MAILFILE, "rb+")))
    if((!init_mailfile() || !(f=fopen(MAILFILE, "rb+"))) && player != NOTHING)
      notify(player, "Error: Cannot access internal mail folders.");
  return f;
}

/* returns size of mailfile in #bytes */
int mailfile_size()
{
  struct stat statbuf;

  if((stat(MAILFILE, &statbuf)) == -1)
    return 0;
  return statbuf.st_size;
}

/* display contents of mail folder */
static void list_mail(dbref player, dbref thing)
{
  FILE *f;
  char buf[256], msgstat;
  int msgto, count=0;
  long msgdate;

  if(!(f=open_mailfile(player)))
    return;

  getnum(f);
  while(getnum(f)) {  /* last message number = 0 */
    msgto=getnum(f);
    msgdate=getlong(f);
    getnum(f);
    msgstat=fgetc(f);
    if(msgto == thing) {
      if(!count++)
        notify(player, "\332\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304 \e[1mMAIL\e[m \304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\277");
      sprintf(buf, "-%d-", count);
      notify(player, "\263 \e[1m%c \e[34m%-8.8s\e[m From: \e[1;36m"
             "%-26.26s\e[m Date: \e[1;31m%s\e[m \263", msgstat, buf,
             uncompress(getstring_noalloc(f)), mktm(player, msgdate));
    } else
      getstring_noalloc(f);
    getstring_noalloc(f);
  }

  fclose(f);
  if(!count) {
    if(thing == player)
      notify(player, "Sorry, no mail.");
    else
      notify(player, "%s has no mail.", db[thing].name);
  } else
    notify(player, "\300\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\331");
}

/* view a specific mail message# */
static void read_mail(dbref player, dbref thing, int num)
{
  FILE *f;
  char buf[30], buff[64], msgfrom[64], msgstat, *p, *r, *s;
  char msg_sdate[11], msg_stime[6];
  int msgnum, msgto, mailcount=0, msgread, a, prev;
  long msgdate;

  if(!(f=open_mailfile(player)))
    return;

  getnum(f);
  while((msgnum=getnum(f))) {
    msgto=getnum(f);
    msgdate=getlong(f);
    prev=ftell(f);
    msgread=getnum(f);
    msgstat=fgetc(f);
    if(msgto == thing) {
      if(++mailcount == num || (!num && (msgstat == 'N' || msgstat == 'U'))) {
        strmaxcpy(msgfrom, uncompress(getstring_noalloc(f)), 64);
        s=uncompress(getstring_noalloc(f));
        sprintf(msg_sdate, "%-10.10s", filter_date(mktm(player, msgdate)));
        sprintf(msg_stime, "%-5.5s", mktm(player, msgdate)+11);

        notify(player, "\332\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\277");
        notify(player, "\263\e[1;36mFrom: \e[37m%-21.21s\e[36m"
               "Message Id: \e[37m%-9d\e[36mStamped by the\e[31m:~~~\e[33m/\\"
               "\e[31m~~~:\e[m\263\e[1;30m\304\277\e[m", msgfrom, mailcount);
        switch(msgstat) {
          case 'R': p="Read"; break;
          case 'U': p="Unread"; break;
          case 'D': p="Deleted"; break;
          case 'F': p="Forwarded"; break;
          case 'A': p="Answered"; break;
          case 'P': p="Protected"; break;
          default: p="New";
        }

        notify(player, "\263\e[1;36mTo: \e[37m%-23.23s\e[36mStatus: "
               "\e[37m%-13.13s\e[36m Officials of \e[31m:\e[5;34m* "
               "\e[;1;33m/  \\ \e[5;34m*\e[;1;31m:\e[m\263 \e[1;30m\263\e[m",
               db[msgto].name, p);
        a=strlen(s);
        sprintf(buf, "%d Char%s", a, a==1?"":"s");
        strcpy(buff, MUD_NAME);
        if(strlen(buff) % 2)
          strcat(buff, ".");
        strcpy(buff, center(buff, 14, ' '));
        notify(player, "\263\e[1;36mDate: \e[37m%s %s     \e[36m"
               "Length: \e[37m%-13.13s\e[36m%s\e[31m: \e[33m/    \\ \e[31m:"
               "\e[m\263 \e[1;30m\263\e[m", msg_sdate, msg_stime, buf, buff);
        notify(player, "\263                                                               \e[1;31m~~~~~~~~\e[m \263\e[1;30m \263\e[m");
        while((r=eval_justify(&s, 72)))
          notify(player, "\263%-72s\263 \e[1;30m\263\e[m", r);
        notify(player, "\300\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\331 \e[1;30m\263\e[m");
        notify(player, "  \e[1;30m\300\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\304\331\e[m");
        if(player == thing) {
          fseek(f, prev, SEEK_SET);
          putnum(f, msgread?:(now/86400));
          if(msgstat == 'U' || msgstat == 'N')
            fputc('R', f);
        }
        fclose(f);
        return;
      }
    }
    getstring_noalloc(f);
    getstring_noalloc(f);
  }

  if(!num)
    notify(player, "You have no unread messages.");
  else
    notify(player, "Invalid Message Number!");
  fclose(f);
}

/* return number of messages in mailbox:
    type=0: all owned messages
    type=1: new/unread messages */
int mail_stats(dbref player, int type)
{
  FILE *f;
  int msgto, count=0;
  char msgstat;

  if(!(f=open_mailfile(player)))
    return 0;

  getnum(f);
  while(getnum(f)) {
    msgto=getnum(f);
    if(getlong(f) < 1) {
      log_error("mail_stats: Corrupt mail header. Resetting mailfile.");
      fclose(f);
      init_mailfile();
      return 0;
    }
    getnum(f);
    msgstat=fgetc(f);
    getstring_noalloc(f);
    getstring_noalloc(f);
    if(msgto == player && (!type || msgstat == 'N' || msgstat == 'U'))
      ++count;
  }

  fclose(f);
  return count;
}

/* provide mail stats for +dbinfo command */
void mail_count(dbref player)
{
  FILE *f;
  int msgtop, count=0, ucount=0, rcount=0, ncount=0;
  char msgstat;

  if(!(f=open_mailfile(player)))
    return;

  msgtop=getnum(f);
  while(getnum(f)) {
    getnum(f);
    getlong(f);
    getnum(f);
    msgstat=fgetc(f);
    getstring_noalloc(f);
    getstring_noalloc(f);
    if(msgstat == 'U')
      ++ucount;
    else if(msgstat == 'N')
      ++ncount;
    else
      ++rcount;
    ++count;
  }
  fclose(f);

  notify(player, "%s", "");
  notify(player, "  \e[1;36mTop Message Number:       \e[37m%d\e[m", msgtop);
  notify(player, "  \e[1;36mTotal Active Messages:    \e[37m%d\e[m", count);
  notify(player, "  \e[1;36mNew Messages:             \e[37m%d\e[m", ncount);
  notify(player, "  \e[1;36mUnread Messages:          \e[37m%d\e[m", ucount);
  notify(player, "  \e[1;36mRead Messages:            \e[37m%d\e[m", rcount);
}

/* process incoming message to local player on the game */
static void deliver_mail(dbref rcpt, dbref from, char *name, char *text)
{
  FILE *f;
  int msgtop;
  char *s;

  if(!(f=open_mailfile(from)))
    return;
  msgtop=getnum(f)+1;

  /* rewind and rewrite msgtop */
  rewind(f);
  putnum(f, msgtop);

  /* position to near-end of file */
  fseek(f, -4, SEEK_END);
  putnum(f, msgtop);
  putnum(f, rcpt);
  putlong(f, now);
  putnum(f, 0);
  fputc('N', f);
  putstring(f, compress(name));
  putstring(f, compress(text));
  putnum(f, 0);
  fclose(f);

  nstat[NS_MAIL]++;
  notify(rcpt, "You sense that you have new mail, message %d, from %s.",
         mail_stats(rcpt, 0), name);
  if(from != NOTHING && *(s=atr_parse(rcpt, from, A_REPLY)))
    notify(from, "Reply message from %s: %s", db[rcpt].name, s);
}

/* send a message to another player */
static void send_mail(dbref player, char *addr, char *msg, int type)
{
  char buf[8192], email[8192], mudname[32];
  char temp[8192], *p, *r, *s=addr;
  dbref *rcpt=NULL, *ptr;
  int cost=0, header=0, count=0, thing;

  *buf='\0';
  *email='\0';

  /* process list of recipients */
  while((r=parse_up(&s, ' '))) {
    /* recipients of the form User@host.xxx use Email/SMTP mode */
    if((p=strchr(r, '@')) && !strchr(r, ' ')) {
      if(strchr(p, '.')) {
        cost+=EMAIL_COST;
        sprintf(buf+strlen(buf), ", %s", r);
        sprintf(email+strlen(email), " %s", r);
        continue;
      }
    }

    /* Recipients of the form User@Mudname use RWHO database */
    if(p) {
      strmaxcpy(mudname, p+1, 28);
      if(strlen(mudname) < 4 || strcasecmp(mudname+strlen(mudname)-4, "MARE"))
        strcat(mudname, "MARE");
      if(strcasecmp(mudname, MUD_NAME)) {
        notify(player, "TinyMARE RWHO Server not yet supported.");
        continue;
      }
      *p='\0';  /* Mudname matches local name of mud. Send locally. */
    }

    /* recipient is local user on game. validate playername. */
    if((thing=lookup_player(r)) <= NOTHING) {
      notify(player, "No such player '%s'.", r);
      continue;
    } if(Guest(thing)) {
      notify(player, "Guests cannot receive +mail.");
      continue;
    } if(inlist(rcpt, thing))  /* never send a player more than 1 message */
      continue;

    PUSH_L(rcpt, thing);
    sprintf(buf+strlen(buf), ", %s", db[thing].name);
    header++;
    cost+=MAIL_COST;
  }

  /* make sure the player has enough money */
  if(!rcpt)
    return;
  if(!power(player, POW_MEMBER) && !charge_money(player, cost, 0)) {
    notify(player, "It costs %d %s to send that +mail.", cost,
           cost==1?MONEY_SINGULAR:MONEY_PLURAL);
    CLEAR(rcpt);
    return;
  }
  
  /* if there are multiple recipients, list them in body of message */
  if(header > 1 || (header && *email)) {
    sprintf(temp, "%s: %s\n\n", *email?"Recipients":"To", buf+2);
    strmaxcpy(temp+strlen(temp), msg, 8001-strlen(temp));
  } else
    strcpy(temp, msg);

  /* change \ characters to newlines */
  s=temp;
  while((s=strchr(s,'\\')))
    *s='\n';

  /* send mail to local users first */
  for(ptr=rcpt;ptr && *ptr != NOTHING;ptr++,count++)
    deliver_mail(*ptr, player, db[player].name, temp);
  CLEAR(rcpt);

  /* next, send email to all non-local recipients */
  if(*email)
    count+=send_email(player, email+1, temp);
  if(!count)
    return;

  switch(type) {
    case 1:  notify(player, "Message forwarded to %s.", buf+2); break;
    case 2:  notify(player, "Replied to %s.", buf+2); break;
    default: notify(player, "Mail sent to %s!", buf+2);
  }
}

/* allows viewing of any mail message in folder (wizard command) */
static void scan_mail(dbref player, char *name, int num)
{
  dbref thing;

  if(!*name) {
    notify(player, "Usage: +mail scan <player> [=message#]");
    return;
  } if((thing=lookup_player(name)) <= NOTHING) {
    notify(player, "No such player '%s'.", name);
    return;
  }

  if(num < 1)
    list_mail(player, thing);
  else
    read_mail(player, thing, num);
}

/* deletes mail from another player (wizard command) */
static void remove_mail(dbref player, char *name, int num)
{
  dbref thing;

  if(!*name || num < 1) {
    notify(player, "Usage: +mail remove <player>=<message#>");
    return;
  } if((thing=lookup_player(name)) <= NOTHING) {
    notify(player, "No such player '%s'.", name);
    return;
  }

  if(delete_message(thing, num))
    notify(player, "Message number %d cleared.", num);
  else
    notify(player, "Invalid message number!");
}

/* msg=  0: Delete all mail stored in file */
/* msg= -1: Clear all messages marked 'D', change 'N' to 'U' */
/* msg= -2: Clear all messages in 'player's mailbox that aren't protected */
/* msg= -3: Clear All in 'player's mailbox */
int delete_message(dbref player, int msg)
{
  FILE *f, *g;
  int a, msgnum, msgto, msgread, count=0, num=0;
  long msgdate;
  char msgstat;

  if(!(f=open_mailfile(player)))
    return 0;
  if(!(g=fopen(MAILFILE".w", "wb"))) {
    perror(MAILFILE".w");
    fclose(f);
    return 0;
  }

  putnum(g, getnum(f));
  while((msgnum=getnum(f))) {
    msgto=getnum(f);
    msgdate=getlong(f);
    msgread=getnum(f);
    msgstat=fgetc(f);
    if(msg > 0 && msgto == player)
      num++;
    if(Invalid(msgto) || Typeof(msgto) != TYPE_PLAYER ||
       (num == msg && msgto == player) || (msg == -1 && msgto == player &&
       msgstat == 'D') || (msg == -2 && msgto == player && msgstat != 'P') ||
       (msg == -3 && msgto == player) || (msgstat != 'P' && msgstat != 'U' &&
       msgstat != 'N' && msgread && MAIL_EXPIRE > 0 &&
       msgread < (now/86400)-(long)MAIL_EXPIRE &&
       !power(msgto, POW_MEMBER))) {  /* delete */
      getstring_noalloc(f);
      getstring_noalloc(f);
      count++;
      continue;
    }
    if(msg == -1 && msgto == player && msgstat == 'N')
      msgstat='U';
    putnum(g, msgnum);
    putnum(g, msgto);
    putlong(g, msgdate);
    putnum(g, msgread);
    putchr(g, msgstat);
    putstring(g, getstring_noalloc(f));
    putstring(g, getstring_noalloc(f));
  }
  fclose(f);

  /* check for any errors on final write to disk */
  for(a=0;a<4;a++)
    if(fputc(0, g) == EOF) {  /* Might be out of space! Don't lose mailfile. */
      perror(MAILFILE".w");
      fclose(g);
      unlink(MAILFILE".w");
      return 0;
    }

  /* move new mailfile over the old */
  fclose(g);
  if(rename(MAILFILE".w", MAILFILE)) {
    perror("Write to Mail Folder");
    unlink(MAILFILE".w");
    return 0;
  }
  return count;
}

/* mark a message# for deletion */
static void delete_mail(dbref player, int msg)
{
  FILE *f;
  int msgnum, msgto, mailcount=0, prev;
  char msgstat;

  if(!(f=open_mailfile(player)))
    return;

  getnum(f);
  while((msgnum=getnum(f))) {
    msgto=getnum(f);
    getlong(f);
    getnum(f);
    prev=ftell(f);
    msgstat=fgetc(f);
    if(msgto == player && ++mailcount == msg) {
      if(msgstat == 'D') {
        notify(player, "Message %d is already marked as deleted.", msg);
        fclose(f);
        return;
      }
      fseek(f, prev, SEEK_SET);
      fputc('D', f);
      fclose(f);
      notify(player, "Message %d marked deleted.", msg);
      return;
    }
    getstring_noalloc(f);
    getstring_noalloc(f);
  }

  notify(player, "Invalid Message Number!");
  fclose(f);
}

/* save a message from deletion */
static void undelete_mail(dbref player, int msg)
{
  FILE *f;
  int msgnum, msgto, mailcount=0, prev;
  char msgstat;

  if(!(f=open_mailfile(player)))
    return;

  getnum(f);
  while((msgnum=getnum(f))) {
    msgto=getnum(f);
    getlong(f);
    getnum(f);
    prev=ftell(f);
    msgstat=fgetc(f);
    if(msgto == player && ++mailcount == msg) {
      if(msgstat != 'D') {
        notify(player, "Message %d is not marked deleted.", msg);
        fclose(f);
        return;
      }
      fseek(f, prev, SEEK_SET);
      fputc('R', f);
      fclose(f);
      notify(player, "Message %d is undeleted.", msg);
      return;
    }
    getstring_noalloc(f);
    getstring_noalloc(f);
  }

  notify(player, "Invalid Message Number!");
  fclose(f);
}

/* protect a message from disappearing from +mail clear or 30-days old */
static void protect_mail(dbref player, int msg)
{
  FILE *f;
  int msgto, msgnum, mailcount=0, prev;

  if(!(f=open_mailfile(player)))
    return;

  getnum(f);
  while((msgnum=getnum(f))) {
    msgto=getnum(f);
    getlong(f);
    getnum(f);
    prev=ftell(f);
    fgetc(f);
    if(msgto == player && ++mailcount == msg) {
      fseek(f, prev, SEEK_SET);
      fputc('P', f);
      fclose(f);
      notify(player, "Message %d is now protected.", msg);
      return;
    }
    getstring_noalloc(f);
    getstring_noalloc(f);
  }

  notify(player, "Invalid Message Number!");
  fclose(f);
}

/* unprotect a message# */
static void unprotect_mail(dbref player, int msg)
{
  FILE *f;
  int msgto, msgnum, mailcount=0, prev;
  char msgstat;

  if(!(f=open_mailfile(player)))
    return;

  getnum(f);
  while((msgnum=getnum(f))) {
    msgto=getnum(f);
    getlong(f);
    getnum(f);
    prev=ftell(f);
    msgstat=fgetc(f);
    if(msgto == player && ++mailcount == msg) {
      if(msgstat != 'P') {
        notify(player, "Message %d is not marked protected.", msg);
        fclose(f);
        return;
      }
      fseek(f, prev, SEEK_SET);
      fputc('R', f);
      fclose(f);
      notify(player, "Message %d is now unprotected.", msg);
      return;
    }
    getstring_noalloc(f);
    getstring_noalloc(f);
  }

  notify(player, "Invalid Message Number!");
  fclose(f);
}

/* forward a letter to another address (local or email) */
static void forward_mail(dbref player, int msg, char *addr)
{
  FILE *f;
  char buf[16384], msgfrom[64], *s, *p;
  int msgto=0, count=0, prev, msgstat;
  long msgdate;

  if(!(f=open_mailfile(player)))
    return;

  getnum(f);
  while(getnum(f)) {
    msgto=getnum(f);
    msgdate=getlong(f);
    getnum(f);
    prev=ftell(f);
    msgstat=fgetc(f);
    if(msgto == player && ++count == msg) {
      strmaxcpy(msgfrom, uncompress(getstring_noalloc(f)), 64);
      p=uncompress(getstring_noalloc(f));
      if(msgstat != 'P') {
        fseek(f, prev, SEEK_SET);
        fputc('F', f);
      }
      fclose(f);
      if((s=strchr(addr, ','))) {
        *s++='\0';
        sprintf(buf, "%s\n\n", s);
      } else
        *buf='\0';
      sprintf(buf+strlen(buf), "-- Begin Forwarded Message --\nFrom: %s\nTime:"
              " %s\n\n", msgfrom, mktm(player, msgdate));
      strmaxcpy(buf+strlen(buf), p, 8001-strlen(buf));
      buf[8000]='\0';
      send_mail(player, addr, buf, 1);
      return;
    }
    getstring_noalloc(f);
    getstring_noalloc(f);
  }

  notify(player, "Invalid Message Number!");
  fclose(f);
}

/* reply back to sender. specify the message# */
static void reply_mail(dbref player, int msg, char *text)
{
  FILE *f;
  char buf[16384], msgfrom[64], *p;
  int count=0, msgto=0, prev, msgstat;
  long msgdate=0;

  if(!(f=open_mailfile(player)))
    return;

  getnum(f);
  while(getnum(f)) {
    msgto=getnum(f);
    msgdate=getlong(f);
    getnum(f);
    prev=ftell(f);
    msgstat=fgetc(f);
    if(msgto == player && ++count == msg) {
      strmaxcpy(msgfrom, uncompress(getstring_noalloc(f)), 64);
      p=uncompress(getstring_noalloc(f));
      if(msgstat != 'P') {
        fseek(f, prev, SEEK_SET);
        fputc('A', f);
      }
      fclose(f);
      sprintf(buf, "On %s in a previous message, %s sent:\n\n%s\n---\n",
              mktm(player, msgdate), msgfrom, p);
      strmaxcpy(buf+strlen(buf), text, 8001-strlen(buf));
      buf[8000]='\0';
      send_mail(player, msgfrom, buf, 2);
      return;
    }
    getstring_noalloc(f);
    getstring_noalloc(f);
  }

  notify(player, "Invalid Message Number!");
  fclose(f);
}

/* Main +mail command */
void do_mail(dbref player, char *cmd, char *text)
{
  char *s=cmd;
  int a;

  if(Typeof(player) != TYPE_PLAYER || Guest(player)) {
    notify(player, "You cannot use +mail.");
    return;
  }

  /* Determine the action the player wants to do */
  parse_up(&s, ' ');
  for(a=0;mailcmd[a].name;a++)
    if(!strcasecmp(mailcmd[a].name, cmd))
      break;

  /* If no command matches, we either view mail or send to a person */
  if(!mailcmd[a].name) {
    a=(!*cmd)?0:(*text)?2:isdigit(*cmd)?1:-1;
    if(a == -1) {
      notify(player, "Unknown +mail command.");
      return;
    } if(a == 2 && *s)
      *(s-1)=' ';       /* patch command/arg1 back up */
    s=cmd;
  }

  if(mailcmd[a].num > 13 && !power(player, POW_SECURITY)) {
    notify(player, "Restricted command.");
    return;
  }

  /* execute appropriate +mail action */

  switch(mailcmd[a].num) {
  case 0: list_mail(player, player); return;
  case 1: read_mail(player, player, atoi(s)); return;
  case 2: send_mail(player, s, text, 0); return;
  case 3:
    if((a=mail_stats(player, 1)))
      notify(player, "You have \e[1mnew\e[m mail: %d message%s.", a,
             a==1?"":"s");
    else
      notify(player, "You have no new mail.");
    return;
  case 4: read_mail(player, player, 0); return;
  case 5:
    if(!*text) {
      if((a=delete_message(player, -2)))
        notify(player, "Mailbox cleared!");
      else
        notify(player, "No mail to clear.");
    } else {
      a=atoi(text);
      if(a > 0 && delete_message(player, a))
        notify(player, "Message number %d cleared.", a);
      else
        notify(player, "Invalid message number!");
    }
    return;
  case 6:
    if((a=delete_message(player, -1)))
      notify(player, "Mailbox rewritten. %d message%s erased.", a,a==1?"":"s");
    else
      notify(player, "No mail to delete.");
    return;
  case 7: delete_mail(player, atoi(text)); return;
  case 8: undelete_mail(player, atoi(text)); return;
  case 9: protect_mail(player, atoi(text)); return;
  case 10: unprotect_mail(player, atoi(text)); return;
  case 11: reply_mail(player, atoi(s), text); return;
  case 12: forward_mail(player, atoi(s), text); return;
  case 13:
    notify(player, "Running TinyMARE Mail System Version " MAIL_VERSION);
    return;
  case 14: scan_mail(player, s, atoi(text)); return;
  case 15: remove_mail(player, s, atoi(text)); return;
  case 16:
    if(init_mailfile()) {
      log_main("Mailfile reset by %s(#%d).", db[player].name, player);
      notify(player, "Mail Initialized.");
    } else
      notify(player, "Sorry, local mail file unavailable.");
    return;
  }
}


/* SMTP Services - Email Handling */

/* internal procedure to send outgoing email to localhost.smtp */
static int send_email(dbref player, char *rcpt, char *msg)
{
  struct sockaddr_in addr;
  struct hostent *hent;
  char buf[8192], buf2[1024], *b, *r, *s;
  FILE *f;
  int fd, count=0;

  if(!*EMAIL_ADDRESS) {
    notify(player, "Sending email is disabled on this game. "
           "Please ask an Administrator for details.");
    return 0;
  }

  /* create internet connection */
  if((fd=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Outbound SMTP");
    notify(player, "Internal error: No available file descriptors.");
    return 0;
  }

  /* attempt to connect to mail server */
  addr.sin_family = AF_INET; /* tcp/ip connection protocol */
  addr.sin_port = htons(25); /* tcp: smtp port */

  /* determine IP address of mail server */
  if(!*MAILHOST)
    addr.sin_addr.s_addr = 0;  /* default to localhost */
  else {
    for(s=MAILHOST;isdigit(*s) || *s == '.';s++);
    if(!*s || !(hent=gethostbyname(MAILHOST)))  /* Blocking DNS call */
      addr.sin_addr.s_addr = inet_addr(MAILHOST);
    else
      addr.sin_addr.s_addr = *(int *)hent->h_addr_list[0];
  }

  if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("Mail Server");
    notify(player, "Internal configuration error: TinyMARE unable to send "
           "request to local mail server. Mail cancelled.");
    close(fd);
    return 0;
  }

  /* introduce ourself */
  f=fdopen(fd, "r+");
  r=buf;
  for(s=db[player].name;*s;s++)
    *r++=tolower(*s);
  *r='\0';
  fprintf(f, "HELO %s\nMail From: <%s@%s>\n", EMAIL_ADDRESS, buf,
          EMAIL_ADDRESS);
  fflush(f);
  fgets(buf2, 512, f);
  fgets(buf2, 512, f);
  fgets(buf2, 512, f);

  /* parse recipients */
  *buf='\0';
  s=rcpt;
  while((r=parse_up(&s, ' '))) {
    fprintf(f, "Rcpt To: <%s>\n", r);
    fflush(f);

    /* make sure that user is valid */
    fgets(buf2, 512, f);
    if(atoi(buf2) == 550)
      notify(player, "%s - User unknown.", r);
    else {
      sprintf(buf+strlen(buf), ", %s", r);
      count++;
    }
  }

  if(!count) {  /* Nothing to send */
    fprintf(f, "QUIT\n");
    fflush(f);
    fclose(f);
    return 0;
  }
  nstat[NS_EMAIL]+=count;
  log_io("SMTP: Outgoing Mail (%d byte%s); From %s, To: %s",
         (int)strlen(msg), strlen(msg)==1?"":"s", db[player].name, rcpt);

  /* print mail headers */
  s=atr_get(player, A_RLNAME);
  fprintf(f, "DATA\nDate: %s\nFrom: %s%s<%c%s@%s>\nTo: %s\n"
             "Content-Type: TEXT/PLAIN; charset=US-ASCII\n"
             "X-Mailer: TinyMARE SMTP %s\n\n",
          mktm(player, now), s, *s?" ":"", tolower(*db[player].name),
          db[player].name+1, EMAIL_ADDRESS, buf+2, MAIL_VERSION);
  
  /* send message */
  s=msg;
  while((r=eval_justify(&s, 72))) {
    for(b=r;*b == '.';b++);
    fprintf(f, "%s%s\n", r, (!*r || *b)?"":".");
  }
  fprintf(f, ".\nQUIT\n");
  fflush(f);

  /* Don't hang up until we read output from the mail server. */
  fgets(buf, 512, f);
  fclose(f);
  return count;
}


/* Inbound SMTP Service */

static char *smtp_cmd[]={"HELO", "EHLO", "MAIL", "RCPT", "DATA",
                         "RSET", "NOOP", "QUIT", "HELP", "VRFY"};

int service_smtp(DESC *d, char *cmd)
{
  char *r, *s=cmd, *host=*EMAIL_ADDRESS?EMAIL_ADDRESS:localhost();
  int a, thing;

  // env[0]: Sender email address
  // env[1]: Space-separated list of recipients
  // env[2]: Message body
  // env[3]: Data input mode

  /* initialize service */
  if(!cmd) {
    output2(d, tprintf("220 %s SMTP TinyMare %s; %s\n", host,
            MAIL_VERSION, strtime(now)));
    WPTR(d->env[0], "");
    WPTR(d->env[1], "");
    return 1;
  }

  /* parse up command string */
  while(isspace(*s))
    s++;
  if(!(r=strspc(&s)))
    return 1;
  for(a=0;a < NELEM(smtp_cmd);a++)
    if(!strcasecmp(smtp_cmd[a], r))
      break;
  if(a == NELEM(smtp_cmd)) {
    output2(d, tprintf("500 Command unrecognized: \"%s%s%s\"\n", r,
            *s?" ":"", s));
    return 1;
  }

  switch(a) {
  case 0: case 1:  // HELO
    output2(d, tprintf("250 %s Hello %s [%s], pleased to meet you\n",
            host, d->addr, ipaddr(d->ipaddr)));
    return 1;
  case 2:  // MAIL
    if(*d->env[0]) {
      output2(d, "503 Sender already specified\n");
      return 1;
    } if(!*s) {
      output2(d, "503 Argument required\n");
      return 1;
    } if(strncasecmp(s, "From", 4) || !(r=strchr(s, ':'))) {
      output2(d, tprintf("501 Syntax error in parameters scanning \"%s\"\n",
              s?:""));
      return 1;
    }

    if((s=strchr(r, '<')))
      r=s;
    r++;
    if((s=strchr(r, '>')))
      *s='\0';
    WPTR(d->env[0], *r?r:"*");
    output2(d, tprintf("250 <%s>... Sender ok\n", r));
    return 1;
  case 3:  // RCPT
    if(!*d->env[0]) {
      output2(d, "503 Need MAIL before RCPT\n");
      return 1;
    }
    if(!*s) {
      output2(d, "503 Argument required\n");
      return 1;
    } else if(strncasecmp(s, "To", 2) || !(r=strchr(s, ':'))) {
      output2(d, tprintf("501 Syntax error in parameters scanning \"%s\"\n",
              s?:""));
      return 1;
    }

    if((s=strchr(r, '<')))
      r=s;
    r++;
    if((s=strchr(r, '>')))
      *s='\0';
    if((s=strchr(r, '@')))
      *s='\0';

    if((thing=lookup_player(r)) <= NOTHING || Guest(thing)) {
      output2(d, tprintf("550 <%s>... User unknown\n", r));
      return 1;
    } if(strlen(d->env[1]) > 1000) {
      output2(d, "550 Maximum number of recipients exceeded\n");
      return 1;
    }

    if(!match_word(d->env[1], db[thing].name))
      WPTR(d->env[1], tprintf("%s%s%s", d->env[1], *d->env[1]?" ":"",
           db[thing].name));
    output2(d, tprintf("250 <%s>... Recipient ok\n", r));
    return 1;
  case 4:  // DATA
    if(!*d->env[0]) {
      output2(d, "503 Need MAIL command\n");
      return 1;
    } if(!*d->env[1]) {
      output2(d, "503 Need RCPT (recipient)\n");
      return 1;
    }
    output2(d, "354 Enter mail, end with \".\" on a line by itself\n");
    WPTR(d->env[2], "");
    WPTR(d->env[3], "0");
    d->state=32;
    return 1;
  case 5:  // RSET
    output2(d, "250 Reset state\n");
    WPTR(d->env[0], "");
    WPTR(d->env[1], "");
    return 1;
  case 6:  // NOOP
    output2(d, "250 OK\n");
    return 1;
  case 7:  // QUIT
    output2(d, tprintf("221 %s closing connection\n", host));
    return 0;
  case 8:  // HELP
    output2(d,
            "214-Available Commands:\n"
            "214-    HELO    EHLO    MAIL    RCPT    DATA\n"
            "214-    RSET    NOOP    QUIT    HELP    VRFY\n");
    if(*TECH_EMAIL)
      output2(d, tprintf(
            "214-To report bugs in the implementation send email to\n"
            "214-    %s\n", TECH_EMAIL));

    output2(d, "214 End of HELP info.\n");
    return 1;
  case 9:  // VRFY
    if((r=strchr(s, '<')))
      s=r+1;
    if((r=strchr(s, '>')))
      *r='\0';
    if((r=strchr(s, '@')))
      *r='\0';

    if((thing=lookup_player(s)) <= NOTHING || Guest(thing)) {
      output2(d, tprintf("550 <%s>... User unknown\n", s));
      return 1;
    }
    r=atr_get(thing, A_RLNAME);
    output2(d, tprintf("250 %s%s<%s@%s>\n", r, *r?" ":"", s, host));
    return 1;
  }

  return 0;
}

int smtp_data_handler(DESC *d, char *text)
{
  char *r, *s;
  dbref thing;

  if(!strcmp(text, ".")) {
    output2(d, "250 Message accepted for delivery.\n");

    if(*d->env[0] != '*')
      log_io("SMTP: Incoming Mail (%d byte%s); From <%s>, To: %s",
             (int)strlen(d->env[2]), strlen(d->env[2])==1?"":"s", d->env[0],
             d->env[1]);

    if(*d->env[0] == '*' || *d->env[2]) {
      s=d->env[1];
      while((r=parse_up(&s, ' ')))
        if((thing=lookup_player(r)) > NOTHING) {
          if(*d->env[0] == '*')
            notify(thing, "%s - User unknown.", d->env[0]+1);
          else {
            deliver_mail(thing, NOTHING, d->env[0], d->env[2]);
            nstat[NS_ERECV]++;
          }
        }
    }
    WPTR(d->env[0], "");
    WPTR(d->env[1], "");
    d->state=31;
    return 1;
  }

  if(*text == '.')
    text++;
  if(*d->env[0] == '*') {  /* Scan for User-unknown */
    if(atoi(text) == 550 && (r=strchr(text, '<'))) {
      r++;
      if((s=strchr(r, '>')))
        *s='\0';
      WPTR(d->env[0], tprintf("*%s", r));
    }
    return 1;
  }

  if(!atoi(d->env[3]) && *text != ' ' && !strchr(text, ':') &&
     strncasecmp(text, "by", 2) && strncasecmp(text, "for", 3)) {
    if(*d->env[2])
      WPTR(d->env[2], tprintf("%s\n", d->env[2]));
    WPTR(d->env[3], "1");
  }
  if(atoi(d->env[3]) == 1) {
    if(!*text)
      return 1;
    WPTR(d->env[3], "2");
  }

  if(atoi(d->env[3]) == 2 || (!atoi(d->env[3]) && (*text == ' ' ||
     !strncasecmp(text, "To: ", 4) || !strncasecmp(text, "Cc: ", 4) ||
     !strncasecmp(text, "Subject: ", 9)))) {
    WPTR(d->env[2], tprintf("%s%s%s", d->env[2], *d->env[2]?"\n":"", text));
    if(strlen(d->env[2]) > 8000)
      d->env[2][8000]='\0';
  }

  return 1;
}
