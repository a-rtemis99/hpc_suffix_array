#include "../common/suffix_array.h"
#include <cuda_runtime.h>
#include <thrust/device_vector.h>
#include <thrust/sort.h>
#include <thrust/copy.h>
#include <vector>       // <--- AGGIUNTO (per std::vector)
#include <algorithm>    // <--- AGGIUNTO (per std::sort)
#include <cassert>
#include <iostream>

// Prototipo della funzione sequenziale (per la strategia ibrida)
extern "C" {
    void build_suffix_array(SuffixArray* sa);
}

// Funzione helper per controllare errori CUDA
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true) {
   if (code != cudaSuccess) {
      fprintf(stderr,"GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}
#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }


// Functor di confronto per Thrust (invariato)
struct SuffixComparator {
    __host__ __device__
    bool operator()(const Suffix& a, const Suffix& b) const {
        if (a.rank[0] != b.rank[0]) {
            return a.rank[0] < b.rank[0];
        }
        return a.rank[1] < b.rank[1];
    }
};


// ====================================================================
// NUOVO KERNEL: Aggiorna i Rank (prima del sort)
// Aggiorna rank[0] e rank[1] per ogni suffisso usando
// l'array d_rank_array calcolato nel passo precedente.
// ====================================================================
__global__
void kernel_update_suffixes(Suffix* d_suffixes, const int* d_rank_array, int k_half, int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= n) return;

    // NOTA: Leggiamo da d_suffixes[tid].index per trovare il *nostro* indice originale
    int index = d_suffixes[tid].index;
    int next_index = index + k_half;

    // Scriviamo nei *nostri* rank (d_suffixes[tid].rank)
    d_suffixes[tid].rank[0] = d_rank_array[index];
    d_suffixes[tid].rank[1] = (next_index < n) ? d_rank_array[next_index] : -1;
}


// ====================================================================
// VERSIONE OTTIMIZZATA (v2) di build_suffix_array_cuda
//
// STRATEGIA: GPU-Resident (quasi)
// 1. Dati Host -> Device (UNA VOLTA)
// 2. Loop (k=...):
//    a. Lancia kernel_update_suffixes (GPU)
//    b. Lancia thrust::stable_sort (GPU)
//    c. Dati Device -> Host (SOLO i rank per il calcolo) (Overhead)
//    d. Calcolo Rank (CPU) (Seriale)
//    e. Dati Host -> Device (SOLO i rank) (Overhead)
// 3. Dati Device -> Host (UNA VOLTA)
//
// ====================================================================

extern "C" // Esporta questa funzione con linkage C per main_cuda.cu
void build_suffix_array_cuda(SuffixArray* sa_host) {
    int n = sa_host->n;

    // STRATEGIA IBRIDA (invariata)
    if (n < 5000000) {
        build_suffix_array(sa_host); // Usa la versione sequenziale C (qsort)
        return;
    }

    // --- 1. PREPARAZIONE DATI HOST ---
    std::vector<Suffix> h_suffixes(n);
    int* h_rank_array = (int*)malloc(n * sizeof(int));
    assert(h_rank_array != NULL);

    // Inizializzazione su Host (solo la prima volta)
    for (int i = 0; i < n; i++) {
        h_suffixes[i].index = i;
        h_suffixes[i].rank[0] = (unsigned char)sa_host->str[i];
        h_suffixes[i].rank[1] = (i + 1 < n) ? (unsigned char)sa_host->str[i + 1] : -1;
    }

    // Ordina la prima versione per calcolare i rank iniziali (k=2)
    // *** QUI C'ERA L'ERRORE ***
    std::sort(h_suffixes.begin(), h_suffixes.end(), SuffixComparator());
    
    // Calcolo Rank (k=2) su Host
    int current_rank = 0;
    // NON serve una copia, basta sovrascrivere h_rank_array
    
    h_rank_array[h_suffixes[0].index] = current_rank;
    for (int i = 1; i < n; i++) {
        if (h_suffixes[i].rank[0] != h_suffixes[i - 1].rank[0] ||
            h_suffixes[i].rank[1] != h_suffixes[i - 1].rank[1]) {
            current_rank++;
        }
        h_rank_array[h_suffixes[i].index] = current_rank;
    }


    // --- 2. PREPARAZIONE DATI DEVICE ---
    thrust::device_vector<Suffix> d_suffixes(n);
    thrust::device_vector<int> d_rank_array(n);

    // Copia i dati Suffix (già ordinati per k=2) su GPU (UNA SOLA VOLTA)
    gpuErrchk(cudaMemcpy(thrust::raw_pointer_cast(d_suffixes.data()),
                         h_suffixes.data(),
                         n * sizeof(Suffix),
                         cudaMemcpyHostToDevice));

    // Copia i dati Rank (calcolati per k=2) su GPU (UNA SOLA VOLTA)
    gpuErrchk(cudaMemcpy(thrust::raw_pointer_cast(d_rank_array.data()),
                         h_rank_array,
                         n * sizeof(int),
                         cudaMemcpyHostToDevice));
                         
    int threadsPerBlock = 256;
    int blocksPerGrid = (n + threadsPerBlock - 1) / threadsPerBlock;

    // --- 3. CICLO DI RADDOPPIO (Orchestrazione Host, Lavoro GPU) ---
    int k;
    for (k = 4; k < 2 * n; k *= 2) {
        
        // 3a. KERNEL: Aggiorna rank[0] e rank[1] di d_suffixes
        //      usando i d_rank_array calcolati nel ciclo precedente.
        kernel_update_suffixes<<<blocksPerGrid, threadsPerBlock>>>(
            thrust::raw_pointer_cast(d_suffixes.data()),
            thrust::raw_pointer_cast(d_rank_array.data()),
            k / 2,
            n
        );
        gpuErrchk(cudaPeekAtLastError());
        gpuErrchk(cudaDeviceSynchronize());

        // 3b. THRUST: Ordina d_suffixes in base ai nuovi rank
        try {
            thrust::stable_sort(d_suffixes.begin(), d_suffixes.end(), SuffixComparator());
        } catch (const thrust::system_error &e) {
            std::cerr << "Thrust error during sort: " << e.what() << std::endl;
            gpuErrchk(cudaGetLastError());
            exit(1);
        }
        gpuErrchk(cudaDeviceSynchronize());

        // 3c. COPIA DtoH: Recupera l'array ordinato SULLA CPU
        //      Questo è ancora un collo di bottiglia, ma è l'unico modo
        //      per calcolare i rank serialmente sulla CPU.
        gpuErrchk(cudaMemcpy(h_suffixes.data(),
                             thrust::raw_pointer_cast(d_suffixes.data()),
                             n * sizeof(Suffix),
                             cudaMemcpyDeviceToHost));

        // 3d. CPU: Calcolo dei nuovi Rank (seriale, ma necessario)
        current_rank = 0;
        h_rank_array[h_suffixes[0].index] = current_rank;
        for (int i = 1; i < n; i++) {
            if (h_suffixes[i].rank[0] != h_suffixes[i - 1].rank[0] ||
                h_suffixes[i].rank[1] != h_suffixes[i - 1].rank[1]) {
                current_rank++;
            }
            h_rank_array[h_suffixes[i].index] = current_rank;
        }

        // 3e. CONTROLLO TERMINAZIONE
        if (current_rank == n - 1) {
            break; // Ordinamento completo
        }

        // 3f. COPIA HtoD: Aggiorna d_rank_array sulla GPU per il prossimo ciclo
        gpuErrchk(cudaMemcpy(thrust::raw_pointer_cast(d_rank_array.data()),
                             h_rank_array,
                             n * sizeof(int),
                             cudaMemcpyHostToDevice));
    }

    // --- 4. FINALIZZAZIONE ---
    // h_suffixes contiene già l'ultimo array ordinato dal passo 3c
    for (int i = 0; i < n; i++) {
        sa_host->sa[i] = h_suffixes[i].index;
    }

    // Cleanup memoria host
    free(h_rank_array);
    // d_suffixes e d_rank_array (thrust::device_vector) sono deallocati automaticamente.
}