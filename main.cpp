/*
	BRC - Behemoth Rank Coding for BWT
	MIT License
	Copyright (c) 2018 Lucas Marsh
	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "brc.hpp"
#include "common.hpp"

#define BUFFER_SIZE (1 << 20)

int encode_stream_serial(FILE * f_input, FILE * f_output) {
	unsigned char * buffer = (unsigned char*)malloc(BUFFER_SIZE);
	memset(buffer, 0, BUFFER_SIZE);
	if(!buffer) 
		return printf(" Failed to allocate input!  \n"), EXIT_FAILURE;

	brc_cxt_s brc_cxt;
	brc_init_cxt(&brc_cxt, BUFFER_SIZE);

	time_t start; 
	double cpu_time = 0;

	size_t bytes_read;
	size_t total_bytes_read = 0;
	size_t total_bytes_written = 0;
	while((bytes_read = fread(buffer, 1, BUFFER_SIZE, f_input)) > 0) {
		total_bytes_read += bytes_read;
		start = clock();

		if(brc_encode(&brc_cxt, buffer, bytes_read) == BRC_EXIT_FAILURE) 
			return printf(" Failed to encode input!  \n"), EXIT_FAILURE;

		cpu_time += ((double)clock() - (double)start) / CLOCKS_PER_SEC;
		fwrite(&brc_cxt.size, 1, sizeof(brc_cxt.size), f_output);
		fwrite(brc_cxt.block, 1, brc_cxt.size, f_output);
		total_bytes_written += brc_cxt.size;

		printf(" read %llu MB => %llu MB, cpu time = %.3f seconds, throughput = %.3f MB/s       \r", 
			(long long)(total_bytes_read / 1000000),
			(long long)(total_bytes_written / 1000000),
			cpu_time,
			((double)total_bytes_read /  1000000.f) / cpu_time
		);
	}

	printf(" read %llu MB => %llu MB, cpu time = %.3f seconds, throughput = %.3f MB/s       \n", 
		(long long)(total_bytes_read / 1000000),
		(long long)(total_bytes_written / 1000000),
		cpu_time,
		((double)total_bytes_read /  1000000.f) / cpu_time
	);

	free(buffer);
	brc_free_cxt(&brc_cxt);
	return EXIT_SUCCESS;
}

int decode_stream_serial(FILE * f_input, FILE * f_output) {
	unsigned char * buffer = (unsigned char*)malloc(BUFFER_SIZE);
	memset(buffer, 0, BUFFER_SIZE);
	if(!buffer) 
		return printf(" Failed to allocate output!  \n"), EXIT_FAILURE;

	brc_cxt_s brc_cxt;
	brc_init_cxt(&brc_cxt, BUFFER_SIZE);

	time_t start; 
	double cpu_time = 0;

	size_t bytes_read;
	size_t total_bytes_read = 0;
	size_t total_bytes_written = 0;
	while((bytes_read = fread(&brc_cxt.size, 1, sizeof(brc_cxt.size), f_input)) > 0) {
		total_bytes_read += bytes_read;

		if(brc_cxt.size > brc_cxt.eob) 
			return printf(" Read invalid data!  \n"), EXIT_FAILURE;

		bytes_read = fread(brc_cxt.block, 1, brc_cxt.size, f_input);
		total_bytes_read += bytes_read;

		size_t original_size;
		start = clock();

		if(brc_decode(&brc_cxt, buffer, &original_size) == BRC_EXIT_FAILURE) 
			return printf(" Failed to decode input!  \n"), EXIT_FAILURE;

		cpu_time += ((double)clock() - (double)start) / CLOCKS_PER_SEC;


		fwrite(buffer, 1, original_size, f_output);
		total_bytes_written += original_size;

		printf(" read %llu MB => %llu MB, cpu time = %.3f seconds, throughput = %.3f MB/s       \r", 
			(long long)(total_bytes_read / 1000000),
			(long long)(total_bytes_written / 1000000),
			cpu_time,
			((double)total_bytes_written /  1000000.f) / cpu_time
		);
	}

	printf(" read %llu MB => %llu MB, cpu time = %.3f seconds, throughput = %.3f MB/s       \n", 
		(long long)(total_bytes_read / 1000000),
		(long long)(total_bytes_written / 1000000),
		cpu_time,
		((double)total_bytes_written /  1000000.f) / cpu_time
	);

	free(buffer);
	brc_free_cxt(&brc_cxt);
	return EXIT_SUCCESS;
}

int encode_stream_parallel(FILE * f_input, FILE * f_output, int num_threads) {
	brc_cxt_s brc_cxt[num_threads];
	unsigned char * buffer[num_threads];

	for(size_t t = 0; t < num_threads; t++) {
		if(brc_init_cxt(&brc_cxt[t], BUFFER_SIZE) == BRC_EXIT_FAILURE)
			return printf(" Failed to allocate brc cxt!  \n"), EXIT_FAILURE;
		buffer[t] = (unsigned char*)malloc(BUFFER_SIZE);
		memset(buffer[t], 0, BUFFER_SIZE);
		if(!buffer[t]) 
			return printf(" Failed to allocate output!  \n"), EXIT_FAILURE;
	}

	time_t start; 
	double cpu_time = 0;
	size_t bytes_read[num_threads] = {0};
	size_t total_bytes_read = 0, total_bytes_written = 0;
	int errs[num_threads] = {0};

	while(1) {
		int s = 0;
		bool end = false;
		for(; s < num_threads; s++) {
			bytes_read[s] = fread(buffer[s], 1, BUFFER_SIZE, f_input);
			if(bytes_read[s] == 0) {
				end = true;
				break;
			}
		}

		start = clock();

		#pragma omp parallel for num_threads(s)
		for(int i = 0; i < s; i++) {
			errs[i] = brc_encode(&brc_cxt[i], buffer[i], bytes_read[i]);
		}

		cpu_time += ((double)clock() - (double)start) / CLOCKS_PER_SEC;

		for(int i = 0; i < s; i++) {
			total_bytes_read += bytes_read[i];
			total_bytes_written += brc_cxt[i].size + sizeof(brc_cxt[s].size);
			fwrite(&brc_cxt[i].size, 1, sizeof(brc_cxt[i].size), f_output);
			fwrite(brc_cxt[i].block, 1, brc_cxt[i].size, f_output);
		}

		printf(" read %llu MB => %llu MB, cpu time = %.3f seconds, throughput = %.3f MB/s       \r", 
			(long long)(total_bytes_read / 1000000),
			(long long)(total_bytes_written / 1000000),
			cpu_time,
			((double)total_bytes_read /  1000000.f) / cpu_time
		);


		if(end) break;
	}

	printf(" read %llu MB => %llu MB, cpu time = %.3f seconds, throughput = %.3f MB/s       \n", 
		(long long)(total_bytes_read / 1000000),
		(long long)(total_bytes_written / 1000000),
		cpu_time,
		((double)total_bytes_read /  1000000.f) / cpu_time
	);

	for(size_t t = 0; t < num_threads; t++) {
		free(buffer[t]);
		brc_free_cxt(&brc_cxt[t]);
	}
	return EXIT_SUCCESS;
}

int decode_stream_parallel(FILE * f_input, FILE * f_output, int num_threads) {
	brc_cxt_s brc_cxt[num_threads];
	unsigned char * buffer[num_threads];

	for(size_t t = 0; t < num_threads; t++) {
		if(brc_init_cxt(&brc_cxt[t], BUFFER_SIZE) == BRC_EXIT_FAILURE)
			return printf(" Failed to allocate brc cxt!  \n"), EXIT_FAILURE;
		buffer[t] = (unsigned char*)malloc(BUFFER_SIZE);
		memset(buffer[t], 0, BUFFER_SIZE);
		if(!buffer[t]) 
			return printf(" Failed to allocate output!  \n"), EXIT_FAILURE;
	}

	time_t start; 
	double cpu_time = 0;
	size_t bytes_read[num_threads] = {0};
	size_t original_sizes[num_threads] = {0};
	size_t total_bytes_read = 0, total_bytes_written = 0;
	int errs[num_threads] = {0};

	while(1) {
		int s = 0;
		bool end = false;
		for(; s < num_threads; s++) {
			fread(&brc_cxt[s].size, 1, sizeof(brc_cxt[s].size), f_input);
			if(brc_cxt[s].size > brc_cxt[s].eob) return printf(" Invalid input!  \n"), EXIT_FAILURE;
			bytes_read[s] = fread(brc_cxt[s].block, 1, brc_cxt[s].size, f_input);
			if(bytes_read[s] == 0) {
				end = true;
				break;
			}
		}

		start = clock();

		#pragma omp parallel for num_threads(s)
		for(int i = 0; i < s; i++) {
			errs[i] = brc_decode(&brc_cxt[i], buffer[i], &original_sizes[i]);
		}

		cpu_time += ((double)clock() - (double)start) / CLOCKS_PER_SEC;

		for(int i = 0; i < s; i++) {
			total_bytes_read += bytes_read[i] + sizeof(brc_cxt[s].size);
			total_bytes_written += original_sizes[i];
			fwrite(buffer[i], 1, original_sizes[i], f_output);
		}

		printf(" read %llu MB => %llu MB, cpu time = %.3f seconds, throughput = %.3f MB/s       \r", 
			(long long)(total_bytes_read / 1000000),
			(long long)(total_bytes_written / 1000000),
			cpu_time,
			((double)total_bytes_written /  1000000.f) / cpu_time
		);


		if(end) break;
	}

	printf(" read %llu MB => %llu MB, cpu time = %.3f seconds, throughput = %.3f MB/s       \n", 
		(long long)(total_bytes_read / 1000000),
		(long long)(total_bytes_written / 1000000),
		cpu_time,
		((double)total_bytes_written /  1000000.f) / cpu_time
	);

	for(size_t t = 0; t < num_threads; t++) {
		free(buffer[t]);
		brc_free_cxt(&brc_cxt[t]);
	}
	return EXIT_SUCCESS;
}

int main(int argc, char ** argv) {
	if(argc < 4) {
		printf(" BRC version %i - Behemoth Rank Coding for BWT \n\
 Lucas Marsh (c) 2018, MIT licensed \n\
 Usage:  brc.exe  <c|d>  input  output  num-threads\n\
 Arguments: \n\
    c : compress \n\
    d : decompress \n\
 Press 'enter' to continue", BRC_VERSION);
		getchar();
		return 0;
	}

	if(strcmp(argv[2], argv[3]) == 0) return perror(" Refusing to write to input, change the output directory! \n"), EXIT_FAILURE;

	FILE* f_input  = fopen(argv[2], "rb");
	FILE* f_output = fopen(argv[3], "wb");
	if (f_input  == NULL) return perror(argv[2]), 1;
	if (f_output == NULL) return perror(argv[3]), 1;

	int num_threads = 4;
	if(argc > 4) num_threads = atoi(argv[4]);

	switch(argv[1][0]) {
		case 'c': {
			if(num_threads > 1) {
				if(encode_stream_parallel(f_input, f_output, num_threads) != EXIT_SUCCESS)
					return printf(" Encoding failed!  \n"), EXIT_FAILURE;
			} else {
				if(encode_stream_serial(f_input, f_output) != EXIT_SUCCESS)
					return printf(" Encoding failed!  \n"), EXIT_FAILURE;
			}
		} break;
		case 'd': {
			if(num_threads > 1) {
				if(decode_stream_parallel(f_input, f_output, num_threads) != EXIT_SUCCESS)
					return printf(" Decoding failed!  \n"), EXIT_FAILURE;
			} else {
				if(decode_stream_serial(f_input, f_output) != EXIT_SUCCESS)
					return printf(" Decoding failed!  \n"), EXIT_FAILURE;
			}
		} break;
		default: printf(" Invalid argument!\n");
	}

	fclose(f_input);
	fclose(f_output);
	return EXIT_SUCCESS;
}
