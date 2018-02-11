/* game/player.c */

#include "externs.h"

struct plist {
  char *name;
  dbref player;
};

hash *playerlist;


void add_player(dbref player)
{
  struct plist *new;
  char *s;

  if(!playerlist)
    playerlist=create_hash();

  /* add player name to table */
  new=malloc(sizeof(struct plist));
  new->player=player;
  SPTR(new->name, db[player].name);
  add_hash(playerlist, new);

  if(!*(s=atr_get(player, A_ALIAS)))
    return;

  /* add the alias to the table */
  new=malloc(sizeof(struct plist));
  new->player=player;
  SPTR(new->name, s);
  add_hash(playerlist, new);
}

void delete_player(dbref player)
{
  char *s;

  if(!playerlist)
    return;

  del_hash(playerlist, db[player].name);
  if(*(s=atr_get(player, A_ALIAS)))
    del_hash(playerlist, s);
}

// Returns a dbref# given a valid playername in the database. Returns NOTHING
// if playername doesn't exist, and AMBIGUOUS if the word "me" is matched.
//
// Note that AMBIGUOUS is returned even if a non-player has entered "me".
//
dbref lookup_player(char *name)
{
  struct plist *ptr;

  if(!playerlist)
    return NOTHING;

  if(!strcasecmp(name, "me"))
    return AMBIGUOUS;
  if(*name == '#') {
    int a=atoi(name+1);

    if(!Invalid(a) && Typeof(a) == TYPE_PLAYER)
      return a;
    return NOTHING;
  }
  while(*name == '*')
    name++;

  ptr=lookup_hash(playerlist, name);
  return ptr?ptr->player:NOTHING;
}

/* Create a player and put it in PLAYER_START or GUEST_START */
dbref create_player(char *name, char class)
{
  dbref startloc=(class == CLASS_GUEST)?GUEST_START:PLAYER_START;
  dbref player=new_object();
  
  if(player == NOTHING)  /* insufficient space to create object */
    return NOTHING;
  
  /* initialize everything */
  if(FORCE_UC_NAMES)
    *name=toupper(*name);
  WPTR(db[player].name, name);
  db[player].location=db[player].link=startloc;
  db[player].owner=player;
  db[player].flags=TYPE_PLAYER;
  db[player].pennies=(class == CLASS_GUEST)?GUEST_BONUS:START_BONUS;
  db[player].create_time=now;

  db[player].data=calloc(1, sizeof(struct playerdata));
  db[player].data->class=(player == GOD)?CLASS_WIZARD:class;
  db[player].data->term=TERM_BEEPS;
  db[player].data->tz=TIMEZONE;
  db[player].data->tzdst=HAVE_DST;

  atr_add(player, A_CHANNEL, player == GOD?("public " DEFAULT_LOG):"public");

  if(player == GOD)
    atr_add(player, A_COLOR, "15");
  else {
    int color=9+rand_num(7);
    atr_add(player, A_COLOR, tprintf("%d,%d", color, color-8));
  }

  if(class != CLASS_GUEST)
    nstat[NS_NEWCR]++;
  ADDLIST(player, db[startloc].contents);

  add_player(player);
  return player;
}

void destroy_player(dbref victim)
{
  struct comlock *ptr;
  DESC *d;
  dbref a;

  /* Check if the victim's currently logged in */
  if((d=Desc(victim))) {
    notify(victim, "Character %s: Deleted.", db[victim].name);
    d->flags |= C_LOGOFF|C_DESTROY;
    return;
  }

  /* Remove +channel-specific lock */
  for(ptr=comlock_list;ptr;ptr=ptr->next)
    PULL(ptr->list, victim);

  /* Clear attributes and settings */
  delete_message(victim, -3);
  delete_player(victim);

  for(a=2;a<db_top;a++)
    if(db[a].owner == victim && a != victim)
      recycle(a);
  recycle(victim);
  validate_links();
}

int boot_off(dbref player)
{
  DESC *d;

  if(!(d=Desc(player)))
    return 0;
  d->flags |= C_LOGOFF;
  return 1;
}

