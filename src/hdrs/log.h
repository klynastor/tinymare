/* hdrs/log.h */
/* external definitions for logging messages */

struct log {
  char *filename;
  char *com_channel;
  int output;
};

extern struct log main_log, error_log, io_log, resolv_log;

#define log_main(fmt,args...)   mud_log(&main_log, fmt, ## args)
#define log_error(fmt,args...)  mud_log(&error_log, fmt, ## args)
#define log_io(fmt,args...)     mud_log(&io_log, fmt, ## args)
#define log_resolv(fmt,args...) mud_log(&resolv_log, fmt, ## args)

#define DEFAULT_LOG "_log_io _log_main _log_err"

#undef perror
#define perror(str) lperror(__FILE__,__LINE__,str)

extern void mud_log(struct log *l, char *fmt, ...) __attribute__((format(printf, 2, 3)));
extern void lperror(char *file, int line, char *header);
