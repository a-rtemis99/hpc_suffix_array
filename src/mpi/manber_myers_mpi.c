// src/mpi/manber_myers_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

// Prototipo della funzione sequenziale (che si trova in manber_myers.c)
void build_suffix_array(SuffixArray* sa);

// Funzioni di supporto per Radix Sort
static inline int get_rank_val(int r) { return r + 1; }

void counting_sort_radix(Suffix* in, Suffix* out, int n, int rank_pass, int max_rank) {
    int* count = (int*)calloc(max_rank + 1, sizeof(int));
    assert(count != NULL);
    for (int i = 0; i < n; i++) count[get_rank_val(in[i].rank[rank_pass])]++;
    for (int i = 1; i <= max_rank; i++) count[i] += count[i - 1];
    for (int i = n - 1; i >= 0; i--) {
        int r_val = get_rank_val(in[i].rank[rank_pass]);
        out[count[r_val] - 1] = in[i];
        count[r_val]--;
    }
    free(count);
}

void radix_sort_suffixes(Suffix* suffixes, int n, int max_rank_val) {
    Suffix* temp_suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    assert(temp_suffixes != NULL);
    counting_sort_radix(suffixes, temp_suffixes, n, 1, max_rank_val);
    counting_sort_radix(temp_suffixes, suffixes, n, 0, max_rank_val);
    free(temp_suffixes);
}

// Funzione principale MPI ottimizzata
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;

    // STRATEGIA IBRIDA: per file piccoli, usa l'approccio sequenziale
    if (n < 1000000) { // Soglia: 1MB
        if (rank == 0) {
            build_suffix_array(sa); // Usa la versione sequenziale veloce
        }
        // Il rank 0 invia il risultato finale a tutti gli altri
        MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);
        return;
    }
    
    // Calcola la distribuzione del lavoro
    int base_chunk = n / size;
    int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0);
    int displ = rank * base_chunk + (rank < remainder ? rank : remainder);

    Suffix* local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix));
    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(local_suffixes && rank_array);

    // Inizializzazione parallela
    for (int i = 0; i < local_n; i++) {
        int global_idx = displ + i;
        local_suffixes[i].index = global_idx;
        local_suffixes[i].rank[0] = sa->str[global_idx];
        local_suffixes[i].rank[1] = (global_idx + 1 < n) ? sa->str[global_idx + 1] : -1;
    }
    
    Suffix* all_suffixes = NULL;
    int* recvcounts_bytes = NULL;
    int* displs_bytes = NULL;

    if (rank == 0) {
        all_suffixes = (Suffix*)malloc(n * sizeof(Suffix));
        recvcounts_bytes = (int*)malloc(size * sizeof(int));
        displs_bytes = (int*)malloc(size * sizeof(int));
    }

    // Calcolo dei conteggi e spostamenti in byte per Gatherv
    int local_n_bytes = local_n * sizeof(Suffix);
    MPI_Gather(&local_n_bytes, 1, MPI_INT, recvcounts_bytes, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        displs_bytes[0] = 0;
        for (int i = 1; i < size; i++) {
            displs_bytes[i] = displs_bytes[i-1] + recvcounts_bytes[i-1];
        }
    }
    
    int max_rank_value = 256;
    
    for (int k = 2; k < 2 * n; k *= 2) {
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);
        
        MPI_Gatherv(local_suffixes, local_n_bytes, MPI_BYTE,
                    all_suffixes, recvcounts_bytes, displs_bytes, MPI_BYTE,
                    0, MPI_COMM_WORLD);
        
        int terminate = 0;
        if (rank == 0) {
            radix_sort_suffixes(all_suffixes, n, max_rank_value + 1);
            
            int current_rank = 0;
            rank_array[all_suffixes[0].index] = current_rank;
            
            for (int i = 1; i < n; i++) {
                if (all_suffixes[i].rank[0] != all_suffixes[i-1].rank[0] ||
                    all_suffixes[i].rank[1] != all_suffixes[i-1].rank[1]) {
                    current_rank++;
                }
                rank_array[all_suffixes[i].index] = current_rank;
            }
            max_rank_value = current_rank;
            
            if (max_rank_value == n - 1) {
                terminate = 1;
            }
        }
        
        MPI_Bcast(&terminate, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (terminate) {
            break;
        }
        
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&max_rank_value, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index; // Usa l'indice salvato
            int next_index = global_idx + k;
            local_suffixes[i].rank[0] = rank_array[global_idx];
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }
    }
    
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = all_suffixes[i].index;
        }
        free(all_suffixes);
        free(recvcounts_bytes);
        free(displs_bytes);
    }
    
    free(local_suffixes);
    free(rank_array);
}