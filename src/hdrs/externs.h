/* hdrs/externs.h */
/* included by every file; sets up our system declarations */

#define _GNU_SOURCE	// Use the GNU C Library Extensions

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "main.h"

/* This macro will evaluate to 0 if we are not using gcc at all */
#ifndef GCC_VERSION
# define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
#endif
#if GCC_VERSION < 2008
# error "TinyMARE requires GCC 2.8.0 or later to compile."
#endif

typedef int dbref;

/* Let 'quad' be our 64-bit int type */
#undef quad
#define quad long long

/* Bit values can range from 0 to 63 and can be specified BV0 to BV63 */
enum bitvalues {
#define BV(d) BV##d=(1LL << d)
  BV(0), BV(1), BV(2), BV(3), BV(4), BV(5), BV(6), BV(7), BV(8), BV(9),
  BV(10),BV(11),BV(12),BV(13),BV(14),BV(15),BV(16),BV(17),BV(18),BV(19),
  BV(20),BV(21),BV(22),BV(23),BV(24),BV(25),BV(26),BV(27),BV(28),BV(29),
  BV(30),BV(31),BV(32),BV(33),BV(34),BV(35),BV(36),BV(37),BV(38),BV(39),
  BV(40),BV(41),BV(42),BV(43),BV(44),BV(45),BV(46),BV(47),BV(48),BV(49),
  BV(50),BV(51),BV(52),BV(53),BV(54),BV(55),BV(56),BV(57),BV(58),BV(59),
  BV(60),BV(61),BV(62),BV(63)
#undef BV
};

/* Also allow variable values, BV(0) to BV(63) */
#define BV(d) (1LL << (d))

/* Macro to access a particular range of bits of var; use as an lvalue */
#define BITFIELD(var, bit, width) \
          (((struct { typeof(var):bit; typeof(var) _f:width; } *)&(var))->_f)

/* Swap the values of two integers, quadwords, doubles, etc. */
#define XCHG(a,b) (void)({ typeof(a) _temp=a; a=b; b=_temp; })
/* ...The same thing can be done using:  a ^= b, b ^= a, a ^= b */

/* Generic min() and max() functions */
#undef min
#undef max
#define min(x,y) ({ typeof(x) _x=x, _y=y; (_x < _y)?_x:_y; })
#define max(x,y) ({ typeof(x) _x=x, _y=y; (_x > _y)?_x:_y; })

/* Optimal way of keeping a number within a set range */
#define RANGE(x,lo,hi) ({ int _val=x, _lo=lo, _hi=hi; \
                          (_val < _lo)?_lo:(_val > _hi)?_hi:_val; })

/* Determines the number of elements in a static array */
#define NELEM(array) (int)(sizeof(array)/sizeof(array[0]))

/* Count the number of bits set in a word */
#ifdef __ia64__
# define popcount(x) ({ unsigned long _result; \
			__asm__("popcnt %0=%1" : "=r" (_result) : "r" (x)); \
			_result; })
#elif defined(__alpha_cix__) && defined(__alpha_fix__)  /* Alpha EV67 */
# define popcount(x) ({ unsigned long _result; \
			__asm__("ctpop %1,%0" : "=r" (_result) : "r" (x)); \
			_result; })
#elif defined(ARCH_64BIT)
# define popcount(x) ({ unsigned long _r=(x), _t; \
			_r -= ((_r >> 1) & 0x5555555555555555); \
			_t  = ((_r >> 2) & 0x3333333333333333); \
			_r  = ((_r & 0x3333333333333333) + _t); \
			_r  = (_r + (_r >> 4)) & 0x0F0F0F0F0F0F0F0F; \
			_r += (_r << 8); \
			_r += (_r << 16); \
			_r += (_r << 32); \
			_r >> 56; })
#else
# define popcount(x) ({ unsigned long _r=(x), _t; \
			_r -= ((_r >> 1) & 0x55555555); \
			_t  = ((_r >> 2) & 0x33333333); \
			_r  = ((_r & 0x33333333) + _t); \
			_r  = (_r + (_r >> 4)) & 0x0F0F0F0F; \
			_r += (_r << 8); \
			_r += (_r << 16); \
			_r >> 24; })
#endif

