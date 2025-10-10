// src/sequential/main_sequential.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "../common/suffix_array.h"

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

void print_suffix_array(SuffixArray* sa) {
    printf("Suffix Array: [");
    for (int i = 0; i < sa->n && i < 20; i++) {
        printf("%d", sa->sa[i]);
        if (i < sa->n - 1 && i < 19) printf(", ");
    }
    if (sa->n > 20) printf(", ...");
    printf("]\n");
}

void print_first_suffixes(SuffixArray* sa, int count) {
    printf("First %d suffixes:\n", count);
    for (int i = 0; i < count && i < sa->n; i++) {
        printf("SA[%d] = %d -> \"", i, sa->sa[i]);
        // Stampa solo i primi 30 caratteri del suffisso
        for (int j = sa->sa[i]; j < sa->n && j < sa->sa[i] + 30; j++) {
            printf("%c", sa->str[j]);
        }
        if (sa->n - sa->sa[i] > 30) printf("...");
        printf("\"\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_string>\n", argv[0]);
        return 1;
    }
    
    const char* text = argv[1];
    int n = strlen(text);
    
    printf("Input string: %s\n", text);
    printf("String length: %d\n", n);
    
    double start_time = get_time();
    
    // Crea il suffix array
    SuffixArray* sa = create_suffix_array(text, n);
    if (!sa) {
        printf("Error: Failed to create suffix array\n");
        return 1;
    }
    
    // Costruisci il suffix array
    build_suffix_array(sa);
    double mid_time = get_time();
    
    // Costruisci LCP array
    build_lcp_array(sa);
    
    // Trova la sottostringa ripetuta più lunga
    char* lrs = find_longest_repeated_substring(sa);
    
    double end_time = get_time();
    
    // Validazione
    int valid = is_valid_suffix_array(sa);
    
    printf("\n=== RESULTS ===\n");
    printf("Valid suffix array: %s\n", valid ? "YES" : "NO");
    
    if (lrs) {
        printf("Longest repeated substring: '%s' (length: %zu)\n", 
               lrs, strlen(lrs));
    } else {
        printf("No repeated substring found\n");
    }
    
    printf("Suffix array construction time: %.6f seconds\n", mid_time - start_time);
    printf("LCP construction + LRS search time: %.6f seconds\n", end_time - mid_time);
    printf("Total execution time: %.6f seconds\n", end_time - start_time);
    
    // Stampa informazioni dettagliate per stringhe piccole
    if (n <= 100) {
        printf("\n=== DETAILED ANALYSIS ===\n");
        print_suffix_array(sa);
        print_first_suffixes(sa, 10);
        
        printf("\nLCP Array: [");
        for (int i = 0; i < sa->n && i < 20; i++) {
            printf("%d", sa->lcp[i]);
            if (i < sa->n - 1 && i < 19) printf(", ");
        }
        if (sa->n > 20) printf(", ...");
        printf("]\n");
    }
    
    // Cleanup
    free(lrs);
    destroy_suffix_array(sa);
    
    return 0;
}