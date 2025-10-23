#include "../common/suffix_array.h" // Per la definizione di SuffixArray e Suffix
#include <cuda_runtime.h>
#include <thrust/device_vector.h>
#include <thrust/sort.h>
#include <thrust/copy.h>
#include <vector>
#include <cassert>
#include <iostream> // Per eventuali messaggi di debug

// Prototipo della funzione sequenziale (per la strategia ibrida)
// Assicurati che sia dichiarata extern "C" se manber_myers.c è C puro
extern "C" {
    void build_suffix_array(SuffixArray* sa);
}


// --- Functor di confronto per Thrust ---
// Questo oggetto dice a Thrust come confrontare due strutture Suffix
// basandosi sulla coppia di rank (rank[0], rank[1]).
// Deve avere `__host__ __device__` per poter essere usato sia da CPU che GPU.
struct SuffixComparator {
    __host__ __device__
    bool operator()(const Suffix& a, const Suffix& b) const {
        if (a.rank[0] != b.rank[0]) {
            return a.rank[0] < b.rank[0];
        }
        // Se rank[0] è uguale, confronta rank[1]
        return a.rank[1] < b.rank[1];
    }
};

// Funzione helper per controllare errori CUDA
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true) {
   if (code != cudaSuccess) {
      fprintf(stderr,"GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}
#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }


// Funzione principale CUDA
extern "C" // Esporta questa funzione con linkage C per main_cuda.cu
void build_suffix_array_cuda(SuffixArray* sa_host) {
    int n = sa_host->n;

    // --- STRATEGIA IBRIDA ---
    // Per input piccoli (< 5MB), esegui sequenziale sulla CPU
    if (n < 5000000) {
        build_suffix_array(sa_host); // Usa la versione sequenziale C (qsort)
        return; // Abbiamo finito
    }

    // --- PREPARAZIONE DATI HOST ---
    std::vector<Suffix> h_suffixes(n);
    int* h_rank_array = (int*)malloc(n * sizeof(int));
    assert(h_rank_array != NULL);

    // Inizializzazione su Host
    for (int i = 0; i < n; i++) {
        h_suffixes[i].index = i;
        h_suffixes[i].rank[0] = (unsigned char)sa_host->str[i];
        h_suffixes[i].rank[1] = (i + 1 < n) ? (unsigned char)sa_host->str[i + 1] : -1;
    }

    // --- PREPARAZIONE DATI DEVICE ---
    thrust::device_vector<Suffix> d_suffixes(n);

    // --- CICLO DI RADDOPPIO (Logica principale su Host, Sort su Device) ---
    int k;
    for (k = 4; k < 2 * n; k *= 2) { // k parte da 4

        // 1. Copia dati Host -> Device
        gpuErrchk(cudaMemcpy(thrust::raw_pointer_cast(d_suffixes.data()),
                             h_suffixes.data(),
                             n * sizeof(Suffix),
                             cudaMemcpyHostToDevice));

        // 2. Ordinamento su Device usando Thrust
        // Usiamo stable_sort per mantenere l'ordine relativo in caso di rank uguali (importante)
        try {
            thrust::stable_sort(d_suffixes.begin(), d_suffixes.end(), SuffixComparator());
        } catch (const thrust::system_error &e) {
            std::cerr << "Thrust error during sort: " << e.what() << std::endl;
            cudaDeviceSynchronize(); // Assicura che tutti gli errori kernel siano riportati
            gpuErrchk(cudaGetLastError()); // Controlla errori CUDA asincroni
            exit(1); // Esce in caso di errore Thrust/CUDA
        }
         gpuErrchk(cudaDeviceSynchronize()); // Sincronizza per essere sicuri che sort sia finito

        // 3. Copia dati ordinati Device -> Host
        gpuErrchk(cudaMemcpy(h_suffixes.data(),
                             thrust::raw_pointer_cast(d_suffixes.data()),
                             n * sizeof(Suffix),
                             cudaMemcpyDeviceToHost));

        // 4. Calcolo Rank su Host (sequenziale, ma veloce)
        int current_rank = 0;
        h_rank_array[h_suffixes[0].index] = current_rank;
        for (int i = 1; i < n; i++) {
            // Confronto diretto della coppia di rank precedente
            if (h_suffixes[i].rank[0] != h_suffixes[i - 1].rank[0] ||
                h_suffixes[i].rank[1] != h_suffixes[i - 1].rank[1]) {
                current_rank++;
            }
            h_rank_array[h_suffixes[i].index] = current_rank;
        }

        // 5. Controlla Terminazione
        if (current_rank == n - 1) {
            break; // Ordinamento completo
        }

        // 6. Aggiorna Rank su Host per il prossimo ciclo
        for (int i = 0; i < n; i++) {
            int next_index = h_suffixes[i].index + k / 2;
            h_suffixes[i].rank[0] = h_rank_array[h_suffixes[i].index];
            h_suffixes[i].rank[1] = (next_index < n) ? h_rank_array[next_index] : -1;
        }
    } // Fine ciclo for

    // --- FINALIZZAZIONE ---
    // Copia gli indici finali (già ordinati su h_suffixes) nell'output sa_host->sa
    for (int i = 0; i < n; i++) {
        sa_host->sa[i] = h_suffixes[i].index;
    }

    // Cleanup memoria host
    free(h_rank_array);
    // d_suffixes (thrust::device_vector) viene deallocata automaticamente quando esce dallo scope
}