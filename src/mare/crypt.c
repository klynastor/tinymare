/*
 * UFC-crypt: ultra fast crypt(3) implementation
 *
 * Copyright (C) 1991, 1992, Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * crypt.c; Standalone C Version, Based on GNU LIBC crypt_util.c, 2.17
 *
 */

#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>  /* For big-endian checking */
#include "main.h"

/* Define 'quad' as a 64-bit integer (using gcc) */
#undef quad
#define quad long long

/* _UFC_64_ should be defined to optimize the library for 64-bit
   architectures. Due to a bug in GCC, do not define on a 32-bit machine. */

#if defined(ARCH_64BIT)
# define _UFC_64_
  typedef unsigned quad ufc_long;
#else
  typedef unsigned int  ufc_long;
#endif


// Permutation done once on the 56 bit key,
// derived from the original 8 byte ASCII key.
//
static const char pc1[56] = { 
  57, 49, 41, 33, 25, 17,  9,  1, 58, 50, 42, 34, 26, 18,
  10,  2, 59, 51, 43, 35, 27, 19, 11,  3, 60, 52, 44, 36,
  63, 55, 47, 39, 31, 23, 15,  7, 62, 54, 46, 38, 30, 22,
  14,  6, 61, 53, 45, 37, 29, 21, 13,  5, 28, 20, 12,  4
};

// How much to rotate each 28 bit half of the pc1 permutated
// 56 bit key before using pc2 to give the i' key
//
static const char rots[16] = { 
  1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1 
};

// Permutation giving the key of the i' DES round 
//
static const char pc2[48] = { 
  14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,
  23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,
  41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
  44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
};

// The E expansion table which selects
// bits from the 32 bit intermediate result.
//
static const int esel[48] = { 
  32,  1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,
   8,  9, 10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
  16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25,
  24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32,  1
};
static int e_inverse[64];

// Permutation done on the 
// result of sbox lookups 
//
static const char perm32[32] = {
  16,  7, 20, 21, 29, 12, 28, 17,  1, 15, 23, 26,  5, 18, 31, 10,
  2,   8, 24, 14, 32, 27,  3,  9, 19, 13, 30,  6, 22, 11,  4, 25
};

// The sboxes
//
static const char sbox[8][4][16]= {
  { { 14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7 },
    {  0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8 },
    {  4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0 },
    { 15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13 }
  },

  { { 15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10 },
    {  3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5 },
    {  0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15 },
    { 13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9 }
  },

  { { 10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8 },
    { 13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1 },
    { 13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7 },
    {  1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12 }
  },

  { {  7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15 },
    { 13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9 },
    { 10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4 },
    {  3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14 }
  },

  { {  2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9 },
    { 14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6 },
    {  4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14 },
    { 11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3 }
  },

  { { 12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11 },
    { 10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8 },
    {  9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6 },
    {  4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13 }
  },

  { {  4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1 },
    { 13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6 },
    {  1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2 },
    {  6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12 }
  },

  { { 13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7 },
    {  1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2 },
    {  7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8 },
    {  2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11 }
  }
};

// This is the final permutation matrix
//
static const char final_perm[64] = {
  40,  8, 48, 16, 56, 24, 64, 32, 39,  7, 47, 15, 55, 23, 63, 31,
  38,  6, 46, 14, 54, 22, 62, 30, 37,  5, 45, 13, 53, 21, 61, 29,
  36,  4, 44, 12, 52, 20, 60, 28, 35,  3, 43, 11, 51, 19, 59, 27,
  34,  2, 42, 10, 50, 18, 58, 26, 33,  1, 41,  9, 49, 17, 57, 25
};

/* The 16 DES keys in BITMASK format */

#ifdef _UFC_64_
  ufc_long _ufc_keytab[16];
#else
  ufc_long _ufc_keytab[16][2];
#endif

#define ascii_to_bin(c) ((c)>='a'?(c-59):(c)>='A'?((c)-53):(c)-'.')
#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

/* Macro to set a bit (0..23) */
#define BITMASK(i) ( (1<<(11-(i)%12+3)) << ((i)<12?16:0) )

// sb arrays:
//
// Workhorses of the inner loop of the DES implementation.
// They do sbox lookup, shifting of this  value, 32 bit
// permutation and E permutation for the next round.
//
// Kept in 'BITMASK' format.

#ifdef _UFC_64_
  ufc_long _ufc_sb0[4096], _ufc_sb1[4096], _ufc_sb2[4096], _ufc_sb3[4096];
  static ufc_long *sb[4] = {_ufc_sb0, _ufc_sb1, _ufc_sb2, _ufc_sb3}; 
