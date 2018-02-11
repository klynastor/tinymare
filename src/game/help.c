/* game/help.c */
/* TinyMare Helptext Version 3.0 - November 5, 2000 */

#include "externs.h"

#define HELP_VERSION	"3.0"
#define HELPFILE	"help/main.txt"


/* Structure for matching help entries */
struct match {
  int index;
  int power;
};

/* List of help topics and their file positions */
struct helpindx {
  char *topic;
  long pos;
  int power;
} *helpindx;

long helpdate;
int helpsize;

// Format of helptext indexes:
//
// &                    <- Main topic, shown when player types 'help'.
// & Topic              <- A specific help topic; 'help Topic'.
// &+ Topic             <- A topic shown when the Combat System is enabled.
// && Topic             <- An immortal topic. Players rank >= 2 can read it.
// && Topic, Security   <- Only players with 'Security' power can read this.
//
// Data from file is printed to the screen until end-of-file or a line
// beginning with a & is reached.


// Reads in the helptext file and indexes all topics.
//
static void init_helptext()
{
  FILE *f;
  struct helpindx *ptr;
  char c, buf[256], *s, *t;
  int i, power, size=0;

  /* Remove existing help index, if any */
  if(helpindx) {
    for(i=0;i<helpsize;i++)
      free(helpindx[i].topic);
    free(helpindx);
    helpindx=NULL;
  }
  helpsize=0;

  /* Retrieve date of file and open it for reading */
  if((helpdate=fdate(HELPFILE)) == -1 ||
     !(f=fopen(HELPFILE, "r"))) {
    perror(HELPFILE);
    return;
  }

  /* Scan for all helptext topics */
  while(ftext(buf, 256, f)) {
    if(*buf != '&') 
      continue;

    /* Remove trailing spaces */
    for(s=strnul(buf);isspace(*(s-1));s--);
    *s='\0';

    /* Skip leading spaces and set 's' to beginning of topic name */

    /* Wizard-only topics begin with && (power == -1) */
    if(*(s=buf+1) == '&') {
      power=-1;
      if((t=strchr(++s, ','))) {  /* Is index restricted by a power? */
        *t++='\0';
        while(isspace(*t))
          t++;
      }
    } else if(*s == '+') {
      continue;
    } else {
      power=-3;  /* Anyone can read this */
      t=NULL;
    }
    while(isspace(*s))
      s++;

    /* Restrict the help index by a specific power? */
    if(t && *t) {
      for(power=0;power < NUM_POWS;power++)
        if(!strcasecmp(powers[power].name, t))
          break;

      /* Remove whitespace before the comma */
      for(t=strnul(s);isspace(*(t-1));t--);
      *t='\0';

      /* No match found? Restrict to POW_SECURITY just to be safe */
      if(power == NUM_POWS) {
        power=POW_SECURITY;
        log_error("Helptext: Unknown power for help index '%s'.", s);
      }
    }

    /* Fast-forward to first non-blank line */
    while((c=fgetc(f)) == '\n' || c == '\r');
    ungetc(c, f);

    /* Add entry to help index */
    if(helpsize >= size) {
      size += 50;
      if(!(ptr=realloc(helpindx, size * sizeof(struct helpindx)))) {
        perror("Help Index");
        for(i=0;i<helpsize;i++)
          free(helpindx[i].topic);
        free(helpindx);

        helpindx=NULL;
        helpsize=0;
        fclose(f);
        return;
      }
      helpindx=ptr;
    }

    SPTR(helpindx[helpsize].topic, s);
    helpindx[helpsize].pos=ftell(f);
    helpindx[helpsize].power=power;
    helpsize++;
  }
  fclose(f);
}

