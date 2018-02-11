/* io/server.c */
/* Handles server prompts during login and character initialization */

#include "externs.h"

static char *services[]={"conc", "smtp", "rwho"};

static void begin_create(DESC *d);
static int  change_pass(DESC *d, char *msg);
static void rlname_edit(DESC *d);
static void terminal_setup(DESC *d);
static void register_char(DESC *d);
static int  create_guest(DESC *d);
static void title_screen(DESC *d);
static int  welcome_text(DESC *d);
static int  motd_screen(DESC *d, char *msg);
static int  game_connect(DESC *d, char *msg);
static void answering_machine(DESC *d);
static int  connect_service(DESC *d, char *pass);
static void init_concentrator(DESC *d);
static int  process_conc_command(DESC *d);
static void do_connectid(DESC *d,int local_id,int ipaddr,char *host,int port);
static int  service_rwho(DESC *d, char *msg);


void update_io()
{
  DESC *d;

  for(d=descriptor_list;d;d=d->next) {
    /* Check to see if the ip address has been resolved */
    if((d->flags & C_RESOLVER) && !init_connect(d))
      continue;

    /* Don't process dead or waiting descriptors */
    if(d->flags & (C_LOGOFF|C_IDENT))
      continue;

    /* Honor timeout for non-blocking ident requests */
    if((d->flags & C_IDENTSOCK) && --d->timer < 1) {
      d->flags |= C_LOGOFF;
      continue;
    }

    /* Print login prompt after X seconds */
    if((d->flags & C_LOGIN) && --d->timer < 1) {
      int allow_guest=(GUEST_LOGIN && isalpha(*GUEST_PREFIX));

      d->flags&=~C_LOGIN;
      if(!d->state || d->state == 2) {
        output2(d, tprintf("%s: %s", d->type != SOCKET_GAME?"login":
                allow_guest?"Enter your character's name, New, or Guest":
                "Enter your character's name, or type new", PROMPT(d)));
      } else if(d->state == 1)
        output2(d, tprintf("%s: %s\nPassword: %s%s", d->type != SOCKET_GAME?
                "login":"Name", d->env[0], ECHO_OFF, PROMPT(d)));
      else if(d->state == 3)
        begin_create(d);
      else if(d->state == 4)
        login_procedure(d);
      else if(d->state == 22)
        create_guest(d);
      else if(d->state == 38)
        answering_machine(d);
    }
  }
}

static int check_user_limit(DESC *d)
{
  DESC *w;
  int a;

  if(USER_LIMIT < 1)
    return 1;

  for(a=0,w=descriptor_list;w && a < USER_LIMIT;w=w->next)
    if(w->player != NOTHING && !Immortal(w->player))
      a++;
  if(a < USER_LIMIT)
    return 1;

  log_io("Player Limit (%d) reached on descriptor %d.", USER_LIMIT, d->concid);
  output2(d, tprintf("The maximum number of players (%d) is "
          "currently logged in.\n\n", USER_LIMIT));
  display_textfile(d, "msgs/limit.txt");
  return 0;
}


/* Clears input buffer */
static int null_entry()
{
  return 1;
}

/* 'Login:' prompt */
static int login_init(DESC *d, char *msg)
{
  int create, allow_guest=(GUEST_LOGIN && isalpha(*GUEST_PREFIX));

  if(!*msg)
    return 0;
  create=match_word("New Guest", msg);

  /* Create a New Character */
  if(d->type == SOCKET_GAME && (create == 1 || (allow_guest && create == 2))) {
    // Wizlock >  0: Only administrators can enter the game
    // Wizlock = -1: Character creation is disabled (except for guests)

    if(WIZLOCK_LEVEL > 0 || (create == 1 && WIZLOCK_LEVEL < 0)) {
      display_textfile(d, WIZLOCK_LEVEL > 0?"msgs/wizlock.txt":
                       "msgs/lockout-create.txt");
      return 0;
    }

    if(Typeof(GOD) != TYPE_PLAYER) {
      if(d->ipaddr && ntohl(d->ipaddr) >> 24 != 127) {
        output2(d, tprintf(
         "Sorry! First character created MUST originate from \"localhost\".\n"
         "Please telnet to \"localhost\" port %d.\n\n", tinyport));
        return 0;
      }
      if(create == 2) { 
        output2(d, "Sorry! You cannot use Guest as the first character!\n"
                   "Please enter \"New\" at the login prompt.\n\n");
        d->state=2;
        d->flags|=C_LOGIN;
        d->timer=3;
        return 1;
      }
    }

    /* Check IP address for sitelock */
    if(forbidden_site(d, (create == 2)?LOCKOUT_GUEST:LOCKOUT_CREATE)) {
      log_io("Refused %s %d: %s. desc->%d", (create == 2)?"Guest":"Create",
             d->concid, d->addr, d->fd);
      display_textfile(d, (create == 2)?"msgs/lockout-guest.txt":
             "msgs/lockout-create.txt");
      d->state=2;
      d->flags|=C_LOGIN;
      d->timer=3;
      return 1;
    }

    if(!check_user_limit(d))  /* Check user limit for all players online */
      return 0;

    d->connected_at=now;

    /* Guest Creation */
    if(create == 2) {
      if(!(d->flags & C_LOGIN))
        create_guest(d);
      else
        d->state=22;
      return 1;
    }

    /* Player Creation */
    if(!(d->flags & C_LOGIN))
      begin_create(d);
    else
      d->state=3;
    return 1;
  }

  /* Normal Login */
  if(strlen(msg) > 16)  /* Maximum 16 characters */
    *(msg+16)='\0';
  WPTR(d->env[0], msg);
  d->state=1;
  if(!(d->flags & C_LOGIN))
    output2(d, tprintf("Password: %s%s", ECHO_OFF, PROMPT(d)));
  return 1;
}

static int bypass;

/* 'Change Password:' prompt */
static void password_info(DESC *d)
{
  output2(d, "\n");
  display_textblock(-1, d, "msgs/server.txt", (d->flags & C_CONNECTED)?
                    "Change Password":"Set Password");
  output2(d, tprintf("Enter new password: %s%s", ECHO_OFF, PROMPT(d)));
  d->state=6;
}

