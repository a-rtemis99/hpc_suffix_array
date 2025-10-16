// src/mpi/main_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/suffix_array.h"
#include "../common/utils.h"

// Prototipo della funzione parallela definita in manber_myers_mpi.c
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size);

int main(int argc, char* argv[]) {
    // ---- Inizializzazione MPI ----
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char* input_str = NULL;
    long n = 0;
    double start_time, end_time, mid_time;

    // ---- Il processo Root (rank 0) gestisce l'input ----
    if (rank == 0) {
        if (argc != 2) {
            fprintf(stderr, "Usage: mpirun -np <num_procs> %s <input_file>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        printf("Reading from file: %s\n", argv[1]);
        input_str = read_file(argv[1], &n);
        if (!input_str) {
            fprintf(stderr, "Error: Failed to read input file\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        printf("File read successfully. String length: %ld\n", n);
    }

    // ---- Trasmissione (Broadcast) dei dati a tutti i processi ----
    start_time = MPI_Wtime();

    // 1. Trasmetti la lunghezza della stringa
    MPI_Bcast(&n, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    // 2. Se non sei il root, alloca memoria per la stringa
    if (rank != 0) {
        input_str = (char*)malloc((n + 1) * sizeof(char));
    }

    // 3. Trasmetti la stringa
    MPI_Bcast(input_str, n + 1, MPI_CHAR, 0, MPI_COMM_WORLD);

    // ---- Esecuzione parallela ----
    SuffixArray* sa = create_suffix_array(input_str, n);
    if (!sa) {
        fprintf(stderr, "Error: Failed to create suffix array on rank %d\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Funzione parallela per costruire il Suffix Array
    build_suffix_array_mpi(sa, rank, size);

    mid_time = MPI_Wtime();

    // ---- Il processo Root finalizza il calcolo e stampa i risultati ----
    if (rank == 0) {
        // Le fasi successive (LCP e LRS) sono veloci e possono rimanere sequenziali
        build_lcp_array(sa);
        char* lrs = find_longest_repeated_substring(sa);
        end_time = MPI_Wtime();

        // Salva i tempi per l'output strutturato
        double sa_construction_time = mid_time - start_time;
        double lcp_search_time = end_time - mid_time;
        double total_execution_time = end_time - start_time;

        // Validazione
        int valid = is_valid_suffix_array(sa);

        // --- OUTPUT LEGGIBILE PER L'UTENTE ---
        printf("\n--- RESULTS ---\n");
        printf("Valid suffix array: %s\n", valid ? "YES" : "NO");
        if (lrs) {
            printf("Longest repeated substring: '%s' (length: %zu)\n", lrs, strlen(lrs));
        } else {
            printf("No repeated substring found\n");
        }
        printf("Suffix array construction time (MPI): %.6f seconds\n", sa_construction_time);
        printf("LCP construction + LRS search time: %.6f seconds\n", lcp_search_time);
        printf("Total execution time: %.6f seconds\n", total_execution_time);

        // --- OUTPUT STRUTTURATO PER LO SCRIPT PYTHON (NON CAMBIA L'OUTPUT UMANO) ---
        // Questo blocco è la chiave per far funzionare lo script di benchmark.
        printf("\n--- STRUCTURED_RESULTS ---\n");
        printf("ACTUAL_STRING_LENGTH:%ld\n", n);
        printf("MPI_PROCESSES:%d\n", size);
        printf("SA_TIME:%.6f\n", sa_construction_time);
        printf("LCP_TIME:%.6f\n", lcp_search_time);
        printf("TOTAL_TIME:%.6f\n", total_execution_time);
        printf("--- END_STRUCTURED_RESULTS ---\n");

        free(lrs);
    }

    // ---- Cleanup ----
    destroy_suffix_array(sa);
    if (rank != 0) {
      free(input_str);
    } else {
      // Il root ha una versione speciale di free per la stringa letta da file
      // che è gestita dentro la struct 'sa' e liberata da destroy_suffix_array
    }
    
    MPI_Finalize();
    return 0;
}