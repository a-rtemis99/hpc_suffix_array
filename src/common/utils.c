#include "utils.h"

char* read_file(const char* filename, long* file_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Errore nell'aprire il file");
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(*file_size + 1);
    if (!buffer) {
        perror("Errore nell'allocare memoria");
        fclose(file);
        return NULL;
    }
    
    fread(buffer, 1, *file_size, file);
    buffer[*file_size] = '\0';
    
    fclose(file);
    return buffer;
}

void write_file(const char* filename, const char* data, long data_size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Errore nel creare il file");
        return;
    }
    
    fwrite(data, 1, data_size, file);
    fclose(file);
}
