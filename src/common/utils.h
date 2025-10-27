#ifndef UTILS_H
#define UTILS_H

#include <stdio.h> 
#include <stdlib.h> 

// Gestione linkage C/C++
#ifdef __cplusplus
extern "C" {
#endif

// Prototipi delle funzioni definite in utils.c

/**
 * @brief Legge l'intero contenuto di un file in una stringa allocata dinamicamente.
 *
 * @param filename Percorso del file da leggere.
 * @param file_size Puntatore a una variabile long dove verrà scritta la dimensione del file (in byte).
 * @return Puntatore alla stringa contenente il file, terminata con '\0'.
 * NULL in caso di errore (file non trovato, errore di lettura, memoria esaurita).
 * Il chiamante è responsabile di liberare la memoria restituita con free().
 */
char* read_file(const char* filename, long* file_size);

/**
 * @brief Scrive una stringa in un file. (Funzione non usata nel progetto principale, ma utile).
 *
 * @param filename Percorso del file da scrivere.
 * @param content Stringa da scrivere nel file.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int write_file(const char* filename, const char* content);

/**
 * @brief Stampa i primi 'n' caratteri di una stringa, seguiti da "...".
 *
 * @param str Stringa da stampare.
 * @param count Numero massimo di caratteri da stampare.
 */
void print_first_chars(const char* str, int count);

/**
 * @brief Stampa gli ultimi 'n' caratteri di una stringa, preceduti da "...".
 *
 * @param str Stringa da stampare.
 * @param length Lunghezza totale della stringa.
 * @param count Numero massimo di caratteri da stampare dalla fine.
 */
void print_last_chars(const char* str, long length, int count);

// Chiusura extern "C"
#ifdef __cplusplus
} // extern "C"
#endif

#endif 