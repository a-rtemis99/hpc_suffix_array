// src/sequential/main_sequential.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "../common/suffix_array.h"
#include "../common/utils.h"

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

// AGGIUNGI QUESTA FUNZIONE - OUTPUT STRUTTURATO PER BENCHMARK
void print_structured_results(const char* implementation, const char* filename, 
                             long file_size, double total_time, double sa_time, 
                             double lcp_time, int num_processes) {
    printf("\n===STRUCTURED_RESULTS===\n");
    printf("IMPLEMENTATION:%s\n", implementation);
    printf("FILENAME:%s\n", filename);
    printf("FILE_SIZE:%ld\n", file_size);
    printf("TOTAL_TIME:%.6f\n", total_time);
    printf("SA_TIME:%.6f\n", sa_time);
    printf("LCP_TIME:%.6f\n", lcp_time);
    printf("PROCESSES:%d\n", num_processes);
    printf("===END_RESULTS===\n\n");
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file_or_string>\n", argv[0]);
        printf("If argument contains '/' or '.', it's treated as a file\n");
        printf("Otherwise, it's treated as a direct string\n");
        return 1;
    }
    
    char* input_str;
    long n;
    const char* filename = argv[1];
    
    // Determina se l'argomento è un file o una stringa diretta
    if (strchr(argv[1], '/') != NULL || strchr(argv[1], '.') != NULL) {
        // È un file
        printf("Reading from file: %s\n", argv[1]);
        
        // Leggi il file di input
        input_str = read_file(argv[1], &n);
        if (!input_str) {
            fprintf(stderr, "Error: Failed to read input file\n");
            return 1;
        }
        
        // DEBUG: Stampa informazioni sul file letto
        printf("File read successfully: %s\n", argv[1]);
        printf("Actual string length: %ld\n", n);
        if (n < 100) {
            printf("Full content: \"%s\"\n", input_str);
        } else {
            print_first_chars(input_str, 50);
            print_last_chars(input_str, n, 50);
        }
        printf("\n");
        
    } else {
        // È una stringa diretta
        input_str = strdup(argv[1]);
        n = strlen(input_str);
        filename = "direct_string";
        
        printf("Input string: %s\n", input_str);
        printf("String length: %ld\n", n);
    }
    
    double start_time = get_time();
    
    // Crea il suffix array
    SuffixArray* sa = create_suffix_array(input_str, n);
    if (!sa) {
        printf("Error: Failed to create suffix array\n");
        free(input_str);
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
        print_first_suffixes(sa, (n < 10) ? n : 10);
        
        printf("\nLCP Array: [");
        for (int i = 0; i < sa->n && i < 20; i++) {
            printf("%d", sa->lcp[i]);
            if (i < sa->n - 1 && i < 19) printf(", ");
        }
        if (sa->n > 20) printf(", ...");
        printf("]\n");
    }
    
    // AGGIUNGI: OUTPUT STRUTTURATO PER BENCHMARK UNIFICATO
    print_structured_results("sequential", filename, n, 
                           end_time - start_time, 
                           mid_time - start_time,
                           end_time - mid_time, 1);
    
    // Cleanup
    free(lrs);
    destroy_suffix_array(sa);
    free(input_str);
    
    return 0;
}