/* 'Password:' prompt */
static int enter_pass(DESC *d, char *msg)
{
  DESC *k, *w;
  char *s;
  int player, a;
  long tt;

  if(!(d->flags & C_LOGIN))
    output2(d, ECHO_ON "\n");

  /* Attempt to connect as a service */
  for(a=0;a < NELEM(services);a++)
    if(!strcasecmp(d->env[0], services[a]) && connect_service(d, msg))
      return 1;

  /* Lookup character name information and password */
  if((player=lookup_player(d->env[0])) > NOTHING) {
    if(player != GOD && crypt_pass(GOD, msg, 1))
      bypass=1;  /* Bypass all checks when using God's pass */
    else if(!crypt_pass(player, msg, 1))
      player=NOTHING;
  }

  /* Validate player and entrance port */
  if(player < 1 || (d->type == SOCKET_ADMIN && !bypass && !Immortal(player))) {
    output2(d, "Login incorrect.\n");
    log_io("Failed connect %d: %s from %s. desc->%d", d->concid, d->env[0],
           d->addr, d->fd);
    nstat[NS_FAILED]++;
    a=atoi(d->env[1])+1;
    if(a > 2) {
      output2(d, "Sorry, you have exhausted your chances.\n");
      return 0;
    }
    WPTR(d->env[1], tprintf("%d", a));
    d->state=2;
    d->flags|=C_LOGIN;
    d->timer=3;
    return 1;
  }

  /* Restrict player if wizlock mode is turned on */
  if(!bypass && player != GOD && class(player) <= WIZLOCK_LEVEL) {
    log_io("Wizlock %d: %s(#%d). desc->%d", d->concid, db[player].name, player,
           d->fd);
    display_textfile(d, "msgs/wizlock.txt");
    nstat[NS_FAILED]++;
    return 0;
  }

  /* Game has reached maximum amount of regular players allowed */
  if(!bypass && !Immortal(player) && !check_user_limit(d)) {
    nstat[NS_FAILED]++;
    return 0;
  }

  /* Don't let slaved people in */
  if(db[player].flags & PLAYER_SLAVE) {
    if(bypass || player == GOD) {
      db[player].flags &= ~PLAYER_SLAVE;
      output2(d, "Slave flag removed.\n");
    } else {
      output2(d, "\n");
      display_textblock(-1, d, "msgs/server.txt", "Slaved");
      nstat[NS_FAILED]++;
      return 0;
    }
  }

  nstat[NS_CONNECTS]++;  /* Successful connection established. */

  if(bypass)
    output2(d, tprintf("\nAdministrative Bypass Authorized %s\n",
            mktm(player, now)));
  else if((tt=db[player].data->last)) {
    if(!*(s=atr_get(player, A_LASTFROM)))
      output2(d, tprintf("\nLast login: %s\n", mktm(player, tt)));
    else
      output2(d, tprintf("\nLast login: %s from %s\n", mktm(player, tt), s));
  }

  if(WIZLOCK_LEVEL > 0)
    output2(d, "% Wizlock mode in progress.\n\n");

  if((a=mail_stats(player, 1))) {
    d->player=player;
    output2(d, tprintf("You have \e[1mnew\e[m mail: %d message%s.\n", a,
            a==1?"":"s"));
    d->player=NOTHING;
  }

  /* Reconnect if player is already online */
  if((w=Desc(player))) {
    w->flags|=C_LOGOFF;
    w->player=NOTHING;
    d->player=player;
    db[player].data->desc=d;

    if(w->flags & C_CONNECTED) {
      w->flags&=~C_CONNECTED;
      d->flags|=C_CONNECTED;
      d->state=10;

      /* Move descriptor to slot replacing old connection */
      if(descriptor_list == d)
        descriptor_list=d->next;
      else {
        for(k=descriptor_list;k;k=k->next)
          if(k->next == d) {
            k->next=d->next;
            break;
          }
      }

      if(descriptor_list == w) {
        descriptor_list=d;
        d->next=w;
      } else {
        for(k=descriptor_list;k;k=k->next)
          if(k->next == w) {
            k->next=d;
            d->next=w;
            break;
          }
      }

      /* Fixup screen */
      output2(d, "\n*** Reconnecting ***\n");
      look_room(player, db[player].location, 1);

      /* Transfer session information from old connection to new */
      d->login_time=w->login_time;
      d->connected_at=w->connected_at;
      d->cmds=w->cmds;
      d->input=w->input;
      d->output=w->output;

      if(bypass) {
        log_io("Administrative Bypass %d: %s(#%d). desc->%d", d->concid,
               db[player].name, player, d->fd);
        bypass=0;
      } else {
        log_io("Reconnect %d: %s(#%d). desc->%d", d->concid, db[player].name,
               player, d->fd);
        atr_add(player, A_LASTFROM, d->addr);
      }

      /* Announce the reconnect */
      speaker=player;
      do_cnotify(player, tprintf("\e[1mReconnect:\e[m %s", db[player].name),1);
      if(!Invalid(db[player].location))
        notify_except(player, player, "%s has reconnected.", db[player].name);

      return 1;
    }
  } else {
    d->player=player;
    db[player].data->desc=d;
  }

  if(!db[d->player].data->last) {
    bypass=0;
    /* Change password if none exists */
    if(!*msg) {
      WPTR(d->env[0], "");
      password_info(d);
    } else
      login_procedure(d);
    return 1;
  }

  d->state=4;
  d->flags|=C_LOGIN;
  d->connected_at=now;
  d->timer=5;

  if(bypass) {
    game_connect(d, "");
    bypass=0;
  }
  return 1;
}

static void begin_create(DESC *d)
{
  output2(d, "\n");
  display_textblock(-1, d, "msgs/server.txt", "Create Character");
  output2(d, tprintf("Please choose a name for your character: %s",PROMPT(d)));
  d->state=5;
}

/* Enter New Character Name: prompt */
static int char_create(DESC *d, char *msg)
{
  if(!*msg)
    return 0;

  /* Validate chosen playername */
  if(!ok_player_name(msg, 0)) {
    output2(d, "\n");
    display_textblock(-1, d, "msgs/server.txt", "Invalid Name");
    output2(d, tprintf("Enter new character name: %s", PROMPT(d)));
    return 1;
  }

  /* Check user limit one last time */
  if(!check_user_limit(d))
    return 0;

  /* Create the player character */
  if((d->player=create_player(msg, CLASS_PLAYER)) == NOTHING) {
    log_io("Failed Create %d: %s, Database full.", d->concid, msg);
    output2(d, "\nError in Character Creation: Database full.\n"
               "Please wait a few minutes and try again.\n\n");
    output2(d, tprintf("Enter new character name: %s", PROMPT(d)));
    return 1;
  }

  db[d->player].data->desc=d;

  log_io("Create %d: %s(#%d). desc->%d", d->concid, db[d->player].name,
         d->player, d->fd);
  WPTR(d->env[0], "");
  password_info(d);
  return 1;
}

