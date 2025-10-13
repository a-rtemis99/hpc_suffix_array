#!/usr/bin/env python3
"""
Benchmark per la versione MPI dell'algoritmo Suffix Array
"""
import subprocess
import time
import os
import pandas as pd
import sys
from datetime import datetime

def run_mpi_benchmark(input_file, num_processes):
    """Esegue benchmark MPI"""
    cmd = ["mpirun", "-np", str(num_processes), "./bin/mpi_suffix_array", input_file]
    
    start_time = time.time()
    try:
        result = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True, 
            timeout=3600  # 1 ora timeout
        )
        execution_time = time.time() - start_time
        
        return {
            'success': result.returncode == 0,
            'time': execution_time,
            'output': result.stdout,
            'error': result.stderr
        }
        
    except subprocess.TimeoutExpired:
        return {'success': False, 'time': 3600, 'error': 'TIMEOUT'}
    except Exception as e:
        return {'success': False, 'time': 0, 'error': str(e)}

def main():
    print("BENCHMARK MPI - Suffix Array")
    print("=" * 60)
    print(f"Avviato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()
    
    # File di test (più piccoli per debug)
    mpi_files = [
        "test_data/banana.txt",
        "test_data/mississippi.txt",
        "test_data/large/random_1MB.txt",
        "test_data/large/random_50MB.txt",
        "test_data/large/random_100MB.txt"
    ]
    
    mpi_results = []
    process_configs = [2, 4]  # Test con 2 e 4 processi

    print("Testing MPI con diverse configurazioni...")
    print("-" * 60)
    
    for test_file in mpi_files:
        if not os.path.exists(test_file):
            print(f"{test_file} - NON TROVATO")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"{os.path.basename(test_file):25} ({file_size_mb:6.1f} MB)")
        
        for num_proc in process_configs:
            print(f"MPI-{num_proc:2} processi...", end=" ", flush=True)
            
            result = run_mpi_benchmark(test_file, num_proc)
            
            if result['success']:
                print(f"{result['time']:7.2f}s")
                mpi_results.append({
                    'file': os.path.basename(test_file),
                    'size_bytes': file_size,
                    'size_mb': file_size_mb,
                    'backend': f'mpi_{num_proc}',
                    'time_seconds': result['time'],
                    'throughput_mb_s': file_size_mb / result['time'],
                    'success': True,
                    'timestamp': datetime.now()
                })
            else:
                print(f"FAILED")
                if result['error']:
                    print(f"      Error: {result['error'][:100]}")

    # Salva risultati
    if mpi_results:
        df_mpi = pd.DataFrame(mpi_results)
        os.makedirs("results/benchmarks", exist_ok=True)
        output_file = "results/benchmarks/mpi_results.csv"
        
        df_mpi.to_csv(output_file, index=False)
        
        print("\n" + "=" * 60)
        print("BENCHMARK MPI COMPLETATO")
        print("=" * 60)
        print(f"Risultati salvati: {output_file}")
        print(f"Test completati: {len(mpi_results)}")
        
    else:
        print("\nNessun test MPI completato!")
        sys.exit(1)

if __name__ == "__main__":
    main()