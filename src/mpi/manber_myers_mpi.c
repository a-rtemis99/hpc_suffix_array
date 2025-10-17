// src/mpi/manber_myers_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

int compare_suffixes(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

static inline int get_rank_val(int r) {
    return r + 1;
}

void counting_sort_radix(Suffix* in, Suffix* out, int n, int rank_pass, int max_rank) {
    int* count = (int*)calloc(max_rank + 1, sizeof(int));
    assert(count != NULL);

    for (int i = 0; i < n; i++) {
        count[get_rank_val(in[i].rank[rank_pass])]++;
    }
    
    for (int i = 1; i <= max_rank; i++) {
        count[i] += count[i - 1];
    }
    
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

void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;
    Suffix* suffixes_global = NULL; 

    if (rank == 0) {
        suffixes_global = (Suffix*)malloc(n * sizeof(Suffix));
        for (int i = 0; i < n; i++) {
            suffixes_global[i].index = i;
            // Rank iniziali basati sui caratteri
            suffixes_global[i].rank[0] = sa->str[i];
            suffixes_global[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
        }
    }

    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array != NULL);

    int suffix_size_bytes = sizeof(Suffix);
    
    int* counts_bytes = (int*)malloc(size * sizeof(int));
    int* displs_bytes = (int*)malloc(size * sizeof(int));
    
    int chunk_size_structs = n / size;
    int remainder = n % size;
    int current_displ_bytes = 0;
    for (int i = 0; i < size; i++) {
        int structs_for_proc = chunk_size_structs + (i < remainder ? 1 : 0);
        counts_bytes[i] = structs_for_proc * suffix_size_bytes;
        displs_bytes[i] = current_displ_bytes;
        current_displ_bytes += counts_bytes[i];
    }
    
    int local_n_bytes = counts_bytes[rank];
    int local_n_structs = local_n_bytes / suffix_size_bytes;
    Suffix* local_suffixes = (Suffix*)malloc(local_n_bytes > 0 ? local_n_bytes : 1);
    
    // ---- FASE 1: Distribuzione Iniziale (UNA SOLA VOLTA) ----
    MPI_Scatterv(suffixes_global, counts_bytes, displs_bytes, MPI_BYTE,
                 local_suffixes, local_n_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);

    int max_rank_value = 256; 

    for (int k = 2; k < 2 * n; k *= 2) {
        // ---- FASE 2: Ordinamento Locale ----
        qsort(local_suffixes, local_n_structs, suffix_size_bytes, compare_suffixes);
        
        // ---- FASE 3: Raccolta sul Root ----
        MPI_Gatherv(local_suffixes, local_n_bytes, MPI_BYTE,
                    suffixes_global, counts_bytes, displs_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
        
        if (rank == 0) {
            // ---- FASE 4: Merge e Calcolo Rank (solo sul Root) ----
            radix_sort_suffixes(suffixes_global, n, max_rank_value + 1);

            int current_rank = 0;
            rank_array[suffixes_global[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                if (suffixes_global[i].rank[0] != suffixes_global[i-1].rank[0] ||
                    suffixes_global[i].rank[1] != suffixes_global[i-1].rank[1]) {
                    current_rank++;
                }
                rank_array[suffixes_global[i].index] = current_rank;
            }
            max_rank_value = current_rank; 
        }
        
        // ---- FASE 5: Broadcast delle sole informazioni essenziali ----
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&max_rank_value, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        if (max_rank_value == n - 1) break;
        
        // ---- FASE 6: Aggiornamento dei rank (IN PARALLELO!) ----
        // Ogni processo aggiorna il proprio pezzo locale.
        for (int i = 0; i < local_n_structs; i++) {
            int next_index = local_suffixes[i].index + k;
            local_suffixes[i].rank[0] = rank_array[local_suffixes[i].index];
            if (next_index < n) {
                local_suffixes[i].rank[1] = rank_array[next_index];
            } else {
                local_suffixes[i].rank[1] = -1;
            }
        }
    }
    
    // L'ultimo Gatherv ha già portato il risultato finale sul root
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = suffixes_global[i].index;
        }
    }

    if(rank == 0) free(suffixes_global);
    free(rank_array);
    free(counts_bytes);
    free(displs_bytes);
    free(local_suffixes);
}