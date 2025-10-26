#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

// Prototipo della funzione sequenziale (che si trova in manber_myers.c)
void build_suffix_array(SuffixArray* sa);

// Funzione di confronto per qsort (usata sia localmente che sul master)
int compare_suffixes(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        if (s1->rank[1] == s2->rank[1]) return 0;
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

// Funzione principale MPI - Master/Worker con qsort sul master
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;

    // STRATEGIA IBRIDA: per file piccoli (< 5MB), esegui sequenziale
    if (n < 5000000) {
        if (rank == 0) {
            build_suffix_array(sa); // Usa la versione sequenziale con qsort
        }
         // Assicurati che tutti i processi abbiano sa->sa allocato per ricevere il Bcast
        if(rank != 0) {
            if (sa->sa == NULL) { // Solo se non già allocato
                 sa->sa = (int*)malloc(n * sizeof(int));
                 assert(sa->sa != NULL);
            }
        }
        MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);
        return;
    }

    // Creazione MPI Datatype per Suffix
    MPI_Datatype suffix_mpi_type;
    int blocklengths[2] = {1, 2};
    MPI_Aint displacements[2];
    MPI_Datatype types[2] = {MPI_INT, MPI_INT};
    Suffix s_temp = {0, {0, 0}}; // Inizializzata per warning
    MPI_Get_address(&s_temp.index, &displacements[0]);
    MPI_Get_address(&s_temp.rank[0], &displacements[1]);
    displacements[1] = displacements[1] - displacements[0]; displacements[0] = 0;
    MPI_Type_create_struct(2, blocklengths, displacements, types, &suffix_mpi_type);
    MPI_Type_commit(&suffix_mpi_type);

    // Calcolo distribuzione
    int base_chunk = n / size;
    int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0); // Num struct locali

    Suffix* local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix));
    assert(local_suffixes != NULL);

    // *** GESTIONE MEMORIA SEMPLICE E ROBUSTA ***
    // rank_array è allocato da TUTTI all'inizio
    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array != NULL);

    // Distribuzione iniziale
    if (rank == 0) {
        Suffix* all_suffixes_temp = (Suffix*)malloc(n * sizeof(Suffix));
        assert(all_suffixes_temp != NULL);
        for(int i=0; i<n; ++i) {
            all_suffixes_temp[i].index = i;
            all_suffixes_temp[i].rank[0] = (unsigned char)sa->str[i];
            all_suffixes_temp[i].rank[1] = (i + 1 < n) ? (unsigned char)sa->str[i + 1] : -1;
        }

        int* sendcounts_structs = (int*)malloc(size * sizeof(int));
        int* displs_structs = (int*)malloc(size * sizeof(int));
        assert(sendcounts_structs != NULL && displs_structs != NULL);
        for(int i=0; i<size; ++i) {
            sendcounts_structs[i] = base_chunk + (i < remainder ? 1 : 0);
            displs_structs[i] = i * base_chunk + (i < remainder ? i : remainder);
        }

        MPI_Scatterv(all_suffixes_temp, sendcounts_structs, displs_structs, suffix_mpi_type,
                     local_suffixes, local_n, suffix_mpi_type, 0, MPI_COMM_WORLD);

        free(all_suffixes_temp);
        free(sendcounts_structs);
        free(displs_structs);
    } else {
        MPI_Scatterv(NULL, NULL, NULL, suffix_mpi_type,
                     local_suffixes, local_n, suffix_mpi_type, 0, MPI_COMM_WORLD);
    }

    // Buffer globali allocati solo sul master (rank 0)
    Suffix* all_suffixes = NULL;
    int* recvcounts_structs = NULL;
    int* displs_structs = NULL;
    if (rank == 0) {
        all_suffixes = (Suffix*)malloc(n * sizeof(Suffix)); assert(all_suffixes != NULL);
        recvcounts_structs = (int*)malloc(size * sizeof(int)); assert(recvcounts_structs != NULL);
        displs_structs = (int*)malloc(size * sizeof(int)); assert(displs_structs != NULL);
    }

    MPI_Gather(&local_n, 1, MPI_INT, recvcounts_structs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        displs_structs[0] = 0;
        for (int i = 1; i < size; i++) {
            displs_structs[i] = displs_structs[i-1] + recvcounts_structs[i-1];
        }
    }

    int k; // Dichiarata fuori dal ciclo per visibilità dopo il break

    // Ciclo principale di Manber-Myers
    for (k = 4; k < 2 * n; k *= 2) {
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        MPI_Gatherv(local_suffixes, local_n, suffix_mpi_type,
                    all_suffixes, recvcounts_structs, displs_structs, suffix_mpi_type,
                    0, MPI_COMM_WORLD);

        int terminate = 0;
        if (rank == 0) {
            qsort(all_suffixes, n, sizeof(Suffix), compare_suffixes);

            // Scrive direttamente in rank_array (che è già allocato)
            int current_rank = 0;
            rank_array[all_suffixes[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                if (compare_suffixes(&all_suffixes[i], &all_suffixes[i-1]) != 0) {
                    current_rank++;
                }
                rank_array[all_suffixes[i].index] = current_rank;
            }

            if (current_rank == n - 1) terminate = 1;
        }

        MPI_Bcast(&terminate, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (terminate) break;

        // Tutti ricevono rank_array nel loro buffer già allocato
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);

        // Aggiornamento Locale
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index;
            int next_index = global_idx + k / 2;
            local_suffixes[i].rank[0] = rank_array[global_idx];
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }
        // *** NESSUN free(rank_array) DENTRO IL CICLO ***
    } // Fine del ciclo for

    // Finalizzazione
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = all_suffixes[i].index;
        }
    }

     if(rank != 0 && sa->sa == NULL) {
        sa->sa = (int*)malloc(n * sizeof(int));
        assert(sa->sa != NULL);
     }
    MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);

    // Cleanup
    MPI_Type_free(&suffix_mpi_type);
    if (rank == 0) {
        free(all_suffixes);
        free(recvcounts_structs);
        free(displs_structs);
    }
    free(local_suffixes);
    // Libera rank_array su TUTTI i processi, una sola volta
    free(rank_array);
}