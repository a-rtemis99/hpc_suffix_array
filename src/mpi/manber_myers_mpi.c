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
    Suffix* suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    assert(suffixes != NULL);

    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array != NULL);

    // Inizializzazione fatta da tutti i processi, è veloce.
    for (int i = 0; i < n; i++) {
        suffixes[i].index = i;
        suffixes[i].rank[0] = sa->str[i];
        suffixes[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
    }

    int suffix_size_bytes = sizeof(Suffix);

    // Buffer temporaneo per la fusione sul root
    Suffix* gather_buffer = NULL;
    if (rank == 0) {
        gather_buffer = (Suffix*)malloc(n * sizeof(Suffix));
        assert(gather_buffer != NULL);
    }

    for (int k = 2; k < n; k *= 2) {
        // ---- 1. Suddivisione logica del lavoro, senza spostare dati ----
        int chunk_size_structs = n / size;
        int remainder = n % size;
        int local_n_structs = chunk_size_structs + (rank < remainder ? 1 : 0);
        int start_index = rank * chunk_size_structs + (rank < remainder ? rank : remainder);

        // ---- 2. Ordinamento Locale ----
        // Ogni processo ordina SOLO la sua fetta dell'array 'suffixes'
        qsort(suffixes + start_index, local_n_structs, suffix_size_bytes, compare_suffix_mpi);
        
        // ---- 3. Raccolta dei Risultati (Gather) ----
        // Prepariamo i conteggi e gli spostamenti per il Gatherv
        int* recvcounts_bytes = NULL;
        int* displs_bytes = NULL;

        if (rank == 0) {
            recvcounts_bytes = malloc(size * sizeof(int));
            displs_bytes = malloc(size * sizeof(int));
            int current_displ_bytes = 0;
            for (int i = 0; i < size; i++) {
                int structs_for_proc = chunk_size_structs + (i < remainder ? 1 : 0);
                recvcounts_bytes[i] = structs_for_proc * suffix_size_bytes;
                displs_bytes[i] = current_displ_bytes;
                current_displ_bytes += recvcounts_bytes[i];
            }
        }
        
        // Ogni processo invia la sua fetta ordinata al root
        MPI_Gatherv(suffixes + start_index, local_n_structs * suffix_size_bytes, MPI_BYTE,
                    gather_buffer, recvcounts_bytes, displs_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
        
        // ---- 4. Fusione (Merge) e Calcolo Rank (Tutto sul Root) ----
        if (rank == 0) {
            // Fusione: riordina l'array raccolto
            qsort(gather_buffer, n, suffix_size_bytes, compare_suffix_mpi);

            // Calcola il nuovo rank_array (solo il root lo fa)
            int current_rank = 0;
            rank_array[gather_buffer[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                if (gather_buffer[i].rank[0] != gather_buffer[i-1].rank[0] ||
                    gather_buffer[i].rank[1] != gather_buffer[i-1].rank[1]) {
                    current_rank++;
                }
                rank_array[gather_buffer[i].index] = current_rank;
            }
            
            free(recvcounts_bytes);
            free(displs_bytes);
        }

        // ---- 5. OTTIMIZZAZIONE: Broadcast del solo rank_array ----
        // Invece di 12*n byte, ora trasmettiamo solo 4*n byte.
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);

        // ---- 6. Aggiornamento dei Rank (in parallelo) ----
        // Tutti i processi aggiornano il proprio array 'suffixes' usando il nuovo rank_array
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
    
    // ---- Finalizzazione ----
    if (rank == 0) {
        // L'ultimo qsort sul root ha già ordinato `gather_buffer` correttamente
        for (int i = 0; i < n; i++) {
            sa->sa[i] = gather_buffer[i].index;
        }
        free(gather_buffer);
    }

    free(suffixes);
    free(rank_array);
}