/* io/news.c */
/* TinyMare News Server Version 0.1 - August 30, 1998 */

#include "externs.h"

#define NEWS_VERSION	"0.1"


#define BBS_MODERATED	0x1	// Only admin can post to group
#define BBS_ADMIN	0x2	// Administration viewing only
#define BBS_VOTING	0x4	// Enable voting on issues

struct bbsinfo {
  char *name;
  char *desc;
  int flags;
} msggroup[]={
  {"General",	"General Discussions Bulletin Board"},
  {"Admin",	"Administration Discussion Only", BBS_ADMIN},
  {"Sales",	"Trading Merchandise Announcements"},
  {"Ideas",	"Storyline/Plot Idea Submissions"},
  {"Flame",	"Concerns Between Players/Admin"},
  {"Changes",	"New Revisions to Winds", BBS_MODERATED},
  {"Voting",	"Public Voting Booth", BBS_MODERATED|BBS_VOTING},
  {0}};

void do_news(player, cmd, text)
dbref player;
char *cmd, *text;
{
  eqtech *eqp;
  eqlist *mgk;
  char buf[256];
  int a, cols=getbitmap(player, B_COLS)-2;

  if(Typeof(player) != TYPE_PLAYER) {
    notify(player, "You cannot use +news.");
    return;
  }

  if(!*cmd) {
    display_textblock(player, NULL, "msgs/news.txt", "");

    /* build header bar */
    if(cols < 48)
      cols=78;
    for(a=0;a<cols;a++)
      buf[a]=240;
    buf[a]='\0';
    notify(player, tprintf("\e[1;34m%s\e[m", buf));

    /* display table of contents */
    notify(player, "\e[1;4;35mMessage Board:\e[m   \e[1;4;35mLast Posted:\e[m   \e[1;4;35mMessages:\e[m   \e[1;4;35mTopic:\e[m");
    for(a=0;msggroup[a].name;a++) {
      if((msggroup[a].flags & BBS_ADMIN) && class(player) == CLASS_PLAYER)
        continue;
      sprintf(buf, "%s%s",msggroup[a].desc,(msggroup[a].flags & BBS_MODERATED)?
              " (Moderated)":"");
      buf[cols-44]='\0';
      notify(player, tprintf("\e[1m%c \e[36m%-16.16s\e[33m%-15.15s\e[32m%-5d\e[33m%-6.6s\e[37m%s\e[m", 
             ' ', msggroup[a].name, " \304\304\304", 0, "", buf));
    }

    if((a=getbitmap(player, B_RACE))) {
      notify(player, "   \e[1m-*-\e[m");
      sprintf(buf, "%s Race Discussion Group", race_to_name(a));
      notify(player, tprintf("\e[1m%c \e[36m%-16.16s\e[33m%-15.15s\e[32m%-5d\e[33m%-6.6s\e[37m%s\e[m", 
             ' ', race_to_name(a), " \304\304\304", 0, "", buf));
    }

    for(eqp=db[player].tech;eqp;eqp=eqp->next)
      if(eqp->type == GUILD)
        break;
    if(eqp)
      for(mgk=eqp->list;mgk;mgk=mgk->next)
        if(mgk->num && mgk->num <= NUM_GUILDS) {
          sprintf(buf, "%s Guild Discussion Group", guild_to_name(mgk->num));
          notify(player, tprintf("\e[1m%c \e[36m%-16.16s\e[33m%-15.15s\e[32m%-5d\e[33m%-6.6s\e[37m%s\e[m", 
                 ' ', guild_to_name(mgk->num), " \304\304\304", 0, "", buf));
        }

    /* display total unread posts */
    notify(player, "");
    return;
  }
}
