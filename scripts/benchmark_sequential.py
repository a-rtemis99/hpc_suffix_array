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
import re

def parse_output(output):
    """Estrae informazioni dettagliate dall'output del programma"""
    result = {
        'lrs_length': 0,
        'lrs_string': 'N/A',
        'suffix_array_length': 0,
        'execution_details': 'N/A'
    }
    
    lines = output.split('\n')
    
    for i, line in enumerate(lines):
        # Cerca lunghezza LRS
        if 'LRS length:' in line or 'Longest repeated substring length:' in line:
            match = re.search(r'(\d+)', line)
            if match:
                result['lrs_length'] = int(match.group(1))
        
        # Cerca stringa LRS
        if 'Longest repeated substring:' in line and 'length' not in line.lower():
            lrs_str = line.split(':', 1)[1].strip()
            if lrs_str and lrs_str != '""':
                result['lrs_string'] = lrs_str[:50]  # Limita a 50 caratteri
        
        # Cerca informazioni sul suffix array
        if 'Suffix array built successfully' in line or 'n =' in line:
            match = re.search(r'n\s*=\s*(\d+)', line)
            if match:
                result['suffix_array_length'] = int(match.group(1))
        
        # Estrai dettagli di esecuzione
        if 'Time for building suffix array:' in line:
            result['execution_details'] = line.strip()
    
    return result

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
        
        # Estrai informazioni dettagliate dall'output
        parsed_info = parse_output(result.stdout)
        
        return {
            'success': result.returncode == 0,
            'time': execution_time,
            'output': result.stdout,
            'error': result.stderr,
            'lrs_length': parsed_info['lrs_length'],
            'lrs_string': parsed_info['lrs_string'],
            'suffix_array_length': parsed_info['suffix_array_length'],
            'execution_details': parsed_info['execution_details']
        }
        
    except subprocess.TimeoutExpired:
        return {
            'success': False, 
            'time': 7200, 
            'error': 'TIMEOUT',
            'lrs_length': 0,
            'lrs_string': 'TIMEOUT',
            'suffix_array_length': 0,
            'execution_details': 'TIMEOUT'
        }
    except Exception as e:
        return {
            'success': False, 
            'time': 0, 
            'error': str(e),
            'lrs_length': 0,
            'lrs_string': 'ERROR',
            'suffix_array_length': 0,
            'execution_details': 'ERROR'
        }

def format_time(seconds):
    """Formatta il tempo in modo leggibile"""
    if seconds < 1:
        return f"{seconds:.2f}s"
    elif seconds < 60:
        return f"{seconds:.2f}s"
    elif seconds < 3600:
        minutes = seconds / 60
        return f"{minutes:.1f}m"
    else:
        hours = seconds / 3600
        return f"{hours:.1f}h"

def main():
    """Funzione principale"""
    print("BENCHMARK SEQUENZIALE - Suffix Array")
    print("============================================================")
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

    print("Testing versione sequenziale...")
    print("------------------------------------------------------------")
    
    for test_file in sequential_files:
        if not os.path.exists(test_file):
            print(f"{os.path.basename(test_file):25} - NON TROVATO")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"{os.path.basename(test_file):25} ({file_size_mb:6.1f} MB)...", end=" ", flush=True)
        
        result = run_benchmark(test_file)
        
        if result['success']:
            time_str = format_time(result['time'])
            print(f"{time_str:>8} - LRS: {result['lrs_length']} chars")
            successful_tests += 1
            
            sequential_results.append({
                'file': os.path.basename(test_file),
                'size_bytes': file_size,
                'size_mb': file_size_mb,
                'backend': 'sequential',
                'time_seconds': result['time'],
                'throughput_mb_s': file_size_mb / result['time'] if result['time'] > 0 else 0,
                'throughput_chars_per_second': file_size / result['time'] if result['time'] > 0 else 0,
                'lrs_length': result['lrs_length'],
                'lrs_string': result['lrs_string'],
                'suffix_array_length': result['suffix_array_length'],
                'execution_details': result['execution_details'],
                'success': True,
                'timestamp': datetime.now()
            })
        else:
            print("FAILED")
            if result['error']:
                print(f"      Error: {result['error'][:100]}")

    # Salva risultati
    if sequential_results:
        df_seq = pd.DataFrame(sequential_results)
        
        # Crea directory se non esiste
        os.makedirs("results/benchmarks", exist_ok=True)
        output_file = "results/benchmarks/sequential_results.csv"
        
        df_seq.to_csv(output_file, index=False)
        
        # Calcola statistiche
        total_time = df_seq['time_seconds'].sum()
        avg_throughput = df_seq['throughput_mb_s'].mean()
        avg_time = df_seq['time_seconds'].mean()
        
        print("\n" + "=" * 60)
        print("BENCHMARK SEQUENZIALE COMPLETATO")
        print("=" * 60)
        print(f"Risultati salvati: {output_file}")
        print(f"Test completati: {successful_tests}/{len(sequential_files)}")
        print(f"Tempo totale stimato: {total_time:.1f}s")
        print(f"Throughput medio: {avg_throughput:.1f} MB/s")
        print(f"Tempo medio: {avg_time:.2f}s")
        
        # Stampa tabella riassuntiva
        print("\nRIEPILOGO DETTAGLIATO:")
        print("-" * 80)
        print(f"{'File':25} {'Size(MB)':>10} {'Time':>8} {'LRS Len':>8} {'LRS Preview':20}")
        print("-" * 80)
        
        for _, row in df_seq.iterrows():
            lrs_preview = row['lrs_string'][:18] + "..." if len(row['lrs_string']) > 18 else row['lrs_string']
            print(f"{row['file']:25} {row['size_mb']:10.1f} {format_time(row['time_seconds']):>8} {row['lrs_length']:>8} {lrs_preview:20}")
            
    else:
        print("\nNessun test completato con successo!")
        sys.exit(1)

if __name__ == "__main__":
    main()