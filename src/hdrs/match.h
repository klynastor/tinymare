/* hdrs/match.h */

extern int match_quiet;  // Suppress "I don't see X" message in function calls

/* match_thing() flags */

#define MATCH_ME          BV0   // Match word 'me' as player
#define MATCH_HERE        BV1   // Match word 'here' as location
#define MATCH_CON         BV2   // Match contents of location
#define MATCH_EXIT        BV3   // Match exits in location
#define MATCH_INVEN       BV4   // Match player's inventory
#define MATCH_ABS         BV5   // Match #dbref
#define MATCH_PLAYER      BV6   // Match *playername

#define MATCH_EVERYTHING  0x7f  // Match all of the above
#define MATCH_RESTRICT    0x1f  // Restricted, non-remote match

#define MATCH_QUIET       BV8   // Don't display error message for bad match
#define MATCH_INVIS       BV9   // Match unseen objects in room as well
#define MATCH_HOME        BV10  // Match word 'home' as #-3
#define MATCH_VARIABLE    BV11  // Match word 'variable' as #-2
#define MATCH_ZONE        BV12  // Match room's zone's #dbref only
#define MATCH_DESTROYED   BV13  // Match destroyed objects owned by God
#define MATCH_FULLNAME    BV14  // Only matches player if full name is typed
#define MATCH_POSSESSIVE  BV15  // Enable matching of <player>'s <object>

/* wild_match() flags */

#define WILD_ENV	15	// 1-10 to record matches in Environment %0-%9
#define WILD_BOOLEAN	BV4	// Match mathematical args such as ! > <
#define WILD_ANSI	BV5	// Don't strip ANSI codes from Environment
#define WILD_RECURSIVE	BV6	// Recursive call within wild_match()