#else
  ufc_long _ufc_sb0[8192], _ufc_sb1[8192], _ufc_sb2[8192], _ufc_sb3[8192];
  static ufc_long *sb[4] = {_ufc_sb0, _ufc_sb1, _ufc_sb2, _ufc_sb3}; 
#endif

// eperm32tab: do 32 bit permutation and E selection
//
// The first index is the byte number in the 32 bit value to be permuted
// The second index is the value of this byte
// The third index selects the two 32 bit values
//
// The table is used and generated internally in init_des to speed it up

static ufc_long eperm32tab[4][256][2];

// do_pc1: permform pc1 permutation in the key schedule generation.
//
// The first index is the byte number in the 8 byte ASCII key
// The second index is the two 28 bits halfs of the result
// The third index selects the 7 bits actually used of each byte
//
// The result is kept with 28 bit per 32 bit with the 4 most significant
// bits zero.

static ufc_long do_pc1[8][2][128];

// do_pc2: permform pc2 permutation in the key schedule generation.
//
// The first index is the septet number in the two 28 bit intermediate values
// The second index is the septet values
//
// Knowledge of the structure of the pc2 permutation is used.
//
// The result is kept with 28 bit per 32 bit with the 4 most significant
// bits zero.

static ufc_long do_pc2[8][128];

// efp: undo an extra e selection and do final
//      permutation giving the DES result.
// 
//      Invoked 6 bit a time on two 48 bit values
//      giving two 32 bit longs.
//

static ufc_long efp[16][64][2];

/* lookup a 6 bit value in sbox */
#define s_lookup(i,s) sbox[i][(((s)>>4) & 0x2)|((s) & 0x1)][((s)>>1) & 0xf];

static ufc_long *_ufc_dofinalperm(ufc_long, ufc_long, ufc_long, ufc_long);
static int initialized;


#ifdef _UFC_64_

/**** 64-bit Version *********************************************************/

#define SBA(sb, v) (*(ufc_long*)((char*)(sb)+(v)))

static ufc_long *_ufc_doit(l1, l2, r1, r2, itr)
ufc_long l1, l2, r1, r2, itr;
{
  ufc_long l, r, s, *k;
  int i;

  l = (l1 << 32) | l2;
  r = (r1 << 32) | r2;

  while(itr--) {
    k = &_ufc_keytab[0];
    for(i=8; i--; ) {
      s = *k++ ^ r;
      l ^= SBA(_ufc_sb3, (s >>  0) & 0xffff);
      l ^= SBA(_ufc_sb2, (s >> 16) & 0xffff);
      l ^= SBA(_ufc_sb1, (s >> 32) & 0xffff);
      l ^= SBA(_ufc_sb0, (s >> 48) & 0xffff);

      s = *k++ ^ l;
      r ^= SBA(_ufc_sb3, (s >>  0) & 0xffff);
      r ^= SBA(_ufc_sb2, (s >> 16) & 0xffff);
      r ^= SBA(_ufc_sb1, (s >> 32) & 0xffff);
      r ^= SBA(_ufc_sb0, (s >> 48) & 0xffff);
    } 
    s=l; l=r; r=s;
  }

  l1 = l >> 32; l2 = l & 0xffffffff;
  r1 = r >> 32; r2 = r & 0xffffffff;
  return _ufc_dofinalperm(l1, l2, r1, r2);
}

// Process the elements of the sb table permuting the
// bits swapped in the expansion by the current salt.

static void shuffle_sb(ufc_long *k, ufc_long saltbits)
{
  ufc_long j, x;

  for(j=4096; j--;) {
    x = ((*k >> 32) ^ *k) & saltbits;
    *k++ ^= (x << 32) | x;
  }
}

#else  /* _UFC_32_ */

/**** 32-bit Version *********************************************************/

#define SBA(sb, v) (*(ufc_long*)((char*)(sb)+(v)))

