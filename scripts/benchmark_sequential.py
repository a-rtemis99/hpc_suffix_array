#!/usr/bin/env python3
"""
Benchmark per la versione sequenziale dell'algoritmo Suffix Array
"""
import subprocess
import time
import os
import pandas as pd
import sys
from datetime import datetime

def run_benchmark(input_file):
    """Esegue il benchmark per un singolo file"""
    cmd = ["./bin/sequential_suffix_array", input_file]
    
    start_time = time.time()
    try:
        result = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True, 
            timeout=7200  # 2 ore timeout
        )
        execution_time = time.time() - start_time
        
        # Estrai risultato LRS
        lrs_info = "N/A"
        for line in result.stdout.split('\n'):
            if "Longest repeated substring" in line or "length:" in line.lower():
                lrs_info = line.strip()[:60]  # Primi 60 caratteri
                break
        
        return {
            'success': result.returncode == 0,
            'time': execution_time,
            'output': result.stdout,
            'error': result.stderr,
            'lrs': lrs_info
        }
        
    except subprocess.TimeoutExpired:
        return {'success': False, 'time': 7200, 'error': 'TIMEOUT'}
    except Exception as e:
        return {'success': False, 'time': 0, 'error': str(e)}

def main():
    """Funzione principale"""
    print("🔵 BENCHMARK SEQUENZIALE - Suffix Array")
    print("=" * 60)
    print(f"Avviato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()
    
    # File di test
    sequential_files = [
        "test_data/banana.txt",
        "test_data/mississippi.txt", 
        "test_data/abcabcabc.txt",
        "test_data/aaaa.txt",
        "test_data/ababab.txt",
        "test_data/large/random_1MB.txt",
        "test_data/large/random_50MB.txt", 
        "test_data/large/random_100MB.txt",
        "test_data/large/random_200MB.txt",
        "test_data/large/random_500MB.txt"
    ]

    sequential_results = []
    successful_tests = 0

    print("🧪 Testing versione sequenziale...")
    print("-" * 60)
    
    for test_file in sequential_files:
        if not os.path.exists(test_file):
            print(f"{test_file} - NON TROVATO")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"📄 {os.path.basename(test_file):25} ({file_size_mb:6.1f} MB)...", end=" ", flush=True)
        
        result = run_benchmark(test_file)
        
        if result['success']:
            print(f"{result['time']:7.2f}s")
            successful_tests += 1
            
            sequential_results.append({
                'file': os.path.basename(test_file),
                'size_bytes': file_size,
                'size_mb': file_size_mb,
                'backend': 'sequential',
                'time_seconds': result['time'],
                'throughput_mb_s': file_size_mb / result['time'] if result['time'] > 0 else 0,
                'throughput_chars_per_second': file_size / result['time'] if result['time'] > 0 else 0,
                'lrs': result['lrs'],
                'success': True,
                'timestamp': datetime.now()
            })
        else:
            print(f"FAILED")
            if result['error']:
                print(f"      Error: {result['error'][:100]}")

    # Salva risultati
    if sequential_results:
        df_seq = pd.DataFrame(sequential_results)
        
        # Crea directory se non esiste
        os.makedirs("results/benchmarks", exist_ok=True)
        output_file = "results/benchmarks/sequential_results.csv"
        
        df_seq.to_csv(output_file, index=False)
        
        print("\n" + "=" * 60)
        print("BENCHMARK SEQUENZIALE COMPLETATO")
        print("=" * 60)
        print(f"Risultati salvati: {output_file}")
        print(f"Test completati: {successful_tests}/{len(sequential_files)}")
        print(f"Tempo totale stimato: {df_seq['time_seconds'].sum():.1f}s")
        
        # Statistiche
        if len(sequential_results) > 1:
            print(f"Throughput medio: {df_seq['throughput_mb_s'].mean():.1f} MB/s")
            print(f"Tempo medio: {df_seq['time_seconds'].mean():.2f}s")
            
    else:
        print("\nNessun test completato con successo!")
        sys.exit(1)

if __name__ == "__main__":
    main()