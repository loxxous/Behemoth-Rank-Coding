g++ -std=c++11 -Ofast -s -static -fopenmp -funroll-loops -ftree-vectorize -mavx main.cpp brc.cpp -o brc_avx
PAUSE