#!/usr/bin/env python3
"""
Benchmark unificato per tutte le implementazioni (Sequential, MPI, CUDA)
"""
import subprocess
import os
import time
import csv
import re
from datetime import datetime

class UnifiedBenchmark:
    def __init__(self):
        self.results = []
        
    def run_sequential_test(self, input_file, output_dir):
        """Esegue test versione sequenziale"""
        print(f"\n--- SEQUENTIAL: {os.path.basename(input_file)} ---")
        
        cmd = ["./bin/sequential_suffix_array", input_file]
        return self._run_test("sequential", cmd, input_file, 1)
    
    def run_mpi_test(self, input_file, num_processes, output_dir):
        """Esegue test versione MPI"""
        print(f"\n--- MPI ({num_processes} processes): {os.path.basename(input_file)} ---")
        
        cmd = ["mpirun", "-np", str(num_processes), "./bin/mpi_suffix_array", input_file]
        return self._run_test("mpi", cmd, input_file, num_processes)
    
    def run_cuda_test(self, input_file, output_dir):
        """Esegue test versione CUDA"""
        print(f"\n--- CUDA: {os.path.basename(input_file)} ---")
        
        cmd = ["./bin/cuda_suffix_array", input_file]
        return self._run_test("cuda", cmd, input_file, 1)  # 1 process ma GPU
    
    def _run_test(self, implementation, cmd, input_file, num_processes):
        """Esegue un test e parserizza l'output strutturato"""
        try:
            start_time = time.time()
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
            execution_time = time.time() - start_time
            
            if result.returncode == 0:
                # Cerca output strutturato
                structured_output = self._parse_structured_output(result.stdout)
                
                if structured_output:
                    result_data = {
                        'implementation': implementation,
                        'filename': os.path.basename(input_file),
                        'file_size_bytes': structured_output.get('FILE_SIZE', 0),
                        'file_size_mb': structured_output.get('FILE_SIZE', 0) / (1024 * 1024),
                        'total_time_seconds': structured_output.get('TOTAL_TIME', execution_time),
                        'sa_time_seconds': structured_output.get('SA_TIME', 0),
                        'lcp_time_seconds': structured_output.get('LCP_TIME', 0),
                        'num_processes': num_processes,
                        'execution_time_seconds': execution_time,
                        'throughput_chars_per_second': structured_output.get('FILE_SIZE', 0) / execution_time,
                        'success': True,
                        'timestamp': datetime.now().isoformat()
                    }
                    
                    print(f"✓ {implementation.upper()} execution successful")
                    print(f"  Time: {result_data['total_time_seconds']:.2f}s")
                    print(f"  Processes: {num_processes}")
                    
                    return result_data
                else:
                    print(f"✗ No structured output from {implementation}")
                    return None
            else:
                print(f"✗ {implementation.upper()} execution failed: {result.stderr}")
                return None
                
        except subprocess.TimeoutExpired:
            print(f"✗ {implementation.upper()} execution timed out")
            return None
        except Exception as e:
            print(f"✗ {implementation.upper()} execution error: {e}")
            return None
    
    def _parse_structured_output(self, output):
        """Parserizza l'output strutturato dal programma C"""
        pattern = r'===STRUCTURED_RESULTS===(.*?)===END_RESULTS==='
        match = re.search(pattern, output, re.DOTALL)
        
        if match:
            data = {}
            lines = match.group(1).strip().split('\n')
            for line in lines:
                if ':' in line:
                    key, value = line.split(':', 1)
                    # Converti valori numerici
                    if key in ['FILE_SIZE', 'PROCESSES']:
                        data[key] = int(value)
                    elif key in ['TOTAL_TIME', 'SA_TIME', 'LCP_TIME']:
                        data[key] = float(value)
                    else:
                        data[key] = value
            return data
        return None
    
    def save_results(self, output_file):
        """Salva tutti i risultati in CSV"""
        if not self.results:
            print("No results to save!")
            return
        
        os.makedirs(os.path.dirname(output_file), exist_ok=True)
        
        with open(output_file, 'w', newline='') as f:
            fieldnames = ['implementation', 'filename', 'file_size_bytes', 'file_size_mb', 
                         'total_time_seconds', 'sa_time_seconds', 'lcp_time_seconds',
                         'num_processes', 'execution_time_seconds', 'throughput_chars_per_second',
                         'success', 'timestamp']
            
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(self.results)
        
        print(f"\nResults saved to: {output_file}")
    
    def generate_comparison_report(self, output_dir):
        """Genera report comparativo tra implementazioni"""
        # [Qui il codice per grafici comparativi che hai già]
        pass

def main():
    benchmark = UnifiedBenchmark()
    
    # File di test (graduali)
    test_files = [
        "test_data/banana.txt",           # Piccolo - debug
        "test_data/large/random_1MB.txt", # Medio - test velocità
        "test_data/large/random_50MB.txt" # Grande - test scaling
    ]
    
    # Configurazioni MPI da testare
    mpi_configs = [1, 2, 4, 8]  # Numero di processi
    
    print("HPC SUFFIX ARRAY - UNIFIED BENCHMARK")
    print("=" * 50)
    
    # Test per ogni file
    for test_file in test_files:
        if not os.path.exists(test_file):
            print(f"File not found: {test_file}")
            continue
        
        # Test sequenziale
        result = benchmark.run_sequential_test(test_file, "results/benchmarks")
        if result:
            benchmark.results.append(result)
        
        # Test MPI (solo per file abbastanza grandi)
        file_size_mb = os.path.getsize(test_file) / (1024 * 1024)
        if file_size_mb >= 1.0:  # Solo per file >1MB
            for num_procs in mpi_configs:
                result = benchmark.run_mpi_test(test_file, num_procs, "results/benchmarks")
                if result:
                    benchmark.results.append(result)
        
        # Test CUDA (se disponibile)
        if os.path.exists("./bin/cuda_suffix_array"):
            result = benchmark.run_cuda_test(test_file, "results/benchmarks")
            if result:
                benchmark.results.append(result)
    
    # Salva risultati
    benchmark.save_results("results/benchmarks/unified_benchmark_results.csv")
    
    print(f"\nBenchmark completed! {len(benchmark.results)} tests executed.")

if __name__ == "__main__":
    main()