#ifndef SUFFIX_ARRAY_H
#define SUFFIX_ARRAY_H

#include <stddef.h>

// Struttura per Suffix Array basata sull'implementazione di riferimento
typedef struct {
    int* sa;           // Suffix Array
    int* rank;         // Array dei rank
    int* lcp;          // LCP Array
    size_t n;          // Lunghezza stringa
} SuffixArray;

// Funzioni dell'implementazione di riferimento
void build_suffix_array(const char* text, int n, int* sa);
void build_lcp_array(const char* text, int n, const int* sa, int* lcp);
int find_longest_repeated_substring(const char* text, int n, char* result);

// Funzioni di utilità
int is_valid_suffix_array(const char* text, int n, const int* sa);
void free_suffix_array(SuffixArray* sa);
void print_suffix_array(const char* text, int n, const int* sa);

#endif