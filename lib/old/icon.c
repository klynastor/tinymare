// Opens an icon file for reading, given a 32-bit CRC <id>. The first file
// in the icons/ directory whose CRC-32 checksum of the filename and path
// (minus the leading "icons/" string) matches <id> is opened and returned.
//
// If no icon is found, a -1 is returned indicating failure. No errno is set.
//
static int open_icon(unsigned int id)
{
  struct dirlist {
    struct dirlist *next;
    int depth;
    char name[0];
  } *dirlist=NULL, *next;

  DIR *d;
  struct dirent *ent;
  struct stat statbuf;
  char buf[2048];
  int depth, len;

  do {
    /* Pop next directory off of the stack */
    if(dirlist) {
      len=sprintf(buf, "icons/%s", dirlist->name);
      depth=dirlist->depth;
      next=dirlist->next;
      free(dirlist);
      dirlist=next;
    } else {
      strcpy(buf, "icons");
      len=5;
      depth=0;
    }

    /* Loop through files in directory */
    if(!(d=opendir(buf)))
      continue;
    while((ent=readdir(d))) {
      if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
        continue;

      sprintf(buf+len, "/%s", ent->d_name);
      if(!stat(buf, &statbuf)) {
        if(S_ISDIR(statbuf.st_mode) && depth < 5) {
          /* Add another directory name onto the stack */
          next=malloc(sizeof(struct dirlist)+strlen(buf+6)+1);
          next->next=dirlist;
          next->depth=depth+1;
          strcpy(next->name, buf+6);
          dirlist=next;
        } else if(S_ISREG(statbuf.st_mode) && compute_crc(buf+6) == id) {
          /* Clear remaining directories */
          while(dirlist) {
            next=dirlist->next;
            free(dirlist);
            dirlist=next;
          }
          closedir(d);
          return open(buf, O_RDONLY);
        }
      }
    }
    closedir(d);
  } while(dirlist);

  return -1;
}

// Sends an icon definition via in-stream Telnet Command codes to the remote
// terminal. The TinyMARE client must wait for this data to arrive before
// displaying any text associated with the icon on the screen.
//
// Icons that do not exist are sent back with modtime=0 and length=0.
//
void send_icon_definition(DESC *d, unsigned int mtime, unsigned int id)
{
  char buf[4096];
  int header[3]={0, htonl(id), 0};  /* Modification time, ID, Length */
  struct stat statbuf;
  int fd, len, i=0;

  /* Open icon file given the CRC-32 ID */
  if((fd=open_icon(id)) != -1) {
    fstat(fd, &statbuf);
    if(statbuf.st_size >= 1 && statbuf.st_size <= 16777216) {
      header[0]=htonl((int)statbuf.st_mtime);
      header[2]=htonl((int)statbuf.st_size);
    }
  }

  /* Send icon header */
  queue_output(d, 2, "\377\372\061", 3);  /* IAC SB 49 */
  output_binary(d, (char *)&header, sizeof(header));

  /* Send icon data */
  if(fd != -1) {
    if(header[2])
      while((len=read(fd, buf, 4096)) > 0)
        output_binary(d, buf, len);
    close(fd);
  }

  /* Close Telnet command */
  queue_output(d, 2, "\377\360", 2);  /* IAC SE */
}

// Sends an icon definition in a base-64 encoded stream to the remote terminal.
// TinyMARE keeps a cache of which icons have been sent to each descriptor and
// only sends icon data that either is not already in the list or if the file
// timestamp changes.
//
// Icon definitions are sent via output queue #2 so that it is received by the
// client before any text using the icon is displayed.
//
static void send_icon_definition(DESC *d, char *file, unsigned int mtime,
                                 unsigned int id, int size)
{
  static const char base64[64]=
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./";

  unsigned char output[8192], buf[512], *s;
  int cols=(((d->cols < 60 || d->cols > 512)?80:d->cols)-1)/4*3;
  int fd, len, i=0, j;

  /* Check to see if player already has the icon with the right modtime */
  for(i=0;i < d->icon_size-1;i+=2)
    if(d->iconlist[i] == id) {
      if(d->iconlist[i+1] == mtime)
        return;
      break;
    }

  /* Open icon file given the CRC-32 ID */
  if((fd=open(file, O_RDONLY)) == -1)
    return;

  /* Send icon header */
  sprintf(buf, "\e[I%08x%08x%08x\n", mtime, id, size);
  queue_output(d, 2, buf, 28);

  /* Send icon data in base64 */
  s=output;
  while((len=read(fd, buf, cols)) > 0) {
    for(j=0;j < len;j+=3) {
      *s++=base64[buf[j] >> 2];
      *s++=base64[((buf[j] & 3) << 4)|(buf[j+1] >> 4)];
      *s++=base64[((buf[j+1] & 15) << 2)|(buf[j+2] >> 6)];
      *s++=base64[buf[j+2] & 63];
    }
    *s++='\n';
    if(s-output > 7680) {
      queue_output(d, 2, output, s-output);
      s=output;
    }
  }
  if(s-output)
    queue_output(d, 2, output, s-output);

  close(fd);

  /* Add index to list of defined icons for that session */
  if(i >= d->icon_size) {
    if(!(d->icon_size & 31))
      d->iconlist=realloc(d->iconlist, (d->icon_size+32)*sizeof(int));
    d->icon_size+=2;
    d->iconlist[i]=id;
  }
  d->iconlist[i+1]=mtime;  /* Update mtime for icon */
}

// Sends an icon definition in a base-128 encoded stream to the remote client.
// TinyMARE keeps a cache of which icons have been sent to each descriptor and
// only sends icon data that either is not already in the list or if the file
// timestamp changes.
//
// Icon definitions are sent via output queue #2 so that it is received by the
// client before any text using the icon is displayed.
//
static void send_icon_definition(DESC *d, char *file, unsigned int mtime,
                                 unsigned int id, int size)
{
  unsigned char output[8192], buf[512], *s, c;
  int fd, len, i=0, j, shift;

  /* Check to see if player already has the icon with the right modtime */
  for(i=0;i < d->icon_size-1;i+=2)
    if(d->iconlist[i] == id) {
      if(d->iconlist[i+1] == mtime)
        return;
      break;
    }

  /* Open icon file */
  if((fd=open(file, O_RDONLY)) == -1)
    return;

  /* Send ANSI header */
  sprintf(buf, "\e]I%08x%08x%08x", mtime, id, size);
  queue_output(d, 2, buf, 27);

  /* Format icon data in base-128 */
  s=output;
  while((len=read(fd, buf, 7161)) > 0) {
    for(shift=0,j=0;j < len;) {
      if(!shift)
        c=buf[j] >> 1;
      else {
        c=((buf[j] << (7-shift)) & 0x7f) | (buf[j+1] >> shift);
        j++;
      }
      *s++=c+(c < 64?48:112);
      if(++shift == 7)
        shift=0;
    }
    queue_output(d, 2, output, s-output);
    s=output;
  }
  queue_output(d, 2, "\n", 1);

  close(fd);

  /* Add index to list of defined icons for that session */
  if(i >= d->icon_size) {
    if(!(d->icon_size & 31))
      d->iconlist=realloc(d->iconlist, (d->icon_size+32)*sizeof(int));
    d->icon_size+=2;
    d->iconlist[i]=id;
  }
  d->iconlist[i+1]=mtime;  /* Update modtime for icon */
}
