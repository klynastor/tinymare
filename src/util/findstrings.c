/* findstrings.c */
/* Finds all the strings used in C programs.
 * Usage: findstrings [-l minlength] [files...]
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int length=3;

void findfiles(FILE *f, char *filename);

int main(int argc, char *argv[])
{
  FILE *f;
  int c, i, count=0;

  while((c = getopt(argc, argv, "l:")) != -1) {
    switch(c) {
    case 'l':  /* Be verbose; show version information */
      length=atoi(optarg);
      break;
    case ':':
      fprintf(stderr, "%s: option requires an argument -- %c\n", *argv,optopt);
      exit(1);
    case '?':
      fprintf(stderr, "%s: illegal option -- %c\n", *argv, optopt);
      fprintf(stderr, "Usage: %s [-l minlength] [files...]\n", *argv);
      exit(1);
    }
  }
  argv+=optind-1;
  argc-=optind-1;

  if(argc < 2) {
    findfiles(stdin, NULL);
    return 0;
  }

  for(i=1;i < argc;i++) {
    if(!(f=fopen(argv[i], "r"))) {
      perror(argv[i]);
      continue;
    }
    findfiles(f, argv[i]);
    fclose(f);
    count++;
  }

  return count;
}

void findfiles(FILE *f, char *filename)
{
  char buf[4096];
  int i, ch;

  /* Two rules: Search for quotation marks, skip over comments */

  while((ch=fgetc(f)) != EOF) {
    /* Skip #include */
    if(ch == '#') {
      if((ch=fgetc(f)) != 'i') {
        ungetc(ch, f);
        continue;
      }
      while((ch=fgetc(f)) != EOF)
        if(ch == '\r' || ch == '\n')
          break;
      continue;
    }
    /* Skip comments */
    if(ch == '/') {
      ch=fgetc(f);
      if(ch == '*') {
        while((ch=fgetc(f)) != EOF)
          if(ch == '*' && fgetc(f) == '/')
            break;
      } else if(ch == '/') {
        while((ch=fgetc(f)) != EOF)
          if(ch == '\r' || ch == '\n')
            break;
      }
      continue;
    }
    if(ch == '"') {
      i=0;
      while((ch=fgetc(f)) != EOF && i < 4095) {
        if(ch == '\r' || ch == '\n')
          break;
        if(ch == '"') {
          if(i >= length) {
            int count=0, len=i;

            buf[i]='\0';
            for(i=0;i < len;i++) {
              if(buf[i] == '%' || buf[i] == '\\') {
                i++;
                while((buf[i] >= '0' && buf[i] <= '9') ||
                      buf[i] == 'e' || buf[i] == '[' || buf[i] == ';')
                  i++;
                continue;
              }
              if((buf[i]|32) >= 'a' && (buf[i]|32) <= 'z')
                count++;
            }
            if(count > 1)
              printf("\"%s\"\n", buf);
          }
          break;
        }
        buf[i++]=ch;
      }
    }
  }
}
