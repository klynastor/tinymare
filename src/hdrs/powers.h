/* hdrs/powers.h */
/* Defines class and power structures */

#define CLASS_GUEST	0  // Guest character, disappears on QUIT
#define CLASS_PLAYER	1  // Fully registered player character
#define CLASS_IMMORTAL	2  // Lowest ranked class designated as immortal
#define CLASS_WIZARD	5  // Highest wizard class

#define NUM_CLASSES	6  // Highest class + 1


#define PW_YES	3	// Usable on anybody except God
#define PW_EQ	2	// Only on people at or below class
#define PW_LT	1	// Only on people below class
#define PW_NO	0

// The database includes a sizable limit of a maximum 255 powers that
// can be defined below. Powers must be in sequential order starting at 0,
// without any gaps. NUM_POWS is the number of powers defined.
//
// If you change this list, don't forget to edit game/powers.c as well.

enum powers {
  POW_ANIMATION, POW_ANNOUNCE, POW_BACKSTAGE, POW_BOOT, POW_BROADCAST,
  POW_BUILD, POW_CAPTURE, POW_CHOWN, POW_COM, POW_COMBAT, POW_CONFIG,
  POW_CONSOLE, POW_DARK, POW_DB, POW_EXAMINE, POW_FREE, POW_FUNCTIONS,
  POW_JOIN, POW_MEMBER, POW_MODIFY, POW_MONEY, POW_NEWPASS, POW_NUKE,
  POW_PCREATE, POW_PFILE, POW_PLANE, POW_QUEUE, POW_REMOTE, POW_SECURITY,
  POW_SHUTDOWN, POW_SITELOCK, POW_SLAVE, POW_SPOOF, POW_STATS, POW_SUMMON,
  POW_TELEPORT, POW_WHO, POW_WIZATTR, POW_WIZFLAGS,
};

/* If you change this number, all .c files must be recompiled! */

#define NUM_POWS 39

extern char *classnames[];
extern char *typenames[];

extern const struct pow_list {
  char *name;			// Name of power
  char init[NUM_CLASSES];	// Inital value for each class
  char max[NUM_CLASSES];	// Maximum value in each class
  char *desc;			// Description of what the power does
} powers[NUM_POWS];
