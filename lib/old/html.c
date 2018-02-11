/* io/html.c */
/* Routines for reading and displaying HTML v3.1, including helptext.html */

#include "externs.h"

struct helpindx {
  char *name;
  long pos;
  int wiz;
} *helpindx;

long helpdate;
int helpsize;


/* reads .html file and matches all topics <a name=...> */
static void init_helptext()
{
  FILE *f;
  struct helpindx *ptr;
  char buf[64], *r, *s;
  int a, b, size=0;
  long pos;

  /* remove existing help index, if any */
  if(helpindx) {
    for(a=0;a<helpsize;a++)
      free(helpindx[a].name);
    free(helpindx);
    helpindx=NULL;
  }
  helpsize=0;
  helpdate=now;

  if(!(f=fopen("msgs/helptext.html", "rb"))) {
    perror("msgs/helptext.html");
    return;
  }

  /* search for all occurances of <a name=...> */
  while(1) {
    /* search for < */
    while((a=getc(f)) != '<')
      if(a == EOF) {
        fclose(f);
        return;
      }

    /* read in html tag, closed by > */
    b=0;
    while((a=getc(f)) != '>') {
      if(a == EOF) {
        fclose(f);
        return;
      }
      if(b == 63 || a == '\n' || a == '\r')
        break;
      buf[b++]=a;
    }
    pos=ftell(f);
    buf[b]=0;
    if(!wild_match(buf, "a name=*", 0, 1))
      continue;

    /* we got a match. remove surrounding "s */
    r=wptr[0];
    if(*r == '"') {
      r++;
      if((s=strchr(r, '\"')))
        *s++='\0';
    } else if((s=strchr(r, ' ')))
      *s++='\0';

    if(helpsize >= size) {
      size += 50;
      if(!(ptr=realloc(helpindx, size * sizeof(struct helpindx)))) {
        perror("Help Index");
        for(a=0;a<helpsize;a++)
          free(helpindx[a].name);
        free(helpindx);

        helpindx=NULL;
        helpsize=0;
        fclose(f);
        return;
      }
      helpindx=ptr;
    }

    SPTR(helpindx[helpsize].name, r);
    helpindx[helpsize].pos=pos;
    helpindx[helpsize].wiz=(s && !strncasecmp(s, "type=wiz", 8));
    helpsize++;
  }
}

/* display a specific helpfile topic */
void do_help(dbref player, char *req)
{
  FILE *f;
  struct io_queue *filelist=NULL, *new, *last=NULL;
  char buf[512];
  int a, mark=-1, count=0, colsize=4;

  /* initialize helptext if first time through or if file has been updated */
  if(!helpindx || !helpsize || helpdate < fdate("msgs/helptext.html"))
    init_helptext();

  if(!helpsize || !(f=fopen("msgs/helptext.html", "rb"))) {
    notify(player, "Helptext currently unavailable.");
    return;
  }

  /* request hypertext version information */
  if(*req == '?') {
    notify(player, "Running TinyMARE HTML 3.1 Emulation");
    a=(int)fsize("msgs/helptext.html");
    notify(player, tprintf("helptext.html: %d Topics Available, %d Byte%s.",
           helpsize, a, a==1?"":"s"));
    notify(player, "");
    while(ftext(buf, 512, f) && *buf == '<' && *(buf+1) == '!')
      notify(player, buf);
    fclose(f);
    return;
  }

  if(!*req)
    req="helptext";  /* default help string */

  /* search for matching topics */
  for(a=0;a<helpsize;a++) {
    if(helpindx[a].wiz && !Immortal(player))
      continue;
    if(!strcasecmp(helpindx[a].name, req))
      break;
    if(string_prefix(helpindx[a].name, req)) {
      if(mark == -1)
        mark=a;
      count++;
      new=malloc(sizeof(struct io_queue)+strlen(helpindx[a].name)+1);
      new->next=NULL;
      strcpy(new->text, helpindx[a].name);
      if(!last)
        filelist=new;
      else
        last->next=new;
      last=new;
      if(strlen(helpindx[a].name)+4 > colsize)
        colsize=strlen(helpindx[a].name)+4;
    }
  }

  /* check match request */
  if(a == helpsize) {
    if(!count) {
      notify(player, "No topics match request.");
      fclose(f);
      return;
    } if(count > 99) {
      notify(player, tprintf("There are %d entries matching your request. "
             "Please refine your selection.", count));
      /* free up list */
      for(last=filelist;last;last=new) {
        new=last->next;
        free(last);
      }
      fclose(f);
      return;
    } if(count > 1) {
      notify(player, tprintf("Ambiguous match. Please pick from these %d topics:", count));
      notify(player, "");
      showlist(player, filelist, colsize);
      fclose(f);
      return;
    }
    a=mark;
  }

  fseek(f, helpindx[a].pos, SEEK_SET);
  html_parse(player, f);
  fclose(f);
}

