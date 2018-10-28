/*
	BRC - Behemoth Rank Coding for BWT

	MIT License

	Copyright (c) 2018 Lucas Marsh

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

	_____________________________________

	Interface and reference implementaion

	Behemoth Rank Coding uses auto-vectorized move to front into frequency sorted buckets in parallel, modelling strength is stable regardlesss of the
	size of the chunk, so BRC can encode the BWT while streaming the output, and optionally doing it in parallel.

	BRC relies in full vectorization in the encoder, decoding is only partially vectorized.

	Nerdy stuff:
	register | max parallel lanes
	_____________________________
	ZMM0-31  | 32 parallel AVX512 lanes 
	YMM0-15  | 16 parallel AVX2 lanes
	XMM0-7   |  8 parallel SSE lanes
	BRC is limited to 16 threads to prevent over pressuring the registers when operating in parallel.

	On enwik9.bwt BRC encodes at 640MB/s using 8 threads and decodes at 610MB/s on 8 threads and acheives modeling ratios near optimal for BWT.

	BRC's modeling strength is about on par with QLFC, all you need to do is use any entropy coder you want on BRC's output.
*/
#include "brc.hpp"
#include "common.hpp"

/* moderate sized buffer for parallel operation */
#define STREAMING_BUFFER_SIZE (16 << 20)

#if 1

int encode_stream(FILE * f_input, FILE * f_output, int num_threads) {
	size_t src_size = STREAMING_BUFFER_SIZE;
	size_t dst_size = STREAMING_BUFFER_SIZE + brc_safe_buffer_size();
	unsigned char * src_buffer = (unsigned char*)malloc(src_size);
	unsigned char * dst_buffer = (unsigned char*)malloc(dst_size);
	memset(src_buffer, 0, src_size);
	memset(dst_buffer, 0, dst_size);
	if(!src_buffer) return printf(" Failed to allocate input!  \n"), EXIT_FAILURE;
	if(!dst_buffer) return printf(" Failed to allocate output! \n"), EXIT_FAILURE;

	time_t start; 
	double cpu_time = 0;

	size_t bytes_read;
	size_t total_bytes_read = 0;
	while((bytes_read = fread(src_buffer, 1, src_size, f_input)) > 0) {

		start = clock();

		size_t output_size = bytes_read + brc_safe_buffer_size();

		total_bytes_read += bytes_read;

		if(encode_brc_buffer_parallel(
			src_buffer, 
			dst_buffer, 
			bytes_read, 
			output_size,
			num_threads
			) != EXIT_SUCCESS)
				return printf(" Failed to decode input! \n"), EXIT_FAILURE;

		cpu_time += ((double)clock() - (double)start) / CLOCKS_PER_SEC;

		fwrite(dst_buffer, 1, output_size, f_output);
	}

	printf(" cpu time = %.3f seconds, throughput = %.3f MB/s\n", 
		cpu_time,
		((double)total_bytes_read /  1000000.f) / cpu_time
	);

	free(src_buffer);
	free(dst_buffer);
}

