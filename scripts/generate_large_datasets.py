"""
Generatore di dataset di grandi dimensioni per testing Suffix Array
(Versione modificata per generare solo i file richiesti dalla traccia)
"""
import os
import random
import string
import hashlib
from datetime import datetime
import random


random.seed(42)

def generate_random_string(length):
    """Genera una stringa casuale di lunghezza specificata"""
    char_set = string.ascii_letters + string.digits
    return ''.join(random.choices(char_set, k=length))

def save_string_with_metadata(filename, content, description=""):
    """Salva la stringa e crea file metadati"""
    # Salva il file principale
    with open(filename, 'w') as f:
        f.write(content)

    # Calcola metadati
    file_size = os.path.getsize(filename)
    checksum = hashlib.md5(content.encode()).hexdigest()

    # Salva metadati
    meta_filename = filename + '.meta'
    with open(meta_filename, 'w') as f:
        f.write(f"Description: {description}\n")
        f.write(f"Generated: {datetime.now().isoformat()}\n")
        f.write(f"Length: {len(content)} characters\n")
        f.write(f"File size: {file_size} bytes\n")
        f.write(f"MD5: {checksum}\n")
        f.write(f"Type: Random\n")

    print(f"Generated: {filename} ({len(content):,} chars, {file_size:,} bytes)")
    print(f"MD5: {checksum}")

def generate_standard_datasets():
    """Genera i dataset random standard richiesti"""
    sizes_mb = [1, 50, 100, 200, 500]

    print("=== GENERATING STANDARD RANDOM DATASETS ===")

    for size_mb in sizes_mb:
        size_chars = size_mb * 1024 * 1024

        filename = f"test_data/large/random_{size_mb}MB.txt"
        if not os.path.exists(filename):
            print(f"Generating {filename}...")
            content = generate_random_string(size_chars)
            save_string_with_metadata(filename, content, f"Random string {size_mb}MB")
        else:
             print(f"Skipping {filename}, already exists.")

def generate_small_test_cases():
    """Rigenera i casi di test piccoli per consistenza"""
    print("\n=== GENERATING SMALL TEST CASES ===")

    test_cases = {
        "banana": "banana",
        "mississippi": "mississippi",
        "abcabcabc": "abcabcabc",
        "aaaa": "a" * 1000,
        "ababab": "ab" * 500
    }

    for name, content in test_cases.items():
        filename = f"test_data/{name}.txt"
        if not os.path.exists(filename):
            with open(filename, 'w') as f:
                f.write(content)
            print(f"Generated: {filename} ({len(content)} chars)")
        else:
            print(f"Skipping {filename}, already exists.")


def main():
    """Funzione principale"""
    print("HPC Suffix Array - Dataset Generator (Random Only)")
    print("=" * 50)

    # Crea le cartelle se non esistono
    os.makedirs("test_data/large", exist_ok=True)

    # Genera i dataset necessari
    generate_small_test_cases()
    generate_standard_datasets()

    print("\n=== GENERATION COMPLETE ===")

    # Stampa riepilogo dei file generati (solo .txt)
    total_size = 0
    print("\nSummary of generated .txt files:")
    for root, dirs, files in os.walk("test_data"):

        for file in sorted(files): # Ordina i file per leggibilità
            if file.endswith('.txt') and not file.endswith('.meta'):
                filepath = os.path.join(root, file)
                try:
                    size = os.path.getsize(filepath)
                    total_size += size
                    print(f"- {filepath:<40}: {size:>12,} bytes")
                except FileNotFoundError:
                     print(f"- {filepath:<40}: ERROR - File not found during summary.")


    print(f"\nTotal size of generated .txt data: {total_size:,} bytes ({total_size/(1024*1024):.1f} MB)")

if __name__ == "__main__":
    main()