static ufc_long *_ufc_doit(l1, l2, r1, r2, itr)
ufc_long l1, l2, r1, r2, itr;
{
  ufc_long s, *k;
  int i;

  while(itr--) {
    k = &_ufc_keytab[0][0];
    for(i=8; i--; ) {
      s = *k++ ^ r1;
      l1 ^= SBA(_ufc_sb1, s & 0xffff); l2 ^= SBA(_ufc_sb1, (s & 0xffff)+4);  
      l1 ^= SBA(_ufc_sb0, s >>= 16);   l2 ^= SBA(_ufc_sb0, (s)         +4); 
      s = *k++ ^ r2; 
      l1 ^= SBA(_ufc_sb3, s & 0xffff); l2 ^= SBA(_ufc_sb3, (s & 0xffff)+4);
      l1 ^= SBA(_ufc_sb2, s >>= 16);   l2 ^= SBA(_ufc_sb2, (s)         +4);

      s = *k++ ^ l1; 
      r1 ^= SBA(_ufc_sb1, s & 0xffff); r2 ^= SBA(_ufc_sb1, (s & 0xffff)+4);  
      r1 ^= SBA(_ufc_sb0, s >>= 16);   r2 ^= SBA(_ufc_sb0, (s)         +4); 
      s = *k++ ^ l2; 
      r1 ^= SBA(_ufc_sb3, s & 0xffff); r2 ^= SBA(_ufc_sb3, (s & 0xffff)+4);  
      r1 ^= SBA(_ufc_sb2, s >>= 16);   r2 ^= SBA(_ufc_sb2, (s)         +4);
    } 
    s=l1; l1=r1; r1=s; s=l2; l2=r2; r2=s;
  }
  return _ufc_dofinalperm(l1, l2, r1, r2);
}

// Process the elements of the sb table permuting the
// bits swapped in the expansion by the current salt.

static void shuffle_sb(ufc_long *k, ufc_long saltbits)
{
  ufc_long j, x;

  for(j=4096;j--;) {
    x = (k[0] ^ k[1]) & saltbits;
    *k++ ^= x;
    *k++ ^= x;
  }
}

#endif


// Initialize unit - may be invoked directly by fcrypt users.

static void init_des()
{
  int comes_from_bit, bit, sg;
  ufc_long j, mask1, mask2;

  /* Create the do_pc1 table used to affect
     pc1 permutation when generating keys */

  for(bit = 0; bit < 56; bit++) {
    comes_from_bit  = pc1[bit] - 1;
    mask1 = 0x40 >> (comes_from_bit % 8);
    mask2 = 0x8000000 >> (bit % 28);
    for(j = 0; j < 128; j++)
      if(j & mask1) 
        do_pc1[comes_from_bit / 8][bit / 28][j] |= mask2;
  }

  /* Create the do_pc2 table used to affect
     pc2 permutation when generating keys */

  for(bit = 0; bit < 48; bit++) {
    comes_from_bit  = pc2[bit] - 1;
    mask1 = 0x40 >> (comes_from_bit % 7);
    mask2 = BITMASK(bit % 24);
    for(j = 0; j < 128; j++)
      if(j & mask1)
        do_pc2[comes_from_bit / 7][j] |= mask2;
  }

  // Now generate the table used to combine 32 bit permutation and e expansion.
  //
  // We use it because we have to permute 16384 32 bit longs into 48 bit
  // in order to initialize sb.
  //
  // Looping 48 rounds per permutation becomes just too slow...

  memset(eperm32tab, 0, sizeof(eperm32tab));
  for(bit = 0; bit < 48; bit++) {
    ufc_long mask1, comes_from;

    comes_from = perm32[esel[bit]-1]-1;
    mask1      = 0x80 >> (comes_from % 8);

    for(j=256;j--;)
      if(j & mask1)
        eperm32tab[comes_from / 8][j][bit / 24] |= BITMASK(bit % 24);
  }
    
  // Create the sb tables:
  //
  // For each 12 bit segment of an 48 bit intermediate result, the sb table
  // precomputes the two 4 bit values of the sbox lookups done with the two
  // 6 bit halves, shifts them to their proper place, sends them through
  // perm32 and finally E expands them so that they are ready for the
  // next DES round.

  for(sg = 0; sg < 4; sg++) {
    int j1, j2;
    int s1, s2;
  
    for(j1 = 0; j1 < 64; j1++) {
      s1 = s_lookup(2 * sg, j1);
      for(j2 = 0; j2 < 64; j2++) {
        ufc_long to_permute, inx;
    
        s2         = s_lookup(2 * sg + 1, j2);
        to_permute = ((s1 << 4)  | s2) << (24 - 8 * sg);

#ifdef _UFC_64_
        inx = ((j1 << 6)  | j2);
        sb[sg][inx]  = 
          ((ufc_long)eperm32tab[0][(to_permute >> 24) & 0xff][0] << 32) |
           (ufc_long)eperm32tab[0][(to_permute >> 24) & 0xff][1];
        sb[sg][inx] |=
          ((ufc_long)eperm32tab[1][(to_permute >> 16) & 0xff][0] << 32) |
           (ufc_long)eperm32tab[1][(to_permute >> 16) & 0xff][1];
        sb[sg][inx] |= 
          ((ufc_long)eperm32tab[2][(to_permute >>  8) & 0xff][0] << 32) |
           (ufc_long)eperm32tab[2][(to_permute >>  8) & 0xff][1];
        sb[sg][inx] |=
          ((ufc_long)eperm32tab[3][(to_permute)       & 0xff][0] << 32) |
           (ufc_long)eperm32tab[3][(to_permute)       & 0xff][1];

#else  /* _UFC_32_ */

        inx = ((j1 << 6)  | j2) << 1;
        sb[sg][inx  ]  = eperm32tab[0][(to_permute >> 24) & 0xff][0];
        sb[sg][inx+1]  = eperm32tab[0][(to_permute >> 24) & 0xff][1];
        sb[sg][inx  ] |= eperm32tab[1][(to_permute >> 16) & 0xff][0];
        sb[sg][inx+1] |= eperm32tab[1][(to_permute >> 16) & 0xff][1];
        sb[sg][inx  ] |= eperm32tab[2][(to_permute >>  8) & 0xff][0];
        sb[sg][inx+1] |= eperm32tab[2][(to_permute >>  8) & 0xff][1];
        sb[sg][inx  ] |= eperm32tab[3][(to_permute)       & 0xff][0];
        sb[sg][inx+1] |= eperm32tab[3][(to_permute)       & 0xff][1];
#endif
      }
    }
  }

  // Create an inverse matrix for esel, telling where
  // to plug out bits if undoing it.

  for(bit=48; bit--;) {
    e_inverse[esel[bit] - 1     ] = bit;
    e_inverse[esel[bit] - 1 + 32] = bit + 48;
  }

  // create efp: the matrix used to undo the E expansion
  // and effect final permutation

  memset(efp, 0, sizeof(efp));
  for(bit = 0; bit < 64; bit++) {
    int o_bit, o_long;
    ufc_long word_value, mask1, mask2;
    int comes_from_f_bit, comes_from_e_bit;
    int comes_from_word, bit_within_word;

    /* See where bit i belongs in the two 32 bit ufc_long's */
    o_long = bit / 32; /* 0..1  */
    o_bit  = bit % 32; /* 0..31 */

    // And find a bit in the e permutated value setting this bit.
    //
    // Note: the e selection may have selected the same bit several
    // times. By the initialization of e_inverse, we only look
    // for one specific instance.

    comes_from_f_bit = final_perm[bit] - 1;          /* 0..63 */
    comes_from_e_bit = e_inverse[comes_from_f_bit];  /* 0..95 */
    comes_from_word  = comes_from_e_bit / 6;         /* 0..15 */
    bit_within_word  = comes_from_e_bit % 6;         /* 0..5  */

    mask1 = 0x20 >> bit_within_word;
    mask2 = 0x1  << (31-o_bit);

    for(word_value = 64; word_value--;)
      if(word_value & mask1)
        efp[comes_from_word][word_value][o_long] |= mask2;
  }

  initialized=1;
}


