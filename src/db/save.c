/* db/save.c */

#include "externs.h"

#define DB_VERSION 2	// TinyMare Compressed Ram v2

extern int db_flags;
static void db_write_object(FILE *f, dbref i);


// Writes a compressed DBREF# to the file. Number of bytes written is 1 if
// (num < 0), or dbref_len for all other valid numbers.
//
void putref(FILE *f, dbref num)
{
  if(num < 0) {
    putchr(f, 0xFF);
    return;
  }

  if(dbref_len >= 2) {
    if(dbref_len >= 3) {
      if(dbref_len == 4)
        putchr(f, num >> 24);
      putchr(f, num >> 16);
    }
    putchr(f, num >> 8);
  }
  putchr(f, num);
}

// Writes a string to the file with terminating null byte.
//
void putstring(FILE *f, char *s)
{
  if(s)
    fputs(s, f);
  putchr(f, 0);
}

// Writes a list of dbref#s to the file, starting with the count.
//
void putlist(FILE *f, dbref *list)
{
  int a=0, *ptr;

  /* Store number of items in list */
  for(ptr=list;ptr && *ptr != NOTHING;ptr++,a++);
  putref(f, a-1);

  for(ptr=list;ptr && *ptr != NOTHING;ptr++)
    putref(f, *ptr);
}


// Procedure to save the entire database to <filename>.
//
dbref db_write(FILE *f, char *filename)
{
  long pos, pos2;
  int i;
  
  /* Initial database modifier flags */
  db_flags=0;

  if((unsigned long)time(NULL) > 0xFFFFFFFF)
    db_flags |= DB_LONGINT;  /* Time must be stored using 64-bits */

  /* Write database header */
  fprintf(f, "MARE");
  putchr(f, DB_VERSION);
  putnum(f, db_top);
  putnum(f, db_flags);
  putchr(f, NUM_POWS);
  /* Select database reference number byte-length */
  dbref_len=(db_top+1 < 0xFF)?1:(db_top+1 < 0xFF00)?2:
             (db_top+1 < 0xFF0000)?3:4;

  /* Save configuration */
  for(i=0;db_rule[i].type;i++) {
    /* Config header */
    putchr(f, db_rule[i].type);

    /* 4-byte length of configuration data will get filled in */
    pos=ftell(f);
    putnum(f, 0);
    db_rule[i].save(f, db_rule[i].type);

    /* Check if any data has been written */
    pos2=ftell(f);
    if(pos2-pos < 5)
      fseek(f, -5, SEEK_CUR);  /* No configuration data, back up 5 bytes */
    else {
      /* Fill in length */
      fseek(f, pos, SEEK_SET);
      putnum(f, pos2-pos-4);
      fseek(f, pos2, SEEK_SET);
    }
  }
  putchr(f, 0);

  /* Write database */
  for(i=0;i<db_top;i++) {
    if(Typeof(i) == TYPE_GARBAGE)
      continue;
    db_write_object(f, i);
  }

  /* Write closing -1 as end of list marker. Check for out-of-disk errors. */
  if(putchr(f, 0xFF) == EOF) {
    perror(filename);
    return 0;
  }

  return db_top;
}

static void db_write_object(FILE *f, dbref i)
{
  ALIST *attr;
  ATRDEF *k;
  
  /* Save object header */
  putref(f, i);
  putstring(f, db[i].name);
  putref(f, db[i].location);
  putref(f, db[i].contents);
  putref(f, db[i].exits);
  putref(f, (db[i].link == HOME)?db_top:
            (db[i].link == AMBIGUOUS)?(db_top+1):db[i].link);
  putref(f, db[i].next);
  putref(f, db[i].owner);
  putnum(f, db[i].flags);
  putnum(f, db[i].plane);
  putnum(f, db[i].pennies);

  if(db_flags & DB_LONGINT) {
    putlong(f, db[i].create_time);
    putlong(f, db[i].mod_time);
  } else {
    putnum(f, db[i].create_time);
    putnum(f, db[i].mod_time);
  }

  /* Save object lists */
  putlist(f, db[i].parents);
  putlist(f, db[i].children);
  if(Typeof(i) == TYPE_ROOM || Typeof(i) == TYPE_ZONE)
    putlist(f, db[i].zone);

  /* Write out special player information */
  if(Typeof(i) == TYPE_PLAYER) {
    putchr(f, db[i].data->class-1);
    putchr(f, db[i].data->rows);
    putchr(f, db[i].data->cols);
    putchr(f, (db[i].data->tzdst << 6) | ((db[i].data->tz & 0x1F) << 1) |
              (db[i].data->passtype << 7) | db[i].data->gender);
    putshort(f, db[i].data->term);
    putnum(f, db[i].data->steps);
    putnum(f, db[i].data->sessions);
    putnum(f, db[i].data->age);

    if(db_flags & DB_LONGINT) {
      putlong(f, db[i].data->last);
      putlong(f, db[i].data->lastoff);
    } else {
      putnum(f, db[i].data->last);
      putnum(f, db[i].data->lastoff);
    }

    fwrite(db[i].data->pass, db[i].data->passtype?20:10, 1, f);
    fwrite(db[i].data->powers, (NUM_POWS+3)/4, 1, f);
  }

  /* Write the attribute list */
  for(attr=db[i].attrs;attr;attr=attr->next) {
    putchr(f, attr->num);
    putref(f, attr->obj);
    putstring(f, attr->text);
  }
  putchr(f, 0);

  /* Save attribute definitions */
  for(k=db[i].atrdefs;k;k=k->next) {
    putchr(f, k->atr.num);
    putshort(f, k->atr.flags);
    putstring(f, k->atr.name);
  }
  putchr(f, 0);
}