int decode_stream(FILE * f_input, FILE * f_output, int num_threads) {
	size_t src_size = STREAMING_BUFFER_SIZE + brc_safe_buffer_size();
	size_t dst_size = STREAMING_BUFFER_SIZE;
	unsigned char * src_buffer = (unsigned char*)malloc(src_size);
	unsigned char * dst_buffer = (unsigned char*)malloc(dst_size);
	memset(src_buffer, 0, src_size);
	memset(dst_buffer, 0, dst_size);
	if(!src_buffer) return printf(" Failed to allocate input!  \n"), EXIT_FAILURE;
	if(!dst_buffer) return printf(" Failed to allocate output! \n"), EXIT_FAILURE;

	time_t start; 
	double cpu_time = 0;

	size_t bytes_read;
	size_t total_bytes_read = 0;
	while((bytes_read = fread(src_buffer, 1, src_size, f_input)) > 0) {
		start = clock();

		size_t output_size = bytes_read - brc_safe_buffer_size();

		total_bytes_read += bytes_read;

		if(decode_brc_buffer_parallel(
			src_buffer, 
			dst_buffer, 
			bytes_read, 
			output_size,
			num_threads
			) != EXIT_SUCCESS)
				return printf(" Failed to decode output! \n"), EXIT_FAILURE;

		cpu_time += ((double)clock() - (double)start) / CLOCKS_PER_SEC;

		fwrite(dst_buffer, 1, output_size, f_output);
	}

	printf(" cpu time = %.3f seconds, throughput = %.3f MB/s\n", 
		cpu_time,
		((double)total_bytes_read /  1000000.f) / cpu_time
	);

	free(src_buffer);
	free(dst_buffer);
}
#else 
int encode_file(FILE * f_input, FILE * f_output, int num_threads) {
	size_t src_size = get_file_size(f_input);
	size_t dst_size = src_size + brc_safe_buffer_size(); // The whole safe buffer size will be utilized
	unsigned char * src_buffer = (unsigned char*)malloc(src_size);
	unsigned char * dst_buffer = (unsigned char*)malloc(dst_size);
	memset(src_buffer, 0, src_size);
	memset(dst_buffer, 0, dst_size);
	if(!src_buffer) return printf(" Failed to allocate input!  \n"), EXIT_FAILURE;
	if(!dst_buffer) return printf(" Failed to allocate output! \n"), EXIT_FAILURE;
	fread(src_buffer, 1, src_size, f_input);

	time_t start; start = clock();

	if(encode_brc_buffer_parallel(src_buffer, dst_buffer, src_size, dst_size, num_threads) != EXIT_SUCCESS)
		return printf(" Failed to decode input! \n"), EXIT_FAILURE;
		
	double cpu_time = ((double)clock() - (double)start) / CLOCKS_PER_SEC;
	printf(" cpu time = %.3f seconds, throughput = %.3f MB/s\n", 
		cpu_time,
		((double)src_size /  1000000.f) / cpu_time
	);

	fwrite(dst_buffer, 1, dst_size, f_output);
	free(src_buffer);
	free(dst_buffer);
}

int decode_file(FILE * f_input, FILE * f_output, int num_threads) {
	size_t src_size = get_file_size(f_input);
	size_t dst_size = src_size - brc_safe_buffer_size();
	unsigned char * src_buffer = (unsigned char*)malloc(src_size);
	unsigned char * dst_buffer = (unsigned char*)malloc(dst_size);
	memset(src_buffer, 0, src_size);
	memset(dst_buffer, 0, dst_size);
	if(!src_buffer) return printf(" Failed to allocate input!  \n"), EXIT_FAILURE;
	if(!dst_buffer) return printf(" Failed to allocate output! \n"), EXIT_FAILURE;
	fread(src_buffer, 1, src_size, f_input);

	time_t start; start = clock();

	if(decode_brc_buffer_parallel(src_buffer, dst_buffer, src_size, dst_size, num_threads) != EXIT_SUCCESS)
		return printf(" Failed to decode input! \n"), EXIT_FAILURE;

	double cpu_time = ((double)clock() - (double)start) / CLOCKS_PER_SEC;
	printf(" cpu time = %.3f seconds, throughput = %.3f MB/s\n", 
		cpu_time,
		((double)dst_size /  1000000.f) / cpu_time
	);
	fwrite(dst_buffer, 1, dst_size, f_output);
	free(src_buffer);
	free(dst_buffer);
}
#endif

int main(int argc, char ** argv) {
	if(argc < 4) {
		printf(" BRC - Behemoth Rank Coding for BWT \n\
 Lucas Marsh (c) 2018, MIT licensed \n\
 Usage:  brc.exe  <c|d>  input  output  num-threads\n\
 Arguments: \n\
    c : compress \n\
    d : decompress \n\
 Press 'enter' to continue");
		getchar();
		return 0;
	}

	if(strcmp(argv[2], argv[3]) == 0) return perror(" Refusing to write to input, change the output directory! \n"), EXIT_FAILURE;

	FILE* f_input  = fopen(argv[2], "rb");
	FILE* f_output = fopen(argv[3], "wb");
	if (f_input  == NULL) return perror(argv[2]), 1;
	if (f_output == NULL) return perror(argv[3]), 1;

	int num_threads = 4;
	if(argc > 4)
		num_threads = atoi(argv[4]);

	switch(argv[1][0]) {
		case 'c': {
			int err = encode_stream(f_input, f_output, num_threads);
		} break;
		case 'd': {
			int err = decode_stream(f_input, f_output, num_threads);
		} break;
		default: printf(" Invalid argument!\n");
	}

	fclose(f_input);
	fclose(f_output);
	return EXIT_SUCCESS;
}