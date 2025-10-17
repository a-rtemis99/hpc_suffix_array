// src/mpi/manber_myers_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

// Prototipo della funzione sequenziale (per la strategia ibrida)
void build_suffix_array(SuffixArray* sa);

// Funzione di confronto per qsort
int compare_suffixes(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        if (s1->rank[1] == s2->rank[1]) return 0;
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

// Funzione principale MPI - Sample Sort (PSRS)
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;
    int local_n_bytes; // <-- CORREZIONE: Dichiarata qui all'inizio

    // STRATEGIA IBRIDA
    if (n < 5000000) { // Soglia ~5MB
        if (rank == 0) {
            build_suffix_array(sa);
        }
        MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);
        return;
    }

    // 1. DISTRIBUZIONE DATI INIZIALE
    int base_chunk = n / size;
    int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0);
    // int displ = ... <-- CORREZIONE: rimossa variabile inutilizzata

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
            sendcounts[i] = (base_chunk + (i < remainder ? 1 : 0)) * sizeof(Suffix);
            displs_scatter[i] = (i * base_chunk + (i < remainder ? i : remainder)) * sizeof(Suffix);
        }
        
        MPI_Scatterv(all_suffixes_temp, sendcounts, displs_scatter, MPI_BYTE, local_suffixes, local_n * sizeof(Suffix), MPI_BYTE, 0, MPI_COMM_WORLD);
        
        free(all_suffixes_temp);
        free(sendcounts);
        free(displs_scatter);
    } else {
        MPI_Scatterv(NULL, NULL, NULL, MPI_BYTE, local_suffixes, local_n * sizeof(Suffix), MPI_BYTE, 0, MPI_COMM_WORLD);
    }

    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array);

    for (int k = 2; k < 2 * n; k *= 2) {
        // 2. ORDINAMENTO LOCALE
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        // 3. CAMPIONAMENTO E SCELTA DEI PIVOT
        int num_samples = size > 0 ? size : 1;
        Suffix* samples = (Suffix*)malloc(num_samples * sizeof(Suffix));
        for(int i=0; i<num_samples; ++i) {
            if (local_n > 0) samples[i] = local_suffixes[i * (local_n / num_samples)];
        }

        Suffix* all_samples = NULL;
        if (rank == 0) {
            all_samples = (Suffix*)malloc(size * num_samples * sizeof(Suffix));
        }
        MPI_Gather(samples, num_samples * sizeof(Suffix), MPI_BYTE, all_samples, num_samples * sizeof(Suffix), MPI_BYTE, 0, MPI_COMM_WORLD);
        free(samples);

        Suffix* pivots = (Suffix*)malloc((size - 1) * sizeof(Suffix));
        if (rank == 0) {
            qsort(all_samples, size * num_samples, sizeof(Suffix), compare_suffixes);
            for(int i=0; i < size - 1; ++i) {
                pivots[i] = all_samples[(i + 1) * num_samples];
            }
            free(all_samples);
        }
        MPI_Bcast(pivots, (size - 1) * sizeof(Suffix), MPI_BYTE, 0, MPI_COMM_WORLD);

        // 4. PARTIZIONAMENTO LOCALE E SCAMBIO GLOBALE (ALLTOALLV)
        int* send_counts = (int*)calloc(size, sizeof(int));
        int current_pivot = 0;
        for(int i=0; i<local_n; ++i) {
            while(current_pivot < size - 1 && compare_suffixes(&local_suffixes[i], &pivots[current_pivot]) >= 0) {
                current_pivot++;
            }
            send_counts[current_pivot]++;
        }

        int* recv_counts = (int*)malloc(size * sizeof(int));
        MPI_Alltoall(send_counts, 1, MPI_INT, recv_counts, 1, MPI_INT, MPI_COMM_WORLD);
        
        int total_recv_count = 0;
        for(int i=0; i<size; ++i) total_recv_count += recv_counts[i];

        Suffix* received_suffixes = (Suffix*)malloc(total_recv_count * sizeof(Suffix));
        assert(received_suffixes);
        
        int* send_displs = (int*)malloc(size * sizeof(int));
        int* recv_displs = (int*)malloc(size * sizeof(int));
        send_displs[0] = 0; recv_displs[0] = 0;
        for(int i=1; i<size; ++i) {
            send_displs[i] = send_displs[i-1] + send_counts[i-1];
            recv_displs[i] = recv_displs[i-1] + recv_counts[i-1];
        }
        
        // Converte conteggi e spostamenti da #structs a #bytes per MPI
        int* send_counts_bytes = (int*)malloc(size * sizeof(int));
        int* send_displs_bytes = (int*)malloc(size * sizeof(int));
        int* recv_counts_bytes = (int*)malloc(size * sizeof(int));
        int* recv_displs_bytes = (int*)malloc(size * sizeof(int));
        for(int i=0; i<size; ++i) {
            send_counts_bytes[i] = send_counts[i] * sizeof(Suffix);
            send_displs_bytes[i] = send_displs[i] * sizeof(Suffix);
            recv_counts_bytes[i] = recv_counts[i] * sizeof(Suffix);
            recv_displs_bytes[i] = recv_displs[i] * sizeof(Suffix);
        }

        MPI_Alltoallv(local_suffixes, send_counts_bytes, send_displs_bytes, MPI_BYTE, received_suffixes, recv_counts_bytes, recv_displs_bytes, MPI_BYTE, MPI_COMM_WORLD);
        
        free(local_suffixes);
        local_suffixes = received_suffixes;
        local_n = total_recv_count;
        
        // 5. MERGE FINALE LOCALE
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        // 6. CALCOLO DEI RANK IN PARALLELO CON SCAN
        int* local_rank_updates = (int*)calloc(local_n, sizeof(int));
        if (local_n > 0) local_rank_updates[0] = 1;
        for(int i=1; i<local_n; ++i) {
            if(compare_suffixes(&local_suffixes[i], &local_suffixes[i-1]) != 0) {
                local_rank_updates[i] = 1;
            }
        }
        
        int local_distinct_count = 0;
        for(int i=0; i<local_n; ++i) local_distinct_count += local_rank_updates[i];
        
        int rank_offset;
        MPI_Scan(&local_distinct_count, &rank_offset, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        rank_offset -= local_distinct_count;

        int current_local_rank = 0;
        for(int i=0; i<local_n; ++i) {
            current_local_rank += local_rank_updates[i];
            rank_array[local_suffixes[i].index] = rank_offset + current_local_rank -1;
        }

        int max_rank_value;
        int last_rank = (local_n > 0) ? rank_array[local_suffixes[local_n-1].index] : -1;
        MPI_Allreduce(&last_rank, &max_rank_value, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
        
        int* all_local_n_structs = (int*)malloc(size * sizeof(int));
        MPI_Allgather(&local_n, 1, MPI_INT, all_local_n_structs, 1, MPI_INT, MPI_COMM_WORLD);
        
        int* displs_gather = (int*)malloc(size * sizeof(int));
        displs_gather[0] = 0;
        for(int i=1; i<size; ++i) displs_gather[i] = displs_gather[i-1] + all_local_n_structs[i-1];

        int* temp_indices = (int*)malloc(local_n * sizeof(int));
        int* temp_ranks = (int*)malloc(local_n * sizeof(int));
        for(int i=0; i<local_n; ++i) {
            temp_indices[i] = local_suffixes[i].index;
            temp_ranks[i] = rank_array[local_suffixes[i].index];
        }

        int* all_indices = (int*)malloc(n * sizeof(int));
        int* all_ranks = (int*)malloc(n * sizeof(int));

        MPI_Allgatherv(temp_indices, local_n, MPI_INT, all_indices, all_local_n_structs, displs_gather, MPI_INT, MPI_COMM_WORLD);
        MPI_Allgatherv(temp_ranks, local_n, MPI_INT, all_ranks, all_local_n_structs, displs_gather, MPI_INT, MPI_COMM_WORLD);

        for(int i=0; i<n; ++i) {
            rank_array[all_indices[i]] = all_ranks[i];
        }

        if(max_rank_value == n-1) {
            free(pivots); free(send_counts); free(recv_counts); free(send_displs); free(recv_displs);
            free(send_counts_bytes); free(send_displs_bytes); free(recv_counts_bytes); free(recv_displs_bytes);
            free(local_rank_updates); free(all_local_n_structs); free(displs_gather);
            free(temp_indices); free(temp_ranks); free(all_indices); free(all_ranks);
            break;
        }

        // 7. AGGIORNAMENTO PARALLELO per il prossimo ciclo
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index; 
            int next_index = global_idx + k;
            local_suffixes[i].rank[0] = rank_array[global_idx];
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }

        free(pivots); free(send_counts); free(recv_counts); free(send_displs); free(recv_displs);
        free(send_counts_bytes); free(send_displs_bytes); free(recv_counts_bytes); free(recv_displs_bytes);
        free(local_rank_updates); free(all_local_n_structs); free(displs_gather);
        free(temp_indices); free(temp_ranks); free(all_indices); free(all_ranks);
    }
    
    // FINALIZZAZIONE: Raccogli il risultato finale sul rank 0
    int* all_local_n_final = (int*)malloc(size*sizeof(int));
    MPI_Gather(&local_n, 1, MPI_INT, all_local_n_final, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    int* displs_final = NULL;
    Suffix* final_suffixes = NULL;
    if(rank == 0) {
        displs_final = (int*)malloc(size*sizeof(int));
        displs_final[0] = 0;
        for(int i=1; i<size; ++i) displs_final[i] = displs_final[i-1] + all_local_n_final[i-1];
        final_suffixes = (Suffix*)malloc(n*sizeof(Suffix));
    }
    
    // Converte conteggi e spostamenti in bytes
    int* recvcounts_final_bytes = NULL;
    int* displs_final_bytes = NULL;
    if(rank==0) {
        recvcounts_final_bytes = (int*)malloc(size*sizeof(int));
        displs_final_bytes = (int*)malloc(size*sizeof(int));
        for(int i=0; i<size; ++i) {
            recvcounts_final_bytes[i] = all_local_n_final[i] * sizeof(Suffix);
            displs_final_bytes[i] = displs_final[i] * sizeof(Suffix);
        }
    }
    local_n_bytes = local_n * sizeof(Suffix); // <-- CORREZIONE: Calcola il valore

    MPI_Gatherv(local_suffixes, local_n_bytes, MPI_BYTE,
                final_suffixes, recvcounts_final_bytes, displs_final_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        for(int i=0; i<n; ++i) {
            sa->sa[i] = final_suffixes[i].index;
        }
        free(final_suffixes);
        free(displs_final);
        free(recvcounts_final_bytes);
        free(displs_final_bytes);
    }
    
    free(all_local_n_final);
    free(local_suffixes);
    free(rank_array);
}