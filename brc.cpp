/*
	BRC - Behemoth Rank Coding for BWT
	MIT License
	Copyright (c) 2018 Lucas Marsh
	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "brc.hpp"

#define BRC_VSRC_FOOTER_SIZE (sizeof(uint32_t) * 256)
#define BRC_RLT_FOOTER_SIZE (1)
#define BRC_PAD_SIZE (16)

/*** basic utilities **/
void brc_memcopy_separate(void * dst, void * src, size_t size) {
	unsigned char * s = (unsigned char*)src;
	unsigned char * d = (unsigned char*)dst;
	if((((size_t)s) % 8) == 0 && (((size_t)d) % 8) == 0) {
		size_t i = 0;
		while((i + 8) < size) {
			*((uint64_t*)&d[i]) = *((uint64_t*)&s[i]);
			i += 8;
		}
		while(i < size) {
			d[i] = s[i];
			i++;
		}
	} else {
		memcpy(dst, src, size);
	}
}

void * brc_aligned_malloc(size_t bytes, size_t alignment) {
	char * base_ptr;
	char * aligned_ptr;

	size_t pad = alignment + sizeof(size_t);
	if((base_ptr =(char*)malloc(bytes + pad)) == NULL)
		return NULL;
	size_t addr = (size_t)(base_ptr + pad);
	aligned_ptr = (char *)(addr - (addr % alignment));
	*((size_t*)aligned_ptr - 1) = (size_t)base_ptr;

	return (void*)aligned_ptr;
}

void brc_aligned_free(void * aligned_ptr) {
	free( (char*)(*((size_t*)aligned_ptr - 1)) );
}

/*** bytewise zero run length coder  ***/
size_t rlt_forwards(unsigned char * src, unsigned char * dst, size_t size) {
	unsigned char * write_head = dst;
	unsigned char * write_end = write_head + size;
	size_t i = 0;
	while(i < size && write_head < write_end) {	
		if(src[i] == 0) {
			size_t run = 1;
			while ((i + run) < size && src[i] == src[i + run])
				run++;
			i += run;
			size_t L = run + 1; 
			size_t msb = 0;
			asm("bsrq %1,%0" : "=r"(msb) : "r"(L));
			while(msb--)
				*write_head++ = (L >> msb) & 1;	
		} else if (src[i] >= 0xfe) {
			*write_head++ = 0xff;
			*write_head++ = src[i++] == 0xff;
		} else {
			*write_head++ = src[i++] + 1;
		}
	}
	if(i < size) {
		brc_memcopy_separate(dst, src, size);
		*(dst + size) = 0;
		return size + BRC_RLT_FOOTER_SIZE;
	} else {
		size_t packed_size = write_head - dst;
		*(dst + packed_size) = 1;
		return packed_size + BRC_RLT_FOOTER_SIZE;
	}
}

size_t rlt_reverse(unsigned char * src, unsigned char * dst, size_t size) {
	size_t unpacked = size - BRC_RLT_FOOTER_SIZE;
	if(*(src + unpacked)  == 0) {
		brc_memcopy_separate(dst, src, unpacked);
		return unpacked;
	} else {	
		unsigned char * write_head = dst;
		unsigned char * write_end = write_head + unpacked;
		size_t i = 0; 
		while(i < unpacked) {
			if(src[i] == 0xff) {
				i++;
				*write_head++ = 0xfe + src[i++];
			} else if (src[i] > 1) {
				*write_head++ = src[i++] - 1;
			} else {
				int rle = 1;
				while (src[i] <= 1 && i < unpacked)
					rle = (rle << 1) | src[i++];
				rle -= 1;
				while(rle--)
					*write_head++ = 0;	
			}
		}
		return write_head - dst;
	}
}

/*** vectorized sorted rank transform ***/
struct vmtf_s {
	unsigned char map[256];
};

inline void init_vmtf(vmtf_s * x) {
	for(size_t i = 0; i < 256; i++)
		x->map[i] = i;
}

inline void inverse_vmtf_update_only(vmtf_s * x, unsigned char r, unsigned char s) {
	size_t j = 0;
	do x->map[j] = x->map[j + 1];
	while(++j < r);
	x->map[r] = s;
}

inline unsigned char inverse_vmtf_update_single(vmtf_s * x, unsigned char r, unsigned char s) {
	inverse_vmtf_update_only(x, r, s);
	return x->map[0];
}

inline void generate_sorted_map(uint32_t * freqs, unsigned char * map) {
	uint32_t freqs_cpy[256];
	for(size_t i = 0; i < 256; i++)
		freqs_cpy[i] = freqs[i];

	for(size_t j = 0; j < 256; j++) {
		size_t max = 0;
		unsigned char bsym = 0;
		for(int i = 0; i < 256; i++) {
			if(freqs_cpy[i] > max) {
				bsym = i;
				max = freqs_cpy[i];
			}
		}
		if(max == 0) break;
		else map[j] = bsym;
		freqs_cpy[bsym] = 0;
	}
}

