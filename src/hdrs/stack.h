/* hdrs/stack.h - Architecture-independent stack manipulation */
/* Written by Byron Stanoszek (gandalf@winds.org), May 30 2003 */

// API Procedures:
//  INIT_STACK;       - Initialize the stack. Call this before initializing
//                      variables at the start of a function.
//  PUSH(x);          - Pushes variable 'x' onto the stack.
//  POP(x);           - Pops the next value from the stack into variable 'x'.
//  CLEAR_STACK;      - Resets the stack size to 0.
//
// API Functions:
//  STACKSZ(type)     - Returns number of 'type' elements in stack. 'type' is
//                      typically char, int, or long, but can be a structure.
//  STACKELEM(type,x) - Returns the 'x'th element in the stack, where 'type' is
//                      the element size. Stack elements begin at 0.
//  STACKTOP()        - Returns 1 if the stack is empty, 0 if not.

#define INIT_STACK  char *__sp=alloca(0), *__ep=__sp, \
                         *__top __attribute__((__unused__))=__ep
#define CLEAR_STACK __sp=__top

#define STACKTOP() (__sp == __top)

#define __align(x) max(sizeof(typeof(x)), sizeof(long))


#ifdef STACK_GROWS_DOWN

#define PUSH(x) ({ \
  if((__sp -= __align(x)) < __ep) { \
    __ep=alloca(__align(x)); \
    asm("" :: "r" (__ep));  /* Never optimize out the alloca() */ \
  } \
  *(typeof(x)*) __sp=x; \
})

#define POP(x) ({ \
  (x) = *(typeof(x)*) __sp; \
  __sp += __align(x); \
})

#define STACKSZ(t) ((__top-__sp)/max(sizeof(t), sizeof(long)))

#define STACKELEM(t,x) (*(t *)(__top-(max(sizeof(t), sizeof(long))*((x)+1))))

#else

#define PUSH(x) ({ \
  if(__sp + __align(x) > __ep) { \
    __ep=alloca(__align(x)) + __align(x); \
    asm("" :: "r" (__ep));  /* Never optimize out the alloca() */ \
  } \
  *(typeof(x)*) __sp=x; \
  __sp += __align(x); \
})

#define POP(x) ({ \
  __sp -= __align(x); \
  (x) = *(typeof(x)*) __sp; \
})

#define STACKSZ(t) ((__sp-__top)/max(sizeof(t), sizeof(long)))

#define STACKELEM(t,x) (*(t *)(__top+(max(sizeof(t), sizeof(long))*(x))))

#endif