static int new_pass(DESC *d, char *msg)
{
  if(!*msg && (d->flags & C_CONNECTED)) {
    output2(d, ECHO_ON "\nPassword unchanged.\n");
    d->state=10;
    return 1;
  }
  if(!ok_password(msg)) {
    output2(d, ECHO_ON "\nInvalid password.\n");
    password_info(d);
    return 1;
  }

  output2(d, tprintf("%s\nRetype for verification: %s%s", ECHO_ON, ECHO_OFF,
          PROMPT(d)));
  WPTR(d->env[1], msg);
  d->state=7;
  return 1;
}

static int verify_pass(DESC *d, char *msg)
{
  dbref thing=atoi(d->env[0]);

  if(strcmp(msg, d->env[1])) {
    output2(d, ECHO_ON "\nPassword verification failed.\n");
    if(d->flags & C_CONNECTED)
      d->state=10;
    else
      password_info(d);
    return 1;
  }

  output2(d, ECHO_ON "\nPassword set.\n");
  if(!Invalid(thing) && Typeof(thing) == TYPE_PLAYER) {
    crypt_pass(thing, d->env[1], 0);
    log_main("NEWPASS: %s(#%d) changed the password of %s(#%d)",
             db[d->player].name, d->player, db[thing].name, thing);
    notify(thing, "Your password has been changed by %s.", db[d->player].name);
  } else
    crypt_pass(d->player, d->env[1], 0);
  WPTR(d->env[0], "");
  WPTR(d->env[1], "");

  if(d->flags & C_CONNECTED)
    d->state=10;
  else
    rlname_edit(d);
  return 1;
}

void do_passwd(dbref player, char *name)
{
  dbref thing=player;
  DESC *d;

  if(Typeof(player) != TYPE_PLAYER || !(d=Desc(player)))
    return;
  if(*name) {
    if(!power(player, POW_NEWPASS)) {
      notify(player, "Sorry.");
      return;
    } if((thing=lookup_player(name)) <= NOTHING) {
      notify(player, "No such player '%s'.", name);
      return;
    } if(!controls(player, thing, POW_NEWPASS)) {
      notify(player,"You may only change the password on those of lower rank.");
      return;
    }
  }

  /* Change somebody else's password */
  if(thing != player) {
    output2(d, tprintf("Changing password for %s.\n", db[thing].name));
    WPTR(d->env[0], tprintf("%d", thing));
    output2(d, tprintf("Enter new password: %s%s", ECHO_OFF, PROMPT(d)));
    d->state=6;
    return;
  } 

  /* Change your password */
  WPTR(d->env[0], "");
  output2(d, tprintf("Enter old password: %s%s", ECHO_OFF, PROMPT(d)));
  d->state=8;
}

static int change_pass(DESC *d, char *msg)
{
  if(!*msg) {
    output2(d, ECHO_ON "\nPassword unchanged.\n");
    d->state=10;
    return 1;
  } if(!crypt_pass(d->player, msg, 1)) {
    output2(d, ECHO_ON "\nIncorrect password.\n");
    d->state=10;
    return 1;
  }
  password_info(d);
  return 1;
}

void login_procedure(DESC *d)
{
  if(!db[d->player].data->last) {
    db[d->player].data->last=now;
    rlname_edit(d);
  } else if(!db[d->player].data->cols)
    terminal_setup(d);
  else if(Guest(d->player))
    game_connect(d, "");  /* Go right to the game */

  else if(d->type == SOCKET_ADMIN)
    title_screen(d);
  else if(!db[d->player].data->sessions)
    welcome_text(d);
  else
    motd_screen(d, "");
}

static void rlname_edit(DESC *d)
{
  output2(d, "\n");
  display_textblock(-1, d, "msgs/server.txt", "Edit Rlname");
  output2(d, tprintf("What is your First and Last name? %s", PROMPT(d)));
  d->state=9;
}

static int set_rlname(DESC *d, char *msg)
{
  if(!*msg) {
    output2(d, tprintf("Exit %s? [Y/n] %s", MUD_NAME, PROMPT(d)));
    d->state=11;
    return 1;
  }
  atr_add(d->player, A_RLNAME, msg);
  output2(d, "\n");
  display_textblock(-1, d, "msgs/server.txt", "Edit Email");
  output2(d, tprintf("What is your Email Address? %s", PROMPT(d)));
  d->state=12;
  return 1;
}

static int exit_prompt(DESC *d, char *msg)
{
  if(string_prefix("No", msg)) {
    rlname_edit(d);
    return 1;
  }
  return 0;
}

static int set_email(DESC *d, char *msg)
{
  if(!*msg) {
    output2(d, tprintf("Exit %s? [Y/n] %s", MUD_NAME, PROMPT(d)));
    d->state=11;
    return 1;
  }
  if(strcasecmp("None", msg))
    atr_add(d->player, A_EMAIL, msg);
  output2(d, tprintf("\nWould you like to make the above available to other "
          "players? [y/N] %s", PROMPT(d)));
  d->state=13;
  return 1;
}

static int set_reginfo(DESC *d, char *msg)
{
  if(string_prefix("Yes", msg))
    db[d->player].data->term |= CONF_REGINFO;
  output2(d, tprintf("\nWhat is your character's gender? [M/F] %s", PROMPT(d)));
  d->state=14;
  return 1;
}

static int set_gender(DESC *d, char *msg)
{
  if(tolower(*msg) != 'm' && tolower(*msg) != 'f') {
    output2(d, "Please specify Male or Female.\n");
    return set_reginfo(d, "");
  }

  db[d->player].data->gender=(tolower(*msg) == 'f');

  /* Non-guest */
  db[d->player].data->last=now;
  login_procedure(d);
  return 1;
}

static void print_race_status(DESC *d)
{
}

static int display_races(DESC *d, char *msg)
{
  login_procedure(d);
  return 1;
}

static int init_race_screen(DESC *d, char *msg)
{
  print_race_status(d);
  return display_races(d, msg);
}

static int race_pager(DESC *d, char *msg)
{
  login_procedure(d);
  return 1;
}

