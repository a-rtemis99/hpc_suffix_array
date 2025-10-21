// src/sequential/main_sequential.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> 
#include <unistd.h>  
#include "../common/suffix_array.h"
#include "../common/utils.h"

// Funzione per misurare il tempo (wall-clock)
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

// Funzione helper per stampare l'array SA (troncato per leggibilità)
void print_suffix_array(SuffixArray* sa) {
    printf("Suffix Array: [");
    for (int i = 0; i < sa->n && i < 20; i++) {
        printf("%d", sa->sa[i]);
        if (i < sa->n - 1 && i < 19) printf(", ");
    }
    if (sa->n > 20) printf(", ...");
    printf("]\n");
}

// Funzione helper per stampare i primi suffissi (troncati)
void print_first_suffixes(SuffixArray* sa, int count) {
    printf("First %d suffixes:\n", count);
    for (int i = 0; i < count && i < sa->n; i++) {
        printf("  SA[%d] = %d -> \"", i, sa->sa[i]);
        // Stampa solo i primi 30 caratteri del suffisso
        for (int j = sa->sa[i]; j < sa->n && j < sa->sa[i] + 30; j++) {
            printf("%c", sa->str[j]);
        }
        if (sa->n - sa->sa[i] > 30) printf("...");
        printf("\"\n");
    }
}

// Funzione per stampare l'output strutturato per gli script
void print_structured_results_for_script(long string_length, double sa_time, double lcp_time, double total_time) {
    printf("\n--- STRUCTURED_RESULTS ---\n");
    printf("ACTUAL_STRING_LENGTH:%ld\n", string_length);
    printf("MPI_PROCESSES:1\n"); // Per il sequenziale, i processi sono sempre 1
    printf("SA_TIME:%.6f\n", sa_time);
    printf("LCP_TIME:%.6f\n", lcp_time);
    printf("TOTAL_TIME:%.6f\n", total_time);
    printf("--- END_STRUCTURED_RESULTS ---\n");
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file_or_string>\n", argv[0]);
        fprintf(stderr, "If argument contains '/' or '.', it's treated as a file.\n");
        fprintf(stderr, "Otherwise, it's treated as a direct string.\n");
        return 1;
    }

    char* input_str = NULL;
    long n = 0;
    const char* filename = argv[1]; // Usato per l'output
    int input_is_file = 0; // Flag per gestire il free alla fine

    // Determina se l'argomento è un file o una stringa diretta
    if (strchr(argv[1], '/') != NULL || strchr(argv[1], '.') != NULL) {
        // È un file
        input_is_file = 1;
        printf("Reading from file: %s\n", argv[1]);

        // Leggi il file di input
        input_str = read_file(argv[1], &n);
        if (!input_str) {
            fprintf(stderr, "Error: Failed to read input file '%s'\n", argv[1]);
            return 1;
        }

        printf("File read successfully: %s (%ld bytes)\n", argv[1], n);
        if (n < 100) {
            printf("Full content: \"%s\"\n", input_str);
        } else {
            print_first_chars(input_str, 50);
            print_last_chars(input_str, n, 50);
        }
        printf("\n");

    } else {
        // È una stringa diretta
        input_is_file = 0; // La stringa originale è argv[1]
        input_str = argv[1]; // Punta direttamente all'argomento
        n = strlen(input_str);
        filename = "direct_string";

        printf("Input string: %s\n", input_str);
        printf("String length: %ld\n", n);
    }

    double start_time = get_time();

    // Crea la struttura SuffixArray (alloca memoria per sa, lcp, e copia str)
    SuffixArray* sa = create_suffix_array(input_str, n);
    if (!sa) {
        fprintf(stderr, "Error: Failed to create suffix array structure (memory allocation failed?)\n");
        if (input_is_file) free(input_str); // Libera solo se allocato da read_file
        return 1;
    }
    // NB: Da qui in poi, `sa->str` contiene la copia della stringa.
    // input_str (se letto da file) può essere liberato dopo create_suffix_array.
    if (input_is_file) {
        free(input_str); // Libera la memoria letta da file, ora abbiamo la copia in sa->str
        input_str = NULL;
    }


    // Costruisci il suffix array (logica in manber_myers.c)
    build_suffix_array(sa);
    double mid_time = get_time();

    // Costruisci LCP array
    build_lcp_array(sa);

    // Trova la sottostringa ripetuta più lunga
    char* lrs = find_longest_repeated_substring(sa);

    double end_time = get_time();

    // Validazione
    int valid = is_valid_suffix_array(sa);

    // Stampa risultati leggibili
    printf("\n=== RESULTS ===\n");
    printf("Valid suffix array: %s\n", valid ? "YES" : "NO");

    if (lrs) {
        printf("Longest repeated substring: '%s' (length: %zu)\n",
               lrs, strlen(lrs));
    } else {
        printf("No repeated substring found or string too short.\n");
    }

    printf("Suffix array construction time: %.6f seconds\n", mid_time - start_time);
    printf("LCP construction + LRS search time: %.6f seconds\n", end_time - mid_time);
    printf("Total execution time: %.6f seconds\n", end_time - start_time);

    // Stampa informazioni dettagliate per stringhe piccole
    if (n <= 100) {
        printf("\n=== DETAILED ANALYSIS (n<=100) ===\n");
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

    // Stampa risultati strutturati per gli script
    print_structured_results_for_script(n,
                                       mid_time - start_time,
                                       end_time - mid_time,
                                       end_time - start_time);

    // Cleanup
    free(lrs); // Libera la stringa LRS allocata da find_longest_repeated_substring
    destroy_suffix_array(sa); // Libera sa, lcp, e la copia di str interna a SuffixArray

    return 0;
}