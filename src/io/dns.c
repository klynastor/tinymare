/* io/dns.c */
/* Non-blocking nameserver interface routines */

#include "externs.h"
#include <sys/socket.h>
#include <errno.h>

#ifndef __CYGWIN__  /* Cygwin-32 does not support lowlevel DNS queries */

/* Remove conflicts with some distributions' .h files */
#undef putshort
#undef putlong
#undef C_ANY

#include <arpa/nameser.h>
#include <resolv.h>

static struct resolve {
  struct resolve *next;
  struct resolve *nextquery;
  char *hostname;
  long expire;
  unsigned int ip;
  unsigned int id:16;
  unsigned int state:8;
} *resolves, *queries;

int dns_query;	/* Size of nameserver lookup queue */

#define STATE_FINISHED	0
#define STATE_FAILED	1

#define NUM_TRIES	3


/* open the nameserver for querying */
int dns_open()
{
  int s, on=1;

  res_init();
  if(!_res.nscount) {
    log_resolv("Startup: No nameservers defined.");
    return -1;
  }

  /* open socket for nameserver communication */
  if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("dns_open");
    return -1;
  } if(setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on)) < 0) {
    perror("dns_open: setsockopt");
    close(s);
    return -1;
  }

  return s;
}

/* send hostname lookup request to nameservers */
static void send_dns_request(struct resolve *rp)
{
  HEADER *hp;
  unsigned char *s;
  char buf[32], packet[PACKETSZ];
  int r, i;

  if(resolv == -1)  /* no nameserver defined */
    return;

  /* make query packet for nameserver */
  s=(char *)&rp->ip;
  sprintf(buf, "%d.%d.%d.%d.in-addr.arpa", s[3], s[2], s[1], s[0]);
  if((r=res_mkquery(QUERY,buf,C_IN,T_PTR,NULL,0,NULL,packet,PACKETSZ)) <= 0) {
    perror("res_mkquery");
    return;
  }

  /* set query id */
  hp=(HEADER *)packet;
  hp->id=rp->id;

  /* send query to all nameservers */
  for(i=0;i<_res.nscount;i++)
    sendto(resolv, packet, r, 0, (struct sockaddr *)&_res.nsaddr_list[i],
           sizeof(struct sockaddr));

  nstat[NS_QUERY]++;
  log_resolv("Sent domain lookup request for \"%s\".", ipaddr(rp->ip));
}

void dns_receive()
{
  HEADER *hp;
  struct sockaddr_in from;
  struct resolve *rp;
  int len, r, datalen, fromlen=sizeof(from);
  short rr, datatype, class, qdatatype, qclass;
  char querystring[32], recvbuf[PACKETSZ], namestring[MAXDNAME];
  unsigned char *s, *end;
  long ttl;

  /* receive message on socket */
  if((len=recvfrom(resolv, recvbuf, PACKETSZ, 0, (struct sockaddr *)&from,
      &fromlen)) < 0) {
    perror("DNS Lookup");
    return;
  }

  /* Check if packet is smaller than the 12-byte header, or if it's too big
     that it was truncated to fit PACKETSZ */
  if(len <= 12 || len >= PACKETSZ)
    return;

  // tc=1: Nameserver packet truncated.
  // qr=0: Query packet received on nameserver communication socket.
  // opcode != 0: Not a standard query. (1 = inverse request, 2 = server status)

  hp=(HEADER *)recvbuf;
  if(hp->tc || !hp->qr || hp->opcode)
    return;

  /* search for id in cache (id is echoed by nameserver) */
  for(rp=queries;rp;rp=rp->next)
    if(rp->id == hp->id)
      break;

  /* check if address has already been resolved */
  if(!rp || rp->state < 2)
    return;

  /* all values returned are in network byte order, so we must convert */
  hp->qdcount=ntohs(hp->qdcount);
  hp->ancount=ntohs(hp->ancount);
  hp->nscount=ntohs(hp->nscount);
  hp->arcount=ntohs(hp->arcount);

  log_resolv("Received nameserver %s reply. (qd:%u an:%u ns:%u ar:%u)",
             inet_ntoa(from.sin_addr), hp->qdcount, hp->ancount, hp->nscount,
             hp->arcount);

  /* Process response */

  /* no domain name record available */
  if(hp->rcode == NXDOMAIN) {
    rp->state=STATE_FAILED;
    rp->expire=now+7200;
    log_resolv("Host %s not found.", ipaddr(rp->ip));
    return;
  }

  /* determine cause of server error */
  if(hp->rcode != NOERROR) {
    if(hp->rcode == REFUSED)
      log_resolv("Query refused on nameserver %s.", inet_ntoa(from.sin_addr));
    else if(hp->rcode == SERVFAIL) {
      log_resolv("Server failure on %s.", inet_ntoa(from.sin_addr));
      rp->state=STATE_FAILED;
      rp->expire=now+86400;
    }
    return;
  }

  /* skip # bytes in header */
  end=recvbuf+len;

  if(!hp->ancount)	/* No answers returned */
    return;
  if(hp->qdcount != 1)	/* Reply does not contain one query */
    return;

  /* construct expected query reply */
  s=(char *)&rp->ip;
  sprintf(querystring, "%d.%d.%d.%d.in-addr.arpa", s[3], s[2], s[1], s[0]);

  /* expand compressed domain name (after header) and compare with query */
  s=recvbuf+12;
  if((r=dn_expand(recvbuf, recvbuf+len, s, namestring, MAXDNAME)) < 0)
    return;
  if(strcasecmp(querystring, namestring))  /* names do not match */
    return;

  s+=r;
  if(s+4 > end)  /* Query resource record truncated */
    return;

  qdatatype=get16(s);	// Data type = Domain name pointer
  qclass=get16(s);	// Class = Arpa Internet

  if(qclass != C_IN || qdatatype != T_PTR)  /* server response does not match */
    return;

  /* extract all resource records */
  for(rr=hp->ancount+hp->nscount+hp->arcount; rr; rr--) {
    if((r=dn_expand(recvbuf, recvbuf+len, s, namestring, MAXDNAME)) < 0)
      return;

    /* get record data */
    s+=r;
    if(s+10 > end)  /* Resource record truncated */
      return;
    datatype=get16(s);
    class=get16(s);
    ttl=get32(s);
    datalen=get16(s);

    /* check that the query comes from the correct resource */
    if(strcasecmp(querystring, namestring) || datatype != qdatatype)
      continue;

    if(class != qclass)  /* Answered class doesn't match queried class */
      return;
    if(!datalen || s+datalen > end)  /* No data or length exceed packet size */
      return;

    /* Nameserver query succeeded */
    if((dn_expand(recvbuf, recvbuf+len, s, namestring, MAXDNAME)) > 0) {
      SPTR(rp->hostname, namestring);
      if(ttl <= 0)
        ttl=86400;
      rp->expire=now+ttl;

      /* Check for a unicode string */
      for(s=rp->hostname;*s >= ' ' && *s < 127;s++);
      if(*s) {
        /* Contains unprintable characters */
        log_resolv("Unicode hostname found for %s", ipaddr(rp->ip));
        rp->state=STATE_FAILED;
      } else {
        /* Normal hostname */
        nstat[NS_RESOLV]++;
        log_resolv("Lookup successful: %s (TTL=%ld)", rp->hostname, ttl);
        rp->state=STATE_FINISHED;
      }
      return;
    }

    if((s+=datalen) > end)
      return;
  }
}

