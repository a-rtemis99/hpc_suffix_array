#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Funzione per leggere un file in memoria
char* read_file(const char* filename, long* file_size);

// Funzione per scrivere un file
void write_file(const char* filename, const char* data, long data_size);

#endif
