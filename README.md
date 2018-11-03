# Behemoth-Rank-Coding
Fast and Strong Burrows Wheeler Model with embedded rank model

BRC is a very careful implementation of non-sequential move to front coding, the encoder is fully vectorized and decoder is partially vectorized. 

BRC acheives compression rates on par with QLFC when paired with an order-0 entropy coder, and BRC can operate in parallel via OpenMP.

Note: time to compress via FSE is not included, these are purely timings of the rank transforms themselves.

Implementation         | Encode speed | Decode speed| Compressed size (via FSE)|
-----------------------|--------------|-------------|---------------------------|
BRCv3_AVX 16-threads     | 545 MB/s     | 512 MB/s    | 185,281,681 bytes         |
BRCv3_AVX 8-threads      | 523 MB/s     | 493 MB/s    | 185,281,681 bytes         |
BRCv3_AVX 4-threads      | 352 MB/s     | 294 MB/s    | 185,281,681 bytes         |
BRCv3_AVX 1-thread       | 108 MB/s     | 93 MB/s    | 185,281,681 bytes         |
