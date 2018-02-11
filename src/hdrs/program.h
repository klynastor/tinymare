/* hdrs/program.h */

struct program {
  int lines;		// Number of lines in the program
  char *data[];		// Contents of all program lines
};

typedef struct varlist VLIST;
struct varlist {
  VLIST *next;
  char *vname;        /* variable name */
  char *str;          /* variable contents */
};

struct progexec {
  dbref privs;		// Player executing the program
  dbref cause;		// Cause of execution

  int line;		// Current line number (1-based)
  int maxline;		// Number of lines in the program
  int cmd_count;	// Number of commands executed

  unsigned int depth:8;	// Current depth for if/for/while/switch
  struct program *prog;	// Copy of the program with included functions

  VLIST *vlist;			// Variable list
  int prog_return[MAX_DEPTH];	// Program line to go back on endif/endfor/etc.
  char *env[MAX_DEPTH][11];	// Environment arguments
  char *result_arg[MAX_DEPTH];	// %@ evaluation from last if/for/etc.
  int state[MAX_DEPTH];		// Specifies type of control loop
  int loop_count[MAX_DEPTH];	// Counter for control loop
  char *loop_pure[MAX_DEPTH];	// Contents of 'for'
};
