/* io/ident.c */
/* identify a user */

#include "externs.h"
#include <sys/socket.h>

// Query the remote host's identity.
// Returns result in 'rslt' with length 32.

void get_ident(char *rslt, int sock)
{
  struct sockaddr_in localaddr, remoteaddr, sin_addr;
  struct timeval timeout;
  fd_set *fileset;
  char buf[128], *r, *s;
  int err, fd, len;

  *rslt='\0';	/* start off with an empty result buffer */

  /* set up local/remote addresses */
  len = sizeof(remoteaddr);
  if(getpeername(sock, (struct sockaddr *)&remoteaddr, &len))
    return;
  len = sizeof(localaddr);
  if(getsockname(sock, (struct sockaddr *)&localaddr, &len))
    return;

  /* open socket connection to remote address */
  if((fd=socket(AF_INET, SOCK_STREAM, 0)) == -1)
    return;
  fcntl(fd, F_SETFL, O_NONBLOCK);
  sin_addr.sin_family=AF_INET;
  sin_addr.sin_addr=localaddr.sin_addr;
  sin_addr.sin_port=0;
  if(bind(fd, (struct sockaddr *)&sin_addr, sizeof(sin_addr))) {
    close(fd);
    return;
  }

  /* attempt to do socket connection in progress, timeout 2 seconds */
  sin_addr.sin_family = AF_INET;
  sin_addr.sin_addr = remoteaddr.sin_addr;
  sin_addr.sin_port = htons(113); /* identd port */
  connect(fd, (struct sockaddr *)&sin_addr, sizeof(sin_addr));

  /* Dynamically allocate fd_set with enough padding for 64-bit longs */
  fileset=alloca((fd/64+1)*8);
  memset(fileset, 0, (fd/64+1)*8);
  FD_SET(fd, fileset);

  timeout.tv_sec=2;
  timeout.tv_usec=0;
  if(select(fd+1, 0, fileset, 0, &timeout) < 1) {
    close(fd);
    return;
  }
  len=sizeof(err);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
  log_resolv(tprintf("Ident timer: %.2fs%s", 2.0-((double)timeout.tv_sec +
             (double)timeout.tv_usec/1000000), err?" (Refused)":""));
  if(err) {
    close(fd);
    return;
  }

  /* send identification request */
  sprintf(buf, "%d,%d\r\n", ntohs(remoteaddr.sin_port),
          ntohs(localaddr.sin_port));
  write(fd, buf, strlen(buf));

  /* wait for return input */
  timeout.tv_sec=2;
  timeout.tv_usec=0;
  if(select(fd+1, fileset, 0, 0, &timeout) < 1) {
    close(fd);
    return;
  }
  log_resolv(tprintf("Response timer: %.2fs", 2.0-((double)timeout.tv_sec +
             (double)timeout.tv_usec/1000000)));

  /* read input */
  len=read(fd, buf, 127);
  close(fd);
  if(len <= 2)  /* read error */
    return;
  buf[len]='\0';

  /* parse input string and search for userid */
  for(r=buf,len=0;len<3;len++,r++)
    if(!(r=strchr(r, ':')))
      return;

  if((s=strchr(r, '\n')))
    *s='\0';
  if((s=strchr(r, '\r')))
    *s='\0';

  while(isspace(*r))
    r++;
  if((len=strlen(r)) < 32)
    memcpy(rslt, r, len+1);
}
