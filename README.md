# Behemoth-Rank-Coding
Fast and Strong Burrows Wheeler Model 

BRC is a very careful implementation of non-sequential move to front coding, the encoder is fully vectorized and decoder is partially vectorized. 

BRC acheives compression rates on par with QLFC when paired with an order-0 entropy coder, and BRC can operate in parallel via OpenMP.

Here's some numbers from BRC on the enwik9.bwt test file, tests were run on an i7-7700HQ @ 3.5Ghz.

Note: time to compress via fpaqc and FSE (32KB blocks) is not included, these are purely timings of the BRC transform itself.

Implementation         | Encode speed | Decode speed| Compressed size (via fpaqc)| Compressed size (via FSE) |
-----------------------|--------------|-------------|---------------------------|----------------------------
BRC4_AVX 16-threads    | 483 MB/s     | 548 MB/s    | 168,556,196 bytes         | 178,988,019 bytes          |
BRC4_AVX 8-threads     | 456 MB/s     | 486 MB/s    | 168,556,196 bytes         | 178,988,019 bytes          |
BRC4_AVX 4-threads     | 303 MB/s     | 316 MB/s    | 168,556,196 bytes         | 178,988,019 bytes          |
BRC4_AVX 1-thread      |  99 MB/s     | 108 MB/s    | 168,556,196 bytes         | 178,988,019 bytes          |