/* called once per second */
void dns_events()
{
  struct resolve *rp, *next, *last=NULL;

  for(rp=queries;rp;rp=next) {
    next=rp->nextquery;

    /* remove any finished or failed queries from list */
    if(rp->state < 2) {
      dns_query--;
      if(last)
        last->next=next;
      else
        queries=next;
      continue;
    }

    /* query has not expired yet */
    if(now < rp->expire) {
      last=rp;
      continue;
    }

    if(rp->state < NUM_TRIES+1) {
      rp->state++;
      rp->expire=now+4;
      send_dns_request(rp);
      last=rp;
      continue;
    }

    /* exceeded maximum number of retries */

    log_resolv("Query for %s timed out.", ipaddr(rp->ip));
    rp->state=STATE_FAILED;
    rp->expire=now+7200;

    /* remove event from query list */
    dns_query--;
    if(last)
      last->next=next;
    else
      queries=next;
  }
}

/* Non-blocking gethostbyaddr(addr). Returns 0 if lookup is incomplete. */
char *dns_lookup(int ip)
{
  FILE *f;
  struct resolve *rp;
  char *r, *s, *addr=ipaddr(ip);
  static unsigned short lookup_id=0;
  static char buf[256];

  /* scan /etc/hosts for matching entry */
  if((f=fopen("/etc/hosts", "r"))) {
    while(ftext(buf, 256, f)) {
      if((s=strchr(buf, '#')))
        *s='\0';
      if(!isdigit(*buf))
        continue;

      /* match ip address */
      s=buf;
      r=strspc(&s);
      if(strcmp(r, addr))
        continue;
      if(!(r=strspc(&s)))
        continue;
      fclose(f);
      return r;
    }
    fclose(f);
  }

  /* if no nameserver present, just return address in dot notation */
  if(resolv == -1)
    return addr;

  /* search for a previously cached entry */
  for(rp=resolves;rp;rp=rp->next)
    if(rp->ip == ip)
      break;

  /* add a new record if not found */
  if(!rp) {
    rp=malloc(sizeof(struct resolve));
    rp->next=resolves;
    rp->state=0;
    rp->expire=0;
    rp->ip=ip;
    resolves=rp;
  }

  /* if the entry is new or has expired, send dns request */
  if(rp->state < 2 && rp->expire <= now) {
    rp->state=2;
    rp->expire=now+4;
    rp->id=++lookup_id;

    /* add entry to the list of queries */
    rp->nextquery=queries;
    queries=rp;
    dns_query++;

    send_dns_request(rp);
    return NULL;
  }

  if(rp->state == STATE_FINISHED)
    return rp->hostname;
  if(rp->state == STATE_FAILED)
    return addr;
  return NULL;
}

int dns_cache_size()
{
  struct resolve *rp;
  int num=0;
  
  for(rp=resolves;rp;rp=rp->next,num++);
  return num;
}

#else  /* __CYGWIN__ */

#include <netdb.h>

/* Under Cygwin-32, dns_lookup is mainly an alias for gethostbyaddr(). */
char *dns_lookup(int ip)
{
  struct hostent *hent;
  struct sockaddr_in addr;
  unsigned char *s;

  addr.sin_addr.s_addr=ip;
  hent = gethostbyaddr((char *)&addr.sin_addr.s_addr, sizeof(int), AF_INET);

  /* Check for unicode characters */
  if(hent)
    for(s=(char *)hent->h_name;*s >= ' ' && *s < 127;s++);

  return (hent && !*s)?(char *)hent->h_name:inet_ntoa(addr.sin_addr);
}

#endif
