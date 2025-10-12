#!/usr/bin/env python3
"""
Benchmark per dataset di grandi dimensioni
"""
import os
import sys
import time
import subprocess
import resource
from datetime import datetime

def get_file_size(filename):
    """Restituisce la dimensione del file in bytes"""
    return os.path.getsize(filename)

def run_sequential_benchmark(input_file, output_dir):
    """Esegue benchmark sulla versione sequenziale"""
    print(f"\n--- Benchmarking: {os.path.basename(input_file)} ---")
    
    file_size = get_file_size(input_file)
    file_size_mb = file_size / (1024 * 1024)
    print(f"File size: {file_size:,} bytes ({file_size_mb:.1f} MB)")
    
    # Costruisce il comando
    cmd = ["./bin/sequential_suffix_array", input_file]
    
    # Misura tempo di esecuzione
    start_time = time.time()
    
    try:
        # Esegue il programma C
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)  # timeout 1 ora
        
        end_time = time.time()
        execution_time = end_time - start_time
        
        if result.returncode == 0:
            print(f"✓ Execution successful")
            print(f"Execution time: {execution_time:.2f} seconds")
            
            # Estrai informazioni dall'output
            output_lines = result.stdout.split('\n')
            for line in output_lines:
                if "Longest repeated substring" in line:
                    print(f"Result: {line}")
                elif "Length" in line and "Time" not in line:
                    print(f"Result: {line}")
            
            # Salva risultati
            benchmark_result = {
                'filename': os.path.basename(input_file),
                'file_size_bytes': file_size,
                'file_size_mb': file_size_mb,
                'execution_time_seconds': execution_time,
                'throughput_chars_per_second': file_size / execution_time if execution_time > 0 else 0,
                'timestamp': datetime.now().isoformat(),
                'success': True
            }
            
            return benchmark_result
            
        else:
            print(f"✗ Execution failed with return code {result.returncode}")
            print(f"Error: {result.stderr}")
            return None
            
    except subprocess.TimeoutExpired:
        print(f"✗ Execution timed out after 1 hour")
        return None
    except Exception as e:
        print(f"✗ Execution error: {e}")
        return None

def main():
    """Funzione principale del benchmark"""
    print("HPC Suffix Array - Large Scale Benchmark")
    print("=" * 50)
    
    # Lista dei file da testare, in ordine di dimensione crescente
    test_files = [
        "test_data/banana.txt",
        "test_data/mississippi.txt", 
        "test_data/abcabcabc.txt",
        "test_data/aaaa.txt",
        "test_data/ababab.txt",
        "test_data/large/random_1MB.txt",
        "test_data/large/random_50MB.txt",
        "test_data/large/random_100MB.txt",
        "test_data/large/random_200MB.txt",
        "test_data/large/random_500MB.txt",
    ]
    
    # Filtra solo i file che esistono
    existing_files = [f for f in test_files if os.path.exists(f)]
    
    if not existing_files:
        print("No test files found! Please generate datasets first.")
        sys.exit(1)
    
    print(f"Found {len(existing_files)} test files")
    
    results = []
    
    # Esegui benchmark su ogni file
    for test_file in existing_files:
        result = run_sequential_benchmark(test_file, "results/benchmarks/large_scale")
        if result:
            results.append(result)
    
    # Stampa riepilogo
    print("\n" + "=" * 50)
    print("BENCHMARK SUMMARY")
    print("=" * 50)
    
    for result in results:
        print(f"\n{result['filename']}:")
        print(f"  Size: {result['file_size_mb']:.1f} MB")
        print(f"  Time: {result['execution_time_seconds']:.2f} s")
        print(f"  Throughput: {result['throughput_chars_per_second']:,.0f} chars/s")
    
    # Salva risultati dettagliati
    output_file = "results/benchmarks/large_scale/benchmark_results.csv"
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    with open(output_file, 'w') as f:
        f.write("filename,file_size_bytes,file_size_mb,execution_time_seconds,throughput_chars_per_second,timestamp,success\n")
        for result in results:
            f.write(f"{result['filename']},{result['file_size_bytes']},{result['file_size_mb']},{result['execution_time_seconds']},{result['throughput_chars_per_second']},{result['timestamp']},{result['success']}\n")
    
    print(f"\nDetailed results saved to: {output_file}")

if __name__ == "__main__":
    main()