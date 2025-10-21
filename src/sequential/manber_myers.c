// Basato su: https://gist.github.com/sumanth232/e1600b327922b6947f51

#include "../common/suffix_array.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Funzione di confronto per qsort
int compare_suffixes_seq(const void* a, const void* b) {
    Suffix* s1 = (Suffix*)a;
    Suffix* s2 = (Suffix*)b;
    if (s1->rank[0] == s2->rank[0]) {
        if (s1->rank[1] == s2->rank[1]) return 0;
        return (s1->rank[1] < s2->rank[1]) ? -1 : 1;
    }
    return (s1->rank[0] < s2->rank[0]) ? -1 : 1;
}

// Funzioni create/destroy
SuffixArray* create_suffix_array(const char* S, int n) {
    SuffixArray* sa = (SuffixArray*)malloc(sizeof(SuffixArray));
    assert(sa != NULL);
    sa->n = n;
    sa->str = (char*)malloc((n + 1) * sizeof(char));
    assert(sa->str != NULL);
    strncpy(sa->str, S, n);
    sa->str[n] = '\0';
    sa->sa = (int*)malloc(n * sizeof(int));
    sa->lcp = (int*)malloc(n * sizeof(int));
    assert(sa->sa != NULL && sa->lcp != NULL);
    // Inizializza LCP a 0 o valori non validi per sicurezza
    memset(sa->lcp, 0, n * sizeof(int));
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

// Algoritmo di Manber-Myers con qsort
void build_suffix_array(SuffixArray* sa) {
    int n = sa->n;
    Suffix* suffixes = (Suffix*)malloc(n * sizeof(Suffix));
    int* rank_array = (int*)malloc(n * sizeof(int)); // Array per mappare index -> rank temporaneo
    int* pos_in_sorted = (int*)malloc(n * sizeof(int)); // Array per mappare index -> posizione nell'array ordinato
    assert(suffixes != NULL && rank_array != NULL && pos_in_sorted != NULL);

    // Inizializzazione (k=1)
    for (int i = 0; i < n; i++) {
        suffixes[i].index = i;
        suffixes[i].rank[0] = (unsigned char)sa->str[i];
        suffixes[i].rank[1] = (i + 1 < n) ? (unsigned char)sa->str[i + 1] : -1;
    }

    // Primo ordinamento (basato sui primi 2 caratteri)
    qsort(suffixes, n, sizeof(Suffix), compare_suffixes_seq);

    // Ciclo di raddoppio
    for (int k = 4; k < 2 * n; k *= 2) { // k parte da 4 perché abbiamo già ordinato per 2 caratteri
        // Calcola i rank basati sull'ordinamento precedente (per k/2 caratteri)
        int current_rank = 0;
        int prev_rank0 = suffixes[0].rank[0];
        suffixes[0].rank[0] = current_rank;
        pos_in_sorted[suffixes[0].index] = 0; // Salva la posizione nell'array ordinato

        for (int i = 1; i < n; i++) {
            // Se la coppia di rank corrente è diversa dalla precedente, incrementa il rank
            if (suffixes[i].rank[0] != prev_rank0 ||
                suffixes[i].rank[1] != suffixes[i - 1].rank[1]) {
                current_rank++;
            }
            prev_rank0 = suffixes[i].rank[0]; // Salva il primo rank (che sta per essere sovrascritto)
            suffixes[i].rank[0] = current_rank; // Sovrascrive il primo rank con il nuovo rank calcolato
            pos_in_sorted[suffixes[i].index] = i;
        }

        // Se tutti i rank sono distinti, si ferma
        if (current_rank == n - 1) break;

        // Aggiorna il secondo rank (rank[1]) per la prossima iterazione
        for (int i = 0; i < n; i++) {
            int next_index = suffixes[i].index + k / 2;
            // Usa l'array pos_in_sorted per trovare il rank del suffisso che inizia a next_index
            suffixes[i].rank[1] = (next_index < n) ? suffixes[pos_in_sorted[next_index]].rank[0] : -1;
        }

        // Ordina di nuovo usando le coppie di rank aggiornate (ora basate su k caratteri)
        qsort(suffixes, n, sizeof(Suffix), compare_suffixes_seq);
    }

    // Copia il risultato finale (gli indici ordinati) in sa->sa
    for (int i = 0; i < n; i++) {
        sa->sa[i] = suffixes[i].index;
    }

    free(suffixes);
    free(rank_array);
    free(pos_in_sorted);
}


// Funzioni LCP, LRS, Validazione
void build_lcp_array(SuffixArray* sa) {
    int n = sa->n;
    int* rank = (int*)malloc(n * sizeof(int)); // rank[i] = posizione del suffisso i nell'array SA ordinato
    assert(rank != NULL);

    // Costruisce l'array rank (inverso di sa->sa)
    for (int i = 0; i < n; i++) {
        rank[sa->sa[i]] = i;
    }

    int h = 0; // Lunghezza LCP corrente
    sa->lcp[0] = 0; // LCP del primo suffisso (indice 0 in SA) è 0 per definizione
    for (int i = 0; i < n; i++) { // Scorre i suffissi nell'ordine originale della stringa
        if (rank[i] > 0) { // Se non è il primo suffisso nell'array ordinato
            int j = sa->sa[rank[i] - 1]; // Indice del suffisso precedente nell'array ordinato
            // Calcola LCP tra suffisso i e suffisso j
            while (i + h < n && j + h < n && sa->str[i + h] == sa->str[j + h]) {
                h++;
            }
            sa->lcp[rank[i]] = h; // Salva LCP nella posizione corretta dell'array LCP
            if (h > 0) h--; // Proprietà di Kasai: LCP successivo è almeno h-1
        }
    }
    free(rank);
}

char* find_longest_repeated_substring(SuffixArray* sa) {
     if (!sa || !sa->lcp || sa->n <= 1) return NULL;

    int max_lcp = 0;
    int max_index = -1; // Indice in SA dove si trova LCP massimo
    for (int i = 1; i < sa->n; i++) { // Inizia da 1 perché lcp[0] è 0
        if (sa->lcp[i] > max_lcp) {
            max_lcp = sa->lcp[i];
            max_index = i; // Salva l'indice in SA dove inizia il suffisso con LCP max
        }
    }

    if (max_lcp == 0) return NULL; // Nessuna sottostringa ripetuta trovata

    char* result = (char*)malloc((max_lcp + 1) * sizeof(char));
    assert(result != NULL);

    // Estrae la sottostringa dall'indice corretto in SA
    strncpy(result, sa->str + sa->sa[max_index], max_lcp);
    result[max_lcp] = '\0';
    return result;
}

int is_valid_suffix_array(SuffixArray* sa) {
    if (!sa || !sa->sa || sa->n <= 0) return 0; 
    int n = sa->n;
    int* seen = (int*)calloc(n, sizeof(int));
    assert(seen != NULL);

    for (int i = 0; i < n; i++) {
        if (sa->sa[i] < 0 || sa->sa[i] >= n || seen[sa->sa[i]]) {
            free(seen);
            return 0; // Indice fuori range o duplicato
        }
        seen[sa->sa[i]] = 1;
    }
    free(seen);

    // Verifica ordinamento lessicografico
    for (int i = 1; i < n; i++) {
        if (strcmp(sa->str + sa->sa[i - 1], sa->str + sa->sa[i]) > 0) {
            return 0; // Non ordinato
        }
    }
    return 1; // Tutto ok
}