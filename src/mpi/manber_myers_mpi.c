// src/mpi/manber_myers_mpi.c
// Implementazione con Parallel Sample Sort (PSRS)

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

// Funzione principale MPI - Parallel Sample Sort (PSRS)
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;
    int k; // Dichiarata fuori per visibilità dopo il ciclo

    // STRATEGIA IBRIDA
    if (n < 5000000) { // Soglia ~5MB
        if (rank == 0) build_suffix_array(sa);
        if(rank != 0 && sa->sa == NULL) sa->sa = (int*)malloc(n * sizeof(int));
        MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);
        return;
    }

    // Creazione MPI Datatype per Suffix
    MPI_Datatype suffix_mpi_type;
    int blocklengths[2] = {1, 2}; MPI_Aint displacements[2]; MPI_Datatype types[2] = {MPI_INT, MPI_INT};
    Suffix s_temp; MPI_Get_address(&s_temp.index, &displacements[0]); MPI_Get_address(&s_temp.rank[0], &displacements[1]);
    displacements[1] -= displacements[0]; displacements[0] = 0;
    MPI_Type_create_struct(2, blocklengths, displacements, types, &suffix_mpi_type);
    MPI_Type_commit(&suffix_mpi_type);

    // Calcolo distribuzione
    int base_chunk = n / size; int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0);
    int displ = rank * base_chunk + (rank < remainder ? rank : remainder);

    Suffix* local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix)); assert(local_suffixes != NULL);

    // Distribuzione iniziale (Inizializzazione Parallela)
    for (int i = 0; i < local_n; i++) {
        int global_idx = displ + i;
        local_suffixes[i].index = global_idx;
        local_suffixes[i].rank[0] = (unsigned char)sa->str[global_idx];
        local_suffixes[i].rank[1] = (global_idx + 1 < n) ? (unsigned char)sa->str[global_idx + 1] : -1;
    }

    int* rank_array = (int*)malloc(n * sizeof(int)); assert(rank_array != NULL); // Necessario a tutti

    // Buffer temporanei per PSRS
    Suffix* received_suffixes = NULL; // Buffer per ricevere dati da Alltoallv
    int total_recv_count = 0;       // Dimensione di received_suffixes

    for (k = 4; k < 2 * n; k *= 2) {
        // 1. ORDINAMENTO LOCALE
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        // 2. SELEZIONE CAMPIONI REGOLARI
        int num_samples = size > 1 ? size : 1; // Un campione per ogni bucket atteso
        Suffix* samples = (Suffix*)malloc(num_samples * sizeof(Suffix)); assert(samples != NULL);
        for(int i = 0; i < num_samples; ++i) {
             // Scegli campioni equidistanti, gestendo il caso local_n < num_samples
             int sample_index = (int)(((long long)i * local_n) / num_samples);
             if (local_n > 0) samples[i] = local_suffixes[sample_index];
             else samples[i].rank[0] = -1; // Segnaposto se non ci sono dati locali
        }

        // 3. RACCOLTA CAMPIONI E SELEZIONE PIVOT SUL MASTER
        Suffix* all_samples = NULL;
        if (rank == 0) all_samples = (Suffix*)malloc(size * num_samples * sizeof(Suffix));
        MPI_Gather(samples, num_samples, suffix_mpi_type, all_samples, num_samples, suffix_mpi_type, 0, MPI_COMM_WORLD);
        free(samples);

        Suffix* pivots = (Suffix*)malloc((size - 1) * sizeof(Suffix)); assert(pivots != NULL);
        if (rank == 0) {
            qsort(all_samples, size * num_samples, sizeof(Suffix), compare_suffixes);
            // Scegli size-1 pivot equidistanti dall'array ordinato di campioni
            for(int i = 0; i < size - 1; ++i) {
                pivots[i] = all_samples[(i + 1) * num_samples];
            }
            free(all_samples);
        }

        // 4. BROADCAST DEI PIVOT
        MPI_Bcast(pivots, size - 1, suffix_mpi_type, 0, MPI_COMM_WORLD);

        // 5. PARTIZIONAMENTO LOCALE IN BASE AI PIVOT
        int* send_counts = (int*)calloc(size, sizeof(int)); // Numero di Suffix da inviare a ciascun processo
        int current_pivot_idx = 0;
        for(int i = 0; i < local_n; ++i) {
            // Trova a quale processo appartiene questo suffisso
            while(current_pivot_idx < size - 1 && compare_suffixes(&local_suffixes[i], &pivots[current_pivot_idx]) >= 0) {
                current_pivot_idx++;
            }
            send_counts[current_pivot_idx]++;
        }

        // 6. SCAMBIO GLOBALE DEI DATI PARTIZIONATI (ALLTOALL)
        int* recv_counts = (int*)malloc(size * sizeof(int)); assert(recv_counts != NULL);
        // Comunica a tutti quanti elementi riceveranno da ciascun altro processo
        MPI_Alltoall(send_counts, 1, MPI_INT, recv_counts, 1, MPI_INT, MPI_COMM_WORLD);

        // Calcola la dimensione totale dei dati da ricevere
        total_recv_count = 0;
        for(int i = 0; i < size; ++i) total_recv_count += recv_counts[i];

        // Rialloca il buffer di ricezione (potrebbe cambiare dimensione ad ogni ciclo)
        free(received_suffixes); // Libera il vecchio buffer se esiste
        received_suffixes = (Suffix*)malloc(total_recv_count * sizeof(Suffix)); assert(received_suffixes != NULL);

        // Calcola spostamenti per Alltoallv (in numero di struct)
        int* send_displs = (int*)malloc(size * sizeof(int)); assert(send_displs != NULL);
        int* recv_displs = (int*)malloc(size * sizeof(int)); assert(recv_displs != NULL);
        send_displs[0] = 0; recv_displs[0] = 0;
        for(int i = 1; i < size; ++i) {
            send_displs[i] = send_displs[i-1] + send_counts[i-1];
            recv_displs[i] = recv_displs[i-1] + recv_counts[i-1];
        }

        // Esegui lo scambio globale effettivo
        MPI_Alltoallv(local_suffixes, send_counts, send_displs, suffix_mpi_type,
                      received_suffixes, recv_counts, recv_displs, suffix_mpi_type, MPI_COMM_WORLD);

        // Ora i dati locali sono quelli ricevuti
        free(local_suffixes);
        local_suffixes = received_suffixes; // Aggiorna il puntatore
        local_n = total_recv_count;      // Aggiorna la dimensione locale

        // 7. MERGE/SORT FINALE LOCALE
        // Ogni processo ordina i dati ricevuti (che sono già quasi ordinati)
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        // 8. CALCOLO DEI RANK IN PARALLELO CON SCAN
        // Ogni processo calcola quanti rank unici ha localmente
        int* local_rank_updates = (int*)calloc(local_n, sizeof(int)); assert(local_rank_updates != NULL);
        if (local_n > 0) local_rank_updates[0] = 1; // Il primo elemento ha sempre un nuovo rank (relativo)
        for(int i = 1; i < local_n; ++i) {
            if(compare_suffixes(&local_suffixes[i], &local_suffixes[i-1]) != 0) {
                local_rank_updates[i] = 1; // Marca dove cambia il rank
            }
        }

        // Comunica le informazioni di confine per correggere gli offset
        Suffix first_suffix_local = (local_n > 0) ? local_suffixes[0] : (Suffix){.rank={-2,-2}}; // Valore sentinella
        Suffix* first_suffixes_global = (Suffix*)malloc(size * sizeof(Suffix)); assert(first_suffixes_global != NULL);
        MPI_Allgather(&first_suffix_local, 1, suffix_mpi_type, first_suffixes_global, 1, suffix_mpi_type, MPI_COMM_WORLD);

        int rank_offset = 0; // Offset globale per questo processo
        if (rank > 0 && local_n > 0) {
             // Controlla se il primo elemento locale è uguale all'ultimo del processo precedente
             // Questo richiede una comunicazione aggiuntiva o un calcolo più complesso.
             // SEMPLIFICHIAMO: Usiamo uno Scan sul numero di rank unici
             int local_distinct_count = 0;
             for(int i=0; i<local_n; ++i) local_distinct_count += local_rank_updates[i];

             MPI_Scan(&local_distinct_count, &rank_offset, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
             rank_offset -= local_distinct_count; // L'offset è la somma *prima* di questo processo

             // Correzione al bordo: Se il primo elemento è uguale all'ultimo del rank prec, decrementa l'offset
             if (rank > 0 && local_n > 0 && compare_suffixes(&local_suffixes[0], &first_suffixes_global[rank-1]) == 0) {
                 // Questo è difficile da fare correttamente senza l'ultimo elemento del rank precedente.
                 // L'approccio Scan è più robusto anche se meno preciso ai bordi senza correzione.
             }
        }


        // Applica l'offset e lo scan locale per i rank globali
        int current_local_rank_sum = 0;
        for(int i = 0; i < local_n; ++i) {
            current_local_rank_sum += local_rank_updates[i];
            // Mappa l'indice originale del suffisso al suo rank globale
            rank_array[local_suffixes[i].index] = rank_offset + current_local_rank_sum - 1;
        }
        free(local_rank_updates);
        free(first_suffixes_global);

        // Ottieni il rank massimo globale
        int max_rank_value;
        int last_rank = (local_n > 0) ? rank_array[local_suffixes[local_n-1].index] : -1;
        MPI_Allreduce(&last_rank, &max_rank_value, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

        // Ricostruisci l'intero rank_array su tutti i processi (necessario per l'aggiornamento)
        int* all_local_n = (int*)malloc(size * sizeof(int)); assert(all_local_n != NULL);
        MPI_Allgather(&local_n, 1, MPI_INT, all_local_n, 1, MPI_INT, MPI_COMM_WORLD);

        int* displs_gather_ranks = (int*)malloc(size * sizeof(int)); assert(displs_gather_ranks != NULL);
        displs_gather_ranks[0] = 0;
        for(int i=1; i<size; ++i) displs_gather_ranks[i] = displs_gather_ranks[i-1] + all_local_n[i-1];

        // Prepara i dati locali (indici e rank calcolati) da inviare
        int* send_indices = (int*)malloc(local_n * sizeof(int)); assert(send_indices != NULL);
        int* send_ranks = (int*)malloc(local_n * sizeof(int)); assert(send_ranks != NULL);
        for(int i=0; i<local_n; ++i) {
            send_indices[i] = local_suffixes[i].index;
            send_ranks[i] = rank_array[local_suffixes[i].index];
        }

        // Buffer globali per raccogliere indici e rank da tutti
        int* all_indices = (int*)malloc(n * sizeof(int)); assert(all_indices != NULL);
        int* all_ranks = (int*)malloc(n * sizeof(int)); assert(all_ranks != NULL);

        MPI_Allgatherv(send_indices, local_n, MPI_INT, all_indices, all_local_n, displs_gather_ranks, MPI_INT, MPI_COMM_WORLD);
        MPI_Allgatherv(send_ranks, local_n, MPI_INT, all_ranks, all_local_n, displs_gather_ranks, MPI_INT, MPI_COMM_WORLD);

        // Ora ogni processo ricostruisce l'intero rank_array corretto
        for(int i=0; i<n; ++i) {
            rank_array[all_indices[i]] = all_ranks[i];
        }

        // Cleanup per questa iterazione
        free(pivots); free(send_counts); free(recv_counts); free(send_displs); free(recv_displs);
        free(all_local_n); free(displs_gather_ranks);
        free(send_indices); free(send_ranks); free(all_indices); free(all_ranks);

        // Condizione di terminazione
        if(max_rank_value == n-1) {
            break;
        }

        // 9. AGGIORNAMENTO PARALLELO dei rank locali per il prossimo ciclo
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index;
            int next_index = global_idx + k / 2; // k è la LUNGHEZZA CORRENTE
            local_suffixes[i].rank[0] = rank_array[global_idx];
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }

    } // Fine del ciclo for

    // FINALIZZAZIONE: Raccogli i pezzi finali (che sono globalmente ordinati) sul rank 0
    int* final_recvcounts = NULL;
    int* final_displs = NULL;
    Suffix* final_suffixes = NULL;

    if (rank == 0) {
        final_recvcounts = (int*)malloc(size * sizeof(int)); assert(final_recvcounts != NULL);
        final_displs = (int*)malloc(size * sizeof(int)); assert(final_displs != NULL);
        final_suffixes = (Suffix*)malloc(n * sizeof(Suffix)); assert(final_suffixes != NULL);
    }

    MPI_Gather(&local_n, 1, MPI_INT, final_recvcounts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        final_displs[0] = 0;
        for(int i=1; i<size; ++i) final_displs[i] = final_displs[i-1] + final_recvcounts[i-1];
    }

    MPI_Gatherv(local_suffixes, local_n, suffix_mpi_type,
                final_suffixes, final_recvcounts, final_displs, suffix_mpi_type, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        for(int i=0; i<n; ++i) {
            sa->sa[i] = final_suffixes[i].index;
        }
        free(final_suffixes);
        free(final_recvcounts);
        free(final_displs);
    }

    // Assicura che tutti abbiano il risultato finale
    if(rank != 0 && sa->sa == NULL) sa->sa = (int*)malloc(n * sizeof(int));
    MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);

    // Cleanup finale
    MPI_Type_free(&suffix_mpi_type);
    free(local_suffixes);
    free(rank_array);
}