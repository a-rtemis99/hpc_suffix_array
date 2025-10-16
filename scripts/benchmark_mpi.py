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
import re

def parse_output(output):
    """Estrae informazioni dettagliate dall'output del programma MPI"""
    result = {
        'lrs_length': 0,
        'lrs_string': 'N/A',
        'suffix_array_length': 0,
        'execution_details': 'N/A',
        'mpi_processes': 0,
        'parallel_efficiency': 0.0
    }
    
    lines = output.split('\n')
    
    for i, line in enumerate(lines):
        # Cerca informazioni MPI
        if 'MPI processes:' in line or 'Processes:' in line:
            match = re.search(r'(\d+)', line)
            if match:
                result['mpi_processes'] = int(match.group(1))
        
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
        
        # Estrai dettagli di esecuzione e efficienza
        if 'Time for building suffix array:' in line:
            result['execution_details'] = line.strip()
        if 'Parallel efficiency:' in line or 'Efficiency:' in line:
            match = re.search(r'(\d+\.\d+)', line)
            if match:
                result['parallel_efficiency'] = float(match.group(1))
    
    return result

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
            'execution_details': parsed_info['execution_details'],
            'mpi_processes': parsed_info['mpi_processes'],
            'parallel_efficiency': parsed_info['parallel_efficiency']
        }
        
    except subprocess.TimeoutExpired:
        return {
            'success': False, 
            'time': 3600, 
            'error': 'TIMEOUT',
            'lrs_length': 0,
            'lrs_string': 'TIMEOUT',
            'suffix_array_length': 0,
            'execution_details': 'TIMEOUT',
            'mpi_processes': num_processes,
            'parallel_efficiency': 0.0
        }
    except Exception as e:
        return {
            'success': False, 
            'time': 0, 
            'error': str(e),
            'lrs_length': 0,
            'lrs_string': 'ERROR',
            'suffix_array_length': 0,
            'execution_details': 'ERROR',
            'mpi_processes': num_processes,
            'parallel_efficiency': 0.0
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
    print("BENCHMARK MPI - Suffix Array")
    print("============================================================")
    print(f"Avviato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()
    
    # File di test
    mpi_files = [
        "test_data/banana.txt",
        "test_data/mississippi.txt",
        "test_data/abcabcabc.txt",
        "test_data/aaaa.txt",
        "test_data/ababab.txt",
        "test_data/large/random_1MB.txt",
        "test_data/large/random_50MB.txt",
        "test_data/large/random_100MB.txt"
    ]
    
    mpi_results = []
    process_configs = [2, 4]  # Test con 2 e 4 processi

    print("Testing MPI con diverse configurazioni...")
    print("------------------------------------------------------------")
    
    for test_file in mpi_files:
        if not os.path.exists(test_file):
            print(f"{os.path.basename(test_file):25} - NON TROVATO")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"{os.path.basename(test_file):25} ({file_size_mb:6.1f} MB)")
        
        for num_proc in process_configs:
            print(f"  MPI-{num_proc:2} processi...", end=" ", flush=True)
            
            result = run_mpi_benchmark(test_file, num_proc)
            
            if result['success']:
                time_str = format_time(result['time'])
                efficiency_str = f"{result['parallel_efficiency']*100:.1f}%" if result['parallel_efficiency'] > 0 else "N/A"
                print(f"{time_str:>8} - LRS: {result['lrs_length']:3} chars - Eff: {efficiency_str:>6}")
                
                mpi_results.append({
                    'file': os.path.basename(test_file),
                    'size_bytes': file_size,
                    'size_mb': file_size_mb,
                    'backend': f'mpi_{num_proc}',
                    'processes': num_proc,
                    'time_seconds': result['time'],
                    'throughput_mb_s': file_size_mb / result['time'] if result['time'] > 0 else 0,
                    'throughput_chars_per_second': file_size / result['time'] if result['time'] > 0 else 0,
                    'lrs_length': result['lrs_length'],
                    'lrs_string': result['lrs_string'],
                    'suffix_array_length': result['suffix_array_length'],
                    'execution_details': result['execution_details'],
                    'parallel_efficiency': result['parallel_efficiency'],
                    'speedup': 0.0,  # Sarà calcolato dopo
                    'success': True,
                    'timestamp': datetime.now()
                })
            else:
                print("FAILED")
                if result['error']:
                    print(f"        Error: {result['error'][:100]}")

    # Calcola speedup rispetto alla versione sequenziale
    if mpi_results:
        # Carica risultati sequenziali per confronto
        sequential_times = {}
        try:
            if os.path.exists("results/benchmarks/sequential_results.csv"):
                df_seq = pd.read_csv("results/benchmarks/sequential_results.csv")
                for _, row in df_seq.iterrows():
                    sequential_times[row['file']] = row['time_seconds']
        except:
            pass
        
        # Calcola speedup per ogni risultato MPI
        for result in mpi_results:
            file_name = result['file']
            if file_name in sequential_times and sequential_times[file_name] > 0:
                result['speedup'] = sequential_times[file_name] / result['time_seconds']
            else:
                result['speedup'] = 0.0

    # Salva risultati
    if mpi_results:
        df_mpi = pd.DataFrame(mpi_results)
        os.makedirs("results/benchmarks", exist_ok=True)
        output_file = "results/benchmarks/mpi_results.csv"
        
        df_mpi.to_csv(output_file, index=False)
        
        # Calcola statistiche
        total_tests = len(mpi_results)
        successful_tests = len([r for r in mpi_results if r['success']])
        
        print("\n" + "=" * 60)
        print("BENCHMARK MPI COMPLETATO")
        print("=" * 60)
        print(f"Risultati salvati: {output_file}")
        print(f"Test completati: {successful_tests}/{total_tests}")
        
        # Stampa tabella riassuntiva comparativa
        if mpi_results:
            print("\nRIEPILOGO DETTAGLIATO MPI:")
            print("-" * 90)
            print(f"{'File':25} {'Proc':>4} {'Time':>8} {'LRS Len':>8} {'Speedup':>8} {'Efficiency':>10} {'LRS Preview':20}")
            print("-" * 90)
            
            for result in mpi_results:
                if result['success']:
                    lrs_preview = result['lrs_string'][:18] + "..." if len(result['lrs_string']) > 18 else result['lrs_string']
                    efficiency_str = f"{result['parallel_efficiency']*100:.1f}%" if result['parallel_efficiency'] > 0 else "N/A"
                    speedup_str = f"{result['speedup']:.2f}x" if result['speedup'] > 0 else "N/A"
                    
                    print(f"{result['file']:25} {result['processes']:4} {format_time(result['time_seconds']):>8} {result['lrs_length']:>8} {speedup_str:>8} {efficiency_str:>10} {lrs_preview:20}")
            
    else:
        print("\nNessun test MPI completato!")
        sys.exit(1)

if __name__ == "__main__":
    main()