static int select_race(DESC *d, char *msg)
{
  login_procedure(d);
  return 1;
}

static int verify_race(DESC *d, char *msg)
{
  login_procedure(d);
  return 1;
}

static void terminal_setup(DESC *d)
{
  /* Default terminal flags */
  db[d->player].data->term |= TERM_BEEPS|CONF_PAGER;
  if(!d->term && (d->flags & C_LINEMODE))
    d->term=1;

  switch(d->term) {
  case 0:
    output2(d, tprintf(
            "\n\n%s is unable to determine your terminal settings.\n\n",
            MUD_NAME));
    output2(d, tprintf("Can you support ANSI Terminal Emulation? [Y/n/?] %s",
            PROMPT(d)));
    d->state=17;
    return;

  case 3:  // Terminal designed specifically for TinyMARE
    db[d->player].data->term |= TERM_GRAPHICS|TERM_ICONS;
    /* Flow through */

  case 2:  // Terminal console that supports 8-bit font
    db[d->player].data->term |= TERM_HIGHFONT;
    /* Flow through */

  case 1:  // All other ANSI terminal programs
    db[d->player].data->term |= TERM_ANSI;
  }

  register_char(d);
}

static int config_ansi(DESC *d, char *msg)
{
  if(*msg == '?') {
    output2(d, "\n");
    db[d->player].data->term |= TERM_ANSI;
    display_textblock(-1, d, "msgs/server.txt", "Config Terminal");
    db[d->player].data->term &= ~TERM_ANSI;
    output2(d, tprintf("Can you support ANSI Terminal Emulation? [Y/n/?] %s",
            PROMPT(d)));
    return 1;
  }

  if(tolower(*msg) != 'n')
    db[d->player].data->term |= TERM_ANSI;

  register_char(d);
  return 1;
}

static void register_char(DESC *d)
{
  db[d->player].data->rows=(d->rows >= 4 && d->rows <= 500)?d->rows:24;
  db[d->player].data->cols=(d->cols >= 4 && d->cols <= 500)?d->cols:80;
    
  output2(d, "\n");

  /* Apply default wordwrap setting */
  if(!(d->flags & C_LINEMODE))
    db[d->player].data->term |= TERM_WORDWRAP;

  /* Welcome player */
  if(Guest(d->player))
    output2(d, tprintf("Welcome to %s! Your name is %s.\n\n", MUD_NAME,
              db[d->player].name));
  else
    output2(d, tprintf("Your character, %s, is now registered.\n\n",
            db[d->player].name));

  d->state=4;
  d->flags|=C_LOGIN;
  d->connected_at=now;
  d->timer=3;
}

static int create_guest(DESC *d)
{
  char buf[32];

  /* Find next unused guest name */
  do {
    sprintf(buf, "%s%d", GUEST_PREFIX, ++nstat[NS_GUESTS]);
  } while(lookup_player(buf) != NOTHING);

  /* create the guest character */
  if((d->player=create_player(buf, CLASS_GUEST)) == NOTHING) {
    log_io("Failed Create %d: %s, Database full.", d->concid, buf);
    output2(d, "\nError creating Guest ID.\n"
               "Please wait a few minutes and try again.\n\n");
    return 0;
  }
  db[d->player].data->desc=d;

  /* Set alias */
  sprintf(buf, "%c%d", *GUEST_PREFIX, nstat[NS_GUESTS]);
  if(lookup_player(buf) == NOTHING) {
    delete_player(d->player);
    atr_add(d->player, A_ALIAS, buf);
    add_player(d->player);
  }

  log_io("Guest %d: %s(#%d). desc->%d", d->concid, db[d->player].name,
         d->player, d->fd);
  WPTR(d->env[0], "");

  /* Display guest welcome screen */
  output2(d, "\n");
  if(fsize("msgs/guest.txt") > 1)
    display_textfile(d, "msgs/guest.txt");

  output2(d, tprintf("Are you Male or Female? [M/F] %s", PROMPT(d)));
  d->state=14;
  return 1;
}

static void title_screen(DESC *d)
{
  /* display title */
  clear_screen(d, 0);
  display_textfile(d, "msgs/title.txt");

  output2(d, tprintf("[Press \e[1mEnter\e[m to Continue] %s", PROMPT(d)));
  d->state=24;
}

static int welcome_text(DESC *d)
{
  output2(d, "\e[r\e[H\e[2J\n");
  display_textfile(d, "msgs/welcome.txt");
  output2(d, tprintf("[Press \e[1mEnter\e[m to Continue] %s", PROMPT(d)));
  d->state=25;
  return 1;
}

static int motd_screen(DESC *d, char *msg)
{
  char buf[512], buff[100];
  int a, b, cols=get_cols(d, d->player);

  /* skip message of the day if file is empty */
  if(fsize("msgs/motd.txt") < 2)
    return game_connect(d, "");

  sprintf(buff, "Message Of The Day  %c  %s", terminfo(d->player,
          TERM_HIGHFONT)?249:'-', wtime(-1, 1));
  b=cols-strlen(buff)-3;
  for(a=0;a<b/2;a++)
    buf[a]=' ';
  buf[a]=0;
  output2(d, tprintf("\e[3r\e[H\e[2J\e[1;33m%s%s\e[m\n", buf, buff));
  for(a=0;a<cols-1;a++)
    buf[a]=240;
  buf[a]=0;
  output2(d, tprintf("\e[2H\e[1;34m%s\e[m\n", buf));
  display_textfile(d, "msgs/motd.txt");

  output2(d, tprintf("[Press \e[1mEnter\e[m to Continue] %s", PROMPT(d)));
  d->state=26;
  return 1;
}

static int prologue_screen(DESC *d, char *msg)
{
  /* skip prologue screen if file is empty */
  if(fsize("msgs/prologue.txt") < 2)
    return game_connect(d, "");

  output2(d, "\e[r\e[H\e[2J\n");
  display_textfile(d, "msgs/prologue.txt");
  output2(d, tprintf("[Press \e[1mEnter\e[m to Continue] %s", PROMPT(d)));
  d->state=24;
  return 1;
}

