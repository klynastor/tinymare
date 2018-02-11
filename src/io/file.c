/* io/file.c */

#include "externs.h"
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>

/* Logfile structures */

struct log
  main_log = {"logs/main", "_log_main", 1},
  error_log = {"logs/error", "_log_err", 1},
  io_log = {"logs/io", "_log_io", 0},
  resolv_log = {"logs/resolv", "_log_resolv", 0};

/* Record a message in the local logfile */
void do_log(dbref player, char *arg1, char *arg2)
{
  if(!strcasecmp(arg1, "main"))
    mud_log(&main_log, "%s", arg2);
  else if(!strcasecmp(arg1, "error"))
    mud_log(&error_log, "%s", arg2);
  else if(!strcasecmp(arg1, "io"))
    mud_log(&io_log, "%s", arg2);
  else if(!strcasecmp(arg1, "resolv"))
    mud_log(&resolv_log, "%s", arg2);
  else
    notify(player, "No such logfile '%s'.", arg1);
}

void mud_log(struct log *l, char *fmt, ...)
{
  FILE *f;
  DESC *d;
  struct tm *tv;
  va_list args; 
  char buf[16384];
  long tt;

  /* Parse message format and truncate after 250 characters */
  va_start(args, fmt);
  vsprintf(buf, fmt, args);
  va_end(args);

  /* Dump all log messages to stdout during -t 'test database' mode */
  if(mare_test_db) {
    printf("%s\n", buf);
    return;
  }
  
  /* Save log message in file */
  if((f=fopen(l->filename, "a")) == NULL) {
    mkdir("logs", 0777);
    f=fopen(l->filename, "a");
  }

  if(f) {
    tt=time(NULL);
    tv=localtime((time_t *)&tt);
    fprintf(f, "%02d/%02d-%02d:%02d:%02d| %s\n", tv->tm_mon+1, tv->tm_mday,
            tv->tm_hour, tv->tm_min, tv->tm_sec, buf);
    fclose(f);
  }

  /* Output to players online at most 250 characters */
  strcpy(buf+250, "...");
  if((boot_flags & BOOT_QUIET) || (!dberrcheck && game_online))
    com_send(l->com_channel, buf);
  else if(l->output) {
    for(d=descriptor_list;d;d=d->next)
      if(!(d->flags & C_NOMESSAGE)) {
        output2(d, buf);
        output2(d, "\n");
        process_output(d, 1);
      }
  }
}

void lperror(char *file, int line, char *header)
{
  log_error("%s:%d: %s: %s", file, line, header, strerror(errno));
}


/* Determine if file <name> is a regular textfile */
static int get_filetype(char *name)
{
  struct stat statbuf;

  if((lstat(name, &statbuf)) == -1)
    return errno;

  if((statbuf.st_mode & S_IFMT) == S_IFDIR) /* directory */
    errno=EISDIR;
  else if((statbuf.st_mode & S_IFMT) != S_IFREG) /* not a regular file. eck! */
    errno=ENOENT;
  else if(statbuf.st_size >= 0x40000) /* file too large */
    errno=EFBIG;
  else
    errno=0;
  return errno;
}

/* Internal function to display a range of lines of a textfile to <player> */
static int display_file(dbref player, char *name, int top, int bot, char *wild)
{
  FILE *f;
  unsigned char buf[1024];
  int a, lin, count=0;

  /* Find and open the requested file */
  if((f=fopen(name, "r")) == NULL) {
    perror(name);
    return -1;
  }

  /* Scan first 1024 bytes of file for null byte */
  if((lin=fread(buf, 1, 1024, f))) {
    for(a=0;a < lin;a++)
      if(!buf[a]) {
        notify(player, "That is a binary file, which cannot be displayed.");
        fclose(f);
        return -1;
      }
  }

  lin=1;
  rewind(f);

  /* If we're showing data not from the top, read lines until desired point */
  while(lin < top) {
    if(!fgets(buf, 1024, f))
      break;
    lin++;
  }

  /* Read and display remainder of file to object */
  while(bot < 1 || lin <= bot) {
    if(!ftext(buf, 1024, f))
      break;
    if(!*wild || wild_match(buf, wild, 0))
      notify(player, "%s", buf);
    lin++; count++;
  }

  fclose(f);
  return count;
}

/* Displays a system directory listing to <player> */
static void list_directory(dbref player, char *name)
{
  DIR *f;
  struct dirent *dir;
  struct textlist *dirlist=NULL, *ptr, *newp, *last;
  int a, len, count=0, colsize=4;
  char buf[256];

  /* Open directory for reading */
  if((f=opendir(name)) == NULL) {
    perror(name);
    notify(player, "Directory does not exist.");
    return;
  }
  notify(player, "%s:", name);
  notify(player, "%s", "");

  /* Read the directory and allocate all its names */
  while((dir=readdir(f))) {
    if(*dir->d_name == '.') /* skip files that begin with . */
      continue;
    sprintf(buf, "%s/%s", name, dir->d_name);

    /* Check to see if it's a viewable file or directory */
    if((a=get_filetype(buf))) {
      if(a != EISDIR)
        continue;
    }

    /* Scan thru directory list to sort */
    last=NULL;
    for(ptr=dirlist;ptr;ptr=ptr->next) {
      if(strcmp(dir->d_name, ptr->text) < 0)
        break;
      last=ptr;
    }
    len=strlen(dir->d_name)+1;
    newp=malloc(sizeof(struct textlist)+len+(a?1:0));
    memcpy(newp->text, dir->d_name, len);
    if(a) {
      newp->text[len-1]='/';
      newp->text[len++]='\0';
    }
    if(!last)
      dirlist=newp;
    else
      last->next=newp;
    newp->next=ptr;

    if(len+3 > colsize)
      colsize=len+3;
    count++;
  }
  closedir(f);

  if(!count) {
    notify(player, "No files in directory.");
    return;
  }

  showlist(player, dirlist, colsize);
}

