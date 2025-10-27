#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> 
#include <cuda_runtime.h> 
#include <unistd.h> 
#include "../common/suffix_array.h" 
#include "../common/utils.h"

// Definizione Macro Error Checking CUDA 
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true) {
   if (code != cudaSuccess) {
      fprintf(stderr,"GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}
// Macro da usare nel codice
#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }


// Prototipo della funzione CUDA (definita in manber_myers.cu)
void build_suffix_array_cuda(SuffixArray* sa_host);

// Prototipi funzioni C (linkage gestito dall'header)
void build_lcp_array(SuffixArray* sa);
char* find_longest_repeated_substring(SuffixArray* sa);
int is_valid_suffix_array(SuffixArray* sa);

// Funzione helper per timing CPU
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

// Funzione per stampare output strutturato
void print_structured_results_for_script(long string_length, double sa_time, double lcp_time, double total_time) {
    printf("\n--- STRUCTURED_RESULTS ---\n");
    printf("ACTUAL_STRING_LENGTH:%ld\n", string_length);
    printf("MPI_PROCESSES:1\n");
    printf("SA_TIME:%.6f\n", sa_time);
    printf("LCP_TIME:%.6f\n", lcp_time);
    printf("TOTAL_TIME:%.6f\n", total_time);
    printf("--- END_STRUCTURED_RESULTS ---\n");
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file_or_string>\n", argv[0]);
        return 1;
    }

    char* input_str_original = NULL;
    long n = 0;
    const char* filename = argv[1];
    int input_is_file = 0;

    // Gestione Input (solo su host)
    if (strchr(argv[1], '/') != NULL || strchr(argv[1], '.') != NULL) {
        input_is_file = 1;
        printf("Reading from file: %s\n", argv[1]);
        input_str_original = read_file(argv[1], &n);
        if (!input_str_original) {
            fprintf(stderr, "Error: Failed to read input file '%s'\n", argv[1]);
            return 1;
        }
        printf("File read successfully: %s (%ld bytes)\n", argv[1], n);
    } else {
        input_is_file = 0;
        input_str_original = argv[1];
        n = strlen(input_str_original);
        filename = "direct_string";
        printf("Input string: %s (length %ld)\n", input_str_original, n);
    }

    // TIMING CON EVENTI CUDA
    cudaEvent_t start_event, stop_event, mid_event;
    gpuErrchk(cudaEventCreate(&start_event));
    gpuErrchk(cudaEventCreate(&mid_event));
    gpuErrchk(cudaEventCreate(&stop_event));

    // Crea struttura SuffixArray su Host
    SuffixArray* sa = create_suffix_array(input_str_original, n);
    if (!sa) {
        fprintf(stderr, "Error: Failed to create suffix array structure (memory allocation?)\n");
        if (input_is_file) free(input_str_original);
        return 1;
    }
    if (input_is_file) {
        free(input_str_original);
        input_str_original = NULL;
    }

    // ESECUZIONE CUDA
    printf("Starting CUDA Suffix Array construction...\n");
    gpuErrchk(cudaEventRecord(start_event, 0)); // Marca l'inizio

    build_suffix_array_cuda(sa); // Chiamata alla funzione principale CUDA

    gpuErrchk(cudaEventRecord(mid_event, 0)); // Marca la fine della costruzione SA

    // FASI SEQUENZIALI POST-CUDA (su Host)
    build_lcp_array(sa);
    char* lrs = find_longest_repeated_substring(sa);

    gpuErrchk(cudaEventRecord(stop_event, 0)); // Marca la fine di tutto

    // Sincronizza CPU e GPU e calcola i tempi dagli eventi
    gpuErrchk(cudaEventSynchronize(stop_event));
    float sa_time_ms = 0.0f, total_time_ms = 0.0f;
    gpuErrchk(cudaEventElapsedTime(&sa_time_ms, start_event, mid_event));
    gpuErrchk(cudaEventElapsedTime(&total_time_ms, start_event, stop_event));

    double sa_construction_time_s = sa_time_ms / 1000.0;
    double lcp_search_time_s = (total_time_ms > sa_time_ms) ? (total_time_ms - sa_time_ms) / 1000.0 : 0.0; // Evita tempi negativi
    double total_execution_time_s = total_time_ms / 1000.0;

    // Validazione e stampa risultati
    int valid = is_valid_suffix_array(sa);
    printf("\n=== RESULTS ===\n");
    printf("Valid suffix array: %s\n", valid ? "YES" : "NO");
    if (lrs) {
        printf("Longest repeated substring: '%s' (length: %zu)\n", lrs, strlen(lrs));
    } else {
        printf("No repeated substring found or string too short.\n");
    }
    printf("Suffix array construction time (CUDA): %.6f seconds\n", sa_construction_time_s);
    printf("LCP construction + LRS search time (CPU): %.6f seconds\n", lcp_search_time_s);
    printf("Total execution time (CUDA + CPU): %.6f seconds\n", total_execution_time_s);

    // Stampa output strutturato per script
    print_structured_results_for_script(n, sa_construction_time_s, lcp_search_time_s, total_execution_time_s);

    // Cleanup
    gpuErrchk(cudaEventDestroy(start_event));
    gpuErrchk(cudaEventDestroy(mid_event));
    gpuErrchk(cudaEventDestroy(stop_event));
    free(lrs);
    destroy_suffix_array(sa);

    return 0;
}