// Setup the unit for a new salt.
// Hopefully we'll not see a new salt in each crypt call.

static unsigned char current_salt[3] = "&&"; /* invalid value */
static ufc_long current_saltbits;
static int direction;

static void setup_salt(char *s)
{
  ufc_long i, j, saltbits;

  if(!initialized)
    init_des();

  if(s[0] == current_salt[0] && s[1] == current_salt[1])
    return;
  current_salt[0] = s[0]; current_salt[1] = s[1];

  /* This is the only crypt change to DES:
     entries are swapped in the expansion table
     according to the bits set in the salt. */

  saltbits = 0;
  for(i = 0; i < 2; i++) {
    ufc_long c=ascii_to_bin(s[i]);
    if(c < 0 || c > 63)
      c = 0;
    for(j = 0; j < 6; j++) {
      if((c >> j) & 0x1)
        saltbits |= BITMASK(6 * i + j);
    }
  }

  /* Permute the sb table values to reflect the changed e selection table */
  shuffle_sb(_ufc_sb0, current_saltbits ^ saltbits); 
  shuffle_sb(_ufc_sb1, current_saltbits ^ saltbits);
  shuffle_sb(_ufc_sb2, current_saltbits ^ saltbits);
  shuffle_sb(_ufc_sb3, current_saltbits ^ saltbits);

  current_saltbits = saltbits;
}

