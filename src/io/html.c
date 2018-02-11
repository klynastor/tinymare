/* io/html.c */
/* Routines for processing HTML v3.1 */

#include "externs.h"

static void html_who(DESC *d);


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

  log_io("HTML Request %d: %s. \"%s\"", d->concid, d->addr, msg);

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

static void html_who(DESC *d)
{
  DESC *w;
  char *doing, *s, buf[100];
  int line=0, total=0, hidden=0, admin=0;

  output(d, tprintf("<HTML><HEAD>\n<TITLE>%s %d Who List</TITLE></HEAD>\n\n",
                    MUD_NAME, tinyport));
  output(d, "<body bgcolor=#000000 text=#8bff00>\n");
  output(d, tprintf("<center><b><font size=+4>%s %d Who List</font></b></center><p>\n",
                    MUD_NAME, tinyport));

  if(*(s=atr_get(0, A_DOING))) {
    output(d, "<font size=+1 color=#ff5555>Quote of the Day: ");
    output(d, tprintf("%s\n</font><p>\n\n", s));
  }
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

    doing=strip_ansi(atr_parse(w->player, w->player, A_DOING));
    for(s=doing;*s;s++)
      if(*s < 32)
        *s=' ';

    strcpy(buf, time_format(TMS, now-w->last_time));

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
