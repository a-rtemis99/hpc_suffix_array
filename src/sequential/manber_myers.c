#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/suffix_array.h"

// Implementazione basata su: https://gist.github.com/sumanth232/e1600b327922b6947f51

typedef struct {
    int index;
    int rank[2];
} Suffix;

int compare_suffix(const void* a, const void* b) {
    Suffix* sa = (Suffix*)a;
    Suffix* sb = (Suffix*)b;
    
    if (sa->rank[0] == sb->rank[0]) {
        return sa->rank[1] - sb->rank[1];
    }
    return sa->rank[0] - sb->rank[0];
}

void build_suffix_array(const char* text, int n, int* sa) {
    if (n <= 0) return;
    
    Suffix* suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    int* rank = (int*)malloc(n * sizeof(int));
    int i, k;
    
    // Inizializzazione per k=1
    for (i = 0; i < n; i++) {
        sa[i] = i;
        rank[i] = text[i];
    }
    
    // Fasi di raddoppio
    for (k = 1; k < n; k *= 2) {
        // Prepara l'array dei suffissi per l'ordinamento
        for (i = 0; i < n; i++) {
            suffixes[i].index = i;
            suffixes[i].rank[0] = rank[i];
            suffixes[i].rank[1] = (i + k < n) ? rank[i + k] : -1;
        }
        
        // Ordina i suffissi
        qsort(suffixes, n, sizeof(Suffix), compare_suffix);
        
        // Aggiorna il suffix array
        for (i = 0; i < n; i++) {
            sa[i] = suffixes[i].index;
        }
        
        // Aggiorna i rank
        int new_rank = 0;
        rank[sa[0]] = new_rank;
        
        for (i = 1; i < n; i++) {
            if (suffixes[i].rank[0] != suffixes[i-1].rank[0] || 
                suffixes[i].rank[1] != suffixes[i-1].rank[1]) {
                new_rank++;
            }
            rank[sa[i]] = new_rank;
        }
        
        // Se tutti i rank sono distinti, abbiamo finito
        if (new_rank == n - 1) {
            break;
        }
    }
    
    free(suffixes);
    free(rank);
}

void build_lcp_array(const char* text, int n, const int* sa, int* lcp) {
    if (n <= 0) return;
    
    int* rank = (int*)malloc(n * sizeof(int));
    int i, j, h;
    
    // Costruisci l'array dei rank
    for (i = 0; i < n; i++) {
        rank[sa[i]] = i;
    }
    
    h = 0;
    lcp[0] = 0;
    
    for (i = 0; i < n; i++) {
        if (rank[i] > 0) {
            j = sa[rank[i] - 1];
            
            // Calcola LCP tra text[i..] e text[j..]
            while (i + h < n && j + h < n && text[i + h] == text[j + h]) {
                h++;
            }
            
            lcp[rank[i]] = h;
            
            if (h > 0) h--;
        }
    }
    
    free(rank);
}

int find_longest_repeated_substring(const char* text, int n, char* result) {
    if (n <= 1) {
        result[0] = '\0';
        return 0;
    }
    
    int* sa = (int*)malloc(n * sizeof(int));
    int* lcp = (int*)malloc(n * sizeof(int));
    
    build_suffix_array(text, n, sa);
    build_lcp_array(text, n, sa, lcp);
    
    // Trova il massimo LCP
    int max_lcp = 0;
    int max_index = 0;
    
    for (int i = 1; i < n; i++) {
        if (lcp[i] > max_lcp) {
            max_lcp = lcp[i];
            max_index = i;
        }
    }
    
    // Copia la sottostringa ripetuta più lunga
    if (max_lcp > 0) {
        strncpy(result, text + sa[max_index], max_lcp);
        result[max_lcp] = '\0';
    } else {
        result[0] = '\0';
    }
    
    free(sa);
    free(lcp);
    return max_lcp;
}

// Funzioni di utilità
int is_valid_suffix_array(const char* text, int n, const int* sa) {
    int* seen = (int*)calloc(n, sizeof(int));
    if (!seen) return 0;
    
    for (int i = 0; i < n; i++) {
        if (sa[i] < 0 || sa[i] >= n || seen[sa[i]]) {
            free(seen);
            return 0;
        }
        seen[sa[i]] = 1;
    }
    
    // Verifica l'ordinamento lessicografico
    for (int i = 1; i < n; i++) {
        if (strcmp(text + sa[i-1], text + sa[i]) > 0) {
            free(seen);
            return 0;
        }
    }
    
    free(seen);
    return 1;
}

void print_suffix_array(const char* text, int n, const int* sa) {
    printf("Indice | Posizione | Suffisso\n");
    printf("-------|-----------|----------\n");
    for (int i = 0; i < n; i++) {
        printf("%6d | %9d | %s\n", i, sa[i], text + sa[i]);
    }
}