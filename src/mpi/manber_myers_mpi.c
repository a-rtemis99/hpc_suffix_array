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
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

// Funzione principale MPI - LA VERSIONE OTTIMIZZATA
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;

    // STRATEGIA IBRIDA: per file piccoli, esegui sequenziale
    if (n < 5000000) { // Soglia aumentata a ~5MB
        if (rank == 0) {
            build_suffix_array(sa);
        }
        MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);
        return;
    }

    // 1. DISTRIBUZIONE DATI INIZIALE (UNA SOLA VOLTA)
    int base_chunk = n / size;
    int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0);
    int displ = rank * base_chunk + (rank < remainder ? rank : remainder);

    Suffix* local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix));
    assert(local_suffixes);

    // Il Rank 0 prepara i dati iniziali
    if (rank == 0) {
        Suffix* all_suffixes_temp = (Suffix*)malloc(n * sizeof(Suffix));
        for(int i=0; i<n; ++i) {
            all_suffixes_temp[i].index = i;
            all_suffixes_temp[i].rank[0] = sa->str[i];
            all_suffixes_temp[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
        }

        // Calcola i parametri per Scatterv
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

    Suffix* sorted_local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix));
    assert(sorted_local_suffixes);

    for (int k = 2; k < 2 * n; k *= 2) {
        // 2. ORDINAMENTO LOCALE
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        // 3. CAMPIONAMENTO E RACCOLTA PIVOT
        // Ogni processo seleziona dei "campioni" dai suoi dati ordinati
        int num_samples = size; // Semplice ma efficace
        Suffix* samples = (Suffix*)malloc(num_samples * sizeof(Suffix));
        for(int i=0; i<num_samples; ++i) {
            samples[i] = local_suffixes[i * (local_n / num_samples)];
        }

        // Il Rank 0 raccoglie tutti i campioni
        Suffix* all_samples = NULL;
        if (rank == 0) {
            all_samples = (Suffix*)malloc(size * num_samples * sizeof(Suffix));
        }
        MPI_Gather(samples, num_samples * sizeof(Suffix), MPI_BYTE, all_samples, num_samples * sizeof(Suffix), MPI_BYTE, 0, MPI_COMM_WORLD);
        free(samples);

        // Il Rank 0 sceglie i "pivot" globali ordinando i campioni
        Suffix* pivots = (Suffix*)malloc((size - 1) * sizeof(Suffix));
        if (rank == 0) {
            qsort(all_samples, size * num_samples, sizeof(Suffix), compare_suffixes);
            for(int i=0; i < size - 1; ++i) {
                pivots[i] = all_samples[(i + 1) * num_samples];
            }
            free(all_samples);
        }
        
        // Il Rank 0 invia i pivot a tutti
        MPI_Bcast(pivots, (size - 1) * sizeof(Suffix), MPI_BYTE, 0, MPI_COMM_WORLD);

        // 4. PARTIZIONAMENTO E SCAMBIO GLOBALE (MPI_Alltoallv)
        // Ogni processo divide i suoi dati in base ai pivot e li invia al processo giusto
        int* send_counts = (int*)calloc(size, sizeof(int));
        int current_pivot = 0;
        for(int i=0; i<local_n; ++i) {
            if(current_pivot < size - 1 && compare_suffixes(&local_suffixes[i], &pivots[current_pivot]) > 0) {
                current_pivot++;
            }
            send_counts[current_pivot]++;
        }

        int* recv_counts = (int*)malloc(size * sizeof(int));
        MPI_Alltoall(send_counts, 1, MPI_INT, recv_counts, 1, MPI_INT, MPI_COMM_WORLD);
        
        int total_recv_count = 0;
        for(int i=0; i<size; ++i) total_recv_count += recv_counts[i];

        Suffix* received_suffixes = (Suffix*)malloc(total_recv_count * sizeof(Suffix));

        int* send_displs = (int*)malloc(size * sizeof(int));
        int* recv_displs = (int*)malloc(size * sizeof(int));
        send_displs[0] = 0;
        recv_displs[0] = 0;
        for(int i=1; i<size; ++i) {
            send_displs[i] = send_displs[i-1] + send_counts[i-1];
            recv_displs[i] = recv_displs[i-1] + recv_counts[i-1];
        }

        MPI_Alltoallv(local_suffixes, send_counts, send_displs, MPI_BYTE, received_suffixes, recv_counts, recv_displs, MPI_BYTE, MPI_COMM_WORLD);
        
        // 5. MERGE LOCALE FINALE
        // Ognuno ha ricevuto i dati che gli competono e fa un merge/sort finale
        qsort(received_suffixes, total_recv_count, sizeof(Suffix), compare_suffixes);

        // Ora `received_suffixes` contiene la porzione globalmente ordinata di questo processo
        free(local_suffixes);
        local_suffixes = received_suffixes;
        local_n = total_recv_count;

        // 6. CALCOLO E DISTRIBUZIONE DEI RANK (efficiente)
        // Raccogliamo solo il numero di elementi e il primo e ultimo rank di ogni blocco
        int* all_local_n = (int*)malloc(size*sizeof(int));
        MPI_Allgather(&local_n, 1, MPI_INT, all_local_n, 1, MPI_INT, MPI_COMM_WORLD);

        // Calcolo dei rank locali
        int* local_rank_updates = (int*)calloc(local_n, sizeof(int));
        if (local_n > 0) local_rank_updates[0] = 1;
        for(int i=1; i<local_n; ++i) {
            if(compare_suffixes(&local_suffixes[i], &local_suffixes[i-1]) != 0) {
                local_rank_updates[i] = 1;
            }
        }
        
        // Scan parallelo per ottenere i rank globali
        int local_sum = 0;
        for(int i=0; i<local_n; ++i) local_sum += local_rank_updates[i];
        
        int global_offset;
        MPI_Scan(&local_sum, &global_offset, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        global_offset -= local_sum;

        // Applica l'offset e lo scan locale per i rank finali
        int current_local_rank = 0;
        for(int i=0; i<local_n; ++i) {
            current_local_rank += local_rank_updates[i];
            rank_array[local_suffixes[i].index] = global_offset + current_local_rank -1;
        }

        // Condividi il rank massimo
        int max_rank_value;
        int last_rank = (local_n > 0) ? rank_array[local_suffixes[local_n-1].index] : -1;
        MPI_Allreduce(&last_rank, &max_rank_value, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

        // Trasmetti l'intero rank_array a tutti
        // Dobbiamo raccoglierlo prima
        int* all_ranks_indices = (int*)malloc(n*sizeof(int));
        int* all_ranks_values = (int*)malloc(n*sizeof(int));
        
        int* send_indices = (int*)malloc(local_n*sizeof(int));
        int* send_values = (int*)malloc(local_n*sizeof(int));
        for(int i=0; i<local_n; ++i) {
            send_indices[i] = local_suffixes[i].index;
            send_values[i] = rank_array[local_suffixes[i].index];
        }

        int* displs_gather = (int*)malloc(size*sizeof(int));
        displs_gather[0] = 0;
        for(int i=1; i<size; ++i) displs_gather[i] = displs_gather[i-1] + all_local_n[i-1];
        
        MPI_Gatherv(send_indices, local_n, MPI_INT, all_ranks_indices, all_local_n, displs_gather, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Gatherv(send_values, local_n, MPI_INT, all_ranks_values, all_local_n, displs_gather, MPI_INT, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            for(int i=0; i<n; ++i) {
                rank_array[all_ranks_indices[i]] = all_ranks_values[i];
            }
        }
        
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);

        if(max_rank_value == n-1) {
            // ... cleanup ...
            break;
        }

        // 7. AGGIORNAMENTO PARALLELO per il prossimo ciclo
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index; 
            int next_index = global_idx + k;
            local_suffixes[i].rank[0] = rank_array[global_idx];
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }

        free(pivots);
        free(send_counts);
        free(recv_counts);
        free(send_displs);
        free(recv_displs);
        free(all_local_n);
        free(local_rank_updates);
        free(send_indices);
        free(send_values);
        free(displs_gather);
        free(all_ranks_indices);
        free(all_ranks_values);
    }
    
    // FINALIZZAZIONE: Raccogli il risultato finale sul rank 0
    Suffix* final_suffixes = NULL;
    if (rank == 0) {
        final_suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    }
    
    int* all_local_n = (int*)malloc(size*sizeof(int));
    MPI_Gather(&local_n, 1, MPI_INT, all_local_n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int* displs_final = NULL;
    if (rank == 0) {
        displs_final = (int*)malloc(size * sizeof(int));
        displs_final[0] = 0;
        for(int i=1; i<size; ++i) displs_final[i] = displs_final[i-1] + all_local_n[i-1];
    }
    
    MPI_Gatherv(local_suffixes, local_n * sizeof(Suffix), MPI_BYTE,
                final_suffixes, all_local_n, displs_final, MPI_BYTE, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        // Ultimo sort di sicurezza sul master
        qsort(final_suffixes, n, sizeof(Suffix), compare_suffixes);
        for(int i=0; i<n; ++i) {
            sa->sa[i] = final_suffixes[i].index;
        }
        free(final_suffixes);
        free(displs_final);
    }
    
    free(all_local_n);
    free(local_suffixes);
    free(rank_array);
}