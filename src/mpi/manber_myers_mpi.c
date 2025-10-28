#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/suffix_array.h"

// Prototipo della funzione sequenziale (per la strategia ibrida)
void build_suffix_array(SuffixArray* sa);

// Funzione di confronto per qsort (usata sia localmente che dal Min-Heap)
int compare_suffixes(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        if (s1->rank[1] == s2->rank[1]) return 0;
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

// --- IMPLEMENTAZIONE MIN-HEAP PER K-WAY MERGE ---

// Nodo dell'heap: contiene il Suffix e l'indice del chunk da cui proviene
typedef struct {
    Suffix s;
    int chunk_index; // L'indice del processo/chunk (0 a p-1)
} HeapNode;

// Funzione per scambiare due nodi dell'heap
void swap_nodes(HeapNode* a, HeapNode* b) {
    HeapNode temp = *a;
    *a = *b;
    *b = temp;
}

// Funzione per mantenere la proprietà del Min-Heap (heapify down)
void min_heapify(HeapNode* heap, int size, int i) {
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    // Confronta usando la stessa funzione di qsort, passando l'indirizzo
    // del Suffix all'interno del nodo dell'heap
    if (left < size && compare_suffixes(&heap[left].s, &heap[smallest].s) < 0) {
        smallest = left;
    }

    if (right < size && compare_suffixes(&heap[right].s, &heap[smallest].s) < 0) {
        smallest = right;
    }

    if (smallest != i) {
        swap_nodes(&heap[i], &heap[smallest]);
        min_heapify(heap, size, smallest);
    }
}

// Funzione per costruire il Min-Heap iniziale
void build_min_heap(HeapNode* heap, int size) {
    for (int i = (size / 2) - 1; i >= 0; i--) {
        min_heapify(heap, size, i);
    }
}

// Funzione che esegue il merge di P (size) chunk ordinati
void merge_k_chunks(Suffix* all_suffixes, int* chunk_sizes, int* chunk_displs, int p, int n, Suffix* sorted_suffixes) {
    
    // 1. Crea l'heap di dimensione P
    HeapNode* heap = (HeapNode*)malloc(p * sizeof(HeapNode));
    assert(heap != NULL);
    
    // 2. Inizializza l'heap con il primo elemento di ogni chunk
    for (int i = 0; i < p; i++) {
        int chunk_start_pos = chunk_displs[i];
        heap[i].s = all_suffixes[chunk_start_pos];
        heap[i].chunk_index = i;
    }
    build_min_heap(heap, p);

    // 3. Array per tenere traccia della posizione corrente in ogni chunk
    int* chunk_counters = (int*)calloc(p, sizeof(int));
    assert(chunk_counters != NULL);

    // 4. Ciclo di merge: estrai il minimo, inserisci il successivo
    for (int i = 0; i < n; i++) {
        // Estrai il nodo minimo (la radice dell'heap)
        HeapNode min_node = heap[0];
        
        // Copia il suffisso minimo nell'array di output
        sorted_suffixes[i] = min_node.s;

        // Avanza nel chunk da cui proveniva il minimo
        int chunk_idx = min_node.chunk_index;
        chunk_counters[chunk_idx]++;

        // Se quel chunk ha ancora elementi, inserisci il prossimo nell'heap
        if (chunk_counters[chunk_idx] < chunk_sizes[chunk_idx]) {
            int next_element_pos = chunk_displs[chunk_idx] + chunk_counters[chunk_idx];
            heap[0].s = all_suffixes[next_element_pos];
            heap[0].chunk_index = chunk_idx;
        } else {
            // Se il chunk è esaurito, sostituisci la radice con l'ultimo
            // elemento dell'heap e riduci la dimensione
            heap[0] = heap[p - 1];
            p--; // L'heap si restringe
        }
        
        // Ripristina la proprietà dell'heap
        if (p > 0) {
            min_heapify(heap, p, 0);
        }
    }

    // Cleanup
    free(heap);
    free(chunk_counters);
}


// Funzione principale MPI - Master/Worker
void build_suffix_array_mpi(SuffixArray* sa, int rank, int size) {
    int n = sa->n;

    // STRATEGIA IBRIDA
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
    Suffix s_temp = {0, {0, 0}};
    MPI_Get_address(&s_temp.index, &displacements[0]);
    MPI_Get_address(&s_temp.rank[0], &displacements[1]);
    displacements[1] = displacements[1] - displacements[0]; displacements[0] = 0;
    MPI_Type_create_struct(2, blocklengths, displacements, types, &suffix_mpi_type);
    MPI_Type_commit(&suffix_mpi_type);

    // Calcolo distribuzione
    int base_chunk = n / size;
    int remainder = n % size;
    int local_n = base_chunk + (rank < remainder ? 1 : 0); 
    Suffix* local_suffixes = (Suffix*)malloc(local_n * sizeof(Suffix));
    assert(local_suffixes != NULL);

    // rank_array è allocato da tutti
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
        int current_displ = 0;
        for(int i=0; i<size; ++i) {
            sendcounts_structs[i] = base_chunk + (i < remainder ? 1 : 0);
            displs_structs[i] = current_displ;
            current_displ += sendcounts_structs[i];
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
    Suffix* all_suffixes = NULL;       // Buffer per Gatherv (non ordinato)
    Suffix* sorted_suffixes = NULL;    // Buffer per il risultato del merge
    int* recvcounts_structs = NULL;
    int* displs_structs = NULL;
    if (rank == 0) {
        all_suffixes = (Suffix*)malloc(n * sizeof(Suffix)); assert(all_suffixes != NULL);
        // Allocato una sola volta e riutilizzato
        sorted_suffixes = (Suffix*)malloc(n * sizeof(Suffix)); assert(sorted_suffixes != NULL); 
        recvcounts_structs = (int*)malloc(size * sizeof(int)); assert(recvcounts_structs != NULL);
        displs_structs = (int*)malloc(size * sizeof(int)); assert(displs_structs != NULL);
    }
    
    // Gather per chunk sizes
    MPI_Gather(&local_n, 1, MPI_INT, recvcounts_structs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        displs_structs[0] = 0;
        for (int i = 1; i < size; i++) {
            displs_structs[i] = displs_structs[i-1] + recvcounts_structs[i-1];
        }
    }

    int k; 

    // Ciclo principale di Manber-Myers
    for (k = 4; k < 2 * n; k *= 2) {
        // Ordinamento locale 
        qsort(local_suffixes, local_n, sizeof(Suffix), compare_suffixes);

        // Raccolta dei chunk localmente ordinati 
        MPI_Gatherv(local_suffixes, local_n, suffix_mpi_type,
                      all_suffixes, recvcounts_structs, displs_structs, suffix_mpi_type,
                      0, MPI_COMM_WORLD);

        int terminate = 0;
        if (rank == 0) {
            // Fonde i 'size' chunk ordinati da 'all_suffixes' in 'sorted_suffixes'
            merge_k_chunks(all_suffixes, recvcounts_structs, displs_structs, size, n, sorted_suffixes);
            
            // Calcola i rank usando l'array 'sorted_suffixes'
            int current_rank = 0;
            rank_array[sorted_suffixes[0].index] = current_rank;
            for (int i = 1; i < n; i++) {
                // Confronta usando il comparator
                if (compare_suffixes(&sorted_suffixes[i], &sorted_suffixes[i-1]) != 0) {
                    current_rank++;
                }
                rank_array[sorted_suffixes[i].index] = current_rank;
            }

            if (current_rank == n - 1) terminate = 1;
        }

        // Broadcast della terminazione
        MPI_Bcast(&terminate, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (terminate) break;

        // Broadcast dei rank
        MPI_Bcast(rank_array, n, MPI_INT, 0, MPI_COMM_WORLD);

        // Aggiornamento locale
        for (int i = 0; i < local_n; i++) {
            int global_idx = local_suffixes[i].index;
            int next_index = global_idx + k / 2;
            local_suffixes[i].rank[0] = rank_array[global_idx];
            local_suffixes[i].rank[1] = (next_index < n) ? rank_array[next_index] : -1;
        }
    } 

    // Finalizzazione
    if (rank == 0) {
        // Assicura che sa->sa contenga l'array ordinato finale
        // (che si trova in sorted_suffixes)
        for (int i = 0; i < n; i++) {
            sa->sa[i] = sorted_suffixes[i].index;
        }
    }

    if(rank != 0 && sa->sa == NULL) {
        sa->sa = (int*)malloc(n * sizeof(int));
        assert(sa->sa != NULL);
    }
    // Trasmette l'array finale sa->sa a tutti
    MPI_Bcast(sa->sa, n, MPI_INT, 0, MPI_COMM_WORLD);

    // Cleanup
    MPI_Type_free(&suffix_mpi_type);
    if (rank == 0) {
        free(all_suffixes);       // Libera il buffer di Gatherv
        free(sorted_suffixes);    // Libera il buffer di merge
        free(recvcounts_structs);
        free(displs_structs);
    }
    free(local_suffixes);
    free(rank_array);
}