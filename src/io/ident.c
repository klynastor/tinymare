/* io/ident.c */
/* identify a user */

#include "externs.h"
#include <errno.h>
#include <sys/socket.h>

// Open a non-blocking socket to query the remote host's identity.
//
void get_ident(DESC *d)
{
  struct sockaddr_in localaddr, sin_addr;
  int len, fd, ret;
  DESC *k;

  len = sizeof(localaddr);
  if(getsockname(d->fd, (struct sockaddr *)&localaddr, &len)) {
    perror("connect_ident: getsockname");
    return;
  }

  /* Open non-blocking socket */
  if((fd=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("connect_ident: socket");
    return;
  }
  fcntl(fd, F_SETFL, O_NONBLOCK);

  /* Bind to a specific internet device */
  sin_addr.sin_family=AF_INET;
  sin_addr.sin_addr=localaddr.sin_addr;
  sin_addr.sin_port=0;
  if(bind(fd, (struct sockaddr *)&sin_addr, sizeof(sin_addr))) {
    perror("connect_ident: bind");
    close(fd);
    return;
  }

  /* Attempt a non-blocking connect() to the remote address */
  sin_addr.sin_family = AF_INET;
  sin_addr.sin_addr.s_addr = d->ipaddr;
  sin_addr.sin_port = htons(113); /* identd port */

  if((ret=connect(fd, (struct sockaddr *)&sin_addr, sizeof(sin_addr))) == -1 &&
     errno != EINPROGRESS) {
    perror("connect_ident: connect");
    close(fd);
    return;
  }

  /* Tell main session to wait for identification */
  d->flags |= C_IDENT;

  /* Initialize descriptor for ident authentication */
  k=initializesock(fd);
  k->flags=C_OUTBOUND|C_IDENTSOCK;
  k->state=40;
  k->timer=3;
  k->concid=d->concid;
  k->ipaddr=d->ipaddr;
  k->port=d->port;
  k->socket=d->socket;
  SPTR(k->addr, d->addr);

  nstat[NS_FILES]++;

  if(ret == -1)  /* connect cannot complete immediately */
    k->flags |= C_NONBLOCK;
  else
    process_ident(k, NULL);
}

// Process input on an ident-connected descriptor.
//
int process_ident(DESC *d, char *msg)
{
  DESC *k;
  char *r=msg;
  int a;

  /* Send initial identification request to remote identd server */
  if(!msg) {
    output2(d, tprintf("%d,%d\r\n", d->port, d->socket));
    d->timer=3;
    return 1;
  }

  /* Match client descriptor */
  for(k=descriptor_list;k;k=k->next)
    if((k->flags & C_IDENT) && k->concid == d->concid)
      break;
  if(!k)
    return 0;  /* Client no longer exists */

  /* Parse input string and search for userid */
  for(a=0;a<3;a++,r++)
    if(!(r=strchr(r, ':')))
      return 0;  /* error in returned string */

  while(*r == ' ' || *r == '\t')
    r++;
  if(strlen(r) < 32)
    WPTR(k->addr, tprintf("%s@%s", r, k->addr));  /* Lookup successful! */

  return 0;
}
