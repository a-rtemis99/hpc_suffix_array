// src/mpi/manber_myers_mpi.c

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

// --- INIZIO CODICE PER RADIX SORT SEQUENZIALE ---
// Funzione di utilità: ottiene il rank specificato (0 o 1)
static inline int get_rank(const Suffix* s, int r) {
    // Aggiungiamo 1 per gestire il rank -1 come 0 (carattere nullo)
    return (s->rank[r] + 1);
}

// Counting sort stabile per i rank (usato da Radix Sort)
void counting_sort_radix(Suffix* in, Suffix* out, int n, int rank_pass, int max_rank) {
    int* count = (int*)calloc(max_rank + 1, sizeof(int));
    
    for (int i = 0; i < n; i++) {
        count[get_rank(&in[i], rank_pass)]++;
    }
    
    for (int i = 1; i <= max_rank; i++) {
        count[i] += count[i - 1];
    }
    
    for (int i = n - 1; i >= 0; i--) {
        int r_val = get_rank(&in[i], rank_pass);
        out[count[r_val] - 1] = in[i];
        count[r_val]--;
    }
    
    free(count);
}

// Radix sort sequenziale per le coppie di rank
void radix_sort_suffixes(Suffix* suffixes, int n) {
    Suffix* temp_suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    // Il rango massimo è n (da -1 a n-1, quindi n+1 valori distinti)
    int max_rank = n + 1;

    // Ordina per il secondo rank (meno significativo)
    counting_sort_radix(suffixes, temp_suffixes, n, 1, max_rank);
    
    // Ordina per il primo rank (più significativo) in modo stabile
    counting_sort_radix(temp_suffixes, suffixes, n, 0, max_rank);
    
    free(temp_suffixes);
}
// --- FINE CODICE PER RADIX SORT SEQUENZIALE ---


void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;
    Suffix* suffixes = NULL;

    if (rank == 0) {
        suffixes = (Suffix*)malloc(n * sizeof(Suffix));
        for (int i = 0; i < n; i++) {
            suffixes[i].index = i;
            suffixes[i].rank[0] = sa->str[i];
            suffixes[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
        }
    }

    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(rank_array != NULL);

    int suffix_size_bytes = sizeof(Suffix);
    
    // Calcolo della suddivisione
    int* sendcounts_bytes = (int*)malloc(size * sizeof(int));
    int* displs_bytes = (int*)malloc(size * sizeof(int));
    
    int chunk_size_structs = n / size;
    int remainder = n % size;
    int current_displ_bytes = 0;
    for (int i = 0; i < size; i++) {
        int structs_for_proc = chunk_size_structs + (i < remainder ? 1 : 0);
        sendcounts_bytes[i] = structs_for_proc * suffix_size_bytes;
        displs_bytes[i] = current_displ_bytes;
        current_displ_bytes += sendcounts_bytes[i];
    }
    
    int local_n_bytes = sendcounts_bytes[rank];
    Suffix* local_suffixes = (Suffix*)malloc(local_n_bytes > 0 ? local_n_bytes : 1);
    
    for (int k = 2; k < n; k *= 2) {
        // 1. Distribuzione dei dati
        MPI_Scatterv(suffixes, sendcounts_bytes, displs_bytes, MPI_BYTE,
                     local_suffixes, local_n_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
        
        // 2. Ordinamento Locale (qsort è ottimo per pezzi piccoli)
        int local_n_structs = local_n_bytes / suffix_size_bytes;
        qsort(local_suffixes, local_n_structs, suffix_size_bytes, compare_suffix_mpi);
        
        // 3. Raccolta sul Root
        MPI_Gatherv(local_suffixes, local_n_bytes, MPI_BYTE,
                    suffixes, sendcounts_bytes, displs_bytes, MPI_BYTE, 0, MPI_COMM_WORLD);
        
        // 4. Lavoro solo sul Root: USA RADIX SORT (O(N)) INVECE DI Q-SORT (O(N log N))
        if (rank == 0) {
            radix_sort_suffixes(suffixes, n); // <-- OTTIMIZZAZIONE CHIAVE!

            int current_rank = 0;
            rank_array[suffixes[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                if (suffixes[i].rank[0] != suffixes[i-1].rank[0] ||
                    suffixes[i].rank[1] != suffixes[i-1].rank[1]) {
                    current_rank++;
                }
                rank_array[suffixes[i].index] = current_rank;
            }
        }
        
        // 5. Broadcast del solo rank_array
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);
        
        // 6. Aggiornamento per il prossimo ciclo sul root
        if (rank == 0) {
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
    }
    
    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            sa->sa[i] = suffixes[i].index;
        }
    }

    if(rank == 0) free(suffixes);
    free(rank_array);
    free(sendcounts_bytes);
    free(displs_bytes);
    free(local_suffixes);
}