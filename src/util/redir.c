/* redir - redirects standard input/output through a socket */
/* Written by Byron Stanoszek, February 4, 1998 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char *argv[])
{
  struct sockaddr_in addr;
  struct hostent *hent;
  char buffer[PIPE_BUF];
  int sock, len;
  fd_set set;
  FILE *f=NULL;

  if(argc < 2) {
    fprintf(stderr, "Usage: %s host port [inputfile]\n", *argv);
    return 1;
  }
  if(argc > 3 && !(f=fopen(argv[3], "r")))
    perror(argv[3]);

  if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return 1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(argc<3?7348:atoi(argv[2]));
  if(!(hent=gethostbyname(argv[1])))
    addr.sin_addr.s_addr = inet_addr(argv[1]);
  else
    addr.sin_addr.s_addr = *(long *)hent->h_addr_list[0];

  if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("connect");
    return 1;
  }

  if(f) {
    while(fgets(buffer, PIPE_BUF, f))
      write(sock, buffer, strlen(buffer));
    fclose(f);
  }

  while(1) {
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    FD_SET(sock, &set);

    select(sock+1, &set, 0, 0, 0);
    if(FD_ISSET(STDIN_FILENO, &set)) {
      if(!(len=read(STDIN_FILENO, buffer, PIPE_BUF))) {
        close(sock);
        return 0;
      }
      write(sock, buffer, len);
    }
    if(FD_ISSET(sock, &set)) {
      if(!(len=read(sock, buffer, PIPE_BUF)))
        return 0;
      write(STDOUT_FILENO, buffer, len);
    }
  }

  return 0;
}
