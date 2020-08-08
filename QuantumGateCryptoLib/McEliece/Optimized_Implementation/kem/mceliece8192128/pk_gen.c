/*
  This file is for public-key generation
*/

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "controlbits.h"
#include "pk_gen.h"
#include "params.h"
#include "benes.h"
#include "root.h"
#include "util.h"

// gets pointer to element mat[row][col] from array mat[x][y]
static inline unsigned char * get(unsigned char * mat, int row, int col)
{
	return mat+((SYS_N/8)*row)+col;
}

/* input: secret key sk */
/* output: public key pk */
int pk_gen(unsigned char * pk, unsigned char * sk, uint32_t * perm, unsigned char * matmem)
{
	int i, j, k;
	int row, c;

	uint64_t buf[1 << GFBITS];

	// unsigned char mat[ GFBITS * SYS_T ][ SYS_N/8 ];
	// allocated on the heap instead avoid exhausting the stack
	unsigned char *mat = matmem;

	unsigned char mask;
	unsigned char b;

	gf g[ SYS_T+1 ]; // Goppa polynomial
	gf L[ SYS_N ]; // support
	gf inv[ SYS_N ];

	//

	g[ SYS_T ] = 1;

	for (i = 0; i < SYS_T; i++) { g[i] = load2(sk); g[i] &= GFMASK; sk += 2; }

	for (i = 0; i < (1 << GFBITS); i++)
	{
		buf[i] = perm[i];
		buf[i] <<= 31;
		buf[i] |= i;
	}

	sort_63b(1 << GFBITS, buf);

	for (i = 0; i < (1 << GFBITS); i++) perm[i] = buf[i] & GFMASK;
	for (i = 0; i < SYS_N;         i++) L[i] = bitrev(perm[i]);

	// filling the matrix

	root(inv, g, L);
		
	for (i = 0; i < SYS_N; i++)
		inv[i] = gf_inv(inv[i]);

	for (i = 0; i < PK_NROWS; i++)
	for (j = 0; j < SYS_N/8; j++)
		*get(mat,i,j) = 0;

	for (i = 0; i < SYS_T; i++)
	{
		for (j = 0; j < SYS_N; j+=8)
		for (k = 0; k < GFBITS;  k++)
		{
			b  = (inv[j+7] >> k) & 1; b <<= 1;
			b |= (inv[j+6] >> k) & 1; b <<= 1;
			b |= (inv[j+5] >> k) & 1; b <<= 1;
			b |= (inv[j+4] >> k) & 1; b <<= 1;
			b |= (inv[j+3] >> k) & 1; b <<= 1;
			b |= (inv[j+2] >> k) & 1; b <<= 1;
			b |= (inv[j+1] >> k) & 1; b <<= 1;
			b |= (inv[j+0] >> k) & 1;

			*get(mat,i*GFBITS + k,j/8) = b;
		}

		for (j = 0; j < SYS_N; j++)
			inv[j] = gf_mul(inv[j], L[j]);

	}

	// gaussian elimination

	for (i = 0; i < (GFBITS * SYS_T + 7) / 8; i++)
	for (j = 0; j < 8; j++)
	{
		row = i*8 + j;			

		if (row >= GFBITS * SYS_T)
			break;

		for (k = row + 1; k < GFBITS * SYS_T; k++)
		{
			mask = *get(mat,row,i) ^ *get(mat,k,i);
			mask >>= j;
			mask &= 1;
			mask = -mask;

			for (c = 0; c < SYS_N/8; c++)
				*get(mat,row,c) ^= *get(mat,k,c) & mask;
		}

		if ( ((*get(mat,row,i) >> j) & 1) == 0 ) // return if not systematic
		{
			return -1;
		}

		for (k = 0; k < GFBITS * SYS_T; k++)
		{
			if (k != row)
			{
				mask = *get(mat,k,i) >> j;
				mask &= 1;
				mask = -mask;

				for (c = 0; c < SYS_N/8; c++)
					*get(mat,k,c) ^= *get(mat,row,c) & mask;
			}
		}
	}

	for (i = 0; i < PK_NROWS; i++)
		memcpy(pk + i*PK_ROW_BYTES, get(mat,i,0) + PK_NROWS/8, PK_ROW_BYTES);

	return 0;
}

