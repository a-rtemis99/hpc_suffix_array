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

    // L'inizializzazione è veloce, quindi viene replicata su tutti i processi
    for (int i = 0; i < n; i++) {
        suffixes[i].index = i;
        suffixes[i].rank[0] = sa->str[i];
        suffixes[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
    }

    int suffix_size_bytes = sizeof(Suffix);

    // Buffer per Allgatherv (solo per il rank 0, per compatibilità con l'interfaccia)
    // Nella nuova logica, non avremo un buffer di raccolta separato.
    // Lavoreremo "in place" sull'array suffixes di ogni processo.

    // Array per i conteggi e gli spostamenti, necessari per Allgatherv
    int* recvcounts_bytes = (int*)malloc(size * sizeof(int));
    int* displs_bytes = (int*)malloc(size * sizeof(int));
    assert(recvcounts_bytes != NULL && displs_bytes != NULL);
    
    int chunk_size_structs = n / size;
    int remainder = n % size;
    int current_displ_bytes = 0;
    for (int i = 0; i < size; i++) {
        int structs_for_proc = chunk_size_structs + (i < remainder ? 1 : 0);
        recvcounts_bytes[i] = structs_for_proc * suffix_size_bytes;
        displs_bytes[i] = current_displ_bytes;
        current_displ_bytes += recvcounts_bytes[i];
    }

    for (int k = 2; k < n; k *= 2) {
        // ---- 1. Ordinamento Locale ----
        // Ogni processo ordina la sua fetta logica dell'array 'suffixes'.
        int local_n_structs = recvcounts_bytes[rank] / suffix_size_bytes;
        int start_index_bytes = displs_bytes[rank];
        qsort((char*)suffixes + start_index_bytes, local_n_structs, suffix_size_bytes, compare_suffix_mpi);
        
        // ---- 2. OTTIMIZZAZIONE: Scambio Globale con Allgatherv ----
        // Ogni processo invia la sua fetta ordinata a TUTTI gli altri
        // e riceve le fette da TUTTI gli altri, ricostruendo l'array completo.
        // In questo modo, il rank 0 non è più un bottleneck.
        MPI_Allgatherv((char*)suffixes + start_index_bytes, local_n_structs * suffix_size_bytes, MPI_BYTE,
                       suffixes, recvcounts_bytes, displs_bytes, MPI_BYTE, MPI_COMM_WORLD);
        
        // ---- 3. Fusione (Merge) e Calcolo Rank (IN PARALLELO!) ----
        // Ora OGNI processo ha l'array completo con blocchi ordinati e può fare il merge.
        qsort(suffixes, n, suffix_size_bytes, compare_suffix_mpi);

        // E OGNI processo può calcolare il nuovo rank_array.
        int current_rank = 0;
        rank_array[suffixes[0].index] = current_rank;
        for (int i = 1; i < n; i++) {
            if (suffixes[i].rank[0] != suffixes[i-1].rank[0] ||
                suffixes[i].rank[1] != suffixes[i-1].rank[1]) {
                current_rank++;
            }
            rank_array[suffixes[i].index] = current_rank;
        }

        // ---- 4. Aggiornamento dei Rank (IN PARALLELO) ----
        // Nessun Bcast necessario! Tutti hanno già il rank_array.
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
    // Solo il rank 0 deve popolare la struttura dati finale per l'output.
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = suffixes[i].index;
        }
    }

    free(suffixes);
    free(rank_array);
    free(recvcounts_bytes);
    free(displs_bytes);
}