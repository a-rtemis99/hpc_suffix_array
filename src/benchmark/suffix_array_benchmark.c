// src/benchmark/suffix_array_benchmark.c
#include "suffix_array_benchmark.h"
#include "../common/suffix_array.h"

// Aggiungi questa funzione helper per sostituire strdup
char* duplicate_string(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
        strcpy(copy, str);
    }
    return copy;
}

double get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

BenchmarkResult* run_benchmark(const char* input_string, int length, const char* impl_name) {
    BenchmarkResult* result = (BenchmarkResult*)malloc(sizeof(BenchmarkResult));
    result->string_length = length;
    result->implementation = duplicate_string(impl_name);
    result->input_type = duplicate_string("random");
    
    double start_total = get_current_time();
    
    // Costruzione Suffix Array
    double start_sa = get_current_time();
    SuffixArray* sa = create_suffix_array(input_string, length);
    if (!sa) {
        free(result->implementation);
        free(result->input_type);
        free(result);
        return NULL;
    }
    build_suffix_array(sa);
    double end_sa = get_current_time();
    
    // Costruzione LCP Array
    double start_lcp = get_current_time();
    build_lcp_array(sa);
    double end_lcp = get_current_time();
    
    // Ricerca LRS
    double start_lrs = get_current_time();
    char* lrs = find_longest_repeated_substring(sa);
    double end_lrs = get_current_time();
    
    double end_total = get_current_time();
    
    // Calcolo metriche
    result->total_time = end_total - start_total;
    result->sa_construction_time = end_sa - start_sa;
    result->lcp_construction_time = end_lcp - start_lcp;
    result->lrs_search_time = end_lrs - start_lrs;
    
    // Stima memoria usata (approssimativa)
    result->memory_used = length * sizeof(int) * 3; // SA, LCP, Rank
    
    // Cleanup
    free(lrs);
    destroy_suffix_array(sa);
    
    return result;
}

void save_results_to_csv(BenchmarkResult** results, int num_results, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error opening file %s\n", filename);
        return;
    }
    
    // Header CSV
    fprintf(file, "implementation,input_type,string_length,total_time,sa_time,lcp_time,lrs_time,memory_used\n");
    
    // Dati
    for (int i = 0; i < num_results; i++) {
        if (results[i]) {
            fprintf(file, "%s,%s,%d,%.6f,%.6f,%.6f,%.6f,%zu\n",
                   results[i]->implementation,
                   results[i]->input_type,
                   results[i]->string_length,
                   results[i]->total_time,
                   results[i]->sa_construction_time,
                   results[i]->lcp_construction_time,
                   results[i]->lrs_search_time,
                   results[i]->memory_used);
        }
    }
    
    fclose(file);
    printf("Results saved to %s\n", filename);
}

char* generate_random_string(int length) {
    char* str = (char*)malloc(length + 1);
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    for (int i = 0; i < length; i++) {
        int key = rand() % (int)(sizeof(charset) - 1);
        str[i] = charset[key];
    }
    str[length] = '\0';
    
    return str;
}

char* generate_repetitive_string(int length, int pattern_length) {
    char* str = (char*)malloc(length + 1);
    char* pattern = generate_random_string(pattern_length);
    
    for (int i = 0; i < length; i++) {
        str[i] = pattern[i % pattern_length];
    }
    str[length] = '\0';
    
    free(pattern);
    return str;
}