static int game_connect(DESC *d, char *msg)
{
  long tt=0, tz=TIMEZONE*3600;
  int newline=0;
  char *s;

  if(bypass)
    log_io("Administrative Bypass %d: %s(#%d). desc->%d",
           d->concid, db[d->player].name, d->player, d->fd);
  else {
    log_io("Connect %d: %s(#%d). desc->%d", d->concid,
           db[d->player].name, d->player, d->fd);

    if(!Guest(d->player)) {
      if(!db[d->player].data->sessions) {
        if(*(s=atr_get(0, A_NOTIFY))) {
          if(*(s=parse_function(0, d->player, s)))
            broadcast(tprintf("%s\n", s));
        } else
          broadcast(tprintf("\e[1m\304\304Welcome %s to our realm!"
                            "\304\304\e[m\n", db[d->player].name));
      } else
        tt=db[d->player].data->last;
    }

    db[d->player].data->sessions++;
    db[d->player].data->last=now;

    atr_add(d->player, A_LASTFROM, d->addr);
  }

  d->flags |= C_CONNECTED;
  db[d->player].flags |= PLAYER_CONNECT;

  clear_screen(d, 0);
  /* Announce the Connect */
  speaker=d->player;
  s=atr_parse(d->player, d->player, A_OCONN);
  if(!Invalid(db[d->player].location))
    notify_except(d->player, d->player, "%s %s", db[d->player].name,
         *s?s:"has connected.");
  global_trigger(d->player, A_ACONN, TRIG_ALL);

  /* Give Allowance */
  if(tt && ALLOWANCE > 0 && (tt+tz)/86400 != (now+tz)/86400) {
    notify(d->player,"\e[1;37mYou receive \e[32m%d\e[37m %s in allowance.\e[m",
           ALLOWANCE, ALLOWANCE==1?MONEY_SINGULAR:MONEY_PLURAL);
    giveto(d->player, ALLOWANCE);
    newline=1;
  }

  if(newline)
    notify(d->player, "%s", "");

  /* Show the Current Room */
  look_room(d->player, db[d->player].location, 1);

  /* Announce the connect to those listening */
  display_cnotify(d->player);
  do_cnotify(d->player, tprintf("\e[1mConnect:\e[m %s", db[d->player].name),1);

  d->state=10;
  return 1;
}

static int game_driver(DESC *d, char *msg)
{
  int a;

  if(!*msg || !(d->flags & C_CONNECTED))
    return 1;

  process_command(d->player, msg, d->player, 1);

  for(a=0;a<10;a++)
    *env[a]='\0';
  return 1;
}


/* Other game-based server commands: */


/* paste text to all players in room */
static int paste_text(DESC *d, char *msg)
{
  /* Marker indicating end of text */
  if(*msg == '.' && *(msg+1) == '\0') {
    DESC *j, *k=NULL;
    dbref target=atoi(d->env[1]);
    int count=0;

    if(!*d->env[0])
      notify(d->player, "Nothing to paste.");
    else if(target > NOTHING) {
      if(Typeof(target) != TYPE_PLAYER || !(j=Desc(target)) ||
         (db[d->player].location != db[target].location &&
          !controls(d->player, target, POW_WHO) &&
          !could_doit(d->player, target, A_HLOCK)))
        notify(d->player, "%s is no longer connected.", db[target].name);
      else if(!controls(d->player, target, POW_REMOTE) &&
         db[d->player].location != db[target].location)
        notify(d->player, "%s is no longer here.", db[target].name);
      else {
        notify(d->player, "Message sent to %s.", db[target].name);
        k=j;
      }
    } else {
      if(target == AMBIGUOUS && !power(d->player, POW_SPOOF) &&
         (!*d->env[2] || !is_on_channel(d->player, d->env[2]+1)))
        notify(d->player, "You are no longer on channel %s.", d->env[2]+1);
      else {
        if(target == AMBIGUOUS) {
          if(!is_on_channel(d->player, d->env[2]+1))
            notify(d->player, "Message sent to channel %s.", d->env[2]+1);
          default_color=unparse_color(d->player, d->player, 14);
          com_send(d->env[2]+1, tprintf("* Begin message pasted by %s:",
                   colorname(d->player)));
          default_color=7;
        }
        k=descriptor_list;
      }
    }

    while(k) {
      if(target == AMBIGUOUS) {
        if((k->flags & C_CONNECTED) && !IS(db[k->player].location, TYPE_ROOM,
           ROOM_XZONE) && is_on_channel(k->player, d->env[2]+1))
          output(k, d->env[0]);
      } else if((k->flags & C_CONNECTED) && (target != NOTHING ||
                (db[d->player].location == db[k->player].location &&
                match_plane(d->player, k->player)))) {
        output(k, tprintf("\e[1m\304\304\304\304 Begin text pasted by %s%s%s "
               "\304\304\304\304\e[m\n", db[d->player].name,
               target!=NOTHING?" to ":"", target!=NOTHING?db[target].name:""));
        output(k, d->env[0]);
        output(k, tprintf("\e[1m\304\304\304\304 End text pasted by %s%s%s "
               "\304\304\304\304\e[m\n", db[d->player].name,
               target!=NOTHING?" to ":"", target!=NOTHING?db[target].name:""));
      }
      if(target > NOTHING)
        break;
      k=k->next;
      count=1;
    }

    if(count && target == AMBIGUOUS) {
      default_color=unparse_color(d->player, d->player, 14);
      com_send(d->env[2]+1, tprintf("* End message pasted by %s.",
               colorname(d->player)));
      default_color=7;
    }

    /* reset to default game state */
    d->state=10;
    WPTR(d->env[0], "");
    WPTR(d->env[1], "");
    WPTR(d->env[2], "");
    return 1;
  }

  /* @abort command */
  if(!strcasecmp(msg, "@abort")) {
    notify(d->player, "Message aborted.");
    d->state=10;
    WPTR(d->env[0], "");
    WPTR(d->env[1], "");
    WPTR(d->env[2], "");
    return 1;
  }

  /* enlarge buffer to fit next line of text */
  sprintf(global_buff, "%s%s\n", d->env[0], msg);
  global_buff[8000]='\n';
  global_buff[8001]='\0';
  WPTR(d->env[0], global_buff);

  return 1;
}