// Displays all or part of a helpfile based on the player's #rows on screen.
// Line numbers begin with 0.
//
// line=0: Viewing a new help <index> page.
//     -1: Display entire helpfile without line breaks.
//      x: Begin displaying at 'x' lines down.
//
// mode=0: Initial 'help' command as typed by player.
//      1: Continuing to next page from a pager prompt.
//      2: Previous page/etc where we need to add 2 extra rows.
//
// Returns the next line number to view, or 0 if finished.
//
static int display_helptext(dbref player, int index, int line, int mode)
{
  static char *pronouns="0123456789abeglnoprstux!#$?";
  static char *no_hilite=
    "ASCII NOMOD MAXHP MAXMP MAXEN CHANT SKILL VIGOR EVADE FUMBLE REGEN "
    "DIVISOR ALIGN HPLOSS MPLOSS ENLOSS NTARGETS STEPS VAULT KILLS MOVES "
    "MAXMOVES DRAIN ENERGY MPRATE ENRATE";

  DESC *d;
  FILE *f;
  char begin=0, end=0, buf[256], temp[1024], word[256], *r, *s, *t, *yield;
  int a, color=0, thisrow=0, rows, more=0, region=9, depth=0;

  /* Initialize helptext if first time through or if file has been updated */
  if(!helpindx || !helpsize || helpdate != fdate(HELPFILE))
    init_helptext();

  d=(Typeof(player) == TYPE_PLAYER)?Desc(player):NULL;

  if(index < 0 || index >= helpsize || (mode && atol(d->env[2]) != helpdate)) {
    notify(player, "Helpfile changed while viewing. Please retype the query.");
    return 0;
  }

  if(!(f=fopen(HELPFILE, "r"))) {
    notify(player, "Helptext currently unavailable.");
    return 0;
  }

  /* Position file pointer at help topic */
  fseek(f, helpindx[index].pos, SEEK_SET);

  /* Determine last row to display */
  if(!d || line == -1 || (d->state != 10 && d->state != 27) ||
     !terminfo(d->player, CONF_PAGER))
    rows=-1;
  else {
    rows=get_rows(d, d->player)-1-(mode == 1 ? 2:0);
    if(rows < 7)
      rows=7;
    rows+=line;
  }

  while(ftext(buf, 256, f)) {
    if(*buf == '&')
      break;

    /* Colorize help screens: */

    /* Prefix region control */
    *temp=0;
    yield=0;
    for(s=buf;isspace(*s);s++);

    /* Lines beginning with an escape are literal */
    if(*s == '\e') {
      notify(player, "%s", buf);
      continue;
    }

    if(!*s)
      region=7, color=0;
    else if((yield=strstr(s, " => ")) || (*s == '>' && *(s+1) == ' '))
      region=6;
    else if(!strncmp(buf, "See also: ", 10))
      sprintf(temp, "\e[1;35mSee also:\e[m %s", buf+10);
    else if(isalnum(*buf) && (t=strrchr(buf, ':')))
      region=*(t+1) ? 14:11;
    else if(region == 9) {
      for(a=0,s=buf;*s;s++) {
        if(isspace(*s))
          a++;
        else if(!a && *s == '(' && (isdigit(*buf) ||
                (*buf >= 'a' && *buf <= 'z'))) {
          a=0;
          break;
        }
      }
      if(a > 2 && (isspace(*buf) || buf[strlen(buf)-1] != '>'))
        region=7;
    }

    /* Infix region control */
    if(!*temp) {
      strcpy(temp, textcolor(7, color?:region));
      t=strnul(temp);
      for(s=buf;*s;) {
        if(s == yield) {
          t+=sprintf(t, "%s", textcolor(region, 15));
          for(a=0;a<4;a++)
            *t++=*s++;
          t+=sprintf(t, "%s", textcolor(15, 10));
          region=10;
        } else if(*s == begin)
          depth++;
        else if(*s == end && --depth == 0) {
          *t++=*s++;
          t+=sprintf(t, "%s", textcolor(color, region));
          begin=end=color=0;
          continue;
        } else if(region == 7 && *s == '\263') {
          t+=sprintf(t, "%s", textcolor(color?:region, 14));
          *t++=*s++;
          t+=sprintf(t, "%s", textcolor(14, color?:region));
          continue;
        } else if(!color && region == 7 &&
                  (*s == '[' || *s == '\'' || *s == '"') && !isspace(*(s+1))) {
          /* Only hilite [functions] if text is between [] */
          if(*s == '[') {
            for(r=s+1;*r && *r != ']';r++)
              if(*r != ' ')
                break;
            if(*r == ']') {
              *t++=*s++;
              continue;
            }
          }

          /* Only hilite standalone quotes */
          if(*s != '[' && s > buf &&
            (isalnum(*(s-1)) || *(s-1) == '_' || (*s == '\'' &&
             (*(s-1) == '>' || *(s-1) == '}' || *(s-1) == ']')))) {
            *t++=*s++;
            continue;
          }

          t+=sprintf(t, "%s", textcolor(region, 15));
          *t++=*s;
          color=15;
          depth=1;
          if(*s == '[') {
            begin='[';
            end=']';
          } else
            end=*s;
          s++;
          continue;
        } else if(!color && (region == 7 || region == 10) && *s == '%') {
          /* %|colorcode| */
          if(*(s+1) == '|') {
            t+=sprintf(t, "%s", textcolor(region, 15));
            *t++=*s++;
            *t++=*s++;
            while(*s != '|')
              *t++=*s++;
            if(*s)
              *t++=*s++;
            t+=sprintf(t, "%s", textcolor(15, region));
            continue;
          }

          /* %va-%vz */
          if(tolower(*(s+1)) == 'v' && isalpha(*(s+2))) {
            t+=sprintf(t, "%s", textcolor(region, 15));
            *t++=*s++;
            *t++=*s++;
            *t++=*s++;
            t+=sprintf(t, "%s", textcolor(15, region));
            continue;
          }

          /* Single-character pronoun */
          if(strchr(pronouns, tolower(*(s+1)))) {
            t+=sprintf(t, "%s", textcolor(region, 15));
            *t++=*s++;
            *t++=*s++;
            t+=sprintf(t, "%s", textcolor(15, region));
            continue;
          }
        } else if(!color && region == 7 && (*s >= 'A' && *s <= 'Z') &&
                  (s == buf || *(s-1) == ' ')) {
          /* Search for configuration keywords */
          for(r=s+1;(*r >= 'A' && *r <= 'Z') || *r == '_';r++);
          if(r-s > 4 && (!*r || *r == ' ')) {
            memcpy(word, s, r-s);
            word[r-s]='\0';
            if(!match_word(no_hilite, word)) {
              t+=sprintf(t, "%s%s", textcolor(region, 15), word);
              t+=sprintf(t, "%s", textcolor(15, region));
              s=r;
              continue;
            }
          }
          while(r > s)
            *t++=*s++;
          continue;
        }
        *t++=*s++;
      }
      strcpy(t, textcolor(color?:region, 7));
    }

    /* Postfix region control */
    if(region == 11 || region == 14)
      region=7;
    else if(region == 6)
      region=10;

    /* Display line of helptext */
    if(rows == -1 || (++thisrow > line && thisrow <= rows))
      notify(player, "%s", temp);
    else if(thisrow > rows && !more) {
      for(s=buf;isspace(*s);s++);
      if(*s)
        more=1;
    }
  }
  fclose(f);

  if(more) {
    output(d, tprintf("\e[1m--Shown %d%%--\e[m More [npqr?] %s",
           rows*100/thisrow, PROMPT(d)));
    WPTR(d->env[0], tprintf("%d", index));
    WPTR(d->env[1], tprintf("%d", rows));
    WPTR(d->env[2], tprintf("%ld", helpdate));
    d->state=27;
    return thisrow;
  }
  return 0;
}

