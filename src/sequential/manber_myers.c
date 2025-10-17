// src/sequential/manber_myers.c

#include "../common/suffix_array.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Si aggiunge 1 per mappare il rank -1 (carattere nullo) a 0.
static inline int get_rank_val(int r) {
    return r + 1;
}

// Counting sort stabile per i rank (usato da Radix Sort)
void counting_sort_radix_seq(Suffix* in, Suffix* out, int n, int rank_pass, int max_rank) {
    int* count = (int*)calloc(max_rank + 1, sizeof(int));
    if (!count) return;

    for (int i = 0; i < n; i++) {
        count[get_rank_val(in[i].rank[rank_pass])]++;
    }
    
    for (int i = 1; i <= max_rank; i++) {
        count[i] += count[i - 1];
    }
    
    for (int i = n - 1; i >= 0; i--) {
        int r_val = get_rank_val(in[i].rank[rank_pass]);
        out[count[r_val] - 1] = in[i];
        count[r_val]--;
    }
    
    free(count);
}

// Radix sort sequenziale per le coppie di rank
void radix_sort_suffixes_seq(Suffix* suffixes, int n, int max_rank_val) {
    Suffix* temp_suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    if(!temp_suffixes) return;

    // Ordina per il secondo rank (meno significativo)
    counting_sort_radix_seq(suffixes, temp_suffixes, n, 1, max_rank_val);
    
    // Ordina per il primo rank (più significativo) in modo stabile
    counting_sort_radix_seq(temp_suffixes, suffixes, n, 0, max_rank_val);
    
    free(temp_suffixes);
}


SuffixArray* create_suffix_array(const char* S, int n) {
    SuffixArray* sa = (SuffixArray*)malloc(sizeof(SuffixArray));
    if (!sa) return NULL;
    sa->n = n;
    sa->str = (char*)malloc((n + 1) * sizeof(char));
    if (!sa->str) { free(sa); return NULL; }
    strncpy(sa->str, S, n);
    sa->str[n] = '\0';
    sa->sa = (int*)malloc(n * sizeof(int));
    sa->lcp = (int*)malloc(n * sizeof(int));
    if (!sa->sa || !sa->lcp) {
        free(sa->str);
        free(sa->sa);
        free(sa->lcp);
        free(sa);
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
    int* rank_array = (int*)malloc(n * sizeof(int));
    assert(suffixes && rank_array);

    // Inizializzazione
    for (int i = 0; i < n; i++) {
        suffixes[i].index = i;
        suffixes[i].rank[0] = sa->str[i];
        suffixes[i].rank[1] = (i + 1 < n) ? sa->str[i + 1] : -1;
    }

    int max_rank_value = 256; // Inizialmente i rank sono caratteri ASCII

    // Ciclo di raddoppio
    for (int k = 2; k < 2 * n; k *= 2) {
        radix_sort_suffixes_seq(suffixes, n, max_rank_value + 1);

        // Ricalcola i rank
        int current_rank = 0;
        rank_array[suffixes[0].index] = current_rank;
        for (int i = 1; i < n; i++) {
            if (suffixes[i].rank[0] != suffixes[i - 1].rank[0] ||
                suffixes[i].rank[1] != suffixes[i - 1].rank[1]) {
                current_rank++;
            }
            rank_array[suffixes[i].index] = current_rank;
        }
        max_rank_value = current_rank;
        
        // Condizione di terminazione anticipata
        if (max_rank_value == n - 1) break;

        // Aggiorna i rank per il prossimo ciclo
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

    for (int i = 0; i < n; i++) {
        sa->sa[i] = suffixes[i].index;
    }

    free(suffixes);
    free(rank_array);
}

void build_lcp_array(SuffixArray* sa) {
    int n = sa->n;
    int* rank = (int*)malloc(n * sizeof(int));
    if (!rank) return;

    for (int i = 0; i < n; i++) {
        rank[sa->sa[i]] = i;
    }

    int h = 0;
    sa->lcp[0] = 0;
    for (int i = 0; i < n; i++) {
        if (rank[i] > 0) {
            int j = sa->sa[rank[i] - 1];
            while (i + h < n && j + h < n && sa->str[i + h] == sa->str[j + h]) {
                h++;
            }
            sa->lcp[rank[i]] = h;
            if (h > 0) h--;
        }
    }
    free(rank);
}

char* find_longest_repeated_substring(SuffixArray* sa) {
    if (!sa || !sa->lcp) return NULL;
    
    int max_lcp = 0;
    int max_index = -1;
    for (int i = 1; i < sa->n; i++) {
        if (sa->lcp[i] > max_lcp) {
            max_lcp = sa->lcp[i];
            max_index = i;
        }
    }

    if (max_lcp == 0) {
        return NULL;
    }

    char* result = (char*)malloc((max_lcp + 1) * sizeof(char));
    if (!result) return NULL;
    
    // Si usa l'indice del suffisso per estrarre la sottostringa
    strncpy(result, sa->str + sa->sa[max_index], max_lcp);
    result[max_lcp] = '\0';
    return result;
}

int is_valid_suffix_array(SuffixArray* sa) {
    int* seen = (int*)calloc(sa->n, sizeof(int));
    if (!seen) return 0;
    for (int i = 0; i < sa->n; i++) {
        if (sa->sa[i] < 0 || sa->sa[i] >= sa->n || seen[sa->sa[i]]) {
            free(seen);
            return 0;
        }
        seen[sa->sa[i]] = 1;
    }
    for (int i = 1; i < sa->n; i++) {
        if (strcmp(sa->str + sa->sa[i - 1], sa->str + sa->sa[i]) > 0) {
            free(seen);
            return 0;
        }
    }
    free(seen);
    return 1;
}