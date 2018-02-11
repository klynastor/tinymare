/* hdrs/config.h */
/* global varable declarations */

// See mare/config.c for special flag explanations.
//
// When adding local configuration definitions, do not exceed
// reference number 255 or the database can become corrupt.
//
// Variable numbers 100-199 are reserved for combat and 200-229 for weather.


// ref#,type,variable,default setting,variable name,flags

/* Locks out Rank < X from the game. Set to -1 to lockout 'new' */
CONF(0,int,WIZLOCK_LEVEL,0,"WIZLOCK_LEVEL",INT)
CONF(1,int,USER_LIMIT,0,"USER_LIMIT",INT)
CONF(2,int,INACTIVENUKE,0,"INACTIVENUKE",INT)  // #days until players get nuked
                                               // ^1 for variability on combat
CONF(3,int,ERR_SIGNAL,1,"ERR_SIGNAL",INT)      // 1=stay alive after segfault
CONF(4,int,LOG_FAILED_CMDS,1,"LOG_FAILED_CMDS",INT)
CONF(5,int,COMMAND_LOGSIZE,150,"COMMAND_LOGSIZE",INT)
CONF(6,int,HIDDEN_ADMIN,0,"HIDDEN_ADMIN",INT)
CONF(7,int,GUEST_LOGIN,0,"GUEST_LOGIN",INT|LBRK)

CONF(8,int,TIMEZONE,-5,"TIMEZONE",INT)         // Hour of BaseSystem from UTC
CONF(9,int,HAVE_DST,1,"HAVE_DST",INT|LBRK)     // 1=daylight savings time

/* database options */
#ifdef __CYGWIN__
CONF(10,int,DB_FORK,0,"DB_FORK",INT)           // Cygwin defaults to 0
#else
CONF(10,int,DB_FORK,1,"DB_FORK",INT)           // 1=Save DB in background
#endif
CONF(11,int,DB_LIMIT,0,"DB_LIMIT",INT)         // DB cannot grow beyond #objs
CONF(12,int,DB_NOTIFY,0,"DB_NOTIFY",INT|LBRK)  // 1=Show "Mudname Autosave..!"

/* environment restrictions */
CONF(15,int,RESTRICT_BUILD,0,"RESTRICT_BUILD",INT)
CONF(16,int,RESTRICT_TELEPORT,0,"RESTRICT_TELEPORT",INT)
CONF(17,int,RESTRICT_HASPOW,0,"RESTRICT_HASPOW",INT)
CONF(18,int,RESTRICT_HIDDEN,0,"RESTRICT_HIDDEN",INT|LBRK)

/* haspow: 1=users can't use powers(), haspow(), or controls() on immortals */
/* hidden: 1=only wizards can set @hlock; hidden users unlisted on 'who' */

/* special features */
CONF(20,int,FIND_MONEY,40,"FIND_MONEY",INT)          // find GP 1 in # steps
CONF(21,int,MAX_DISCOVER,1,"MAX_DISCOVER",INT)       // amt of money discovered
CONF(22,int,ALLOW_COMTITLES,1,"ALLOW_COMTITLES",INT) // 1=show comtitles
CONF(23,int,FORCE_UC_NAMES,0,"FORCE_UC_NAMES",INT|LBRK)// 1=Force uc player names

/* command costs */
CONF(30,int,ZONE_COST,100,"ZONE_COST",INT)
CONF(31,int,ROOM_COST,10,"ROOM_COST",INT)
CONF(32,int,THING_COST,10,"THING_COST",INT)
CONF(33,int,EXIT_COST,1,"EXIT_COST",INT)
CONF(34,int,LINK_COST,1,"LINK_COST",INT)
CONF(35,int,SEARCH_COST,20,"SEARCH_COST",INT)
CONF(36,int,FIND_COST,20,"FIND_COST",INT)
CONF(37,int,PAGE_COST,0,"PAGE_COST",INT)
CONF(38,int,COM_COST,0,"COM_COST",INT)
CONF(39,int,ANNOUNCE_COST,100,"ANNOUNCE_COST",INT)
CONF(40,int,MAIL_COST,5,"MAIL_COST",INT)
CONF(41,int,NEWS_COST,15,"NEWS_COST",INT)
CONF(42,int,EMAIL_COST,25,"EMAIL_COST",INT)
CONF(43,int,QUEUE_COST,64,"QUEUE_COST",INT|LBRK)   // 1/64th GP per command

