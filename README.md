# Behemoth-Rank-Coding
Ultra Fast and Strong Burrows Wheeler Model

BRC is a very careful implementation of non-sequential move to front coding, the encoder is fully vectorized and decoder is partially vectorized. 

BRC acheives compression rates on par with QLFC when paired with an order-0 entropy coder, and has nice properties which allows compression to remain about the same regardless of the chunk size making streaming compression of BWT chunks viable.

BRC can also operate in parallel via OpenMP.

Here's some numbers from BRC versus gmtf_v2f by Eugene Shelwien on enwik9.bwt.
Note: time to compress via fpaqc is not included, these are purely timings of the rank transforms themselves.

Implementation         | Encode speed | Decode speed| Compressed size (via fpaqc)|
-----------------------|--------------|-------------|---------------------------|
BRC_AVX 8-threads      | 640 MB/s     | 610 MB/s    | 170,798,057 bytes         |
BRC_AVX 4-threads      | 390 MB/s     | 345 MB/s    | 170,795,044 bytes         |
BRC_AVX 1-thread       | 138 MB/s     | 137 MB/s    | 170,792,585 bytes         |
MTF_GC70_AVX2x64       | 119 MB/s     | 159 MB/s    | 188,870,259 bytes         |