/* encrypts passwords and compares them against compressed data */
int crypt_pass(dbref player, char *pass, int check)
{
  unsigned char salt[3], r[10];
  unsigned quad des;
  int a;

  if(!check) {
    /* encrypt new password using MD5 */
    a=(rand() & 65535) | (rand() << 16);
    memcpy(db[player].data->pass+4, md5_hash(pass, a), 16);
    a=htonl(a);
    memcpy(db[player].data->pass, (char *)&a, 4);
    db[player].data->passtype=1;
    return 1;
  }

  /* compare MD5 hash */
  if(db[player].data->passtype) {
    memcpy((char *)&a, db[player].data->pass, 4);
    a=ntohl(a);
    return !memcmp(md5_hash(pass, a), db[player].data->pass+4, 16);
  }

  /* encrypt DES key */
  r[0]=salt[0]=db[player].data->pass[0] >> 2;
  r[1]=salt[1]=((db[player].data->pass[0]&3)<<4)+(db[player].data->pass[1]>>4);
  salt[2]='\0';
  for(a=0;a<2;a++)
    salt[a]=salt[a]<12?salt[a]+46:salt[a]<38?salt[a]+53:(salt[a]+59);
  des=des_crypt(pass, salt);

  /* convert salt+key into 76-bit binary */
  r[0]=(r[0] << 2)|(r[1] >> 4);
  r[1]=(r[1] << 4)|(des >> 60);
  for(a=0;a<7;a++)
    r[a+2]=des >> (52-(a*8));
  r[9]=des << 4;

  /* pass or void password */
  return !memcmp(r, db[player].data->pass, 10);
}

/* wizard command to create a new player */
void do_pcreate(dbref player, char *name)
{
  DESC *d;
  dbref who;

  if(Typeof(player) != TYPE_PLAYER || !(d=Desc(player)))
    return;
  if(!ok_player_name(name, 0)) {
    notify(player, "You can't give a player that name.");
    return;
  }

  if((who=create_player(name, CLASS_PLAYER)) == NOTHING) {
    notify(player, "The database has reached the maximum number of objects. "
           "Please @destroy something first.");
    return;
  }

  nstat[NS_NEWCR]++;
  notify(player, "New player %s created.",unparse_object(player, who));
  log_main("PCREATE: %s(#%d) created a new player: %s(#%d)",
           db[player].name, player, db[who].name, who);

  crypt_pass(who, "", 0);
  WPTR(d->env[0], tprintf("%d", who));
  output2(d, tprintf("Enter a password for %s: %s%s", db[who].name, ECHO_OFF,
          PROMPT(d)));
  d->state=6;
}

/* wizard command to utterly destroy a player */
void do_nuke(dbref player, char *name)
{
  dbref victim=lookup_player(name);

  if(victim <= NOTHING) {
    notify(player, "No such player '%s'.", name);
    return;
  }
  
  if(player == victim) {
    notify(player, "You cannot nuke yourself, silly!");
    return;
  } else if(victim == GOD || !controls(player, victim, POW_NUKE)) {
    notify(player, "You don't have the authority to do that.");
    return;
  } else if(Immortal(victim)) {
    notify(player, "That is not a normal player character.");
    return;
  } else if(db[0].owner == victim) {
    notify(player, "Sorry, that character holds essential database objects.");
    return;
  }

  /* notify the player and log the command */
  notify(player, "%s - nuked.", db[victim].name);
  log_main("%s(#%d) executed: @nuke %s(#%d)", db[player].name, player,
           db[victim].name, victim);

  /* destroy 'em */
  destroy_player(victim);
}

void list_players(dbref player, char *prefix)
{
  struct textlist *charlist=NULL, *ptr, *last, *new;
  int a, len, count=0, colsize=4;
  char buf[32];

  /* go through the list of players */
  for(a=0;a<db_top;a++) {
    if(Typeof(a) != TYPE_PLAYER || (*prefix &&
       !string_prefix(db[a].name, prefix)))
      continue;

    len=sprintf(buf, "%s%s", db[a].name, ((db[a].flags & PLAYER_CONNECT) &&
       (controls(player,a,POW_WHO) || could_doit(player,a,A_HLOCK)))?"*":"");

    /* add the string to the column-display */
    new=malloc(sizeof(struct textlist)+len+1);
    memcpy(new->text, buf, len+1);

    /* insert sort */
    for(ptr=charlist,last=NULL;ptr;last=ptr,ptr=ptr->next)
      if(strcasecmp(ptr->text, buf) >= 0)
        break;
    new->next=ptr;
    if(last)
      last->next=new;
    else
      charlist=new;

    if(len+4 > colsize)
      colsize=len+4;
    ++count;
  }

  if(!count) {
    notify(player, "There are no player names that begin with '%s'.", prefix);
    return;
  }

  if(*prefix)
    notify(player, "Listing all player names beginning with %s:", prefix);
  else
    notify(player, "Listing all players on the game:");
  notify(player, "%s", "");
  showlist(player, charlist, colsize);
  notify(player, "\304\304\304");
  notify(player, "Total Players: %d.", count);
}
