// src/sequential/manber_myers.c
#include "../common/suffix_array.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Comparison function for sorting suffixes
int compare_suffix(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    
    if (s1->rank[0] == s2->rank[0]) {
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

SuffixArray* create_suffix_array(const char* S, int n) {
    SuffixArray* sa = (SuffixArray*)malloc(sizeof(SuffixArray));
    if (!sa) return NULL;
    
    sa->n = n;
    sa->str = (char*)malloc((n + 1) * sizeof(char));
    if (!sa->str) {
        free(sa);
        return NULL;
    }
    strncpy(sa->str, S, n);
    sa->str[n] = '\0';
    
    sa->sa = (int*)malloc(n * sizeof(int));
    sa->lcp = (int*)malloc(n * sizeof(int));
    
    if (!sa->sa || !sa->lcp) {
        destroy_suffix_array(sa);
        return NULL;
    }
    
    return sa;
}

void destroy_suffix_array(SuffixArray* sa) {
    if (sa) {
        free(sa->str);
        free(sa->sa);
        free(sa->lcp);
        free(sa);
    }
}

void build_suffix_array(SuffixArray* sa) {
    int n = sa->n;
    Suffix* suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    int* rank = (int*)malloc(n * sizeof(int));
    
    // Initialize suffixes with first character ranks
    for (int i = 0; i < n; i++) {
        suffixes[i].index = i;
        suffixes[i].rank[0] = sa->str[i];
        suffixes[i].rank[1] = ((i + 1) < n) ? sa->str[i + 1] : -1;
    }
    
    // Sort suffixes by first 2 characters
    qsort(suffixes, n, sizeof(Suffix), compare_suffix);
    
    // Main doubling phases
    for (int k = 4; k < 2 * n; k = k * 2) {
        // Assign new ranks to first suffix
        int new_rank = 0;
        rank[suffixes[0].index] = 0;
        
        // Assign ranks to remaining suffixes
        int prev_rank0 = suffixes[0].rank[0];
        int prev_rank1 = suffixes[0].rank[1];
        
        for (int i = 1; i < n; i++) {
            if (suffixes[i].rank[0] == prev_rank0 && 
                suffixes[i].rank[1] == prev_rank1) {
                suffixes[i].rank[0] = new_rank;
            } else {
                new_rank++;
                prev_rank0 = suffixes[i].rank[0];
                prev_rank1 = suffixes[i].rank[1];
                suffixes[i].rank[0] = new_rank;
            }
            rank[suffixes[i].index] = i;
        }
        
        // Assign next ranks
        for (int i = 0; i < n; i++) {
            int next_index = suffixes[i].index + k / 2;
            suffixes[i].rank[1] = (next_index < n) ? rank[next_index] : -1;
        }
        
        // Sort suffixes again
        qsort(suffixes, n, sizeof(Suffix), compare_suffix);
    }
    
    // Store the result in suffix array
    for (int i = 0; i < n; i++) {
        sa->sa[i] = suffixes[i].index;
    }
    
    free(suffixes);
    free(rank);
}

void build_lcp_array(SuffixArray* sa) {
    int n = sa->n;
    int* rank = (int*)malloc(n * sizeof(int));
    
    // Build inverse suffix array (rank)
    for (int i = 0; i < n; i++) {
        rank[sa->sa[i]] = i;
    }
    
    int h = 0; // Current LCP length
    sa->lcp[0] = 0;
    
    for (int i = 0; i < n; i++) {
        if (rank[i] > 0) {
            int j = sa->sa[rank[i] - 1];
            
            // Compute LCP between current suffix and previous one
            while (i + h < n && j + h < n && 
                   sa->str[i + h] == sa->str[j + h]) {
                h++;
            }
            
            sa->lcp[rank[i]] = h;
            
            if (h > 0) h--;
        }
    }
    
    free(rank);
}

char* find_longest_repeated_substring(SuffixArray* sa) {
    build_suffix_array(sa);
    build_lcp_array(sa);
    
    int max_lcp = 0;
    int max_index = 0;
    
    // Find maximum LCP value
    for (int i = 1; i < sa->n; i++) {
        if (sa->lcp[i] > max_lcp) {
            max_lcp = sa->lcp[i];
            max_index = i;
        }
    }
    
    if (max_lcp == 0) {
        return NULL; // No repeated substring found
    }
    
    // Extract the longest repeated substring
    char* result = (char*)malloc((max_lcp + 1) * sizeof(char));
    if (!result) return NULL;
    
    strncpy(result, sa->str + sa->sa[max_index], max_lcp);
    result[max_lcp] = '\0';
    
    return result;
}

int is_valid_suffix_array(SuffixArray* sa) {
    // Check that all indices are unique and within bounds
    int* seen = (int*)calloc(sa->n, sizeof(int));
    
    for (int i = 0; i < sa->n; i++) {
        if (sa->sa[i] < 0 || sa->sa[i] >= sa->n) {
            free(seen);
            return 0;
        }
        if (seen[sa->sa[i]]) {
            free(seen);
            return 0;
        }
        seen[sa->sa[i]] = 1;
    }
    
    // Check lexicographic order
    for (int i = 1; i < sa->n; i++) {
        char* suffix1 = sa->str + sa->sa[i - 1];
        char* suffix2 = sa->str + sa->sa[i];
        
        if (strcmp(suffix1, suffix2) > 0) {
            free(seen);
            return 0;
        }
    }
    
    free(seen);
    return 1;
}