/* mare/utils.c */
/* Miscellaneous utility functions used throughout netmare */

#include "externs.h"
#include <math.h>


/* Time Display Functions */

/* Timezone List */
static struct {
  char *zone;
  int have_dst;
  int utc_offset;
  char *name;
} tzones[]={
  {"GMT",  0,     0, "Greenwich Mean"},
  {"UT",   0,     0, "Universal Coordinated"},
  {"UTC",  0,     0, "Universal Coordinated"},
  {"WET",  0,     0, "Western European"},
  {"BST",  1,     0, "British Summer"},
  {"CET",  0,  +100, "Central European"},
  {"MET",  0,  +100, "Middle European"},
  {"MEWT", 0,  +100, "Middle European Winter"},
  {"MEST", 1,  +100, "Middle European Summer"},
  {"SWT",  0,  +100, "Swedish Winter"},
  {"SST",  1,  +100, "Swedish Summer"},
  {"FWT",  0,  +100, "French Winter"},
  {"FST",  1,  +100, "French Summer"},
  {"EET",  0,  +200, "Eastern Europe"},
  {"BT",   0,  +300, "Baghdad"},
  {"IT",   0,  +330, "Iran"},
  {"ZP4",  0,  +400, "Russia Zone 3"},
  {"ZP5",  0,  +500, "Russia Zone 4"},
  {"IST",  0,  +530, "Indian Standard"},
  {"ZP6",  0,  +600, "Russia Zone 5"},
  {"NSUT", 0,  +630, "North Sumatra"},
  {"WAST", 0,  +700, "West Australia Standard"},
  {"WADT", 1,  +700, "West Australia Daylight"},
  {"SSUT", 0,  +700, "South Sumatra"},
  {"JT",   0,  +730, "Java"},
  {"WST",  0,  +800, "Western Australia Standard"},
  {"WDT",  1,  +800, "Western Australia Daylight"},
  {"CCT",  0,  +800, "China Coast"},
  {"MT",   0,  +830, "Moluccas"},
  {"JST",  0,  +900, "Japan Standard"},
  {"KST",  0,  +900, "Korea Standard"},
  {"CAST", 0,  +930, "Central Australian Standard"},
  {"CADT", 1,  +930, "Central Australian Daylight"},
  {"SAT",  0,  +930, "Southern Australian Standard"},
  {"SADT", 1,  +930, "Southern Australian Daylight"},
  {"EAST", 0, +1000, "Eastern Australian Standard"},
  {"EADT", 1, +1000, "Eastern Australian Daylight"},
  {"GST",  0, +1000, "Guam Standard"},
  {"NZST", 0, +1200, "New Zealand Standard"},
  {"NZDT", 1, +1200, "New Zealand Daylight"},
  {"NZT",  0, +1200, "New Zealand"},
  {"IDLE", 0, +1200, "International Date Line East"},

  {"WAT",  0,  -100, "West Africa"},
  {"AT",   0,  -200, "Azores"},
  {"BRST", 0,  -300, "Brazil Standard"},
  {"GLST", 0,  -300, "Greenland Standard"},
  {"NFT",  0,  -330, "Newfoundland Standard"},
  {"NST",  0,  -330, "Newfoundland Standard"},
  {"NDT",  1,  -330, "Newfoundland Daylight"},
  {"AST",  0,  -400, "Atlantic Standard"},
  {"ADT",  1,  -400, "Atlantic Daylight"},
  {"EST",  0,  -500, "Eastern Standard"},
  {"EDT",  1,  -500, "Eastern Daylight"},
  {"CST",  0,  -600, "Central Standard"},
  {"CDT",  1,  -600, "Central Daylight"},
  {"MST",  0,  -700, "Mountain Standard"},
  {"MDT",  1,  -700, "Mountain Daylight"},
  {"PST",  0,  -800, "Pacific Standard"},
  {"PDT",  1,  -800, "Pacific Daylight"},
  {"YST",  0,  -900, "Yukon Standard"},
  {"YDT",  1,  -900, "Yukon Daylight"},
  {"HST",  0, -1000, "Hawaii Standard"},
  {"HDT",  1, -1000, "Hawaii Daylight"},
  {"AHST", 0, -1000, "Alaska-Hawaii Standard"},
  {"AHDT", 1, -1000, "Alaska-Hawaii Daylight"},
  {"CAT",  0, -1000, "Central Alaska"},
  {"NT",   0, -1100, "Nome"},
  {"IDLW", 0, -1200, "International Date Line West"},
  {0}};

