// src/mpi/main_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h> // Aggiunto per assert
#include "../common/suffix_array.h"
#include "../common/utils.h"

// Prototipo della funzione parallela (definita in manber_myers_mpi.c)
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size);
// Prototipo della funzione sequenziale (definita in manber_myers.c)
void build_suffix_array(SuffixArray* sa);
// Prototipo LCP/LRS (definite in manber_myers.c)
void build_lcp_array(SuffixArray* sa);
char* find_longest_repeated_substring(SuffixArray* sa);
int is_valid_suffix_array(SuffixArray* sa);


int main(int argc, char* argv[]) {
    // ---- Inizializzazione MPI ----
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char* input_str_original = NULL; // Rinominato per chiarezza
    long n = 0;
    double start_time, end_time, mid_time;

    // ---- Il processo Root (rank 0) gestisce l'input ----
    if (rank == 0) {
        if (argc != 2) {
            fprintf(stderr, "Usage: mpirun -np <num_procs> %s <input_file>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1); // Usa MPI_Abort per terminare tutti i processi
        }
        printf("Reading from file: %s\n", argv[1]);
        input_str_original = read_file(argv[1], &n); // Salva in variabile temporanea
        if (!input_str_original) {
            fprintf(stderr, "Error: Failed to read input file\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        printf("File read successfully. String length: %ld\n", n);
    }

    // ---- Trasmissione (Broadcast) dei dati a tutti i processi ----
    start_time = MPI_Wtime(); // Inizia a misurare il tempo dopo la lettura file

    // 1. Trasmetti la lunghezza della stringa
    MPI_Bcast(&n, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    // 2. Alloca memoria per il buffer di ricezione su tutti i processi
    char* str_buffer = (char*)malloc((n + 1) * sizeof(char));
    assert(str_buffer != NULL);
    if (rank == 0) {
        // Il root copia la stringa letta nel buffer da trasmettere
        strncpy(str_buffer, input_str_original, n + 1);
        // Libera la memoria originale letta da file SOLO DOPO LA COPIA
        free(input_str_original);
        input_str_original = NULL; // Per sicurezza
    }

    // 3. Trasmetti la stringa usando str_buffer
    MPI_Bcast(str_buffer, n + 1, MPI_CHAR, 0, MPI_COMM_WORLD);

    // ---- Esecuzione parallela ----
    // Tutti i processi creano la struttura SA usando la stringa ricevuta nel buffer
    SuffixArray* sa = create_suffix_array(str_buffer, n);
    if (!sa) {
        fprintf(stderr, "Error: Failed to create suffix array on rank %d\n", rank);
        free(str_buffer); // Libera il buffer prima di abortire
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    // create_suffix_array fa una copia interna di str_buffer in sa->str.
    // Possiamo liberare str_buffer qui perché non serve più.
    free(str_buffer);
    str_buffer = NULL;


    // Funzione parallela per costruire il Suffix Array
    build_suffix_array_mpi(sa, rank, size);

    mid_time = MPI_Wtime();

    // ---- Il processo Root finalizza il calcolo e stampa i risultati ----
    if (rank == 0) {
        // Le fasi successive (LCP e LRS) rimangono sequenziali sul root
        build_lcp_array(sa);
        char* lrs = find_longest_repeated_substring(sa);
        end_time = MPI_Wtime();

        double sa_construction_time = mid_time - start_time;
        double lcp_search_time = end_time - mid_time;
        double total_execution_time = end_time - start_time;

        int valid = is_valid_suffix_array(sa);

        printf("\n--- RESULTS ---\n");
        printf("Valid suffix array: %s\n", valid ? "YES" : "NO");
        if (lrs) {
            printf("Longest repeated substring: '%s' (length: %zu)\n", lrs, strlen(lrs));
        } else {
            printf("No repeated substring found or string too short.\n");
        }
        printf("Suffix array construction time (MPI): %.6f seconds\n", sa_construction_time);
        printf("LCP construction + LRS search time: %.6f seconds\n", lcp_search_time);
        printf("Total execution time: %.6f seconds\n", total_execution_time);

        printf("\n--- STRUCTURED_RESULTS ---\n");
        printf("ACTUAL_STRING_LENGTH:%ld\n", n);
        printf("MPI_PROCESSES:%d\n", size);
        printf("SA_TIME:%.6f\n", sa_construction_time);
        printf("LCP_TIME:%.6f\n", lcp_search_time);
        printf("TOTAL_TIME:%.6f\n", total_execution_time);
        printf("--- END_STRUCTURED_RESULTS ---\n");

        free(lrs); // Libera la stringa LRS solo sul root
    }

    // ---- Cleanup ----
    // destroy_suffix_array libera sa->str (la copia), sa->sa, sa->lcp, e sa stesso.
    // Deve essere chiamata da TUTTI i processi.
    destroy_suffix_array(sa);

    MPI_Finalize();
    return 0;
}