void html_parse(dbref player, FILE *f)
{
  char buf[1024], line[8192], word[1024], *str, *wp, *endp;
  int maxcol, text=0, linebreak=0, blanklines=0, wlen=0, llen=0;
  int color=7, oldcolor=7, startcolor=7, begincolor=7;

  /* initialize variables */
  if((maxcol=get_cols(NULL, player)-2) < 38)
    maxcol=38;
  *line='\0';
  wp=word;

  /* now process each line of the incoming text */

  while(linebreak >= 0 && fgets(buf, 1024, f))
    for(str=buf;*str;str++) {
      /* artifical space after words that are bigger than maxcol */
      if(str > buf && wlen >= maxcol)
        *(--str)=' ';

      /* ampersand parsing */
      if(*str == '&') {
        if(*(str+1) == '#') {  /* &#000; digit form */
          if(atoi(str+2)) {
            *wp++=atoi(str+2);
            wlen++;
          }
          str++;
          while(isdigit(*(str+1)))
            str++;
        }
        else if(!strncasecmp(str+1, "lt", 2)) { *wp++='<'; wlen++; str+=2; }
        else if(!strncasecmp(str+1, "gt", 2)) { *wp++='>'; wlen++; str+=2; }
        else if(!strncasecmp(str+1, "amp", 3)) { *wp++='&'; wlen++; str+=3; }
        else if(!strncasecmp(str+1, "quot", 4)) { *wp++='"'; wlen++; str+=4; }
        else if(!strncasecmp(str+1, "nbsp", 4)) { *wp++=' '; wlen++; str+=4; }
        else if(!strncasecmp(str+1, "laquo", 5)) { *wp++=174; wlen++; str+=5; }
        else if(!strncasecmp(str+1, "raquo", 5)) { *wp++=175; wlen++; str+=5; }
        else if(!strncasecmp(str+1, "copy", 4)) {
          *wp++='('; *wp++='C'; *wp++=')'; wlen+=3; str+=4;
        }

        if(*(str+1) == ';')
          str++;
        continue;
      }

      /* html tag indexing */
      if(*str == '<' && (endp=strchr(str, '>'))) {
        ++str;
        *endp='\0';
        if(*str == '/') {
          str++;
          if(match_word("a font", str)) {
            strcpy(wp, textcolor(color, 7));
            wp+=strlen(wp); color=7;
          } else if(!strcasecmp(str, "b")) {
            strcpy(wp, textcolor(color, color&~8));
            wp+=strlen(wp); color&=~8;
          } else if(!strcasecmp(str, "u")) {
            strcpy(wp, textcolor(color, color&~256));
            wp+=strlen(wp); color&=~256;
          } else if(match_word("table ul", str)) {
            blanklines=0; linebreak=2;
          }
        } else {
          if(wild_match(str, "a name=*", 0, 0)) linebreak=-1;
          else if(!strcasecmp(str, "br"))
            linebreak=1;
          else if(!blanklines && match_word("li tr", str))
            linebreak=1;
          else if(!strncasecmp(str, "table", 5)) {
            blanklines=0; linebreak=2;
          } else if(!strcasecmp(str, "p")) {
            blanklines=0; linebreak=2;
          } else if(!strncasecmp(str, "a ", 2)) {
            oldcolor=color;
            strcpy(wp, textcolor(color, 268));
            wp+=strlen(wp); color=268;
          } else if(!strcasecmp(str, "b")) {
            strcpy(wp, textcolor(color, color|8));
            wp+=strlen(wp); color|=8;
          } else if(!strcasecmp(str, "u")) {
            strcpy(wp, textcolor(color, color|256));
            wp+=strlen(wp); color|=256;
          } else if(wild_match(str, "font color=*", 0, 1)) {
            char temp[256], *s=temp;

            oldcolor=color;
            strcpy(temp, wptr[0]);
            if(*s == '\"') s++;
            if(*s == '#') s++;
            if(*s > '7' || *(s+2) > '7' || *(s+4) > '7')
              color=8;
            else
              color=0;

            if(*s > '1')
              color|=1;
            if(*(s+2) > '1')
              color|=2;
            if(*(s+4) > '1')
              color|=4;

            strcpy(wp, textcolor(oldcolor, color));
            wp+=strlen(wp);
          }
        }

        /* continue reading after the ending > of the tag */
        str=endp;
        if(!linebreak)
          continue;
      }

      /* end of word. add the word to the line */
      if(linebreak || isspace(*str) || *str == '\r' || *str == '\n') {
        *wp='\0';
        if(llen && llen+wlen+1 > maxcol) {
          notify(player, tprintf("%s%s\e[m", textcolor(7, begincolor), line));
          begincolor=startcolor;
          *line='\0';
          llen=0;
        }

        if(*word) {
          if(llen) {
            strcat(line, " ");
            llen++;
          }
          strcat(line, word);
          llen+=wlen;
        }

        if(linebreak == -1)
          break;
        if(linebreak > 0) {
          if(text)
            blanklines+=linebreak;
          linebreak=0;
        }

        wp=word;
        wlen=0;
        startcolor=color;
        continue;
      }

      while(blanklines > 0) {
        notify(player, tprintf("%s%s\e[m", textcolor(7, begincolor), line));
        *line='\0';
        llen=0;
        begincolor=startcolor;
        blanklines--;
      }

      /* Just a normal letter. Add to the word. */
      *wp++=*str;
      text=1;
      wlen++;
    }

  if(llen)
    notify(player, tprintf("%s%s\e[m", textcolor(7, begincolor), line));
}


