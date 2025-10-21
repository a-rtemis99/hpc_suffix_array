#ifndef SUFFIX_ARRAY_H
#define SUFFIX_ARRAY_H

#include <stdio.h>

// Struttura usata internamente per l'ordinamento
typedef struct {
    int index;
    int rank[2];
} Suffix;

// Struttura principale esposta all'utente
typedef struct {
    char* str;    // Stringa originale (solo rank 0 la alloca da file)
    int n;        // Lunghezza della stringa
    int* sa;      // Suffix array finale (risultato)
    int* lcp;     // Array LCP (calcolato sequenzialmente alla fine)
} SuffixArray;

// --- Prototipi delle funzioni ---

// Funzioni principali (definite in manber_myers.c o manber_myers_mpi.c)
SuffixArray* create_suffix_array(const char* S, int n);
void destroy_suffix_array(SuffixArray* sa);
void build_suffix_array(SuffixArray* sa); // Versione sequenziale
void build_lcp_array(SuffixArray* sa);
char* find_longest_repeated_substring(SuffixArray* sa);
int is_valid_suffix_array(SuffixArray* sa);

// Funzione MPI (definita in manber_myers_mpi.c)
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size);

#endif