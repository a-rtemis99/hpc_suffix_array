#ifndef UTILS_H
#define UTILS_H

// Funzione per leggere un file
char* read_file(const char* filename, long* file_size);

// Funzione per scrivere un file
int write_file(const char* filename, const char* content);

// Funzioni di utilità per stampare stringhe
void print_first_chars(const char* str, int n);
void print_last_chars(const char* str, long length, int n);

#endif