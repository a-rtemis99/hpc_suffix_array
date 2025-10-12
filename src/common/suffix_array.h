// src/common/suffix_array.h
#ifndef SUFFIX_ARRAY_H
#define SUFFIX_ARRAY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Struttura temporanea per l'ordinamento
typedef struct {
    int index;
    int rank[2];
} Suffix;

typedef struct {
    char* str;      // Stringa originale
    int n;          // Lunghezza della stringa
    int* sa;        // Suffix array
    int* lcp;       // Array LCP (Longest Common Prefix)
} SuffixArray;

// Funzioni principali
SuffixArray* create_suffix_array(const char* str, int n);
void destroy_suffix_array(SuffixArray* sa);
void build_suffix_array(SuffixArray* sa);
void build_lcp_array(SuffixArray* sa);
char* find_longest_repeated_substring(SuffixArray* sa);
int is_valid_suffix_array(SuffixArray* sa);

#endif