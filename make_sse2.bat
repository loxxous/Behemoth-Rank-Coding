g++ -std=c++11 -Ofast -s -static -fopenmp -funroll-loops -ftree-vectorize -msse2 main.cpp brc.cpp -o brc_sse2
PAUSE