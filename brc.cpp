/*
	BRC - Behemoth Rank Coding for BWT

	MIT License

	Copyright (c) 2018 Lucas Marsh

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "brc.hpp"

size_t brc_safe_buffer_size() {
	return BRC_BLOCK_HEADER_SIZE + (BRC_SUB_HEADER_SIZE * BRC_MAX_THREADS);
}

struct vmtf_s {
	unsigned char map[256];
};

inline void init_vmtf(vmtf_s * x) {
	for(size_t i = 0; i < 256; i++)
		x->map[i] = i;
}

inline void inverse_vmtf_update(vmtf_s * x, unsigned char q) {
	size_t j = 0;
	do x->map[j] = x->map[j + 1];
	while(++j < q);
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

int encode_brc_buffer_serial(unsigned char * src, unsigned char * dst, size_t src_size, size_t dst_size) {
	unsigned char * read_head  = src;
	unsigned char * write_head = dst + BRC_SUB_HEADER_SIZE;

	vmtf_s state;
	init_vmtf(&state);

	size_t bucket[256];
	unsigned char sort_map[256], s, r;
	uint32_t freqs[256] = {0};

	size_t unique_syms = 0;
	for (size_t i = 0; i < src_size; i++) {
		s = read_head[i];
		if (freqs[s] == 0) {
			state.map[s] = unique_syms;
			unique_syms++;
		}
		freqs[s]++;
	}

	memcpy(dst, freqs, BRC_SUB_HEADER_SIZE); 

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
	return EXIT_SUCCESS;
}

int decode_brc_buffer_serial(unsigned char * src, unsigned char * dst, size_t src_size, size_t dst_size) {
	unsigned char * read_head  = src + BRC_SUB_HEADER_SIZE;
	unsigned char * write_head = dst;

	vmtf_s state;
	init_vmtf(&state);

	size_t bucket[256], bucket_end[256];
	unsigned char sort_map[256], s, r;
	uint32_t freqs[256] = {0};

	memcpy(freqs, src, BRC_SUB_HEADER_SIZE); 

	size_t total = 0;
	for(size_t i = 0; i < 256; i++)
		total += freqs[i];
	if(total != dst_size) 
		return EXIT_FAILURE;
	
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
	
	for(size_t i = 0, s = state.map[0]; i < dst_size; i++)	{
		write_head[i] = s;
		if(bucket[s] < bucket_end[s]) {
			r = read_head[bucket[s]++];
			if(r) {
				inverse_vmtf_update(&state, r);
				state.map[r] = s;
				s = state.map[0];
			}
		}
		else if(unique_syms--) {
			inverse_vmtf_update(&state, unique_syms);
			s = state.map[0];
		}
	}
	return EXIT_SUCCESS;
}

int encode_brc_buffer_parallel(unsigned char * src, unsigned char * dst, size_t src_size, size_t dst_size, uint32_t num_threads) {
	if((dst_size - src_size) < brc_safe_buffer_size())
		return printf(" Allocation not large enough! \n"), EXIT_FAILURE;

	if(num_threads < 1) num_threads =  1;
	if(num_threads > BRC_MAX_THREADS) num_threads = BRC_MAX_THREADS;

	unsigned char * write_head = dst + BRC_BLOCK_HEADER_SIZE;
	uint32_t step = src_size / BRC_MAX_THREADS, magic = BRC_MAGIC | BRC_VERSION;

	memcpy(dst, &step, sizeof(uint32_t));
	memcpy(dst + sizeof(uint32_t), &magic, sizeof(uint32_t));

	#define BRC_ENCODE() {                                                            \
		size_t src_start_pos = step * i;                                              \
		size_t src_end_pos = __min(step * (i + 1), src_size);                         \
		size_t dst_start_pos = (BRC_SUB_HEADER_SIZE + step) * i;                      \
		size_t dst_end_pos = __min((BRC_SUB_HEADER_SIZE + step) * (i + 1), dst_size); \
		if(i == BRC_MAX_THREADS - 1)                                                  \
			src_end_pos = src_size;                                                   \
		errs[i] = encode_brc_buffer_serial(                                           \
			src + src_start_pos,                                                      \
			write_head + dst_start_pos,                                               \
			src_end_pos - src_start_pos,                                              \
			dst_end_pos - dst_start_pos                                               \
		);                                                                            \
	}
	

	int errs[BRC_MAX_THREADS];
	if(1) {
		#pragma omp parallel for num_threads(num_threads)
		for(size_t i = 0; i < BRC_MAX_THREADS; i++) {
			BRC_ENCODE();
		}
	} else {
		for(size_t i = 0; i < BRC_MAX_THREADS; i++) {
			BRC_ENCODE();
		}
	}

	int err = 0;
	for(size_t i = 0; i < BRC_MAX_THREADS; i++)
		err += errs[i];

	return EXIT_SUCCESS;
}

int decode_brc_buffer_parallel(unsigned char * src, unsigned char * dst, size_t src_size, size_t dst_size, uint32_t num_threads) {
	if((src_size - dst_size) < brc_safe_buffer_size())
		return printf(" Allocation not large enough! \n"), EXIT_FAILURE;

	if(num_threads <  1) num_threads =  1;
	if(num_threads > BRC_MAX_THREADS) num_threads = BRC_MAX_THREADS;

	unsigned char * read_head = src + BRC_BLOCK_HEADER_SIZE;
	uint32_t step, magic;
	memcpy(&step, src, sizeof(uint32_t));
	memcpy(&magic, src + sizeof(uint32_t), sizeof(uint32_t));

	if((magic & 0xffff0000) != BRC_MAGIC) return printf(" Data not in BRC format! \n"), EXIT_FAILURE;
	if((magic & 0x0000ffff) > BRC_VERSION) return printf(" Invalid BRC revision detected! \n"), EXIT_FAILURE;

	#define BRC_DECODE() {                                                            \
		size_t dst_start_pos = step * i;                                              \
		size_t dst_end_pos = __min(step * (i + 1), src_size);                         \
		size_t src_start_pos = (BRC_SUB_HEADER_SIZE + step) * i;                      \
		size_t src_end_pos = __min((BRC_SUB_HEADER_SIZE + step) * (i + 1), dst_size); \
		if(i == BRC_MAX_THREADS - 1)                                                  \
			dst_end_pos = dst_size;                                                   \
		errs[i] = decode_brc_buffer_serial(                                           \
			read_head + src_start_pos,                                                \
			dst + dst_start_pos,                                                      \
			src_end_pos - src_start_pos,                                              \
			dst_end_pos - dst_start_pos                                               \
		);                                                                            \
	}
	
	int errs[BRC_MAX_THREADS];
	if(num_threads > 1) {
		#pragma omp parallel for num_threads(num_threads)
		for(size_t i = 0; i < BRC_MAX_THREADS; i++) {
			BRC_DECODE();
		}
	} else {
		for(size_t i = 0; i < BRC_MAX_THREADS; i++) {
			BRC_DECODE();
		}
	}

	int err = 0;
	for(size_t i = 0; i < BRC_MAX_THREADS; i++)
		err += errs[i];

	return err;
}