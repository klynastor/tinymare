/* Open a connection to another port from within mare */
DESC *open_outbound(char *host, int port)
{
  DESC *d;
  struct sockaddr_in addr;
  struct hostent *hent;
  int sock, ip;
  char *s;

  if(port < 1 || port > 65535)
    return 0;
  if((unsigned int)open_files < 0x7fffffff &&
     nstat[NS_FILES] >= (int)(open_files-6))
    return 0;

  if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("open_outbound");
    return 0;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  for(s=host;isdigit(*s) || *s == '.';s++);
  if(!*s || !(hent=gethostbyname(host))) {  /* Blocks if DNS can't resolve */
    if((ip=inet_addr(host)) == INADDR_NONE) {
      log_error("Open Outbound: Host '%s' not found.", host);
      close(sock);
      return 0;
    }
    addr.sin_addr.s_addr=ip;
  } else
    addr.sin_addr.s_addr=*(int *)hent->h_addr_list[0];

  if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return 0;
  }

  nstat[NS_FILES]++;
  d=initializesock(sock);
  d->concid=++nstat[NS_LOGINS];
  d->flags|=C_OUTBOUND;
  d->state=-1;

  /* Save internet address */
  d->port=port;
  SPTR(d->addr, host);

  log_io("Outbound %d: %s port %d. desc->%d", d->concid, host, port, d->fd);
  return d;
}
