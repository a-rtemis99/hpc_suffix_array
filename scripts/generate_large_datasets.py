#!/usr/bin/env python3
"""
Generatore di dataset di grandi dimensioni per testing Suffix Array
"""
import os
import random
import string
import hashlib
import gzip
from datetime import datetime

def generate_random_string(length):
    """Genera una stringa casuale di lunghezza specificata"""
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def generate_repetitive_string(length, pattern_length=1000):
    """Genera una stringa con pattern ripetuti (per testare casi interessanti)"""
    base_pattern = ''.join(random.choices(string.ascii_lowercase, k=pattern_length))
    repeats = length // pattern_length
    remainder = length % pattern_length
    
    result = base_pattern * repeats + base_pattern[:remainder]
    return result

def generate_dna_sequence(length):
    """Genera una sequenza DNA-like"""
    bases = ['A', 'C', 'G', 'T']
    return ''.join(random.choices(bases, k=length))

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
        f.write(f"Type: {'Random' if 'random' in description else 'Repetitive' if 'repetitive' in description else 'DNA'}\n")
    
    print(f"Generated: {filename} ({len(content):,} chars, {file_size:,} bytes)")
    print(f"MD5: {checksum}")

def generate_standard_datasets():
    """Genera i dataset standard richiesti"""
    sizes_mb = [1, 50, 100, 200, 500]
    
    print("=== GENERATING STANDARD DATASETS ===")
    
    for size_mb in sizes_mb:
        size_chars = size_mb * 1024 * 1024
        
        # Dataset random
        filename = f"test_data/large/random_{size_mb}MB.txt"
        if not os.path.exists(filename):
            content = generate_random_string(size_chars)
            save_string_with_metadata(filename, content, f"Random string {size_mb}MB")
        
        # Dataset con pattern ripetuti (solo per dimensioni più piccole)
        if size_mb <= 100:
            filename = f"test_data/large/repetitive_{size_mb}MB.txt"
            if not os.path.exists(filename):
                content = generate_repetitive_string(size_chars)
                save_string_with_metadata(filename, content, f"Repetitive string {size_mb}MB")

def generate_real_world_datasets():
    """Scarica o genera dataset del mondo reale"""
    print("\n=== GENERATING REAL-WORLD DATASETS ===")
    
    # Genera sequenza DNA grande
    dna_size = 10 * 1024 * 1024  # 10MB
    dna_file = "test_data/real_world/dna_10MB.txt"
    if not os.path.exists(dna_file):
        content = generate_dna_sequence(dna_size)
        save_string_with_metadata(dna_file, content, "DNA sequence 10MB")

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
        with open(filename, 'w') as f:
            f.write(content)
        print(f"Generated: {filename} ({len(content)} chars)")

def main():
    """Funzione principale"""
    print("HPC Suffix Array - Large Dataset Generator")
    print("=" * 50)
    
    # Crea le cartelle se non esistono
    os.makedirs("test_data/large", exist_ok=True)
    os.makedirs("test_data/real_world", exist_ok=True)
    
    # Genera tutti i dataset
    generate_small_test_cases()
    generate_standard_datasets() 
    generate_real_world_datasets()
    
    print("\n=== GENERATION COMPLETE ===")
    
    # Stampa riepilogo
    total_size = 0
    for root, dirs, files in os.walk("test_data"):
        for file in files:
            if file.endswith('.txt') and not file.endswith('.meta'):
                filepath = os.path.join(root, file)
                size = os.path.getsize(filepath)
                total_size += size
                print(f"{filepath}: {size:,} bytes")
    
    print(f"\nTotal test data: {total_size:,} bytes ({total_size/(1024*1024):.1f} MB)")

if __name__ == "__main__":
    main()