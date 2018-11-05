# Behemoth-Rank-Coding
Fast and Strong Burrows Wheeler Model with embedded rle-0

BRC is a very careful implementation of non-sequential move to front coding, the encoder is fully vectorized and decoder is partially vectorized. 

BRC acheives compression rates on par with QLFC when paired with an order-0 entropy coder, and BRC can operate in parallel via OpenMP.

Here's some numbers from BRC on the enwik9.bwt test file, tests were run on an i7-7700HQ @ 3.5Ghz.

Note: time to compress via fpaqc and FSE (32KB blocks) is not included, these are purely timings of the rank transforms themselves.

Implementation         | Encode speed | Decode speed| Compressed size (via fpaqc)| Compressed size (via FSE) |
-----------------------|--------------|-------------|---------------------------|----------------------------
BRC4_AVX 16-threads    | 483 MB/s     | 548 MB/s    | 168,556,196 bytes         | 178,988,019 bytes          |
BRC4_AVX 8-threads     | 456 MB/s     | 486 MB/s    | 168,556,196 bytes         | 178,988,019 bytes          |
BRC4_AVX 4-threads     | 303 MB/s     | 316 MB/s    | 168,556,196 bytes         | 178,988,019 bytes          |
BRC4_AVX 1-thread      |  99 MB/s     | 108 MB/s    | 168,556,196 bytes         | 178,988,019 bytes          |
BRC3_AVX 16-threads    | 545 MB/s     | 512 MB/s    | 170,928,089 bytes         | 185,281,681 bytes          |
BRC3_AVX 8-threads     | 523 MB/s     | 493 MB/s    | 170,928,089 bytes         | 185,281,681 bytes          |
BRC3_AVX 4-threads     | 352 MB/s     | 294 MB/s    | 170,928,089 bytes         | 185,281,681 bytes          |
BRC3_AVX 1-thread      | 108 MB/s     | 93 MB/s     | 170,928,089 bytes         | 185,281,681 bytes          |
BRC2_AVX 16-threads    | 696 MB/s     | 760 MB/s    | 171,033,530 bytes         | 195,116,241 bytes          |
BRC2_AVX 8-threads     | 660 MB/s     | 685 MB/s    | 171,033,530 bytes         | 195,116,241 bytes          |
BRC2_AVX 4-threads     | 423 MB/s     | 435 MB/s    | 171,033,530 bytes         | 195,116,241 bytes          |
BRC2_AVX 1-thread      | 145 MB/s     | 148 MB/s    | 171,033,530 bytes         | 195,116,241 bytes          |

BRC2 used purely rank coding, BRC3 used rank coding hybridized with a byte aligned structured model, BRC4 uses rank coding and byte aligned rle-0. BRC4 attains the highest ratios of all 3 recorded versions, while being the second fastest transform.
