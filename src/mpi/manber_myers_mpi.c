// src/mpi/manber_myers_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

void build_suffix_array(SuffixArray* sa);

int compare_suffixes(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        if (s1->rank[1] == s2->rank[1]) return 0;
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;

    if (n < 5000000) {
        if (rank == 0) build_suffix_array(sa);
        MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);
        return;
    }

    // --- NUOVA PARTE: Creazione del tipo di dato MPI per la struct Suffix ---
    MPI_Datatype suffix_mpi_type;
    int blocklengths[2] = {1, 2};
    MPI_Aint displacements[2];
    MPI_Datatype types[2] = {MPI_INT, MPI_INT};
    
    Suffix s_temp;
    MPI_Get_address(&s_temp.index, &displacements[0]);
    MPI_Get_address(&s_temp.rank, &displacements[1]);
    displacements[1] = displacements[1] - displacements[0];
    displacements[0] = 0;

    MPI_Type_create_struct(2, blocklengths, displacements, types, &suffix_mpi_type);
    MPI_Type_commit(&suffix_mpi_type);
    // --- FINE NUOVA PARTE ---

    int base_chunk = n / size;
    int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0);

    Suffix* local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix));
    assert(local_suffixes);

    if (rank == 0) {
        Suffix* all_suffixes_temp = (Suffix*)malloc(n * sizeof(Suffix));
        for(int i=0; i<n; ++i) {
            all_suffixes_temp[i].index = i;
            all_suffixes_temp[i].rank[0] = sa->str[i];
            all_suffixes_temp[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
        }

        int* sendcounts = (int*)malloc(size * sizeof(int));
        int* displs_scatter = (int*)malloc(size * sizeof(int));
        for(int i=0; i<size; ++i) {
            sendcounts[i] = base_chunk + (i < remainder ? 1 : 0); // Ora in numero di struct
            displs_scatter[i] = i * base_chunk + (i < remainder ? i : remainder); // Ora in numero di struct
        }
        
        // Ora usiamo il nostro tipo custom, non MPI_BYTE
        MPI_Scatterv(all_suffixes_temp, sendcounts, displs_scatter, suffix_mpi_type,
                     local_suffixes, local_n, suffix_mpi_type, 0, MPI_COMM_WORLD);
        
        free(all_suffixes_temp);
        free(sendcounts);
        free(displs_scatter);
    } else {
        MPI_Scatterv(NULL, NULL, NULL, suffix_mpi_type,
                     local_suffixes, local_n, suffix_mpi_type, 0, MPI_COMM_WORLD);
    }
    
    // ... (il resto del codice rimane quasi identico, ma dobbiamo adattare le chiamate MPI)
    // Per semplicità e robustezza, torniamo all'approccio con Radix Sort sul master,
    // che è più facile da debuggare e comunque molto performante.

    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array);

    Suffix* all_suffixes = NULL;
    int* recvcounts = NULL;
    int* displs = NULL;
    if (rank == 0) {
        all_suffixes = (Suffix*)malloc(n * sizeof(Suffix));
        recvcounts = (int*)malloc(size * sizeof(int));
        displs = (int*)malloc(size * sizeof(int));
    }

    MPI_Gather(&local_n, 1, MPI_INT, recvcounts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        displs[0] = 0;
        for (int i = 1; i < size; i++) {
            displs[i] = displs[i-1] + recvcounts[i-1];
        }
    }

    int max_rank_value = 256;
    
    for (int k = 2; k < 2 * n; k *= 2) {
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);
        
        MPI_Gatherv(local_suffixes, local_n, suffix_mpi_type,
                    all_suffixes, recvcounts, displs, suffix_mpi_type,
                    0, MPI_COMM_WORLD);
        
        int terminate = 0;
        if (rank == 0) {
            // Usiamo qsort per semplicità, ma Radix Sort qui sarebbe l'ottimizzazione finale
            qsort(all_suffixes, n, sizeof(Suffix), compare_suffixes);
            
            int current_rank = 0;
            rank_array[all_suffixes[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                if (compare_suffixes(&all_suffixes[i], &all_suffixes[i-1]) != 0) {
                    current_rank++;
                }
                rank_array[all_suffixes[i].index] = current_rank;
            }
            max_rank_value = current_rank;
            
            if (max_rank_value == n - 1) terminate = 1;
        }
        
        MPI_Bcast(&terminate, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (terminate) break;
        
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);
        
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index; 
            int next_index = global_idx + k;
            local_suffixes[i].rank[0] = rank_array[global_idx];
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }
    }
    
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = all_suffixes[i].index;
        }
    }

    // Libera la memoria
    MPI_Type_free(&suffix_mpi_type);
    if (rank == 0) {
        free(all_suffixes);
        free(recvcounts);
        free(displs);
    }
    free(local_suffixes);
    free(rank_array);
}