/* Translate a text URL into one with symbol characters escaped */
char *expand_url(char *t)
{
  const char *esc_chars="<>#%{}|\\^~[]';?:@=&\"`+";
  static char buf[1024];
  char *s=buf;

  for(;*t && s-buf < 1021;t++)
    if(strchr(esc_chars, *t)) {
      sprintf(s, "%%%02x", *t);
      s+=3;
    } else if(*t >= 32 && *t < 127)
      *s++=(*t == ' ') ? '+' : *t;

  *s='\0';
  return buf;
}

/* Parse an escaped URL into a full-text one with all symbols included */
char *parse_url(char *t)
{
  static char buf[1024];
  char c, *s=buf;

  for(;*t && s-buf < 1023;t++)
    if(*t == '%' && isxdigit(*(t+1)) && isxdigit(*(t+2))) {
      if(*++t <= '9')
        c = (*t-'0') << 4;
      else
        c = (toupper(*t)-'A'+10) << 4;

      if(*++t <= '9')
        c |= (*t-'0');
      else
        c |= (toupper(*t)-'A'+10);

      if(c >= 32 && c < 127)
        *s++=c;
    } else
      *s++=(*t == '+') ? ' ' : *t;

  *s='\0';
  return buf;
}

/* Write HTML output to user and disconnect */
int html_request(DESC *d, char *msg)
{
  char request[256], *r, *s;
  int found=1;

  /* ignore anything other than a html request */
  if(strncmp(msg, "GET ", 4))
    return 0;

  log_io(tprintf("HTML Request %d: %s. \"%s\"", d->concid, d->addr, msg));

  msg+=4;
  r=s=strspc(&msg);

  while(*s == '/')               /* remove leading /'s */
    s++;
  strmaxcpy(request, s, 256);
  s=strnul(request);
  while(s > request && *(s-1) == '/')  /* remove excess /'s */
    *--s='\0';

  /* search string for matches */
  if(!*request || !strcasecmp(request, "who"))
    html_who(d);
  else if(!ok_filename(request))
    found=0;
  else        /* Future Expansion */
    found=0;

  /* Print HTTP/1.0 File Header */
  if(*msg) {
    output2(d, tprintf("HTTP/1.0 %s\n", found?"200 OK":"404 Not Found"));
    output2(d, tprintf("Date: %s\n", strtime(now)));
    output2(d, tprintf("Server: %s\n", mud_version));
    output2(d, "Content-Type: text/html\n\n");
  }

  if(!found)
    output2(d, tprintf("<HTML><HEAD>\n<TITLE>404 Not Found</TITLE>\n"
               "</HEAD><BODY>\n<H1>Not Found</H1>\n"
               "The requested URL %s was not found on this server.<P>\n"
               "<HR>\n<ADDRESS>%s Server (%s) at %s Port %d</ADDRESS>\n"
               "</BODY></HTML>\n", r, mud_version, MUD_NAME,
	       localhost(), d->socket));

  return 0;
}

