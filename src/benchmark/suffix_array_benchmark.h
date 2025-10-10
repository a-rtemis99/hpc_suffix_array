#ifndef SUFFIX_ARRAY_BENCHMARK_H
#define SUFFIX_ARRAY_BENCHMARK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

typedef struct {
    double total_time;
    double sa_construction_time;
    double lcp_construction_time;
    double lrs_search_time;
    size_t memory_used;
    int string_length;
    char* implementation;
    char* input_type;
} BenchmarkResult;

// Function declarations
double get_current_time();
BenchmarkResult* run_benchmark(const char* input_string, int length, const char* impl_name);
void save_results_to_csv(BenchmarkResult** results, int num_results, const char* filename);
void generate_benchmark_charts(const char* csv_filename);
char* generate_random_string(int length);
char* generate_repetitive_string(int length, int pattern_length);

#endif