/* Display the message of the day */
void do_motd(dbref player)
{
  if(fsize("msgs/motd.txt") < 2)
    notify(player, "There is no message of the day.");
  else
    display_file(db[player].owner, "msgs/motd.txt", 0, 0, "*");
}

/* Get line from file removing leading \r\n */
char *ftext(char *buf, int len, FILE *f)
{
  char *s;

  if(!fgets(buf, len, f))
    return NULL;

  s=buf+strlen(buf)-1;
  if(s >= buf && (*s == '\n' || *s == '\r'))
    *s--='\0';
  if(s >= buf && *s == '\r')
    *s='\0';
  return buf;
}

void do_text(dbref player, char *arg1, char *arg2, char **argv)
{
  int rslt, top=1, bot=0;
  char filename[270];
  dbref thing;

  /* If no filename present, view local directory */
  if(!*arg1)
    arg1=".";

  /* Find object to display file to */
  if(!argv[3] || !strcmp(argv[3], ""))
    thing = player;
  else {
    thing = match_thing(player, argv[3], MATCH_EVERYTHING);
    if(thing == NOTHING)
      return;
  }
  if(Typeof(thing) != TYPE_PLAYER && !power(thing, POW_ANIMATION))
    thing=db[thing].owner;

  /* Check permissions */
  if(!controls(player, thing, POW_REMOTE)) {
    notify(player, "Permission denied.");
    return;
  }

  /* Make sure we are viewing a valid secure filename, please */
  if(!ok_filename(arg1)) {
    notify(player, "Bad file name.");
    return;
  }

  sprintf(filename, "textfiles/%s", arg1);
  if(get_filetype(filename)) {
    if(errno == EISDIR) {
      list_directory(thing, filename);
      if(player != thing && !Quiet(player))
        notify(player, "Player %s just saw directory '%s'.",
               unparse_object(player, thing), filename);
      return;
    }
    notify(player, "%s.", strerror(errno));
    return;
  }

  /* select file length */
  if(argv[1])
    if((top=atoi(argv[1])) < 1)
      top=1;
  if(argv[2])
    if((bot=atoi(argv[2])) < 1)
      bot=0;

  rslt=display_file(thing, filename, top, bot, argv[4] ?: "");

  /* display end message */
  if(!rslt)
    notify(player, "No lines to view.");
  else if(player != thing && !Quiet(player))
    notify(player, "Player %s just saw file '%s'.",
           unparse_object(player, thing), arg1);
}

void showlist(dbref player, struct textlist *dirlist, int colsize)
{
  struct textlist *ptr, *next;
  char temp[32], buf[512], *s=buf;
  int a, cols=get_cols(NULL, player)-1;

  sprintf(temp, "%%-%d.%ds", colsize, colsize);

  a=0;
  for(ptr=dirlist;ptr;ptr=next) {
    next=ptr->next;
    if(++a >= cols/colsize || !ptr->next) {
      strcpy(s, ptr->text);
      notify(player, "%s", buf);
      *buf=0; a=0; s=buf;
    } else
      s+=sprintf(s, temp, ptr->text);
    free(ptr);
  }
}

/* Display local unix file to connection */
void display_textfile(DESC *d, char *filename)
{
  FILE *fd;
  char buf[8192];

  if(!(fd=fopen(filename, "r"))) {
    char *r, *s=filename;

    if((r=strchr(s, '/')))
      s=r+1;
    sprintf(buf, "[No %s file present]\n", s);
    output2(d, buf);
    return;
  }

  while(fgets(buf, 8192, fd))
    output2(d, buf);
  fclose(fd);
}

/* Display file index block entry, to descriptor or player */
/* set <pattern> to NULL to display file header (before first &) */
void display_textblock(dbref player, DESC *d, char *filename, char *pattern)
{
  FILE *fd;
  char buf[8192], *r, *s;

  if(!(fd=fopen(filename, "r"))) {
    s=filename;
    if((r=strchr(s, '/')))
      s=r+1;
    if(d)
      output2(d, tprintf("[No %s file present]\n", s));
    else
      notify(player, "[No %s file present]", s);
    return;
  }

  /* scan for beginning of text block */
  while(pattern) {
    if(!ftext(buf, 8192, fd)) {
      if(d)
        output2(d, tprintf("[No data available for %s]\n", pattern));
      else
        notify(player, "[No data available for %s]", pattern);
      fclose(fd);
      return;
    }

    if(*buf == '&') {
      for(s=buf+1;isspace(*s);s++);
      if(!*pattern || string_prefix(s, pattern))
        break;
    }
  }

  /* print data until next text block is found */
  while(ftext(buf, 8192, fd)) {
    if(*buf == '&')
      break;
    if(d)
      output2(d, tprintf("%s\n", buf));
    else
      notify(player, "%s", buf);
  }

  fclose(fd);
}