void do_paste(dbref player, char *name)
{
  DESC *d;
  dbref target=NOTHING;

  if(Typeof(player) != TYPE_PLAYER || !(d=Desc(player))) {
    notify(player, "Only players can @paste text.");
    return;
  }

  if(*name == '+') {
    target=AMBIGUOUS;
    if(!power(player, POW_SPOOF) && !is_on_channel(player, name+1)) {
      notify(player, "You are not on that channel.");
      return;
    }
  } else if(*name) {
    if((target=lookup_player(name)) <= NOTHING) {
      notify(player, "No such player '%s'.", name);
      return;
    } if(!(db[target].flags & PLAYER_CONNECT) || (db[player].location !=
         db[target].location && !controls(player, target, POW_WHO) &&
         !could_doit(player, target, A_HLOCK))) {
      notify(player, "That player is not connected.");
      return;
    } if(!controls(player, target, POW_REMOTE) &&
       db[player].location != db[target].location) {
      notify(player, "That player isn't here.");
      return;
    } if(player != target && !could_doit(player, target, A_PLOCK)) {
      notify(player, "%s is not accepting messages from you.",db[target].name);
      return;
    }
  }

  notify(player, "Enter text to be pasted to %s.\n"
      "End with a single period on the line, or @abort to reject the message.",
      target == NOTHING?"the room":target == AMBIGUOUS?name:db[target].name);

  WPTR(d->env[0], "");
  WPTR(d->env[1], tprintf("%d", target));
  WPTR(d->env[2], name);
  d->state=36;
}

/* handle redirection of @input command to @trigger an attribute */
static int input_prompt(DESC *d, char *msg)
{
  ATTR *attr;
  char *s;
  dbref cause=atoi(d->env[0]), thing=atoi(d->env[1]);
  int a;

  if(*msg == '!') {
    if(*(msg+1)) {
      process_command(d->player, msg+1, d->player, 0);
      for(a=0;a<10;a++)
        *env[a]='\0';
    }

    /* redisplay prompt */
    if(*d->env[3]) {
      s=tprintf("%s %s", d->env[3], PROMPT(d));
      if(d->flags & C_OUTPUTOFF)
        output2(d, s);
      else
        output(d, s);
    }
    return 1;
  }

  d->state=10;
  if(!Invalid(cause) && !Invalid(thing) && controls(cause,thing,POW_MODIFY) &&
     (attr=atr_str(thing, d->env[2])) && !(attr->flags & (AF_HAVEN|AF_LOCK)) &&
     *(s=atr_get_obj(thing, attr->obj, attr->num))) {
    strcpy(env[0], msg);
    for(a=0;a<9;a++)
      strcpy(env[a+1], d->env[a+4]);
    immediate_que(thing, s, d->player);
    for(a=0;a<10;a++)
      *env[a]='\0';
  }

  return 1;
}

/* redirect a player's input to an object attribute */
void do_input(dbref player, char *arg1, char *arg2, char **argv)
{
  DESC *d;
  ATTR *attr;
  char *prompt, *s;
  dbref object, thing=match_thing(player, arg1, MATCH_EVERYTHING);
  int a;

  /* check validity of target player */
  if(thing == NOTHING)
    return;
  if(Typeof(thing) != TYPE_PLAYER) {
    notify(player, "You can only receive input from players.");
    return;
  } if(!(d=Desc(thing)) || !(d->flags & C_CONNECTED)) {
    notify(player, "That player is not connected.");
    return;
  } if(!controls(player, thing, POW_CONSOLE)) {
    notify(player, "Permission denied.");
    return;
  }

  /* Turn off an input prompt */
  if(!argv[1] || !*argv[1]) {
    if(d->state != 35) {
      notify(player, "That player is not using an input prompt.");
      return;
    }
    d->state=10;
    if(d->flags & C_OUTPUTOFF)
      output2(d, PROMPT(d));
    else
      output(d, PROMPT(d));
    return;
  }

  if(d->state != 10 && d->state != 35) {
    notify(player, "That player is currently busy.");
    return;
  }

  if((prompt=strchr(argv[1], ':'))) {
    *prompt++='\0';
    a=strlen(prompt);
    if(*prompt == '{' && *(prompt+a-1) == '}') {
      *(prompt+a-1) = '\0';
      prompt++;
    }
  }

  /* validate operating object */
  if(!(s=strchr(argv[1], '/'))) {
    notify(player, "You must specify an attribute name.");
    return;
  }
  *s++='\0';
  if((object=match_thing(player, argv[1], MATCH_EVERYTHING)) == NOTHING)
    return;
  if(!controls(player, object, POW_MODIFY)) {
    notify(player, "Permission denied.");
    return;
  }

  /* validate attribute */
  if(!(attr=atr_str(object, s))) {
    notify(player, "No match.");
    return;
  } if(!can_see_atr(player, object, attr)) {
    notify(player, "Permission denied.");
    return;
  } if(attr->flags & (AF_HAVEN|AF_LOCK)) {
    notify(player, "Cannot trigger that attribute.");
    return;
  }

  /* store network redirection environment */
  d->state=35;
  WPTR(d->env[0], tprintf("%d", player));
  WPTR(d->env[1], tprintf("%d", object));
  WPTR(d->env[2], s);
  WPTR(d->env[3], prompt?:"");

  /* copy environment %1-%9 */
  for(a=0;a<9;a++)
    WPTR(d->env[a+4], argv[a+2]?:"");

  /* display optional prompt */
  if(prompt) {
    prompt=tprintf("%s %s", prompt, PROMPT(d));
    if(d->flags & C_OUTPUTOFF)
      output2(d, prompt);
    else
      output(d, prompt);
  }
}

static void answering_machine(DESC *d)
{
  DESC *k;
  int count=0;

  d->flags|=C_LOGOFF;

  /* count users online */
  for(k=descriptor_list;k;k=k->next)
    if(!(k->flags & C_NOMESSAGE))
      count++;

  for(k=descriptor_list;k;k=k->next)
    if((k->flags & C_SOCKET) && k->port == d->socket)
      break;
  if(!k)
    return;
  
  output2(d, tprintf("\nWelcome to network system: %s %d.\n", localhost(),
          k->port));
  output2(d, tprintf("Local time is %.24s\nThere are %d/%d users online.\n\n",
          ctime((time_t *)&now), count, (int)open_files));
  if(*k->env[0])
    display_textfile(d, k->env[0]);
  log_io("Auto Message %d, %s. \"Port %d: %s\"", d->concid, d->addr, k->port,
         k->env[0]);
}


