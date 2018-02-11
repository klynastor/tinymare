/* mare/rand.c */
/* Mersenne Twister pseudo-random number generator implementation */

// A C-program for MT19937, with initialization improved 2002/1/26.
// Coded by Takuji Nishimura and Makoto Matsumoto.
//
// Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
// All rights reserved.                          
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//   3. The names of its contributors may not be used to endorse or promote 
//      products derived from this software without specific prior written 
//      permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Any feedback is very welcome.
// http://www.math.keio.ac.jp/matumoto/emt.html
// email: matumoto@math.keio.ac.jp

#include <stdlib.h>
#include <stdio.h>

#define N              624                   // length of state vector
#define M              397                   // a period parameter
#define K              0x9908B0DF            // a magic constant
#define hiBit(u)       ((u) & 0x80000000)    // mask all but highest   bit of u
#define loBit(u)       ((u) & 0x00000001)    // mask all but lowest    bit of u
#define loBits(u)      ((u) & 0x7FFFFFFF)    // mask     the highest   bit of u
#define mixBits(u, v)  (hiBit(u)|loBits(v))  // move hi bit of u to hi bit of v

static unsigned int state[N];  // State vector
static int index;  // Index into state vector from which next value is computed


void mt_srand(int seed)
{
  unsigned int last=seed;
  int j;

  index=N;
  state[0]=seed;
  for(j=1;j < N;j++)
    last=state[j]=1812433253 * (last ^ (last >> 30)) + j;
}

int mt_rand()
{
  unsigned int y;

  if(index >= N) {
    unsigned int *p0=state, *p2=state+2, *pM=state+M, s0, s1;
    int j;

    for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++)
      *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    for(pM=state, j=M; --j; s0=s1, s1=*p2++)
      *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    s1 = state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);
    y = s1;
    index = 1;
  } else
    y = state[index++];

  y ^= (y >> 11);
  y ^= (y <<  7) & 0x9D2C5680;
  y ^= (y << 15) & 0xEFC60000;
  return (y ^ (y >> 18));
}

// A repeatable version of mt_rand() which outputs a number of random numbers
// given a specific seed. This is used in the MARE srand() softcode function
// and is not a part of the mt_rand library.
//
void mt_repeatable_rand(char *buff, int modulo, int seed, int n, int count)
{
  unsigned int state[N], last=seed;  /* Local state vector */
  unsigned int *p0, *p2, *pM, s0, s1, y;
  char *s=buff;
  int c=0, j;

  /* Seed the state vector */
  state[0]=seed;
  for(j=1;j < N;j++)
    last=state[j]=1812433253 * (last ^ (last >> 30)) + j;

  while(c < n+count) {
    /* Rebuild the array */
    p0=state, p2=state+2, pM=state+M;
    for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++)
      *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    for(pM=state, j=M; --j; s0=s1, s1=*p2++)
      *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    s1 = state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    if(c+624 < n) {
      c+=624;
      continue;
    }

    if(c < n)
      j=n-c, c=n;
    else
      j=0;
    for(;j < 624 && c < n+count;c++, j++) {
      y = j ? state[j]:s1;
      y ^= (y >> 11);
      y ^= (y <<  7) & 0x9D2C5680;
      y ^= (y << 15) & 0xEFC60000;

      if(s != buff) {
        if(s-buff > 7989)
          return;
        *s++=' ';
      }
      s+=sprintf(s, "%d", (y ^ (y >> 18)) % modulo);
    }
  }
}
