#include "suffix_array_benchmark.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== SUFFIX ARRAY BENCHMARK SUITE ===\n\n");
    
    // Configurazione benchmark
    int sizes[] = {1000, 5000, 10000, 50000, 100000, 500000, 1000000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    int repetitions = 3;
    
    // Array per memorizzare i risultati
    BenchmarkResult** all_results = malloc(num_sizes * repetitions * sizeof(BenchmarkResult*));
    int result_count = 0;
    
    // Esegui benchmark per ogni dimensione
    for (int i = 0; i < num_sizes; i++) {
        int size = sizes[i];
        printf("Testing size: %d\n", size);
        
        for (int rep = 0; rep < repetitions; rep++) {
            printf("  Repetition %d/%d... ", rep + 1, repetitions);
            
            // Genera stringa di test
            char* test_string = generate_random_string(size);
            
            // Esegui benchmark
            BenchmarkResult* result = run_benchmark(test_string, size, "sequential");
            
            if (result) {
                all_results[result_count++] = result;
                printf("Completed (%.3f s)\n", result->total_time);
            } else {
                printf("FAILED\n");
            }
            
            free(test_string);
        }
        printf("\n");
    }
    
    // Salva risultati
    save_results_to_csv(all_results, result_count, "results/csv/benchmark_results_sequential.csv");
    
    // Pulizia
    for (int i = 0; i < result_count; i++) {
        free(all_results[i]->implementation);
        free(all_results[i]->input_type);
        free(all_results[i]);
    }
    free(all_results);
    
    printf("Benchmark completed! Results saved to results/csv/\n");
    return 0;
}