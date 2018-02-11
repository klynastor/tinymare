/* tests the integrity of compressed text & displays hex output */
void do_compress(player, arg1, arg2, argv, str)
dbref player;
char *arg1, *arg2, **argv, *str;
{
  char *comp, *decomp;
  char buf[16384], *p;
  struct timeval start, stop, tcomp, tdecomp;

  gettimeofday(&start, NULL);
  comp = compress(str);
  gettimeofday(&stop, NULL);
  tcomp.tv_sec = stop.tv_sec-start.tv_sec;
  tcomp.tv_usec = stop.tv_usec-start.tv_usec;
  for(*buf='\0',p=comp;*p;p++) {
    if(*p > 0x1F && *p < 0x7F)
      sprintf(buf+strlen(buf), "%c", *p);
    else
      sprintf(buf+strlen(buf), "<%02X>", *p);
  }
  notify(player,
    tprintf("Compressed  : %s (%d bytes)", buf, strlen(comp)));

  gettimeofday(&start, NULL);
  decomp = uncompress(comp);
  gettimeofday(&stop, NULL);
  tdecomp.tv_sec = stop.tv_sec-start.tv_sec;
  tdecomp.tv_usec = stop.tv_usec-start.tv_usec;
  notify(player,
    tprintf("Decompressed: %s (%d bytes)", decomp, strlen(decomp)));
  notify(player, tprintf("Ratio: %.2f%%",
    100-(((double)strlen(comp)/(double)strlen(decomp))*100)));
  notify(player, tprintf("Compression time  : %f seconds",
    (double)tcomp.tv_sec+((double)tcomp.tv_usec/1000000)));
  notify(player, tprintf("Decompression time: %f seconds",
    (double)tdecomp.tv_sec+((double)tdecomp.tv_usec/1000000)));
}
