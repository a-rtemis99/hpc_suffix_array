#!/usr/bin/env python3
"""
CUDA Benchmark - DA ESEGUIRE SOLO SU KAGGLE CON GPU
"""
import subprocess
import time
import os
import pandas as pd
from datetime import datetime

def run_cuda_benchmark(input_file):
    """Esegue benchmark CUDA"""
    cmd = ["./bin/cuda_suffix_array", input_file]
    
    start_time = time.time()
    try:
        result = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True, 
            timeout=3600  # 1 ora timeout
        )
        execution_time = time.time() - start_time
        
        # Estrai risultato LRS
        lrs_info = "N/A"
        for line in result.stdout.split('\n'):
            if "Longest repeated substring" in line or "length:" in line.lower():
                lrs_info = line.strip()[:60]
                break
        
        return {
            'success': result.returncode == 0,
            'time': execution_time,
            'output': result.stdout,
            'error': result.stderr,
            'lrs': lrs_info
        }
        
    except subprocess.TimeoutExpired:
        return {'success': False, 'time': 3600, 'error': 'TIMEOUT'}
    except Exception as e:
        return {'success': False, 'time': 0, 'error': str(e)}

def main():
    print("CUDA BENCHMARK - KAGGLE GPU")
    print("=" * 60)
    print(f"Avviato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("ATTENZIONE: Questo script richiede GPU NVIDIA")
    print()
    
    # File di test per CUDA (più piccoli per limiti memoria GPU)
    cuda_files = [
        "test_data/banana.txt",
        "test_data/mississippi.txt",
        "test_data/large/random_1MB.txt",
        "test_data/large/random_50MB.txt", 
        "test_data/large/random_100MB.txt"
    ]

    cuda_results = []
    successful_tests = 0

    print("Testing versione CUDA...")
    print("-" * 60)
    
    for test_file in cuda_files:
        if not os.path.exists(test_file):
            print(f"{test_file} - NON TROVATO")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"{os.path.basename(test_file):25} ({file_size_mb:6.1f} MB)...", end=" ", flush=True)
        
        result = run_cuda_benchmark(test_file)
        
        if result['success']:
            print(f"{result['time']:7.2f}s")
            successful_tests += 1
            
            cuda_results.append({
                'file': os.path.basename(test_file),
                'size_bytes': file_size,
                'size_mb': file_size_mb,
                'backend': 'cuda',
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
    if cuda_results:
        df_cuda = pd.DataFrame(cuda_results)
        
        os.makedirs("results/benchmarks", exist_ok=True)
        output_file = "results/benchmarks/cuda_results.csv"
        
        df_cuda.to_csv(output_file, index=False)
        
        print("\n" + "=" * 60)
        print("BENCHMARK CUDA COMPLETATO")
        print("=" * 60)
        print(f"Risultati salvati: {output_file}")
        print(f"Test completati: {successful_tests}/{len(cuda_files)}")
        print(f"Tempo totale: {df_cuda['time_seconds'].sum():.1f}s")
        
        if len(cuda_results) > 1:
            print(f"Throughput medio: {df_cuda['throughput_mb_s'].mean():.1f} MB/s")
            
    else:
        print("\nNessun test CUDA completato!")
        # Non uscire con errore per non bloccare gli altri benchmark

if __name__ == "__main__":
    main()