static void ufc_mk_keytab(char *key)
{
  ufc_long v1, v2, *k1;
  int i;

#ifdef _UFC_64_
    ufc_long v, *k2 = &_ufc_keytab[0];
#else
    ufc_long v, *k2 = &_ufc_keytab[0][0];
#endif

  v1 = v2 = 0; k1 = &do_pc1[0][0][0];
  for(i = 8; i--;) {
    v1 |= k1[*key   & 0x7f]; k1 += 128;
    v2 |= k1[*key++ & 0x7f]; k1 += 128;
  }

  for(i = 0; i < 16; i++) {
    k1 = &do_pc2[0][0];

    v1 = (v1 << rots[i]) | (v1 >> (28 - rots[i]));
    v  = k1[(v1 >> 21) & 0x7f]; k1 += 128;
    v |= k1[(v1 >> 14) & 0x7f]; k1 += 128;
    v |= k1[(v1 >>  7) & 0x7f]; k1 += 128;
    v |= k1[(v1      ) & 0x7f]; k1 += 128;

#ifdef _UFC_64_
    v <<= 32;
#else
    *k2++ = v;
    v = 0;
#endif

    v2 = (v2 << rots[i]) | (v2 >> (28 - rots[i]));
    v |= k1[(v2 >> 21) & 0x7f]; k1 += 128;
    v |= k1[(v2 >> 14) & 0x7f]; k1 += 128;
    v |= k1[(v2 >>  7) & 0x7f]; k1 += 128;
    v |= k1[(v2      ) & 0x7f];

    *k2++ = v;
  }

  direction = 0;
}

// Undo an extra E selection and do final permutations

static ufc_long *_ufc_dofinalperm(l1, l2, r1, r2)
ufc_long l1,l2,r1,r2;
{
  static ufc_long ary[2];
  ufc_long v1, v2, x;

  x = (l1 ^ l2) & current_saltbits; l1 ^= x; l2 ^= x;
  x = (r1 ^ r2) & current_saltbits; r1 ^= x; r2 ^= x;

  v1=v2=0; l1 >>= 3; l2 >>= 3; r1 >>= 3; r2 >>= 3;

  v1 |= efp[15][ r2         & 0x3f][0]; v2 |= efp[15][ r2 & 0x3f][1];
  v1 |= efp[14][(r2 >>= 6)  & 0x3f][0]; v2 |= efp[14][ r2 & 0x3f][1];
  v1 |= efp[13][(r2 >>= 10) & 0x3f][0]; v2 |= efp[13][ r2 & 0x3f][1];
  v1 |= efp[12][(r2 >>= 6)  & 0x3f][0]; v2 |= efp[12][ r2 & 0x3f][1];

  v1 |= efp[11][ r1         & 0x3f][0]; v2 |= efp[11][ r1 & 0x3f][1];
  v1 |= efp[10][(r1 >>= 6)  & 0x3f][0]; v2 |= efp[10][ r1 & 0x3f][1];
  v1 |= efp[ 9][(r1 >>= 10) & 0x3f][0]; v2 |= efp[ 9][ r1 & 0x3f][1];
  v1 |= efp[ 8][(r1 >>= 6)  & 0x3f][0]; v2 |= efp[ 8][ r1 & 0x3f][1];

  v1 |= efp[ 7][ l2         & 0x3f][0]; v2 |= efp[ 7][ l2 & 0x3f][1];
  v1 |= efp[ 6][(l2 >>= 6)  & 0x3f][0]; v2 |= efp[ 6][ l2 & 0x3f][1];
  v1 |= efp[ 5][(l2 >>= 10) & 0x3f][0]; v2 |= efp[ 5][ l2 & 0x3f][1];
  v1 |= efp[ 4][(l2 >>= 6)  & 0x3f][0]; v2 |= efp[ 4][ l2 & 0x3f][1];

  v1 |= efp[ 3][ l1         & 0x3f][0]; v2 |= efp[ 3][ l1 & 0x3f][1];
  v1 |= efp[ 2][(l1 >>= 6)  & 0x3f][0]; v2 |= efp[ 2][ l1 & 0x3f][1];
  v1 |= efp[ 1][(l1 >>= 10) & 0x3f][0]; v2 |= efp[ 1][ l1 & 0x3f][1];
  v1 |= efp[ 0][(l1 >>= 6)  & 0x3f][0]; v2 |= efp[ 0][ l1 & 0x3f][1];

  ary[0] = v1; ary[1] = v2;
  return ary;
}

// DES Entry Point: Perform DES encryption and return a 64-bit value.
//
quad des_crypt(char *key, char *salt)
{
  ufc_long *s;
  char ktab[9]={0};
  int i;

  /* Hack DES tables according to salt */
  setup_salt(salt);

  /* Setup key schedule */
  for(i=0;i<8 && key[i];i++)
    ktab[i]=key[i];
  ufc_mk_keytab(ktab);

  /* Do the 25 DES encryptions */
  s = _ufc_doit(0, 0, 0, 0, 25);

  /* Return 64-bit value */
  return ((quad)s[0] << 32) | (unsigned int)s[1];
}


