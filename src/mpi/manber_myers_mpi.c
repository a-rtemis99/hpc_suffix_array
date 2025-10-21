// src/mpi/manber_myers_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h" 

// Prototipo della funzione sequenziale (che si trova in manber_myers.c)
void build_suffix_array(SuffixArray* sa);

// Funzione di confronto per qsort (usata per l'ordinamento locale)
int compare_suffixes(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        if (s1->rank[1] == s2->rank[1]) return 0;
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

// CODICE PER RADIX SORT SEQUENZIALE (usato dal master per il merge)
static inline int get_rank_val(int r) {
    // Mappa -1 a 0, 0 a 1, 1 a 2, ecc.
    return r + 1;
}

// Counting sort stabile (usato da Radix Sort)
void counting_sort_radix(Suffix* in, Suffix* out, int n, int rank_pass, int max_rank_val) {
    // max_rank_val è il valore massimo prima di aggiungere 1.
    // L'array count deve avere dimensione max_rank_val + 1 + 1 (per lo 0 extra)
    int count_size = max_rank_val + 2;
    int* count = (int*)calloc(count_size, sizeof(int));
    assert(count != NULL); // Verifica allocazione

    // Conta le occorrenze di ogni rank
    for (int i = 0; i < n; i++) {
        int rank_value = get_rank_val(in[i].rank[rank_pass]);
        assert(rank_value >= 0 && rank_value < count_size); 
        count[rank_value]++;
    }

    // Calcola le posizioni cumulative
    for (int i = 1; i < count_size; i++) {
        count[i] += count[i - 1];
    }

    // Costruisce l'array di output ordinato
    for (int i = n - 1; i >= 0; i--) {
        int rank_value = get_rank_val(in[i].rank[rank_pass]);
        assert(count[rank_value] > 0); 
        out[count[rank_value] - 1] = in[i];
        count[rank_value]--;
    }

    free(count);
}

// Radix sort sequenziale per le coppie di rank (usato dal master)
void radix_sort_suffixes(Suffix* suffixes, int n, int max_rank_val) {
    Suffix* temp_suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    assert(temp_suffixes != NULL); // Verifica allocazione

    // Ordina per il secondo rank
    counting_sort_radix(suffixes, temp_suffixes, n, 1, max_rank_val);

    // Ordina per il primo rank (più significativo) in modo stabile
    counting_sort_radix(temp_suffixes, suffixes, n, 0, max_rank_val);

    free(temp_suffixes);
}


// Funzione principale MPI - Master/Worker con radix sort sul master
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;

    // STRATEGIA IBRIDA: per file piccoli (< 5MB), esegue sequenziale
    if (n < 5000000) {
        if (rank == 0) {
            build_suffix_array(sa);
        }
        // Il rank 0 invia il risultato finale a tutti gli altri
        // Allocazione necessaria per i processi non-root
        if(rank != 0) {
           sa->sa = (int*)malloc(n * sizeof(int));
           assert(sa->sa != NULL);
        }
        MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);
        // NB: i processi non-root non avranno sa->str e sa->lcp inizializzati,
        // ma build_suffix_array_mpi restituisce solo sa->sa.
        return;
    }

    // Creazione MPI Datatype per Suffix
    MPI_Datatype suffix_mpi_type;
    int blocklengths[2] = {1, 2}; // 1 int per index, 2 int per rank
    MPI_Aint displacements[2];
    MPI_Datatype types[2] = {MPI_INT, MPI_INT};
    Suffix s_temp; // Usata per calcolare gli offset
    MPI_Get_address(&s_temp.index, &displacements[0]);
    MPI_Get_address(&s_temp.rank[0], &displacements[1]); // Offset del primo elemento di rank
    // Calcola lo spostamento relativo dall'inizio della struct
    displacements[1] = displacements[1] - displacements[0];
    displacements[0] = 0; // L'indice inizia all'offset 0

    MPI_Type_create_struct(2, blocklengths, displacements, types, &suffix_mpi_type);
    MPI_Type_commit(&suffix_mpi_type);

    // Calcolo della distribuzione del lavoro (numero di struct)
    int base_chunk = n / size;
    int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0); // Num struct locali

    Suffix* local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix));
    assert(local_suffixes != NULL);

    // Distribuzione iniziale dei dati (solo rank 0 invia)
    if (rank == 0) {
        Suffix* all_suffixes_temp = (Suffix*)malloc(n * sizeof(Suffix));
        assert(all_suffixes_temp != NULL);
        for(int i=0; i<n; ++i) { // Popola i dati iniziali
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
        // Usa il tipo MPI personalizzato e i conteggi/spostamenti in numero di struct
        MPI_Scatterv(all_suffixes_temp, sendcounts_structs, displs_structs, suffix_mpi_type,
                     local_suffixes, local_n, suffix_mpi_type, 0, MPI_COMM_WORLD);

        free(all_suffixes_temp);
        free(sendcounts_structs);
        free(displs_structs);
    } else {
        // I processi non-root ricevono solo
        MPI_Scatterv(NULL, NULL, NULL, suffix_mpi_type,
                     local_suffixes, local_n, suffix_mpi_type, 0, MPI_COMM_WORLD);
    }

    // Array per i rank (necessario a tutti per l'aggiornamento locale)
    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array != NULL);

    // Buffer globali allocati solo sul master (rank 0)
    Suffix* all_suffixes = NULL;
    int* recvcounts_structs = NULL;
    int* displs_structs = NULL;
    if (rank == 0) {
        all_suffixes = (Suffix*)malloc(n * sizeof(Suffix)); assert(all_suffixes != NULL);
        recvcounts_structs = (int*)malloc(size * sizeof(int)); assert(recvcounts_structs != NULL);
        displs_structs = (int*)malloc(size * sizeof(int)); assert(displs_structs != NULL);
    }

    // Raccoglie il numero di struct locali (local_n) da ogni processo sul master
    MPI_Gather(&local_n, 1, MPI_INT, recvcounts_structs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Il master calcola gli spostamenti per Gatherv (in numero di struct)
    if (rank == 0) {
        displs_structs[0] = 0;
        for (int i = 1; i < size; i++) {
            displs_structs[i] = displs_structs[i-1] + recvcounts_structs[i-1];
        }
    }

    int max_rank_value = 256; // Massimo rango iniziale (ASCII)

    // Ciclo principale di Manber-Myers
    for (int k = 2; k < 2 * n; k *= 2) { // Il limite 2*n assicura la terminazione
        // 1. Ordinamento Locale (qsort è efficiente qui)
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        // 2. Raccolta dei chunk ordinati sul root (usando il tipo MPI custom)
        MPI_Gatherv(local_suffixes, local_n, suffix_mpi_type,
                    all_suffixes, recvcounts_structs, displs_structs, suffix_mpi_type,
                    0, MPI_COMM_WORLD);

        int terminate = 0;
        if (rank == 0) {
            // 3. Merge Globale sul Master usando radix sort
            radix_sort_suffixes(all_suffixes, n, max_rank_value); // Usa max_rank_value calcolato

            // 4. Calcolo dei nuovi rank sul master
            int current_rank = 0;
            rank_array[all_suffixes[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                // Confronta le coppie di rank per vedere se sono diverse
                if (all_suffixes[i].rank[0] != all_suffixes[i-1].rank[0] ||
                    all_suffixes[i].rank[1] != all_suffixes[i-1].rank[1]) {
                    current_rank++;
                }
                rank_array[all_suffixes[i].index] = current_rank;
            }
            max_rank_value = current_rank; 

            // Condizione di terminazione
            if (max_rank_value == n - 1) {
                terminate = 1;
            }
        }

        // 5. Broadcast della flag di terminazione
        MPI_Bcast(&terminate, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (terminate) {
            break; // Esce dal ciclo se l'ordinamento è completo
        }

        // 6. Broadcast dei dati essenziali: rank_array e max_rank_value
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&max_rank_value, 1, MPI_INT, 0, MPI_COMM_WORLD); // Serve per il prossimo radix sort

        // 7. Aggiornamento Locale dei Rank (in parallelo)
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index;
            int next_index = global_idx + k; // Indice del suffisso che inizia k posizioni dopo
            local_suffixes[i].rank[0] = rank_array[global_idx]; // Nuovo rank basato su k caratteri
            // Nuovo rank basato sui successivi k caratteri
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }
    }

    // Finalizzazione: il rank 0 ha già l'array finale all_suffixes ordinato
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = all_suffixes[i].index;
        }
    }
     if(rank != 0 && sa->sa == NULL) { // Allocazione per i non-root se non fatto dalla ibrida
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
    free(rank_array);
}