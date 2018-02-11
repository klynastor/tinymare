// A fixed-up version of the bigram compression that first began in July 1995.
// It corrects some original mistakes, making the compression a little faster.
// Of course, we can't change anything now that the original compress.c has
// been in use for 6 years.
//
// If you have a new game in which you want to implement bigrams, then use
// this file instead of the one found in db/compress.c.
//
// This file dated June 24, 2001  <gandalf@winds.org>

/* db/compress.c */
/* Handles RAM compression routines for swift database loading and saving */

#include "externs.h"

/* Top 155 bigrams in the WindsMARE database as of 7/19/1995 */ 
const char tokens[155][2] = {
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
  
  "wi", "e.", "p ", "ds", "ss", "fo", "ic", "pa",
  "us", "if", "mi", "rn", "si", "ag", "ly", "ig",  /* codes 1-26 */
  "ns", "ir", "ho", "ld", "h.", "im", "da", "sh",
  "ie", "s." };

/* code 27 = ESC */
/* code 28 = Run-Length Encoding */
/* code 29 = TAB */
/* code 30 = Bell */
/* code 31 = Newline */

static char token_table[128][128];

void init_compress()
{
  int i;
  
  for(i=0;i<129;i++)
    token_table[(int)tokens[i][0]][(int)tokens[i][1]] = i+127;
  for(; i < 155;i++)
    token_table[(int)tokens[i][0]][(int)tokens[i][1]] = i-128;
}

char *compress(unsigned char *s)
{
  static char buf[8192];
  unsigned char token, last=0, *r=buf;
  int count=0;

  do {
    /* first try tokenizing the next two characters */
    if(*s > 31 && *s < 127 && (token=token_table[(int)s[0]][(int)s[1]]))
      s++;

    /* these characters are changed singularly */
    else if(*s == '\t')
      token=29;
    else if(*s == '\a')
      token=30;
    else if(*s == '\n')
      token=31;

    /* else the character is stored as-is */
    else
      token=*s;

    /* detect run-lengths */
    if(token == last && count < 255) {
      *r++=token;
      count++;
    } else if(count < 4) {
      *r++=last=token;
      count=1;
    } else {
      /* back up and add RLE code to string */
      r-=count;

      *r++=28;
      *r++=count;
      *r++=last;

      /* now add the next character found */
      *r++=last=token;
      count=1;
    }
  } while(*s++);

  return buf;
}

char *uncompress(unsigned char *s)
{
  static unsigned char buf[16384];
  unsigned char *r=buf;
  int rep=0;

  while(*s) {
    if(*s > 126) {
      *r++=tokens[*s - 127][0];
      *r++=tokens[*s - 127][1];
    } else if(*s < 27) {
      *r++=tokens[*s + 128][0];
      *r++=tokens[*s + 128][1];
    } else if((*s >= 32 && *s < 127) || *s == 27) {
      *r++=*s;
    } else if(*s != 28) {
      *r++=(*s == 29)?'\t':(*s == 30)?'\a':'\n';
    } else {
      /* Run-Length Encoding (Char 28) */
      if(r-buf > 7997 || !(rep=*++s))
        break;
      s++;
    }

    if(--rep < 0)
      s++;
  }
  
  *r='\0';
  if(r-buf > 8000)
    buf[8000]='\0';
  return buf;
}