/* Rotates a 32-bit word right or left */
#define ROR(x,n) (((x) >> ((n) & 0x1F)) | ((x) << (32-((n) & 0x1F))))
#define ROL(x,n) (((x) << ((n) & 0x1F)) | ((x) >> (32-((n) & 0x1F))))

/* Reverses the bits in a single byte */
#define revbyte(b) ({ unsigned char _b=b; \
  _b=(((_b * 0x0802 & 0x22110) | (_b * 0x8020 & 0x88440)) * 0x10101) >> 16; })

/* Gets a big-endian 16-bit or 32-bit int from a stream, incrementing ptr */
#define get16(p) (p+=2, \
                  ((unsigned char)p[-2] << 8) | \
                   (unsigned char)p[-1])
#define get32(p) (p+=4, \
                  ((unsigned char)p[-4] << 24) | \
                  ((unsigned char)p[-3] << 16) | \
                  ((unsigned char)p[-2] << 8) | \
                   (unsigned char)p[-1])

/* Puts a big-endian 16-bit or 32-bit int to a stream, incrementing ptr */
#define put16(p,x) ({ unsigned short _x=x; \
		      p+=2; p[-2]=_x >> 8; p[-1]=_x; })
#define put32(p,x) ({ unsigned int _x=x; \
		      p+=4; p[-4]=_x >> 24; p[-3]=_x >> 16; \
		      p[-2]=_x >> 8; p[-1]=_x; })

/* Check for missing standards */
#ifndef NULL
# define NULL ((void *)0)
#endif

#ifndef SIGCHLD
# define SIGCHLD SIGCLD
#endif
#ifndef SIGFPE
# define SIGFPE SIGEMT
#endif

/* Corrects a bug with realloc() & memmove() in SunOS 4.x */
#ifdef _SUN_4
# define realloc(ptr, size) ({ void *_mem=ptr; int _sz=size; \
			       if(!_mem) _mem=malloc(_sz); \
			       else _mem=realloc(_mem, _sz); _mem; })
# define memmove(d,s,n) builtin_memmove(d,s,n)
#endif

/* Only GNU Libc and SunOS 5.0+ have an atoll() function */
#if !defined(__GLIBC__) && !(defined(__sun__) && !defined(_SUN_4))
# define atoll(x) builtin_atoll(x)
#endif

/* QNX has its strcasecmp-style functions in a different header file */
#ifdef __QNX__
# include <strings.h>
#endif

/* Handle special 64-bit byteswap */
#ifndef htonll
# if defined(__x86_64__)
#  define htonll(x) ({ quad _r=x; \
                       __asm__("bswapq %0" : "=r" (_r) : "0" (_r)); _r; })
# else
#  define htonll(x) ({ quad _r=(x), _res; \
		       if(htons(1) == 1) _res=_r; \
		       else _res=(((quad)htonl(_r)) << 32) | htonl(_r >> 32); \
		       _res; })
# endif
# define ntohll(x) htonll(x)
#endif

#ifdef __CYGWIN__
# include <sys/cygwin.h>
# define __GLIBC__ 2
#endif

/* O_BINARY is only defined on Windows systems */
#ifndef O_BINARY
# define O_BINARY 0
#endif

/* Choose implementation of strsignal() for your system */
#if !defined(__GLIBC__) && !defined(__sun__)
# if defined(__hpux__)
#  define strsignal(x) ""
# elif defined(__osf__)
#  define strsignal(x) __sys_siglist[x]
# else
#  define strsignal(x) sys_siglist[x]
# endif
#endif

/* Define macros for GNU LIBC extensions if we don't have GNU LIBC 2.2 */
#if !defined(__GLIBC__) || ((__GLIBC__ * 1000) + __GLIBC_MINOR__ < 2002)
#define strchrnul(s,c) ({ char *_s=s; (strchr(_s, c) ?: (_s+strlen(_s))); })
#endif

/* Most C libraries map ctype.h functions into memory, which can slow things
   down quite a bit. Here we use our own macros and avoid locale-based maps. */
   
#undef isspace
#undef isdigit
#undef isalpha
#undef isalnum
#undef isxdigit
#define isspace(c)  ({ char _c=c; (_c == ' ' || _c == '\t'); })
#define isdigit(c)  ({ char _c=c; (_c >= '0' && _c <= '9'); })
#define isalpha(c)  ({ char _c=(c) | 32; (_c >= 'a' && _c <= 'z'); })
#define isalnum(c)  ({ char _ch=c; (isdigit(_ch) || isalpha(_ch)); })
#define isxdigit(c) ({ char _ch=c; (isdigit(_ch) || \
		       ((_ch | 32) >= 'a' && ((_ch | 32) <= 'f'))); })