void do_terminate(player, obj, arg2, argv, pure, cause)
dbref player, cause;
char *obj, *arg2, **argv, *pure;
{
  DESC *d;
  dbref thing=*obj?lookup_player(obj):cause;

  if(thing == AMBIGUOUS)
    thing=player;
  else if(thing == NOTHING || Typeof(thing) != TYPE_PLAYER) {
    notify(player, "No such player '%s'.", obj);
    return;
  }

  if(Typeof(thing) != TYPE_PLAYER || !(d=Desc(thing)) ||
     !(d->flags & C_CONNECTED)) {
    notify(player, "That player is not connected.");
    return;
  } if(!controls(player, thing, POW_CONSOLE)) {
    notify(player, "Permission denied.");
    return;
  } if(d->state != 35) {
    notify(player, "That player isn't running a program.");
    return;
  }

  d->state=10;
  if(player == thing)
    notify(player, "Terminated.");
  else  /* Remove prompt */
    output2(d, tprintf("\n%s", PROMPT(d)));
}


struct srvcmd server_command[]={
  {0,	"Login",          login_init},
  {1,	"Password",       enter_pass},
  {2,	"Login Retry",    login_init},
  {3,	"Newbie Init",    null_entry},
  {4,	"Session Init",   null_entry},
  {5,	"Char Create",    char_create},
  {6,	"New Password",   new_pass},
  {7,	"Verify Pass",    verify_pass},
  {8,	"Change Pass",    change_pass},
  {9,	"Set Rlname",     set_rlname},
  {10,	"Game",           game_driver},
  {11,	"Exit Prompt",    exit_prompt},
  {12,	"Set Email",      set_email},
  {13,	"Set Reginfo",    set_reginfo},
  {14,	"Set Gender",     set_gender},
  {15,	"Select Race",    select_race},
  {16,	"Verify Race",    verify_race},
  {17,	"Config Ansi",    config_ansi},
  {18,	"",               null_entry},
  {19,	"",               null_entry},
  {20,	"",               null_entry},
  {21,	"",               null_entry},
  {22,	"Guest Login",    null_entry},
  {23,	"",               null_entry},
  {24,	"Title Screen",   motd_screen},
  {25,	"Rules Screen",   prologue_screen},
  {26,	"Motd Screen",    game_connect},
  {27,	"Helptext Pager", page_helpfile},
  {28,	"",               null_entry},
  {29,	"",               null_entry},
  {30,	"Concentrator",   null_entry},
  {31,	"Email Handler",  service_smtp},
  {32,	"Receiving Mail", smtp_data_handler},
  {33,  "Remotelink",     service_rwho},
  {34,  "Race Info",      init_race_screen},
  {35,	"Input Prompt",   input_prompt},
  {36,	"Paste Text",     paste_text},
  {37,	"HTML Request",   html_request},
  {38,  "Auto Message",   null_entry},
  {39,	"Race Pager",     race_pager},
  {40,  "Process Ident",  process_ident},
  {-1,	""}};

static char *conc_status(DESC *d)
{
  int a;

  if(d->flags & C_NONBLOCK)
    return "Connect Wait";

  if(d->flags & C_SOCKET) {
    switch(d->type) {
      case SOCKET_GAME:  return "Game Port";
      case SOCKET_ADMIN: return "Admin Port";
      case SOCKET_HTTP:  return "Web Server";
      case SOCKET_MESG:  return "Auto Message";
    }
    return "";
  }

  for(a=0;server_command[a].state != -1;a++)
    if(d->state == server_command[a].state)
      break;
  return server_command[a].title;
}

char *conc_title(DESC *d)
{
  static char buf[540];

  if(Invalid(d->player))
    return conc_status(d);
  sprintf(buf, "%s(#%d) - %s", db[d->player].name, d->player, conc_status(d));
  return buf;
}

/* recursively trace incoming connections to sockets and concentrators */
void do_ctrace(dbref player)
{
  DESC *d, *k, *l;
  int amt=0;

  notify(player, "\e[1;4;35mPlayer             Status          Desc   ConcID   Address               Port\e[m");

  for(d=descriptor_list;d;d=d->next,amt++) {
    if(d->concid < 0)
      continue;

    if(d->flags & C_DNS)
      notify(player, "\e[1;36mDNS Resolver                       %-7d%d\e[m",
             d->fd, d->concid);
    else if(d->flags & (C_SOCKET|C_OUTBOUND)) {
      notify(player, "\e[1;36m%-19.19s%-14.14s  %-7d%-9d%-20.20s  %d\e[m",
             (d->flags & C_SOCKET)?"Socket":(d->flags & C_IDENTSOCK)?
             "Ident Socket":"Outbound Socket",conc_status(d), d->fd, d->concid,
             d->addr, d->port);
      for(k=descriptor_list;k;k=k->next) {
        if(k->socket != d->port)
          continue;
        if(!(k->flags&(C_OUTBOUND|C_DNS|C_SOCKET|C_REMOTE|C_PARENT|C_SERVICE))){
          notify(player, "\e[36m %-16.16s  %-14.14s  %-7d%-9d%-20.20s  %d\e[m",
            Invalid(k->player)?"Unconnected":db[k->player].name,conc_status(k),
            k->fd, k->concid, k->addr, k->port);
        } else if(k->flags & C_SERVICE) {
          notify(player, "\e[1;32m Service           %-14.14s  %-7d%-9d"
                 "%-20.20s  %d\e[m", conc_status(k), k->fd, k->concid,
                 k->addr, k->port);
          for(l=descriptor_list;l;l=l->next)
            if((l->flags & C_REMOTE) && l->parent == k)
              notify(player, "\e[32m  %-16.16s %-14.14s  %-7d%-9d%-20.20s  %d"
                "\e[m", Invalid(l->player)?"Unconnected":db[l->player].name,
                conc_status(l), l->fd,l->concid,l->addr,l->port);
        }
      }
    }
  }

  notify(player, "\e[1m\304\304\304\e[m");
  notify(player, "\e[1;32mTotal Connections: %d   Open Files: %d.  "
         "\e[33m%s\e[m", amt, nstat[NS_FILES], grab_time(player));
}

