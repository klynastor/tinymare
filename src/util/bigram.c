/* util/bigram.c */
/* Recovers compressed text in corrupted Tinymare data files */

#include <stdlib.h>
#include <stdio.h>

/* Top 154 bigrams in the WindsMARE database as of 7/19/1995 */ 
char *tokens[154] = {
  "qu",
  "e ", "th", " t", "he", "s ", "ou", "in", " a",
  "st", " s", "t ", "er", "es", "n ", "al", "d ",
  "to", "an", "ar", "ea", "ro", "o ", "re", "or",
  "nd", " o", " w", " f", "on", " i", " b", "wa",
  "r ", "ll", "as", "y ", " c", "at", "u ", "om",
  "ng", "l ", "le", " d", "ut", "ow", ". ", "k ",
  "h ", "nt", "f ", "en", "se", "me", "ne", "g ",  /* codes 127-255 */
  ", ", "rt", "it", "of", "no", "hi", "is", "ve",
  "il", "te", "ra", " h", "lo", " m", " l", "li",
  "ge", " n", " r", "ac", "ee", "ai", "ck", "ma",
  "el", "we", "co", "a ", " y", "so", "de", "m ",
  "yo", "t.", "oo", "lk", "Yo", " p", "do", "un",
  "  ", "la", "et", "ha", "ti", " e", "ri", "ta",
  "ad", "Th", "tr", "gh", "ed", "ur", "ca", "fr",
  "wn", "be", " T", "pe", "ch", "ks", "id", "ba",
  "ce", "rs", " g", "ay", "up", " u", "em", "ot",
  
  "wi", "e.", "p ", "ds", "ss", "fo", "ic", "pa", "us",  /* codes 1-9 */
 
  "mi", "rn", "si", "ag", "ly", "ig", "ns", "ir",  /* codes 11-26 */
  "ho", "ld", "h.", "im", "da", "sh", "ie", "s." };

/* code 10 = DB Newline */
/* code 27 = ESC */
/* code 28 = Universal Argument */
/* code 29 = TAB */
/* code 30 = Bell */
/* code 31 = Linefeed */

int main(int argc, char *argv[])
{
  FILE *f;
  char *cmd=*argv;
  int a, c;

  if(argc < 2) {
    fprintf(stderr, "Usage: %s file ...\n", cmd);
    exit(1);
  }

  while(--argc) {
    argv++;
    if(!(f=fopen(*argv, "r"))) {
      perror(*argv);
      continue;
    }

    while((c=fgetc(f)) != EOF) {
      switch(c) {
      case 0: case 10: case 31:
        fputc('\n', stdout);
	break;
      case 1 ... 9:
        fputc(tokens[c+128][0], stdout);
        fputc(tokens[c+128][1], stdout);
        break;
      case 11 ... 26:
        fputc(tokens[c+127][0], stdout);
        fputc(tokens[c+127][1], stdout);
        break;
      case 27:
        fputc('\e', stdout);
        break;
      case 28:
        a=fgetc(f);
        if(a > 0 && a < 10)
          a+=3;
        else
          a+=2;
        c=fgetc(f);
        for(;a>0;a--)
          fputc(c, stdout);
        break;
      case 29:
        fputc('\t', stdout);
        break;
      case 30:
        fputc('\a', stdout);
        break;
      case 32 ... 126:
        fputc(c, stdout);
        break;
      case 127 ... 255:
        fputc(tokens[c-127][0], stdout);
        fputc(tokens[c-127][1], stdout);
      }
    }

    fputc('\n', stdout);
    fclose(f);
  }
  return 0;
}
