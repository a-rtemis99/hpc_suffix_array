#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* read_file(const char* filename, long* file_size) {
    FILE* file = fopen(filename, "rb");  // Apri in modalità binary
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    // Determina la dimensione del file
    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (*file_size <= 0) {
        fprintf(stderr, "Error: File is empty or cannot determine size\n");
        fclose(file);
        return NULL;
    }

    // Alloca memoria per il contenuto + carattere nullo
    char* buffer = (char*)malloc(*file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed for file content\n");
        fclose(file);
        return NULL;
    }

    // Leggi l'intero file
    size_t bytes_read = fread(buffer, 1, *file_size, file);
    if (bytes_read != (size_t)*file_size) {
        fprintf(stderr, "Error: Read %zu bytes, expected %ld\n", bytes_read, *file_size);
        free(buffer);
        fclose(file);
        return NULL;
    }

    // Aggiungi il terminatore nullo
    buffer[*file_size] = '\0';

    fclose(file);
    
    printf("Successfully read file: %s (%ld bytes)\n", filename, *file_size);
    return buffer;
}

int write_file(const char* filename, const char* content) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return -1;
    }

    fprintf(file, "%s", content);
    fclose(file);
    return 0;
}

void print_first_chars(const char* str, int n) {
    printf("First %d characters: \"", n);
    for (int i = 0; i < n && str[i] != '\0'; i++) {
        printf("%c", str[i]);
    }
    printf("\"\n");
}

void print_last_chars(const char* str, long length, int n) {
    if (length <= n) {
        print_first_chars(str, length);
        return;
    }
    
    printf("Last %d characters: \"", n);
    for (long i = length - n; i < length; i++) {
        printf("%c", str[i]);
    }
    printf("\"\n");
}