/* prog/hash.c */
/* dynamic hash table management */

#include "externs.h"

#define HASH_SIZE 239	// Size of table array. Relatively prime.


// Attempt to return a unique number between 0 and HASH_SIZE-1 for each primary
// key (char *) field in the structure. Entries are case insensitive.
//
static int hash_name(char *name)
{
  unsigned short op=0;
  int j, shift=0;
  char *s;
  
  for(s=name;*s;s++) {
    j = tolower(*s);
    op ^= j<<(shift);
    op ^= j>>(16-shift);
    shift = (shift+11)%16;
  }

  return op % HASH_SIZE;
}

// Initializes a static hash table using a pointer to a structure array.
// The table cannot be modified once created.
//
// The first element in each array index must contain a distinct (char *)
// string by which to initialize the primary key of the hash table.
//
// entry=Pointer to first index in structure array.
// total=Total size of all the elements in the array.
// size=Size of 1 array item in struct.
// 
hash *make_hash_table(void *entry, int total, int size)
{
  hash *op;
  int i, sizes[HASH_SIZE]={0};
  char *ptr;
  
  /* Allocate memory for the entire hash table and all expected bucket sizes */
  op=malloc((HASH_SIZE*2+(total/size))*sizeof(hash *));

  /* Go through the array and count the # elements that fall in each bucket */
  for(ptr=entry;ptr-(char *)entry < total;ptr+=size)
    sizes[hash_name(*(char **)ptr)]++;

  /* Determine locations of all hash buckets in giant allocated array */
  op[0].ptr=op+HASH_SIZE;
  op[0].ptr[sizes[0]].ptr=NULL;
  for(i=1;i < HASH_SIZE;i++) {
    op[i].ptr=op[i-1].ptr+sizes[i-1]+1;
    op[i].ptr[sizes[i]].ptr=NULL;
  }

  /* Go through the array again, this time setting the index pointers */
  for(ptr=entry;ptr-(char *)entry < total;ptr+=size) {
    i=hash_name(*(char **)ptr);
    op[i].ptr[--sizes[i]].ptr=(hash *)ptr;
  }

  return op;
}

// Look up an entry in the hash table by name. Return a pointer to the index
// of the array.
//
void *lookup_hash(hash *tab, char *name)
{
  int i=hash_name(name), j;

  for(j=0;tab[i].ptr[j].ptr;j++)
    if(!strcasecmp((char *)tab[i].ptr[j].ptr->ptr, name))
      return tab[i].ptr[j].ptr;
  return NULL;
}


// Create an empty hash table.
//
hash *create_hash()
{
  hash *op;
  int i;

  op=malloc(HASH_SIZE * sizeof(hash *));

  for(i=0;i<HASH_SIZE;i++) {
    op[i].ptr=malloc(sizeof(hash *));
    op[i].ptr[0].ptr=NULL;
  }

  return op;
}

// Add a pointer to structure to an existing hash table.
// Structure must not be part of an array.
//
void add_hash(hash *tab, void *entry)
{
  hash *ptr=entry;
  int j, i=hash_name((char *)ptr->ptr);

  /* count size of hash pointer */
  for(j=0;tab[i].ptr[j].ptr;j++);

  tab[i].ptr=realloc(tab[i].ptr, (j+2) * sizeof(hash *));
  tab[i].ptr[j].ptr=entry;
  tab[i].ptr[j+1].ptr=NULL;
}

// Delete an entry from a hash table, by name.
//
void del_hash(hash *tab, char *name)
{
  int j, loc=-1, i=hash_name(name);

  for(j=0;tab[i].ptr[j].ptr;j++)
    if(!strcasecmp((char *)tab[i].ptr[j].ptr->ptr, name))
      loc=j;
  if(loc == -1)
    return;

  /* free contents of structure (must not be part of an array) */
  free(tab[i].ptr[loc].ptr);

  if(j > 1)
    tab[i].ptr[loc].ptr=tab[i].ptr[j-1].ptr;
  tab[i].ptr[j-1].ptr=NULL;
  tab[i].ptr=realloc(tab[i].ptr, j * sizeof(hash *));
}