static char *months[12]={
 "January", "February", "March", "April", "May", "June",
 "July", "August", "September", "October", "November", "December"};

static char *days[7]={
 "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

static char *gamedays[7]={
 "Luna", "Jinn", "Undine", "Dryad", "Sylphid", "Gaia", "Solaris"};

/* Retrieve game (weather) time */
char *wtime(unsigned long td, int typ)
{
  static char buf[40];
  char temp[10];
  int sec, day, month, year;
  quad tt;

  if(HOURS_PER_DAY < 1)
    HOURS_PER_DAY=24;
  if(BEGINTIME > now)
    BEGINTIME=now;

  if(td == -1) {
    struct timeval current;
    gettimeofday(&current, NULL);
    tt=((quad)current.tv_sec-BEGINTIME)*24/HOURS_PER_DAY;
    tt+=((quad)current.tv_usec*24/HOURS_PER_DAY)/1000000;
  } else
    tt=(quad)td*24/HOURS_PER_DAY;

  year=tt/31449600;
  sec=tt%31449600;
  day=sec/86400;
  sec%=86400;
  month=(day<180)?day/30:(day==180 || day==181)?5:(day>181 && day<362)?
        (day-2)/30:11;

  if(typ)
    sprintf(buf, "%s, ", gamedays[day%7]);
  else
    sprintf(buf, "%-3.3s ", gamedays[day%7]);
  sprintf(temp, "%02d:%02d:%02d", sec/3600, (sec%3600)/60, sec%60);
  if(typ)
    strcat(buf, tprintf("%s %d, %s%d", months[month], ((day>181?day-2:day)-
           (month*30))+1, (year+FIRST_YEAR) < 10?"Year ":"", year+FIRST_YEAR));
  else
    strcat(buf, tprintf("%-3.3s %2d %s %04d", months[month],
           ((day>181?day-2:day)-(month*30))+1, temp, year+FIRST_YEAR));
  return buf;
}

/* Convert time() output into full date format */
char *mkdate(char *str)
{
  static char buf[32];
  int a;

  *buf=0;
  for(a=0;a<7;a++)
    if(!strncmp(days[a], str, 3)) {
      sprintf(buf, "%s, ", days[a]);
      break;
    }

  for(a=0;a<12;a++)
    if(!strncmp(months[a], str+4, 3)) {
      sprintf(buf+strlen(buf), "%s %d, %04d", months[a], atoi(str+8),
              atoi(str+20));
      break;
    }

  return buf;
}

/* Convert time() output into "mm/dd/yyyy" format */
char *filter_date(char *string)
{
  static char buf[12];
  int a;

  for(a=0;a<12;a++)
    if(!strncmp(string+4, months[a], 3))
      break;
  sprintf(buf, "%02d/%02d/%04d", a+1, atoi(string+8), atoi(string+20));
  return buf;
}

/* Converts # of seconds into readable time format */
char *strtime(long tt)
{
  char *s=ctime((time_t *)&tt);

  s[strlen(s)-1]='\0';
  return s;
}

// Internal function to store a string-representation of tt.nano, based on
// user's choice.
//
void timefmt(char *buf, char *fmt, long tt, int usec)
{
  struct tm *tm=localtime(&tt);
  char *s=buf, *t=fmt;

  for(t=fmt;*t && s-buf < 8000;t++) {
    if(!isalpha(*t)) {
      *s++=*t;
      continue;
    }
    switch(*t) {
      case 'd': s+=sprintf(s, "%02d", tm->tm_mday); break;
      case 'D': s+=sprintf(s, "%.3s", days[(tm->tm_wday+6) % 7]); break;
      case 'e': s+=sprintf(s, "%2d", tm->tm_mday); break;
      case 'F': s+=sprintf(s, "%s", months[tm->tm_mon]); break;
      case 'g': s+=sprintf(s, "%d", tm->tm_hour); break;
      case 'G': s+=sprintf(s, "%d", ((tm->tm_hour+11) % 12)+1); break;
      case 'h': s+=sprintf(s, "%02d", tm->tm_hour); break;
      case 'H': s+=sprintf(s, "%02d", ((tm->tm_hour+11) % 12)+1); break;
      case 'i': s+=sprintf(s, "%02d", tm->tm_min); break;
      case 'I': s+=sprintf(s, "%d", tm->tm_isdst == 1); break;
      case 'j': s+=sprintf(s, "%d", tm->tm_yday+1); break;
      case 'J': s+=sprintf(s, "%d", tm->tm_mday); break;
      case 'm': s+=sprintf(s, "%02d", tm->tm_mon+1); break;
      case 'M': s+=sprintf(s, "%.3s", months[tm->tm_mon]); break;
      case 'n': s+=sprintf(s, "%d", tm->tm_mon+1); break;
      case 'p': s+=sprintf(s, "%cm", tm->tm_hour < 12?'a':'p'); break;
      case 'P': s+=sprintf(s, "%cM", tm->tm_hour < 12?'A':'P'); break;
      case 's': s+=sprintf(s, "%02d", tm->tm_sec); break;
      case 'S': s+=sprintf(s, (tm->tm_mday >= 11 && tm->tm_mday <= 13)?"th":
                    (tm->tm_mday % 10 == 1)?"st":(tm->tm_mday % 10 == 2)?"nd":
                    (tm->tm_mday % 10 == 3)?"rd":"th"); break;
      case 'w': s+=sprintf(s, "%d", tm->tm_wday); break;
      case 'W': s+=sprintf(s, "%s", days[(tm->tm_wday+6) % 7]); break;
      case 'x': s+=sprintf(s, "%ld", tt); break;
      case 'u': s+=sprintf(s, "%06d", usec%1000000); break;
      case 'y': s+=sprintf(s, "%02d", tm->tm_year%100); break;
      case 'Y': s+=sprintf(s, "%d", tm->tm_year+1900); break;
      default:
        *s++=*t;
    }
  }

  *s='\0';
  buf[8000]='\0';
}

/* Set player's individual timezone for use with mktm() */
void do_tzone(dbref player, char *arg1)
{
  int tz=atoi(arg1), dst=HAVE_DST;
  char *s;

  if(Typeof(player) != TYPE_PLAYER) {
    notify(player, "Only players may change their timezones.");
    return;
  } if(!*arg1) {
    notify(player, "Usage: +tzone <gmt offset>:<dst? y|n>");
    notify(player, "%s", "");
    notify(player, "You are currently set at: %d:%c",
           db[db[player].owner].data->tz,
           db[db[player].owner].data->tzdst ? 'y':'n');
    return;
  }

  if(tz < -12 || tz > 13 || (!isdigit(*arg1) && *arg1 != '-')) {
    notify(player,
           "Offset from Greenwich Mean Time can only be from -12 to 13.");
    return;
  }
  if((s=strchr(arg1, ':')))
    dst=(*++s == 'y' || *s == 'Y')?1:0;

  db[db[player].owner].data->tz=tz;
  db[db[player].owner].data->tzdst=dst;
  notify(player, "Local time set to: %s", mktm(player, now));

}

/* Converts # of seconds into readable time string w/timezone setting */
char *mktm(dbref thing, long tt)
{
  long utcdiff;

  /* Calculate difference from UTC, if present */
  utcdiff = (((long)db[db[thing].owner].data->tz)-((long)TIMEZONE))*3600;

  /* Calculate daylight savings time differences */
  if(localtime((time_t *)&tt)->tm_isdst)
    utcdiff += ((long)db[db[thing].owner].data->tzdst-(HAVE_DST?1:0))*3600;

  tt += utcdiff;

  /* Generate ascii string */
  return strtime(tt);
}

/* Convert time representation in numbers & words into #seconds */
long get_date(char *str, dbref thing, long epoch)
{
#if 0
  static struct {
    char *word;
    int index;
    int value;
  } keyword[]={
    {"seconds",    5, 1},
    {"minutes",    4, 1},
    {"hours",      3, 1},
    {"days",       2, 1},
    {"weeks",      2, 7},
    {"fortnights", 2, 14},
    {"months",     1, 1},
    {"years",      0, 1},
    {"score",      0, 20},
    {0}};

  static char *ord[15]={
    "last", "this", "next", "first", "second", "third", "fourth", "fifth",
    "sixth", "seventh", "eighth", "ninth", "tenth", "eleventh", "twelfth"};

  static int ordval[15]={-1, 0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
#endif
  static int month_days[12]={31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  struct tm tm;
  char ch, buf[strlen(str)+1], *r, *s, *t;
  int a, x, tm_isset[6]={0}, tm_rel[6]={0};
  long tt;

  strcpy(buf, str);

  /* Null string = current time */
  if(!*buf)
    return now;

  /* String contains # seconds since epoch */
  for(s=buf;(isdigit(*s) || *s == '.' || (s == buf && *s == '-'));s++);
  if(!*s)
    return atol(buf)+epoch;

  /* Set default time value based on player's active timezone */
  memcpy(&tm, localtime((time_t *)&now), sizeof(struct tm));

  s=buf;
  while((r=strspc(&s))) {
    /* No-effect keywords */
    if(!*r || match_word("from to of on in and now today", r))
      continue;

    /* Special keywords */
    if(!strcasecmp(r, "ago")) {
      for(a=0;a<6;a++)
        tm_rel[a]=-tm_rel[a];
      continue;
    } if(!strcasecmp(r, "tomorrow")) {
      tm_rel[2]++;
      continue;
    } if(!strcasecmp(r, "yesterday")) {
      tm_rel[2]--;
      continue;
    } if(match_word("a.m. am", r)) {
      if(tm_isset[5]++ || !tm_isset[3])
        return -1;
      if(tm.tm_hour < 1 || tm.tm_hour > 12)
        return -1;
      if(tm.tm_hour == 12)
        tm.tm_hour=0;
      continue;
    } if(match_word("p.m. pm", r)) {
      if(tm_isset[5]++ || !tm_isset[3])
        return -1;
      if(tm.tm_hour < 1 || tm.tm_hour > 12)
        return -1;
      if(tm.tm_hour != 12)
        tm.tm_hour+=12;
      continue;
    }

    /* Strip punctuation symbols */
    if(r[strlen(r)-1] == ',')
      r[strlen(r)-1] = '\0';
    if(r[strlen(r)-1] == '.')
      r[strlen(r)-1] = '\0';

    /* Parse date mm/dd/yyyy format */
    if(strchr(r, ch='/') || strchr(r, ch='-') || strchr(r, ch='.')) {
      /* Month */
      if(tm_isset[1]++)
        return -1;
      t=strchr(r, ch);
      *t++='\0';
      tm.tm_mon=atoi(r)-1;
      if(tm.tm_mon < 0)
        for(a=0;a<12;a++)
          if(!strcasecmp(months[a], r) ||
             (strlen(r) == 3 && !strncasecmp(months[a], r, 3))) {
            tm.tm_mon=a;
            break;
          }
      if(tm.tm_mon < 0 || tm.tm_mon > 11)
        return -1;

      /* Day */
      if(tm_isset[2]++)
        return -1;
      if((tm.tm_mday=atoi(t)) < 1)
        return -1;

      /* Year */
      if((r=strchr(t, ch))) {
        if(tm_isset[0]++)
          return -1;
        r++;
        tm.tm_year=atoi(r);
        if(tm.tm_year < 0)
          return -1;
        if(tm.tm_year < 40)
          tm.tm_year+=100;
        else if(tm.tm_year >= 100 && tm.tm_year < 1900)
          return -1;
        else if(tm.tm_year >= 1900)
          tm.tm_year-=1900;
      }

      /* Blank out hh:mm:ss if not yet specified */
      if(!tm_isset[3])
        tm.tm_hour=tm.tm_min=tm.tm_sec=0;

      continue;
    }

    /* Parse time hh:mm:ss format */
    if(strchr(r, ':')) {
      if(tm_isset[3]++)
        return -1;

      /* Hour */
      tm.tm_hour=atoi(r);
      if(tm.tm_hour < 0 || tm.tm_hour > 23)
        return -1;

      /* Minute */
      r=strchr(r, ':')+1;
      tm.tm_min=atoi(r);
      if(tm.tm_min < 0 || tm.tm_min > 59)
        return -1;

      /* Second */
      if(strchr(r, ':')) {
        r=strchr(r, ':')+1;
        tm.tm_sec=atoi(r);
        if(tm.tm_sec < 0 || tm.tm_sec > 59)
          return -1;
      } else
        tm.tm_sec=0;

      /* Set A.M. or P.M. */
      if(strchr(r, 'a') || strchr(r, 'A')) {
        tm_isset[5]=1;
        if(tm.tm_hour < 1 || tm.tm_hour > 12)
          return -1;
        if(tm.tm_hour == 12)
          tm.tm_hour=0;
      } else if(strchr(r, 'p') || strchr(r, 'P')) {
        tm_isset[5]=1;
        if(tm.tm_hour < 1 || tm.tm_hour > 12)
          return -1;
        if(tm.tm_hour != 12)
          tm.tm_hour+=12;
      }
      continue;
    }

    /* Match relative time displacement */

    /* Ordinal values */

    /* Set day of month or year */
    if(isdigit(*r)) {
      if((x=atoi(r)) < 1)
        return -1;
      if(x < 1900) {  /* Try setting the date */
        if(tm_isset[2]++)
          return -1;
        tm.tm_mday=x;
        continue;
      }
      if(tm_isset[0]++)  /* Must be a year */
        return -1;
      tm.tm_year=x-1900;
      continue;
    }

    /* Scan weekdays */
    for(a=0;a<7;a++)
      if(!strcasecmp(days[a], r) ||
         (strlen(r) == 3 && !strncasecmp(days[a], r, 3)))
        break;
    if(a != 7)
      continue;

    /* Scan monthnames */
    if(!tm_isset[1]) {
      for(a=0;a<12;a++)
        if(!strcasecmp(months[a], r) ||
           (strlen(r) == 3 && !strncasecmp(months[a], r, 3)))
          break;
      if(a != 12) {
        tm.tm_mon=a;
        tm_isset[1]=1;
        continue;
      }

      /* Blank out hh:mm:ss if not yet specified */
      if(!tm_isset[3])
        tm.tm_hour=tm.tm_min=tm.tm_sec=0;
    }

    /* Scan timezones for a match */
    if(!tm_isset[4]++) {
      for(a=0;tzones[a].name;a++)
        if(!strcasecmp(tzones[a].zone, r))
          break;
      if(tzones[a].name)
        continue;
    }

    return -1;  /* No keyword match */
  }

  /* Check day of month */
  if(tm.tm_mday > month_days[tm.tm_mon] || (tm.tm_mon == 1 &&
     tm.tm_mday == month_days[1] && ((tm.tm_year+1900) % 4 ||
     (!(tm.tm_year % 100) && (tm.tm_year+1900)/100 % 4))))
    return -1;

  tm.tm_sec   += tm_rel[5];
  tm.tm_min   += tm_rel[4];
  tm.tm_hour  += tm_rel[3];
  tm.tm_mday  += tm_rel[2];
  tm.tm_mon   += tm_rel[1];
  tm.tm_year  += tm_rel[0];
  tm.tm_isdst = -1;

  return tt=mktime(&tm);
}

// Prints # of seconds in one of three formats. Format =
//  0:  17d, 16h, 45m	All time values, TMA
//  1:  17d 16:45	Online column in who list, TML
//  2:  2w		Idle column in who list, TMS
//  3:  2 weeks		Full format, TMF
//  4:  1 week          Full format, TMF2 (no 'a' or 'an')
//
char *time_format(int format, long dt)
{
  const char *ys[6]={"year", "week", "day", "hour", "minute", "second"};
  const long ts[6]={31536000, 604800, 86400, 3600, 60, 1};

  static char buf[64];
  long amt=0;
  int i;
  
  if(dt < 0)
    dt=0;

  switch(format) {
  case 0:
    for(*buf='\0',i=0;i<6;i+=i?1:2)  /* Skip identifier for weeks */
      if((amt=dt/ts[i]) || (i == 5 && !*buf)) {
        sprintf(buf+strlen(buf), "%s%ld%c", *buf?", ":"", amt, *ys[i]);
        dt-=amt*ts[i];
      }
    return buf;
  case 1:
    if(dt/86400 > 999)
      return "999:23:59";
    else if(dt/86400 > 99)
      sprintf(buf, "%ld:%02ld:%02ld", dt/86400, dt/3600%24, dt/60%60);
    else if(dt/86400)
      sprintf(buf, "%ldd %02ld:%02ld", dt/86400, dt/3600%24, dt/60%60);
    else
      sprintf(buf, "%02ld:%02ld", dt/3600, dt/60%60);
    return buf;
  }

  for(i=0;i<6;i++)
    if((amt=dt/ts[i]) || i == 5)
      break;

  if(format == 2)
    sprintf(buf, "%ld%c", amt, *ys[i]);
  else if(format == 3 && amt == 1)
    sprintf(buf, "%s %s", (i == 3)?"an":"a", ys[i]);
  else
    sprintf(buf, "%ld %ss", amt, ys[i]);

  return buf;
}

char *grab_time(dbref player)
{
  static char buf[16];
  char *s=Invalid(player)?strtime(now):mktm(player, now);

  sprintf(buf, "[%d%-3.3s %c.m.]", (atoi(s+11)+11)%12+1, s+13,
          (atoi(s+11) > 11)?'p':'a');
  return buf;
}


/* Built-in versions of some library functions */

#undef atof  /* externs.h */

/* Convert a floating-point string to a double, filtering out exponents */
double atof2(const char *str)
{
  char buf[101], *s;

  /* Some versions of the C-Library crash when converting long floating-point
     strings to double. So, we only copy the first 100 characters. */
  strmaxcpy(buf, str, 101);

  /* Filter exponents */
  if((s=strchr(buf, 'e')))
    *s='\0';
  if((s=strchr(buf, 'E')))
    *s='\0';
  return atof(buf);
}

#if !defined(__GLIBC__) && !(defined(__sun__) && !defined(_SUN_4))
/* 64-bit ascii-to-long-long */
quad builtin_atoll(const char *buf)
{
  quad value=0;
  int neg=0;

  while(isspace(*buf))
    buf++;

  if(*buf == '-')
    neg=1, buf++;
  else if(*buf == '+')
    buf++;

  while(isdigit(*buf)) {
    value *= 10;
    if(value < 0)  /* check for overflow */
      return neg?0x8000000000000000LL:0x7fffffffffffffffLL;
    value += *(buf++)-'0';
  }

  return neg?-value:value;
}
#endif

#ifdef _SUN_4
// Unoptimized function to copy a [possibly] overlapping memory area from <src>
// to <dest>.
//
void *builtin_memmove(void *dest, void *src, int len)
{
  char *d=dest, *s=src;
  int i;

  if(d > s)
    for(i=0;i < len;i++)
      d[i]=s[i];
  else if(d < s)
    for(i=len-1;i >= 0;i--)
      d[i]=s[i];

  return dest;
}
#endif


/* String helper utility functions */

/* Convert <x,y> coordinate to direction from <0,0> in degrees */
double degrees(double x, double y)
{
  double var;

  if(!x)
    return (y > 0)?90:(y < 0)?270:0;
  var=90 * atan(y/x) / asin(1);
  if(x < 0)
    return var+180;
  if(var < 0)
    var+=360;
  return var;
}


/* Displays a floating point number with trailing decimal zeroes removed */
char *print_float(char *buf, double val)
{
  char *s;

  s=buf+sprintf(buf, "%.9f", val)-1;
  while(*s == '0')
    *s--='\0';
  if(*s == '.')
    *s='\0';

  return buf;
}

/* Returns 1 if <prefix> is a prefix of <string> */
int string_prefix(const char *string, const char *prefix)
{
  if(!*prefix)
    return 0;
  while(*string && *prefix && tolower(*string) == tolower(*prefix))
    string++, prefix++;
  return (*prefix == '\0');
}

/* Removes all whitespace surrounding string 'str'. */
/* Str is modified to point to the first non-whitespace character. */
void trim_spaces(char **str)
{
  char *s=strnul(*str)-1;

  while(s >= *str && *s == ' ')
    *s--='\0';
  while(**str == ' ')
    (*str)++;
}

/* Function to split up a list given a seperator */
/* Note str will get hacked up */
char *parse_up(char **str, int delimit)
{
  char *s=*str, *os=*str;
  int depth;

  if(!*s)
    return 0;
  while(*s && *s != delimit)
    if(*s++ == '{') {
      depth=1;
      while(depth && *s) {
        if(*s == '{')
          depth++;
        else if(*s == '}')
          depth--;
        s++;
      }
    }

  if(*s)
    *s++='\0';
  *str=s;
  return os;
}

/* Same as above, but removes surrounding {}'s */
char *parse_perfect(char **str, int del)
{
  char *old=parse_up(str, del);
  int a;

  if(!old)
    return NULL;
  a=strlen(old);
  if(*old == '{' && *(old+a-1) == '}') {
    *(old+a-1) = '\0';
    old++;
  }
  return old;
}

/* Like parse_up, but skips over symbols inside ansi escape codes */
char *ansi_parse_up(char **str, int delimit)
{
  char *s=*str, *os=*str;
  int depth;

  if(!*s)
    return 0;
  while(*s && *s != delimit) {
    /* Skip escape sequences */
    if(*s == '\e') {
      while(*s == '\e')
        skip_ansi(s);
      continue;
    }

    if(*s++ == '{') {
      depth=1;
      while(depth && *s) {
        if(*s == '{')
          depth++;
        else if(*s == '}')
          depth--;
        s++;
      }
    }
  }

  if(*s)
    *s++='\0';
  *str=s;
  return os;
}

/* Separates string by whitespace */
char *strspc(char **s)
{
  char *str=*s;

  if(!**s)
    return NULL;
  while(**s && !isspace(**s))
    (*s)++;
  while(isspace(**s))
    *(*s)++='\0';
  return str;
}

/* Center string in field of 'len' spaces, padding with 'fill' characters */
char *center(char *str, int len, char fill)
{
  static char buf[8192];
  int i=strlen(str), j;

  if(len > i) {
    j=(len-i)/2;
    memset(buf, fill, j);
    strcpy(buf+j, str);
    j+=i;
    memset(buf+j, fill, len-j);
    buf[len]='\0';
  } else
    strmaxcpy(buf, str, len+1);

  return buf;
}

/* Justify sentence/description in width of characters */
char *eval_justify(char **str, int width)
{
  static char buf[1024];
  char *p=*str, *s=*str;
  int a=0, b=0;

  if(!*s)
    return NULL;

  for(;*s;s++) {
    /* Newline */
    if(*s == '\n') {
      *str=s+1;
      buf[a]='\0';
      return buf;
    }

    /* Whitespace */
    if(*s == '\t') {
      b+=8-((a+b) % 8);
      continue;
    } 
    if(*s == ' ') {
      b++;
      continue;
    }

    /* Printable Character */

    /* Too many spaces to fit in <width> */
    if(a+b >= width) {
      if(!b && s-p != width) {
        a-=s-p;
        s=p;
        while(a > 0 && buf[a-1] == ' ')
          a--;
      }
      *str=s;
      buf[a]='\0';
      return buf;
    }

    /* Fill in extra spaces before our first letter */
    if(b) {
      while(b)
        buf[a++]=' ', b--;
      p=s;
    }

    buf[a++]=*s;
    if(*s == ',')
      p=s+1;
  }

  buf[a]='\0';
  *str=s;
  return buf;
}

/* String editing utility */
int edit_string(dest, string, old, new)
char *dest, *string, *old, *new;
{
  int len, newlen, count=0, i=0;

  /* Nothing to do? */
  if(!*old) {
    strcpy(dest, string);
    return 0;
  }

  /* Prepend string */
  if(!strcmp(old, "^")) {
    len=strlen(new);
    memcpy(dest, new, len);
    strmaxcpy(dest+len, string, 8001-len);
    return 1;
  }

  /* Append string */
  if(!strcmp(old, "$")) {
    len=strlen(string);
    memcpy(dest, string, len);
    strmaxcpy(dest+len, new, 8001-len);
    return 1;
  }

  len=strlen(old);
  newlen=strlen(new);

  /* Match and substitute */
  while(i < 8000 && *string)
    if(i+newlen <= 8000 && !strncmp(string, old, len)) {
      memcpy(dest+i, new, newlen);
      i += newlen;
      string += len;
      count++;
    } else
      dest[i++]=*string++;
  dest[i]=0;

  return count;
}


/* Memory Management Utilities */

// stralloc(): Allocates a constant memory space for strings whose sizes are
// assumed never to change or be freed until the process exits. Heap space is
// generated by calls to malloc() in 8k-sized chunks and grows when needed.
//
// Since many strings are initially loaded from configuration files, this
// function will dramatically decrease the overhead induced with using malloc()
// at initialization time.
//
char *stralloc(char *str)
{
  /* Structure size=8192 on 64-bit machines */
  static struct chunk {
    struct chunk *next;
    int cur;
    char text[8180];
  } *heap;

  struct chunk *ptr, *last=NULL;
  int len=strlen(str)+1;
  char *s;

  /* These are constant strings, so returning "" is allowed in this case */
  if(!*str)
    return "";

  /* This function works well only with small sizes to allocate */
  if(len >= 1024)
    return memcpy(malloc(len), str, len);

  /* Search for a free chunk */
  for(ptr=heap;ptr;last=ptr,ptr=ptr->next)
    if(len <= 8180-ptr->cur)
      break;

  /* Allocate a new chunk. We don't use realloc() here because we don't want
     string pointers inside the text area to change. */
  if(!ptr) {
    ptr=malloc(sizeof(struct chunk));
    ptr->next=NULL;
    ptr->cur=0;
    if(last)
      last->next=ptr;
    else
      heap=ptr;
  }

  /* Copy the string */
  s=memcpy(ptr->text+ptr->cur, str, len);
  ptr->cur += len;

  /* If chunk is full enough, detach it from the linked list */
  if(ptr->cur > 8176) {
    if(last)
      last->next=ptr->next;
    else
      heap=ptr->next;
  }

  return s;
}