/**** MD5 Hash Section *******************************************************/

struct MD5Context {
  int buf[4];
  int bits[2];
  unsigned int in[64];
};

static void MD5Transform(unsigned int buf[4], unsigned int const in[16]);

// Byte-swap 32-bit integers; Big-endian only!
//
static void swap_int32(unsigned int *buf, int longs)
{
  /* Optimize this function out on little-endian machines */
  if(htons(1) == 1)
    for(;--longs >= 0;buf++)
      *buf=(*buf >> 24)|(*buf << 24)|
            ((*buf >> 8) & 0xff00)|((*buf & 0xff00) << 8);
}

// Start MD5 accumulation. Set bit count to 0 and buffer to mysterious
// initialization constants.
//
static void MD5Init(struct MD5Context *ctx)
{
  ctx->buf[0] = 0x67452301;
  ctx->buf[1] = 0xefcdab89;
  ctx->buf[2] = 0x98badcfe;
  ctx->buf[3] = 0x10325476;

  ctx->bits[0] = 0;
  ctx->bits[1] = 0;
}

// Update context to reflect the concatenation of another buffer full
// of bytes.
//
static void MD5Update(struct MD5Context *ctx, unsigned char const *buf, int len)
{
  unsigned int t=ctx->bits[0];

  /* Update bitcount */
  if((ctx->bits[0] = t+(len << 3)) < t)
    ctx->bits[1]++;		/* Carry from low to high */
  ctx->bits[1] += len >> 29;

  t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

  /* Handle any leading odd-sized chunks */
  if(t) {
    unsigned char *p=(unsigned char *)ctx->in + t;

    t=64-t;
    if(len < t) {
      memcpy(p, buf, len);
      return;
    }
    memcpy(p, buf, t);
    swap_int32(ctx->in, 16);
    MD5Transform(ctx->buf, ctx->in);
    buf += t;
    len -= t;
  }

  /* Process data in 64-byte chunks */
  while(len >= 64) {
    memcpy(ctx->in, buf, 64);
    swap_int32(ctx->in, 16);
    MD5Transform(ctx->buf, ctx->in);
    buf += 64;
    len -= 64;
  }

  /* Handle any remaining bytes of data. */
  memcpy(ctx->in, buf, len);
}

// Final wrapup: Pad to 64-byte boundary with the bit pattern 1 [0]*
// up to 64-bit count of bits processed, MSB-first.
//
static void MD5Final(unsigned char digest[16], struct MD5Context *ctx)
{
  unsigned char *p;
  int count;

  /* Compute number of bytes mod 64 */
  count = (ctx->bits[0] >> 3) & 0x3F;

  /* Set the first char of padding to 0x80. This is safe since there is
     always at least one byte free. */
  p = (unsigned char *)ctx->in + count;
  *p++ = 0x80;

  /* Bytes of padding needed to make 64 bytes */
  count = 63-count;

  /* Pad out to 56 mod 64 */
  if(count < 8) {
    /* Pad the first block to 64 bytes */
    memset(p, 0, count);
    swap_int32(ctx->in, 16);
    MD5Transform(ctx->buf, ctx->in);

    /* Now pad the next block with 56 bytes */
    memset(ctx->in, 0, 56);
  } else {
    memset(p, 0, count-8);   /* Just pad block to 56 bytes */
  }
  swap_int32(ctx->in, 14);

  /* Append length in bits and transform */
  ctx->in[14] = ctx->bits[0];
  ctx->in[15] = ctx->bits[1];

  MD5Transform(ctx->buf, ctx->in);
  swap_int32(ctx->buf, 4);
  memcpy(digest, ctx->buf, 16);
  memset(ctx->buf, 0, 16);
}

/* The four core functions */

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) (y ^ (z & (x ^ y)))
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
                (w += f(x, y, z)+data, w=(w<<s) | (w>>(32-s)), w+=x)

