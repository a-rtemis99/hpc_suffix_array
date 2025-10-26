// src/mpi/manber_myers_mpi.c
// VERSIONE DEFINITIVA: Master-Worker con qsort locale e RADIX SORT sul master.

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

// Prototipo della funzione sequenziale (che si trova in manber_myers.c)
void build_suffix_array(SuffixArray* sa);

// Funzione di confronto per qsort (usata solo per l'ordinamento locale)
int compare_suffixes(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        if (s1->rank[1] == s2->rank[1]) return 0;
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

// --- INIZIO CODICE PER RADIX SORT (usato dal master per il merge) ---
static inline int get_rank_val(int r) {
    return r + 1; // Mappa -1 a 0, 0 a 1, ecc.
}

void counting_sort_radix(Suffix* in, Suffix* out, int n, int rank_pass, int max_rank_val) {
    int count_size = max_rank_val + 2; // +1 per max_rank_val, +1 per il -1 mappato a 0
    int* count = (int*)calloc(count_size, sizeof(int));
    assert(count != NULL);

    for (int i = 0; i < n; i++) {
        count[get_rank_val(in[i].rank[rank_pass])]++;
    }
    for (int i = 1; i < count_size; i++) {
        count[i] += count[i - 1];
    }
    for (int i = n - 1; i >= 0; i--) {
        int r_val = get_rank_val(in[i].rank[rank_pass]);
        out[count[r_val] - 1] = in[i];
        count[r_val]--;
    }
    free(count);
}

void radix_sort_suffixes(Suffix* suffixes, int n, int max_rank_val) {
    Suffix* temp_suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    assert(temp_suffixes != NULL);

    // Ordina per il secondo rank (meno significativo)
    counting_sort_radix(suffixes, temp_suffixes, n, 1, max_rank_val);
    
    // Ordina per il primo rank (più significativo) in modo stabile
    counting_sort_radix(temp_suffixes, suffixes, n, 0, max_rank_val);
    
    free(temp_suffixes);
}
// --- FINE CODICE PER RADIX SORT ---


// Funzione principale MPI - Master/Worker con RADIX SORT sul master
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;

    // STRATEGIA IBRIDA: per file piccoli (< 5MB), esegui sequenziale
    if (n < 5000000) {
        if (rank == 0) {
            build_suffix_array(sa); // Usa la versione sequenziale con qsort
        }
        if(rank != 0 && sa->sa == NULL) {
           sa->sa = (int*)malloc(n * sizeof(int));
           assert(sa->sa != NULL);
        }
        MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);
        return;
    }

    // Creazione MPI Datatype per Suffix
    MPI_Datatype suffix_mpi_type;
    int blocklengths[2] = {1, 2};
    MPI_Aint displacements[2];
    MPI_Datatype types[2] = {MPI_INT, MPI_INT};
    Suffix s_temp = {0, {0, 0}}; // Inizializzata
    MPI_Get_address(&s_temp.index, &displacements[0]);
    MPI_Get_address(&s_temp.rank[0], &displacements[1]);
    displacements[1] = displacements[1] - displacements[0]; displacements[0] = 0;
    MPI_Type_create_struct(2, blocklengths, displacements, types, &suffix_mpi_type);
    MPI_Type_commit(&suffix_mpi_type);

    // Calcolo distribuzione
    int base_chunk = n / size;
    int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0);
    int displ = rank * base_chunk + (rank < remainder ? rank : remainder);

    Suffix* local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix));
    assert(local_suffixes != NULL);

    // Inizializzazione PARALLELA
    for (int i = 0; i < local_n; i++) {
        int global_idx = displ + i;
        local_suffixes[i].index = global_idx;
        local_suffixes[i].rank[0] = (unsigned char)sa->str[global_idx];
        local_suffixes[i].rank[1] = (global_idx + 1 < n) ? (unsigned char)sa->str[global_idx + 1] : -1;
    }

    // rank_array allocato da TUTTI all'inizio (gestione memoria semplice)
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

    MPI_Gather(&local_n, 1, MPI_INT, recvcounts_structs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        displs_structs[0] = 0;
        for (int i = 1; i < size; i++) {
            displs_structs[i] = displs_structs[i-1] + recvcounts_structs[i-1];
        }
    }

    int k; 
    int max_rank_value = 256; // Massimo rango iniziale (ASCII)

    for (k = 4; k < 2 * n; k *= 2) {
        // 1. Ordinamento Locale (qsort è efficiente per pezzi piccoli)
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        // 2. Raccolta sul Root
        MPI_Gatherv(local_suffixes, local_n, suffix_mpi_type,
                    all_suffixes, recvcounts_structs, displs_structs, suffix_mpi_type,
                    0, MPI_COMM_WORLD);

        int terminate = 0;
        if (rank == 0) {
            // 3. Merge Globale sul Master usando RADIX SORT (O(N))
            radix_sort_suffixes(all_suffixes, n, max_rank_value); // max_rank_value + 1 gestito dentro la funzione

            // 4. Calcolo dei nuovi Rank sul Master
            int current_rank = 0;
            rank_array[all_suffixes[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                if (all_suffixes[i].rank[0] != all_suffixes[i-1].rank[0] ||
                    all_suffixes[i].rank[1] != all_suffixes[i-1].rank[1]) {
                    current_rank++;
                }
                rank_array[all_suffixes[i].index] = current_rank;
            }
            max_rank_value = current_rank; // Aggiorna per il prossimo ciclo di Radix Sort

            if (current_rank == n - 1) terminate = 1;
        }

        // 5. Broadcast terminazione
        MPI_Bcast(&terminate, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (terminate) break;

        // 6. Broadcast rank_array e max_rank_value
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&max_rank_value, 1, MPI_INT, 0, MPI_COMM_WORLD); 

        // 7. Aggiornamento Locale
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index;
            int next_index = global_idx + k / 2;
            local_suffixes[i].rank[0] = rank_array[global_idx];
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }
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
    free(rank_array);
}