// Helptext pager command input.
//
// d->env[0] := Current help index#
// d->env[1] := Next line# to display
// d->env[2] := Date of helpfile for checksumming
//
int page_helpfile(DESC *d, char *msg)
{
  int line=atoi(d->env[1]);
  int show=0;

  switch(tolower(*msg)) {
  case 'h': case '?':  /* Pager command summary */
    output(d,
           "-------Commands while viewing helptext-------\n"
           "    h  Display this help message\n"
           "    n  Next page\n"
           "    p  Previous page\n"
           "    q  Quit the helpfile pager\n"
           "    r  Retype current page\n"
           "    t  Type entire file without pagebreaks\n"
           "---------------------------------------------\n\n");
    output(d, tprintf("Enter command: %s", PROMPT(d)));
    return 1;
  case 'p':  /* Previous page */
    line-=get_rows(d, d->player)*2-4;
    if(line < 0)
      line=0;
    show=2;
    break;
  case 'q':  /* Quit pager */
    break;
  case 'r':  /* Redraw page */
    line-=get_rows(d, d->player)-1;
    if(line < 0)
      line=0;
    show=2;
    break;
  case 't':  /* Type entire file */
    line=-1;
    show=1;
    break;
  default:   /* Show next page */
    show=1;
  }

  if(show) {
    if(terminfo(d->player, TERM_ANSI))  /* Erase 'More' prompt */
      notify(d->player, "\e[A\e[2K\e[A");
    if(display_helptext(d->player, atoi(d->env[0]), line, show))
      return 1;
  }

  /* Exit help viewer */
  d->state=10;
  WPTR(d->env[0], "");
  WPTR(d->env[1], "");
  WPTR(d->env[2], "");

  return 1;
}   