/* flag costs */
CONF(50,int,LINK_OK_COST,0,"LINK_OK_COST",INT)
CONF(51,int,JUMP_OK_COST,0,"JUMP_OK_COST",INT)
CONF(52,int,ABODE_COST,0,"ABODE_COST",INT)
CONF(53,int,DARK_COST,10,"DARK_COST",INT)
CONF(54,int,HEALING_COST,10,"HEALING_COST",INT)
CONF(55,int,ILLNESS_COST,10,"ILLNESS_COST",INT)
CONF(56,int,OCEANIC_COST,50,"OCEANIC_COST",INT)
CONF(57,int,SHAFT_COST,100,"SHAFT_COST",INT|LBRK)

/* internal limits */
CONF(60,int,MAX_OUTPUT,32768,"MAX_OUTPUT",INT)
CONF(61,int,QUEUE_QUOTA,100,"QUEUE_QUOTA",INT)
CONF(62,int,MAX_PLAYER_CMDS,50000,"MAX_PLAYER_CMDS",INT)
CONF(63,int,MAX_WIZARD_CMDS,100000,"MAX_WIZARD_CMDS",INT)
CONF(64,int,MAX_PLAYER_WAIT,5000,"MAX_PLAYER_WAIT",INT)
CONF(65,int,MAX_WIZARD_WAIT,10000,"MAX_WIZARD_WAIT",INT)
CONF(66,int,FUNC_RECURSION,30,"FUNC_RECURSION",INT)
CONF(67,int,MAX_FUNCTIONS,4000,"MAX_FUNCTIONS",INT)
CONF(68,int,LOCK_LIMIT,10,"LOCK_LIMIT",INT|LBRK)

/* timer intervals */
CONF(70,int,DUMP_INTERVAL,4000,"DUMP_INTERVAL",INT)
CONF(71,int,WEATHER_INTERVAL,271,"WEATHER_INTERVAL",INT)
CONF(72,int,ATIME_INTERVAL,60,"ATIME_INTERVAL",INT)
CONF(73,int,WANDER_INTERVAL,180,"WANDER_INTERVAL",INT)
CONF(74,int,WANDER_RATE,7,"WANDER_RATE",INT|LBRK)  // # DB Loops per Interval

/* network timers */
CONF(75,int,TIMEOUT_LOGIN,60,"TIMEOUT_LOGIN",INT)
CONF(76,int,TIMEOUT_SESSION,0,"TIMEOUT_SESSION",INT)
CONF(77,int,LIMIT_LOGIN,300,"LIMIT_LOGIN",INT)
CONF(78,int,LIMIT_SESSION,0,"LIMIT_SESSION",INT|LBRK)

/* host & mail configuration */
CONF(80,char *,MUD_NAME,DEFAULT_MUDNAME,"MUD_NAME",CHAR|SAVE)
CONF(81,char *,TECH_EMAIL,DEFAULT_EMAIL,"TECH_EMAIL",CHAR|SAVE)
CONF(82,char *,MAILHOST,"","MAILHOST",CHAR)     // default=localhost
CONF(83,char *,EMAIL_ADDRESS,"","EMAIL_ADDRESS",CHAR)  // incoming email addr
CONF(84,int,MAIL_EXPIRE,30,"MAIL_EXPIRE",INT|LBRK) // #days read +mail expires

/* storyline game defaults */
CONF(90,dbref,PLAYER_START,0,"PLAYER_START",DBREF|ROOM)
CONF(91,dbref,GUEST_START,0,"GUEST_START",DBREF|ROOM)
CONF(92,dbref,GAME_DRIVER,0,"GAME_DRIVER",DBREF)
CONF(93,dbref,DUNGEON_CELL,0,"DUNGEON_CELL",DBREF|ROOM)
CONF(94,char *,GUEST_PREFIX,"Guest","GUEST_PREFIX",CHAR|LBRK)

/* money settings */
CONF(95,int,START_BONUS,150,"START_BONUS",INT)
CONF(96,int,GUEST_BONUS,100,"GUEST_BONUS",INT)
CONF(97,int,ALLOWANCE,50,"ALLOWANCE",INT)
CONF(98,char *,MONEY_SINGULAR,"Gold Piece","MONEY_SINGULAR",CHAR)
CONF(99,char *,MONEY_PLURAL,"Gold Pieces","MONEY_PLURAL",CHAR|LBRK)

/* world settings */
CONF(200,long,BEGINTIME,0,"BEGINTIME",LONG)          // MARE Start Date
CONF(201,int,FIRST_YEAR,1,"FIRST_YEAR",INT)          // Game Time - First Year
CONF(202,int,HOURS_PER_DAY,24,"HOURS_PER_DAY",INT)   // #hours RL = 1 game day
CONF(203,int,MOONPHASE,2551443,"MOONPHASE",INT|LBRK) // #secs full revolution

/* current weather data */
CONF(210,int,TIMEOFDAY,1,"TIMEOFDAY",INT)       // 0=night, 1=day