#undef toupper
#undef tolower
#define toupper(c)  ({ char _c=c; (_c < 'a' || _c > 'z')?_c:(_c & ~32); })
#define tolower(c)  ({ char _c=c; (_c < 'A' || _c > 'Z')?_c:(_c | 32); })

#undef atof
#define atof(str) atof2(str)

/* Advance pointer after whitespace */
#define skip_whitespace(x) ({ while(*(x) == ' ') (x)++; })

/* Advance pointer beyond an ansi escape code */
#define skip_ansi(x) ({ if(*++x == '[')  /* Variable-length CSI code */ \
			  while(*++x && *x < 64); \
			else if(*x && *x < 48) x++;  /* 2-digit sequence */ \
			if(*x) x++; })

/* Copy an ansi escape code from 'x' to 'd', advancing pointers */
#define copy_ansi(d,x) ({ if(*(x+1)) { *d++=*x++; \
			    if(*x == '[') { \
			      *d++=*x++; \
			      while(*x && *x < 64) *d++=*x++; \
			    } else if(*x && *x < 48) *d++=*x++; \
			    if(*x) *d++=*x++; \
			  } else x++; })

/* Strip an ansi code by modifying string contents */
#define strip_ansi(x) ({ char *_s=x, *_t=_s, *_r=_s; \
			 do { \
			   while(*_s == '\e') skip_ansi(_s); \
			   *_t++=*_s; \
			 } while(*_s++); _r; })

/* Determine if a string contains all integers */
#define isnumber(str) ({ char *_s, *_str=str; \
                         for(_s=_str;isdigit(*_s);_s++); !*_s && _s != _str; })

/* tprintf(): Prints to a temporary variable 16k in size */
extern char global_buff[16384];
#define tprintf(format, args...) (sprintf(global_buff, format, ## args), \
				  (char *)global_buff)

/* Like strncpy but doesn't pad with zeroes. n=buffer size instead of siz-1 */
#define strmaxcpy(d, s, n) ({ char *_d=d; const char *_s=s; \
			      int _n=n, _len=strlen(_s)+1; \
			      if(_n > 0) { \
				memcpy(_d, _s, (_len > _n)?_n:_len); \
				_d[_n-1]='\0'; \
			      } _d; })

/* Returns a pointer to the end null byte in a string */
#define strnul(str)    ({ char *_str=str; _str+strlen(_str); })

/* Copies a string to a pointer allocated on the stack */
#define APTR(ptr, str) ({ char *_str=str; int _len=strlen(_str)+1; \
			  void *_p=alloca(_len); \
			  ptr=memcpy(_p, _str, _len); })

/* Sets a new pointer regardless of previous content; does not free ptr */
#define SPTR(ptr, str) ({ char *_str=str; int _len=strlen(_str)+1; \
			  ptr=memcpy(malloc(_len), _str, _len); })

/* Writes a string to a pointer, freeing previous contents */
#define WPTR(ptr, str) ({ char **_p=&(ptr), *_str=str; \
			  int _len=strlen(_str)+1; \
			  *_p=memcpy((free(*_p),malloc(_len)), _str, _len); })

/* Support architecture-independent stack algorithms */
#include "stack.h"


/* TinyMare-specific Section */

extern char *mud_copyright;
extern char *mud_compiler_version;
extern char *mud_compile_date;
extern char *mud_version;

extern char system_info[64];
extern char system_version[64];
extern char system_model[16];
extern int  system_ncpu;
extern int  system_cpu_mhz;

extern int tinyport;

#undef TIMEOFDAY

#define CONF(a,b,c,d,e,f) extern b c;
#include "config.h"
#undef CONF

/* Database structures and all other local definitions */
#include "net.h"
#include "log.h"
#include "powers.h"
#include "queue.h"
#include "match.h"

#include "db.h"

/* Function declarations */
#include "defs.h"

/* System-specific declarations */

#ifndef __QNX__
extern void *alloca(size_t);
#endif
