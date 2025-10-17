// src/mpi/manber_myers_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

int compare_suffix_mpi(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;
    Suffix* suffixes = NULL;

    if (rank == 0) {
        suffixes = (Suffix*)malloc(n * sizeof(Suffix));
        for (int i = 0; i < n; i++) {
            suffixes[i].index = i;
            suffixes[i].rank[0] = sa->str[i];
            suffixes[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
        }
    }

    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array != NULL);

    int suffix_size_bytes = sizeof(Suffix);
    
    // Calcolo della suddivisione (fatto una volta)
    int* sendcounts_bytes = (int*)malloc(size * sizeof(int));
    int* displs_bytes = (int*)malloc(size * sizeof(int));
    assert(sendcounts_bytes != NULL && displs_bytes != NULL);
    
    int chunk_size_structs = n / size;
    int remainder = n % size;
    int current_displ_bytes = 0;
    for (int i = 0; i < size; i++) {
        int structs_for_proc = chunk_size_structs + (i < remainder ? 1 : 0);
        sendcounts_bytes[i] = structs_for_proc * suffix_size_bytes;
        displs_bytes[i] = current_displ_bytes;
        current_displ_bytes += sendcounts_bytes[i];
    }
    
    int local_n_bytes = sendcounts_bytes[rank];
    int local_n_structs = local_n_bytes / suffix_size_bytes;
    Suffix* local_suffixes = (Suffix*)malloc(local_n_bytes);
    assert(local_suffixes != NULL);

    for (int k = 2; k < n; k *= 2) {
        // 1. Distribuzione dei dati a ogni iterazione
        MPI_Scatterv(suffixes, sendcounts_bytes, displs_bytes, MPI_BYTE,
                     local_suffixes, local_n_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
        
        // 2. Ordinamento Locale (lavoro parallelo)
        qsort(local_suffixes, local_n_structs, suffix_size_bytes, compare_suffix_mpi);
        
        // 3. Raccolta sul Root
        MPI_Gatherv(local_suffixes, local_n_bytes, MPI_BYTE,
                    suffixes, sendcounts_bytes, displs_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
        
        // 4. Lavoro solo sul Root: Merge e calcolo Rank
        if (rank == 0) {
            qsort(suffixes, n, suffix_size_bytes, compare_suffix_mpi);

            int current_rank = 0;
            rank_array[suffixes[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                if (suffixes[i].rank[0] != suffixes[i-1].rank[0] ||
                    suffixes[i].rank[1] != suffixes[i-1].rank[1]) {
                    current_rank++;
                }
                rank_array[suffixes[i].index] = current_rank;
            }
        }
        
        // 5. OTTIMIZZAZIONE CHIAVE: Broadcast del solo rank_array
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);
        
        // 6. Aggiornamento per il prossimo ciclo (fatto in parallelo su root)
        // Solo il root prepara il buffer `suffixes` per la prossima iterazione.
        // Gli altri processi lo riceveranno tramite Scatterv.
        if (rank == 0) {
            for (int i = 0; i < n; i++) {
                int next_index = suffixes[i].index + k;
                suffixes[i].rank[0] = rank_array[suffixes[i].index];
                if (next_index < n) {
                    suffixes[i].rank[1] = rank_array[next_index];
                } else {
                    suffixes[i].rank[1] = -1;
                }
            }
        }
    }
    
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = suffixes[i].index;
        }
    }

    if(rank == 0) free(suffixes);
    free(rank_array);
    free(sendcounts_bytes);
    free(displs_bytes);
    free(local_suffixes);
}