// Top-level command to request a specific help topic from the helptext.
//
void do_help(dbref player, char *req)
{
  struct match exact={-1}, mark={-1};
  struct textlist *filelist=NULL, *ptr, *new, *last=NULL;
  int len, index, count=0, colsize=4, wiz=Immortal(player);
  long size;

  /* Display version information */
  if(*req == '?')
    notify(player, "Running TinyMARE Helptext System Version " HELP_VERSION);

  /* Initialize helptext if first time through or if file has been updated */
  if(!helpindx || !helpsize || helpdate != fdate(HELPFILE))
    init_helptext();

  if(!helpsize) {
    notify(player, "Helptext currently unavailable.");
    return;
  }

  if(*req == '?') {
    size=fsize(HELPFILE);
    notify(player, "Helptext: %d Topics Available, %ld byte%s.",
           helpsize, size, size==1?"":"s");
    notify(player, "Last Updated: %s", mktm(player, helpdate));
    return;
  }

  /* Search for matching topics */
  for(index=0;index < helpsize;index++) {
    /* Only immortals can see wizard topics */
    if((helpindx[index].power == -1 && !wiz) ||
       (helpindx[index].power > -1 && !power(player, helpindx[index].power)))
      continue;
    if(helpindx[index].power == -2)
      continue;
    /* Exact matches are preferred over abbreviations */
    if(!strcasecmp(helpindx[index].topic, req)) {
      if(exact.index == -1 || helpindx[index].power > exact.power) {
        exact.index=index;
        exact.power=helpindx[index].power;
      }
      continue;
    }

    /* Maintain a list of ambiguous matches */
    if(string_prefix(helpindx[index].topic, req)) {
      if(mark.index == -1 || helpindx[index].power > mark.power) {
        mark.index=index;
        mark.power=helpindx[index].power;
      }

      /* Only list unique entries */
      for(ptr=filelist;ptr;ptr=ptr->next)
        if(!strcasecmp(ptr->text, helpindx[index].topic))
          break;
      if(ptr)
        continue;

      count++;
      len=strlen(helpindx[index].topic)+1;
      new=malloc(sizeof(struct textlist)+len);
      new->next=NULL;
      memcpy(new->text, helpindx[index].topic, len);
      if(!last)
        filelist=new;
      else
        last->next=new;
      last=new;

      /* Calculate biggest match for ambiguous display */
      if(len+3 > colsize)
        colsize=len+3;
    }
  }

  if(exact.index != -1)  /* Exact match found */
    display_helptext(player, exact.index, 0, 0);
  else if(count == 1)    /* One prefix matched */
    display_helptext(player, mark.index, 0, 0);
  else if(count == 0) {
    notify(player, "No topics match request.");
    return;
  } else if(count > 99) {
    notify(player, "There are %d entries matching your request. "
           "Please refine your selection.", count);
  } else {
    notify(player, "Ambiguous match. Please pick from these %d topics:",count);
    notify(player, "%s", "");
    showlist(player, filelist, colsize);  /* Automatically frees list */
    return;
  }

  /* Free up list */
  for(last=filelist;last;last=new) {
    new=last->next;
    free(last);
  }
}