void html_who(DESC *d)
{
  DESC *w;
  char *doing, *s, buf[100];
  int gend, line=0, total=0, hidden=0, admin=0;

  output(d, tprintf("<HTML><HEAD>\n<TITLE>%s %d Who List</TITLE></HEAD>\n\n",
                    MUD_NAME, tinyport));
  output(d, "<body bgcolor=#000000 text=#8bff00>\n");
  output(d, tprintf("<center><b><font size=+4>%s %d Who List</font></b></center><p>\n",
                    MUD_NAME, tinyport));

  if(*(s=atr_get(0, A_DOING))) {
    output(d, "<font size=+1 color=#ff5555>Quote of the Day: ");
    output(d, tprintf("%s\n</font><p>\n\n", s));
  }
#ifdef COMBAT_SYS
  if(COMBAT)
    output(d, "<pre width=78><u><font color=#ff55ff>Name          Online  Idle  Level       G   Occupation  Doing                 </font></u>\n");
  else
#endif
    output(d, "<pre width=78><u><font color=#ff55ff>Name              Alias     Online  Idle  Doing                               </font></u>\n");

  for(w=descriptor_list;w;w=w->next) {
    if(!(w->flags & C_CONNECTED))
      continue;
    total++;
    if(*atr_get(w->player, A_HLOCK)) {
      hidden++;
      continue;
    }
    if(Immortal(w->player))
      admin++;

    gend=db[w->player].data->gender;
    doing=ansi_strip(atr_parse(w->player, w->player, A_DOING));
    for(s=doing;*s;s++)
      if(*s < 32)
        *s=' ';

    strcpy(buf, time_format(TMS, now-w->last_time));

#ifdef COMBAT_SYS
    if(COMBAT)
      output(d, tprintf("<font color=%s>%-9.9s  %9.9s  %4.4s  %-10.10s  %c  "
             "%c%-11.11s %-22.22s</font>\n",
           (line^=1)?"#55ffff":"#00aaaa", db[w->player].name,
           time_format(TML, now-w->login_time), buf,
           levelname(getbitmap(w->player, B_LEVEL), gend), gend?'F':'M',
           (w->flags & C_OUTPUTOFF)?'&':Immortal(w->player)?'*':' ',
           guild_to_name(getbitmap(w->player, B_GUILD)), doing));
    else
#endif
      output(d, tprintf("<font color=%s>%-16.16s  %-5.5s  %9.9s  "
             "%4.4s  %-36.36s</font>\n",
           (line^=1)?"#55ffff":"#00aaaa", db[w->player].name,
           atr_get(w->player, A_ALIAS), time_format(TML, now-w->login_time),
           buf, doing));
  }

  output(d, "<font color=#ffffff>---</font>\n");

  if(RESTRICT_HIDDEN)
    total-=hidden;
  sprintf(buf, "Total Players: %d", total);
  if(admin)
    strcat(buf, tprintf("   Administrators: %d", admin));
  if(hidden && !RESTRICT_HIDDEN)
    strcat(buf, tprintf("   Hidden: %d", hidden));
  output(d, tprintf("<font color=#55ff55>%s.  </font><font color=#ffff55>%s</font>\n</pre><p>",
         buf, grab_time(NOTHING)));
  output(d, "</body></HTML>\n");
}
