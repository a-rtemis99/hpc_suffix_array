#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../common/suffix_array.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <string>\n", argv[0]);
        printf("Example: %s \"banana\"\n", argv[0]);
        return 1;
    }
    
    const char* text = argv[1];
    int n = strlen(text);
    
    printf("🔍 Analisi stringa: \"%s\" (lunghezza: %d)\n", text, n);
    printf("📚 Implementazione basata su: https://gist.github.com/sumanth232/e1600b327922b6947f51\n\n");
    
    clock_t start = clock();
    
    // Trova direttamente la LRS usando l'implementazione di riferimento
    char lrs[256];
    int lrs_length = find_longest_repeated_substring(text, n, lrs);
    
    clock_t end = clock();
    double time_spent = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("✅ Elaborazione completata in %.6f secondi\n", time_spent);
    
    // Verifica aggiuntiva con suffix array esplicito
    int* sa = (int*)malloc(n * sizeof(int));
    int* lcp = (int*)malloc(n * sizeof(int));
    
    build_suffix_array(text, n, sa);
    build_lcp_array(text, n, sa, lcp);
    
    if (is_valid_suffix_array(text, n, sa)) {
        printf("✅ Suffix Array valido\n");
    } else {
        printf("❌ Suffix Array non valido!\n");
    }
    
    printf("📊 Risultati:\n");
    printf("   - Lunghezza LRS: %d\n", lrs_length);
    printf("   - Sottostringa ripetuta più lunga: \"%s\"\n", 
           lrs_length > 0 ? lrs : "(nessuna)");
    
    // Stampa il suffix array (solo per stringhe piccole)
    if (n <= 50) {
        printf("\n");
        print_suffix_array(text, n, sa);
    }
    
    free(sa);
    free(lcp);
    return 0;
}