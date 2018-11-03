# Behemoth-Rank-Coding
Fast and Strong Burrows Wheeler Model with embedded rank model

BRC is a very careful implementation of non-sequential move to front coding, the encoder is fully vectorized and decoder is partially vectorized. 

BRC acheives compression rates on par with QLFC when paired with an order-0 entropy coder, and BRC can operate in parallel via OpenMP.

Note: time to compress via fpaqc and FSE is not included, these are purely timings of the rank transforms themselves.

Implementation         | Encode speed | Decode speed| Compressed size (via fpaqc)| Compressed size (via FSE) |
-----------------------|--------------|-------------|---------------------------|----------------------------
BRC3_AVX 16-threads    | 545 MB/s     | 512 MB/s    | 170,928,089 bytes         | 185,281,681 bytes          |
BRC3_AVX 8-threads     | 523 MB/s     | 493 MB/s    | 170,928,089 bytes         | 185,281,681 bytes          |
BRC3_AVX 4-threads     | 352 MB/s     | 294 MB/s    | 170,928,089 bytes         | 185,281,681 bytes          |
BRC3_AVX 1-thread      | 108 MB/s     | 93 MB/s     | 170,928,089 bytes         | 185,281,681 bytes          |
BRC2_AVX 16-threads    | 696 MB/s     | 760 MB/s    | 171,033,530 bytes         | 195,116,241 bytes          |
BRC2_AVX 8-threads     | 660 MB/s     | 685 MB/s    | 171,033,530 bytes         | 195,116,241 bytes          |
BRC2_AVX 4-threads     | 423 MB/s     | 435 MB/s    | 171,033,530 bytes         | 195,116,241 bytes          |
BRC2_AVX 1-thread      | 145 MB/s     | 148 MB/s    | 171,033,530 bytes         | 195,116,241 bytes          |
MTF_GC70_AVX2x64       | 119 MB/s     | 159 MB/s    | 188,870,259 bytes         | 205,013,403 bytes          |