// The core of the MD5 algorithm, this alters an existing MD5 hash to
// reflect the addition of 16 longwords of new data. MD5Update blocks
// the data and converts bytes into longwords for this routine.
//
static void MD5Transform(unsigned int buf[4], unsigned int const in[16])
{
  register unsigned int a, b, c, d;

  a = buf[0];
  b = buf[1];
  c = buf[2];
  d = buf[3];

  MD5STEP(F1, a, b, c, d, in[0]  + 0xd76aa478, 7);
  MD5STEP(F1, d, a, b, c, in[1]  + 0xe8c7b756, 12);
  MD5STEP(F1, c, d, a, b, in[2]  + 0x242070db, 17);
  MD5STEP(F1, b, c, d, a, in[3]  + 0xc1bdceee, 22);
  MD5STEP(F1, a, b, c, d, in[4]  + 0xf57c0faf, 7);
  MD5STEP(F1, d, a, b, c, in[5]  + 0x4787c62a, 12);
  MD5STEP(F1, c, d, a, b, in[6]  + 0xa8304613, 17);
  MD5STEP(F1, b, c, d, a, in[7]  + 0xfd469501, 22);
  MD5STEP(F1, a, b, c, d, in[8]  + 0x698098d8, 7);
  MD5STEP(F1, d, a, b, c, in[9]  + 0x8b44f7af, 12);
  MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
  MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
  MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
  MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
  MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
  MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

  MD5STEP(F2, a, b, c, d, in[1]  + 0xf61e2562, 5);
  MD5STEP(F2, d, a, b, c, in[6]  + 0xc040b340, 9);
  MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
  MD5STEP(F2, b, c, d, a, in[0]  + 0xe9b6c7aa, 20);
  MD5STEP(F2, a, b, c, d, in[5]  + 0xd62f105d, 5);
  MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
  MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
  MD5STEP(F2, b, c, d, a, in[4]  + 0xe7d3fbc8, 20);
  MD5STEP(F2, a, b, c, d, in[9]  + 0x21e1cde6, 5);
  MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
  MD5STEP(F2, c, d, a, b, in[3]  + 0xf4d50d87, 14);
  MD5STEP(F2, b, c, d, a, in[8]  + 0x455a14ed, 20);
  MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
  MD5STEP(F2, d, a, b, c, in[2]  + 0xfcefa3f8, 9);
  MD5STEP(F2, c, d, a, b, in[7]  + 0x676f02d9, 14);
  MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

  MD5STEP(F3, a, b, c, d, in[5]  + 0xfffa3942, 4);
  MD5STEP(F3, d, a, b, c, in[8]  + 0x8771f681, 11);
  MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
  MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
  MD5STEP(F3, a, b, c, d, in[1]  + 0xa4beea44, 4);
  MD5STEP(F3, d, a, b, c, in[4]  + 0x4bdecfa9, 11);
  MD5STEP(F3, c, d, a, b, in[7]  + 0xf6bb4b60, 16);
  MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
  MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
  MD5STEP(F3, d, a, b, c, in[0]  + 0xeaa127fa, 11);
  MD5STEP(F3, c, d, a, b, in[3]  + 0xd4ef3085, 16);
  MD5STEP(F3, b, c, d, a, in[6]  + 0x04881d05, 23);
  MD5STEP(F3, a, b, c, d, in[9]  + 0xd9d4d039, 4);
  MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
  MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
  MD5STEP(F3, b, c, d, a, in[2]  + 0xc4ac5665, 23);

  MD5STEP(F4, a, b, c, d, in[0]  + 0xf4292244, 6);
  MD5STEP(F4, d, a, b, c, in[7]  + 0x432aff97, 10);
  MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
  MD5STEP(F4, b, c, d, a, in[5]  + 0xfc93a039, 21);
  MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
  MD5STEP(F4, d, a, b, c, in[3]  + 0x8f0ccc92, 10);
  MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
  MD5STEP(F4, b, c, d, a, in[1]  + 0x85845dd1, 21);
  MD5STEP(F4, a, b, c, d, in[8]  + 0x6fa87e4f, 6);
  MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
  MD5STEP(F4, c, d, a, b, in[6]  + 0xa3014314, 15);
  MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
  MD5STEP(F4, a, b, c, d, in[4]  + 0xf7537e82, 6);
  MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
  MD5STEP(F4, c, d, a, b, in[2]  + 0x2ad7d2bb, 15);
  MD5STEP(F4, b, c, d, a, in[9]  + 0xeb86d391, 21);

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}

#define permute24(buf, s, i, a, b, c) \
    (buf[i++] = s[c] & 0x3f, buf[i++] = (s[c] >> 6) | ((s[b] & 0xf) << 2), \
     buf[i++] = (s[b] >> 4) | ((s[a] & 0x3) << 4), buf[i++] = s[a] >> 2)

