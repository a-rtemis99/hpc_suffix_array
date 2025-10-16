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

    // Tutti i processi allocano il buffer ----
    // Il root lo userà per i calcoli e l'invio, gli altri per ricevere i dati.
    Suffix* suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    assert(suffixes != NULL);

    // Solo il processo root INIZIALIZZA l'array con i dati di partenza.
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            suffixes[i].index = i;
            suffixes[i].rank[0] = sa->str[i];
            suffixes[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
        }
    }
    
    // Tutti i processi hanno bisogno del rank_array
    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array != NULL);

    int suffix_size_bytes = sizeof(Suffix);

    // ---- Ciclo di Raddoppio Parallelo ----
    for (int k = 2; k < n; k *= 2) {
        int* sendcounts_bytes = NULL;
        int* displs_bytes = NULL;
        int local_n_structs;

        if (rank == 0) {
            sendcounts_bytes = malloc(size * sizeof(int));
            displs_bytes = malloc(size * sizeof(int));
            
            int chunk_size_structs = n / size;
            int remainder = n % size;
            int current_displ_bytes = 0;

            for (int i = 0; i < size; i++) {
                int structs_for_proc = chunk_size_structs + (i < remainder ? 1 : 0);
                sendcounts_bytes[i] = structs_for_proc * suffix_size_bytes;
                displs_bytes[i] = current_displ_bytes;
                current_displ_bytes += sendcounts_bytes[i];
            }
        }

        int local_n_bytes;
        MPI_Scatter(sendcounts_bytes, 1, MPI_INT, &local_n_bytes, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        local_n_structs = local_n_bytes / suffix_size_bytes;
        
        Suffix* local_suffixes = (Suffix*)malloc(local_n_bytes);
        assert(local_suffixes != NULL);

        MPI_Scatterv(suffixes, sendcounts_bytes, displs_bytes, MPI_BYTE,
                     local_suffixes, local_n_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
        
        qsort(local_suffixes, local_n_structs, suffix_size_bytes, compare_suffix_mpi);

        MPI_Gatherv(local_suffixes, local_n_bytes, MPI_BYTE,
                    suffixes, sendcounts_bytes, displs_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
        
        free(local_suffixes);
        if (rank == 0) {
            free(sendcounts_bytes);
            free(displs_bytes);
        }

        if (rank == 0) {
            qsort(suffixes, n, suffix_size_bytes, compare_suffix_mpi);
        }

        MPI_Bcast(suffixes, n * suffix_size_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);

        int current_rank = 0;
        rank_array[suffixes[0].index] = current_rank;
        for (int i = 1; i < n; i++) {
            if (suffixes[i].rank[0] != suffixes[i-1].rank[0] ||
                suffixes[i].rank[1] != suffixes[i-1].rank[1]) {
                current_rank++;
            }
            rank_array[suffixes[i].index] = current_rank;
        }

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
    
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = suffixes[i].index;
        }
    }

    free(suffixes);
    free(rank_array);
}