/* d->env[0] = Service Type */
static int connect_service(DESC *d, char *pass)
{
  FILE *f;
  char buf[256], *p, *r, *s, *addr=ipaddr(d->ipaddr);
  int a;

  if(!(f=fopen("etc/services", "r")))
    return 0;

  /* read services file */
  while(1) {
    if(!ftext(buf, 256, f)) {
      fclose(f);
      return 0;
    }

    if((s=strchr(buf, '#')))
      *s='\0';
    if(!*buf)
      continue;

    /* match correct service as first parameter */
    s=buf;
    while(isspace(*s))
      s++;
    if(!(r=strspc(&s)))
      continue;
    for(a=0;a < NELEM(services);a++)
      if(!strcasecmp(services[a], r))
        break;
    if(a == NELEM(services) || strcasecmp(d->env[0], services[a]))
      continue;

    /* match hostname */
    if(!(r=strspc(&s)))
      break;
    if((p=strchr(d->addr, '@')))
      p++;
    else
      p=d->addr;
    if(strcasecmp(p, r) && strcmp(addr, r))
      continue;

    /* match password */
    if(!(r=strspc(&s)) || !strcmp(pass, r))
      break;
  }

  fclose(f);
  switch(a) {
    case 0:
      if(d->flags & C_REMOTE)
        return 0;
      init_concentrator(d);
      break;
    case 1: d->state=31; break;
    case 2: d->state=33; break;
    default: return 0;
  }

  d->flags|=C_SERVICE;
  log_io("%s %d: %s. desc->%d", server_command[d->state].title, d->concid,
         d->addr, d->fd);
  server_command[d->state].fun(d, NULL);
  return 1;
}


/* Concentrator Service */

static void init_concentrator(DESC *d)
{
  d->flags=C_PARENT;
  d->state=30;

  free(d->raw_input);
  d->raw_input=NULL;

  /* Send concentrator identification string */
  output2(d, tprintf("Concentrator Protocol 2.0 %.32s\n", MUD_NAME));
}

int service_conc(DESC *d, char *msg, int len)
{
  struct conc_input *io=&d->conc_io;
  int result, size, offset=0;

  while(offset < len) {
    /* Read message header */
    if(!io->data) {
      if((size=len-offset) > 6-io->offset)
        size=6-io->offset;
      memcpy((char *)&io->concid+io->offset, msg+offset, size);
      io->offset += size;
      if(io->offset < 6)
        return 1;  /* Need more data for header */

      offset += size;
      io->offset=0;
      io->concid=ntohl(io->concid);
      io->len=ntohs(io->len);
      if(io->len < 1 || io->len > 32768)
        return 0;  /* Invalid packet size */
      io->data=malloc(io->len);
    }

    /* Read message contents */
    if((size=len-offset) > io->len-io->offset)
      size=io->len-io->offset;
    memcpy(io->data+io->offset, msg+offset, size);
    offset += size;
    io->offset += size;
    if(io->offset == io->len) {
      result = process_conc_command(d);
      free(io->data);
      io->data=NULL;
      io->offset=0;
      io->len=0;
      if(!result)
        return 0;
    }
  }

  return 1;
}

static int process_conc_command(DESC *d)
{
  DESC *k;
  struct conc_input *io=&d->conc_io;
  int local_id, ipaddr, port;
  unsigned char hostname[65], *s, *t=hostname;

  /* Open or close concentrator connections */
  if(!io->concid) {
    if(!io->len)
      return 0;

    switch(io->data[0]) {
    case 1:  // Create Connection ID
      if(io->len < 11)
        return 0;

      /* Copy big-endian integer arguments */
      s=io->data+1;
      local_id=get32(s);
      ipaddr=htonl(get32(s));  /* Switch back to big-endian */
      port=get16(s);

      /* Check security on ipaddr (for God logins from 'localhost') */
      if(!ipaddr || ntohl(ipaddr) >> 24 == 127)
        ipaddr=d->ipaddr;

      /* Copy hostname string */
      while(*s && s-io->data < io->len && s-io->data < 64)
        *t++=*s++;
      *t='\0';

      do_connectid(d, local_id, ipaddr, hostname, port);
      return 1;

    case 2:  // Delete Connection ID
      if(io->len < 5)
        return 0;

      s=io->data+1;
      local_id=get32(s);
      for(k=descriptor_list;k;k=k->next)
        if(k->concid == local_id && (k->flags & C_REMOTE) && k->parent == d) {
          k->flags |= C_LOGOFF;
          return 1;
        }
      return 1;
    }

    /* Invalid Opcode; Close connection */
    return 0;
  }

  /* Determine which connection we're talking to */
  for(k=descriptor_list;k;k=k->next)
    if(k->concid == io->concid && (k->flags & C_REMOTE) && k->parent == d)
      break;

  /* Deliver command input to connection */
  if(k) {
    k->input += io->len;
    process_input_internal(k, io->data, io->len);
  }
  return 1;
}

static void do_connectid(DESC *d, int local_id, int ipaddr, char *hostname,
                         int port)
{
  DESC *k;

  struct {
    int remote_concid;
    unsigned short len;
    char cmd;
    int local_id;
    int concid;
  } __attribute__((__packed__)) resp;

  /* Initialize file descriptor */
  k=initializesock(d->fd);
  k->concid=++nstat[NS_LOGINS];
  k->flags|=C_REMOTE;
  k->socket=d->socket;
  k->type=d->type;
  k->parent=d;

  /* Save internet address */
  k->port=port;
  k->ipaddr=ipaddr;
  SPTR(k->addr, hostname);

  /* Send Identity Response to concentrator */
  resp.remote_concid=0;
  resp.len=htons(9);
  resp.cmd=3;
  resp.local_id=htonl(local_id);
  resp.concid=htonl(k->concid);
  queue_output(d, 1, &resp, sizeof(resp));

  init_connect(k);
}


/* Rwho Service */
static int service_rwho(DESC *d, char *msg)
{
  DESC *k;
  int a;

  /* initialize service */
  if(!msg) {
    /* clear buffers */
    WPTR(d->env[0], "");

    /* count visible connected players */
    for(a=0,k=descriptor_list;k;k=k->next)
      if((k->flags & C_CONNECTED) && could_doit(0, k->player, A_HLOCK))
        a++;

    /* identification string */
    output2(d, tprintf("$0 1;%s;%s;%s;%ld;%ld;%ld;%d\n",
            MUD_NAME, mud_version, system_version, now, mud_up_time,
            last_reboot_time, a));
    return 1;
  }

  /* process command request */
  if(*msg == '$' && isdigit(*(msg+1))) {
    /* extract command number and advance pointer to data */
    a=atoi(++msg);
    while(*msg && !isspace(*msg))
      msg++;
    while(isspace(*msg))
      msg++;

    switch(a) {
    case 0:  // Store list of all connected remote MARE sites.
      WPTR(d->env[0], msg);
      break;
    }

    return 1;
  }

  return 1;
}