// MD5 Entry Point: Perform MD5 hashing and return a 16-byte buffer.
//
char *md5_crypt(char *pw, char *salt, int len)
{
  static unsigned char final[16];
  struct MD5Context ctx, ctx1;
  int i, pw_len=strlen(pw);

  MD5Init(&ctx);		/* Initialize the context */
  MD5Update(&ctx, pw, pw_len);	/* Password first */
  MD5Update(&ctx, "$1$", 3);	/* MD5 magic version string */
  MD5Update(&ctx, salt, len);	/* Our raw salt */

  /* Next, insert the result of MD5(pw,salt,pw) */
  MD5Init(&ctx1);
  MD5Update(&ctx1, pw, pw_len);
  MD5Update(&ctx1, salt, len);
  MD5Update(&ctx1, pw, pw_len);
  MD5Final(final, &ctx1);

  for(i=pw_len;i > 0;i-=16)
    MD5Update(&ctx, final, i>16?16:i);

  /* We need a null byte for the next loop */
  *final='\0';
  for(i=pw_len;i;i >>= 1) {
    if(i & 1)
      MD5Update(&ctx, final, 1);
    else
      MD5Update(&ctx, pw, 1);
  }
  MD5Final(final, &ctx);

  /* "Slow down hackers" Loop */
  for(i=0;i<1000;i++) {
    MD5Init(&ctx);
    if(i & 1)
      MD5Update(&ctx, pw, pw_len);
    else
      MD5Update(&ctx, final, 16);

    if(i % 3)
      MD5Update(&ctx, salt, len);

    if(i % 7)
      MD5Update(&ctx, pw, pw_len);

    if(i & 1)
      MD5Update(&ctx, final, 16);
    else
      MD5Update(&ctx, pw, pw_len);

    MD5Final(final, &ctx);
  }

  return final;
}


/* UNIX crypt function (supports both DES and MD5) */

char *crypt(char *key, char *salt)
{
  static char outbuf[35];
  unsigned char *s;
  quad des;
  int i, j;

  /* Encrypt the key */
  if(strncmp(salt, "$1$", 3)) {
    /* Use DES encryption */
    des=des_crypt(key, salt);

    /* Put the salt in the output */
    outbuf[0]=salt[0]?:'.';
    outbuf[1]=salt[1]?:'.';

    /* Copy 64-bit binary data to buffer in 6-bit chunks */
    for(i=0;i < 10;i++)
      outbuf[i+2]=(des >> (58-(i*6))) & 0x3f;
    outbuf[12]=(des << 2) & 0x3f;
    outbuf[13]='\0';

    /* Convert 6-bit data to ASCII characters */
    for(i=2;i<13;i++)
      outbuf[i]=bin_to_ascii(outbuf[i]);
  } else {
    /* Set up the output prefix */
    strncpy(outbuf, salt, 11);
    salt[11]='\0';
    if((s=strchr(outbuf+3, '$')))
      *s='\0';
    i=strlen(outbuf+3);
    outbuf[i+3]='$';

    /* Use MD5 Hashing */
    s=md5_crypt(key, outbuf+3, i);

    /* Convert binary data to 6-bit in 24-bit chunks */
    i+=4, j=i;
    permute24(outbuf, s, i, 0, 6, 12);
    permute24(outbuf, s, i, 1, 7, 13);
    permute24(outbuf, s, i, 2, 8, 14);
    permute24(outbuf, s, i, 3, 9, 15);
    permute24(outbuf, s, i, 4, 10, 5);
    outbuf[i++]=s[11] & 0x3f;
    outbuf[i++]=s[11] >> 6;
    outbuf[i]='\0';

    /* Convert 6-bit data to ASCII characters */
    for(;j<i;j++)
      outbuf[j]=bin_to_ascii(outbuf[j]);
  }

  return outbuf;
}

// Take a simple hash of a password and 4-byte salt, return the binary hash.
//
char *md5_hash(char *pw, int salt)
{
  static char result[16];
  struct MD5Context c;

  salt = htonl(salt);

  MD5Init(&c);
  MD5Update(&c, (char *)&salt, 4);
  MD5Update(&c, pw, strlen(pw));
  salt ^= htonl(0xaa55a55a);
  MD5Update(&c, (char *)&salt, 4);
  MD5Final(result, &c);

  return result;
}


/**** CRC-32 Section *********************************************************/

static unsigned int crc32[256];

// Initializes the crc32 table. 'crc' is the crc of byte i; other entries are
// filled in based on the fact that crc32[i^j] = crc32[i] ^ crc32[j].
//
static void init_crc32()
{
  unsigned int crc=1;
  int i, j;

  crc32[0]=0;
  for(i=128;i;i >>= 1) {
    crc=(crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    for(j=0;j < 256;j+=2*i)
      crc32[i+j]=crc ^ crc32[j];
  }
}

// External function to compute the crc32 value of a C string.
//
int compute_crc(unsigned char *s)
{
  unsigned int c=-1;

  if(!crc32[1])
    init_crc32();  /* Initializes crc32[] on the first run */

  while(*s)
    c=crc32[(c ^ (*s++)) & 0xff] ^ (c >> 8);

  return ~c;
}
