/*
	BRC - Behemoth Rank Coding for BWT
	MIT License
	Copyright (c) 2018 Lucas Marsh
	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#pragma once

#include "common.hpp"

#define BRC_VERSION 4
#define BRC_EXIT_SUCCESS 0
#define BRC_EXIT_FAILURE -1

struct brc_cxt_s {
	unsigned char * block; 
	size_t size;
	size_t eob;
};

/* allocate memory for BRC encoder or decoder */
int brc_init_cxt(brc_cxt_s * brc_cxt, size_t src_size);

/* free all memory associated with brc */
void brc_free_cxt(brc_cxt_s * brc_cxt);

/* generates BRC block transform from 'src' and stores it in 'brc_cxt'; returns 0 for successful encode, else -1 */
int brc_encode(brc_cxt_s * brc_cxt, unsigned char * src, size_t src_size);

/* undoes BRC block transform from 'brc_cxt' and stores it in 'dst'; returns 0 for successful encode, else -1 */
int brc_decode(brc_cxt_s * brc_cxt, unsigned char * dst, size_t * dst_size);
