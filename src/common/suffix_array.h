// src/common/suffix_array.h
#ifndef SUFFIX_ARRAY_H
#define SUFFIX_ARRAY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int index;
    int rank[2];
} Suffix;

typedef struct {
    int* sa;       // Suffix Array
    int* lcp;      // Longest Common Prefix array
    int n;         // Length of the input string
    char* str;     // Original string
} SuffixArray;

// Function declarations - QUESTE SONO LE FUNZIONI IMPLEMENTATE IN manber_myers.c
SuffixArray* create_suffix_array(const char* S, int n);
void destroy_suffix_array(SuffixArray* sa);
void build_suffix_array(SuffixArray* sa);
void build_lcp_array(SuffixArray* sa);
char* find_longest_repeated_substring(SuffixArray* sa);
int is_valid_suffix_array(SuffixArray* sa);

#endif