int vsrc_forwards(unsigned char * src, unsigned char * dst, size_t src_size) {
	unsigned char * read_head  = src;
	unsigned char * write_head = dst;

	vmtf_s state;
	init_vmtf(&state);

	size_t bucket[256] = {0};
	uint32_t freqs[256] = {0};
	unsigned char sort_map[256], s, r;

	size_t unique_syms = 0;
	for (size_t i = 0; i < src_size; i++) {
		s = read_head[i];
		if (freqs[s] == 0)
			state.map[s] = unique_syms++;
		freqs[s]++;
	}

	brc_memcopy_separate(dst + src_size, freqs, BRC_VSRC_FOOTER_SIZE); 
	generate_sorted_map(freqs, sort_map);

	for(size_t i = 0, bucket_pos = 0; i < unique_syms; i++) {
		s = sort_map[i];
		bucket[s] = bucket_pos;
		bucket_pos += freqs[s];
	}

	for(size_t i = 0; i < src_size; i++) {
		s = read_head[i]; 
		r = state.map[s]; 
		write_head[bucket[s]++] = r;
		if(r) {
			for(size_t i = 0; i < 256; i++)
				state.map[i] += (state.map[i] < r);
			state.map[s] = 0;
		}
	}
	return src_size + BRC_VSRC_FOOTER_SIZE;
}

int vsrc_reverse(unsigned char * src, unsigned char * dst, size_t src_size) {
	unsigned char * read_head  = src;
	unsigned char * write_head = dst;
	size_t dst_size = src_size - BRC_VSRC_FOOTER_SIZE;

	vmtf_s state;
	init_vmtf(&state);

	size_t bucket[256] = {0}, bucket_end[256] = {0};
	uint32_t freqs[256] = {0};
	unsigned char sort_map[256], s, r;

	brc_memcopy_separate(freqs, src + dst_size, BRC_VSRC_FOOTER_SIZE); 

	size_t total = 0;
	for(size_t i = 0; i < 256; i++)
		total += freqs[i];

	if(total != dst_size) 
		return printf(" Invalid sub header detected! \n"), BRC_EXIT_FAILURE;
	
	size_t unique_syms = 0;
	for(size_t i = 0; i < 256; i++)
		if(freqs[i] > 0) 
			unique_syms++;
	
	generate_sorted_map(freqs, sort_map);

	for(size_t i = 0, bucket_pos = 0; i < unique_syms; i++) {
		s = sort_map[i];
		state.map[read_head[bucket_pos]] = s;
		bucket[s] = bucket_pos + 1;
		bucket_pos += freqs[s];
		bucket_end[s] = bucket_pos;
	}

	s = state.map[0];
	for(size_t i = 0; i < dst_size; i++) {
		write_head[i] = s, r = 0xff;
		if(bucket[s] < bucket_end[s]) r = read_head[bucket[s]++];
		if(r) s = inverse_vmtf_update_single(&state, r, s); 
	}

	return dst_size;
}

/*** BRC TRANSFORM ***/
size_t brc_safe_memory_bound(size_t x) {
	return x + BRC_VSRC_FOOTER_SIZE + BRC_RLT_FOOTER_SIZE + BRC_PAD_SIZE;
}

int brc_init_cxt(brc_cxt_s * brc_cxt, size_t src_size) {
	size_t mempool = brc_safe_memory_bound(src_size);
	brc_cxt->block = (unsigned char*)brc_aligned_malloc(mempool, 8);
	if(brc_cxt->block == NULL) return BRC_EXIT_FAILURE;
	brc_cxt->eob = mempool;
	return BRC_EXIT_SUCCESS;
}

void brc_free_cxt(brc_cxt_s * brc_cxt) {
	brc_aligned_free(brc_cxt->block);
	brc_cxt->block = NULL;
	brc_cxt->size = 0;
	brc_cxt->eob = 0;
}

int brc_encode(brc_cxt_s * brc_cxt, unsigned char * src, size_t src_size) {
	int dst_size = vsrc_forwards(src, brc_cxt->block, src_size);
	if(dst_size == BRC_EXIT_FAILURE) return BRC_EXIT_FAILURE;

	unsigned char * swap = (unsigned char*)brc_aligned_malloc(brc_safe_memory_bound(dst_size), 8);
	if(swap == NULL) return BRC_EXIT_FAILURE;
	brc_memcopy_separate(swap, brc_cxt->block, dst_size);

	size_t packed_size = rlt_forwards(swap, brc_cxt->block, dst_size);

	brc_cxt->size = (size_t)packed_size;
	brc_aligned_free(swap);

	return BRC_EXIT_SUCCESS;
}

int brc_decode(brc_cxt_s * brc_cxt, unsigned char * dst, size_t * dst_size) {
	unsigned char * swap = (unsigned char*)brc_aligned_malloc(brc_safe_memory_bound(brc_cxt->size), 8);
	if(swap == NULL) return BRC_EXIT_FAILURE;
	brc_memcopy_separate(swap, brc_cxt->block, brc_cxt->size);

	size_t origin_size = rlt_reverse(swap, brc_cxt->block, brc_cxt->size);

	brc_cxt->size = (size_t)origin_size;
	brc_aligned_free(swap);

	origin_size = vsrc_reverse(brc_cxt->block, dst, brc_cxt->size);
	if(origin_size == BRC_EXIT_FAILURE) return BRC_EXIT_FAILURE;
	*dst_size = (size_t)origin_size;

	return BRC_EXIT_SUCCESS;
}
