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
    """Estrae informazioni dettagliate dall'output del programma MPI."""
    result = {
        'lrs_length': 0, 'lrs_string': 'N/A', 'sa_time': 0.0, 'lcp_time': 0.0,
        'total_time': 0.0, 'mpi_processes': 0, 'suffix_array_length': 0,
        'parallel_efficiency': 0.0, 'execution_details': 'N/A'
    }

    # 1. Cerca i dati leggibili dall'utente per un fallback
    lrs_match = re.search(r"Longest repeated substring:.*\(length: (\d+)\)", output)
    if lrs_match:
        result['lrs_length'] = int(lrs_match.group(1))

    lrs_str_match = re.search(r"Longest repeated substring: '([^']*)'", output)
    if lrs_str_match:
        result['lrs_string'] = lrs_str_match.group(1)

    # 2. Estrae i dati precisi dal blocco strutturato (il nostro obiettivo primario)
    if '--- STRUCTURED_RESULTS ---' in output:
        # Isola solo il blocco di dati che ci interessa
        structured_data = output.split('--- STRUCTURED_RESULTS ---')[1]

        # Cerca ogni etichetta che abbiamo definito nel C
        sa_time_match = re.search(r"SA_TIME:([\d.]+)", structured_data)
        if sa_time_match: result['sa_time'] = float(sa_time_match.group(1))

        lcp_time_match = re.search(r"LCP_TIME:([\d.]+)", structured_data)
        if lcp_time_match: result['lcp_time'] = float(lcp_time_match.group(1))

        total_time_match = re.search(r"TOTAL_TIME:([\d.]+)", structured_data)
        if total_time_match: result['total_time'] = float(total_time_match.group(1))

        procs_match = re.search(r"MPI_PROCESSES:(\d+)", structured_data)
        if procs_match: result['mpi_processes'] = int(procs_match.group(1))

        len_match = re.search(r"ACTUAL_STRING_LENGTH:(\d+)", structured_data)
        if len_match: result['suffix_array_length'] = int(len_match.group(1))
    
    # Se il total_time non è stato trovato nel blocco strutturato, prova a prenderlo dall'output leggibile
    if result['total_time'] == 0.0:
        total_time_fallback = re.search(r"Total execution time: ([\d.]+)", output)
        if total_time_fallback:
            result['total_time'] = float(total_time_fallback.group(1))

    return result

def run_mpi_benchmark(input_file, num_processes):
    """Esegue benchmark MPI"""
    cmd = ["mpirun", "--oversubscribe", "-np", str(num_processes), "./bin/main_mpi", input_file]
    
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
            'total_time': parsed_info['total_time'],
            'sa_time': parsed_info['sa_time'],
            'lcp_time': parsed_info['lcp_time'],
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
            'total_time': 0.0,
            'sa_time': 0.0,
            'lcp_time': 0.0,
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
            'total_time': 0.0,
            'sa_time': 0.0,
            'lcp_time': 0.0,
            'mpi_processes': num_processes,
            'parallel_efficiency': 0.0
        }

def format_time(seconds):
    """Formatta il tempo in modo leggibile"""
    if seconds < 1:
        return f"{seconds*1000:.0f}ms"
    elif seconds < 60:
        return f"{seconds:.2f}s"
    else:
        minutes = seconds / 60
        return f"{minutes:.1f}m"

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
        #"test_data/large/random_100MB.txt",
        #"test_data/large/random_200MB.txt",
        #"test_data/largerandom_500MB.txt"
    ]
    
    mpi_results = []
    process_configs = [2, 4, 8]  # Test con 2, 4 e 8 processi

    print("Testing MPI con diverse configurazioni...")
    print("------------------------------------------------------------")
    
    for test_file in mpi_files:
        if not os.path.exists(test_file):
            print(f"{os.path.basename(test_file):25} - NON TROVATO")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"{os.path.basename(test_file):25} ({file_size_mb:6.2f} MB)")
        
        for num_proc in process_configs:
            print(f"  MPI-{num_proc:2} processi...", end=" ", flush=True)
            
            result = run_mpi_benchmark(test_file, num_proc)
            
            if result['success']:
                time_str = format_time(result['total_time'])
                print(f"OK ({time_str:>7}) - LRS Length: {result['lrs_length']}")
                
                mpi_results.append({
                    'file': os.path.basename(test_file),
                    'size_bytes': file_size,
                    'size_mb': file_size_mb,
                    'backend': f'mpi_{num_proc}',
                    'processes': num_proc,
                    'time_seconds': result['total_time'],
                    'sa_time': result['sa_time'],
                    'lcp_time': result['lcp_time']
                })
            else:
                print(f"FAILED - Error: {result['error']}")

    # Calcola speedup e efficienza
    if mpi_results:
        df = pd.DataFrame(mpi_results)
        
        # Carica i tempi della versione sequenziale per il confronto
        seq_times = {}
        try:
            df_seq = pd.read_csv("results/csv/sequential_results.csv")
            seq_times = pd.Series(df_seq.sa_time.values, index=df_seq.file).to_dict()
        except FileNotFoundError:
            print("\nAttenzione: file dei risultati sequenziali non trovato. Speedup non calcolato.")
        
        df['speedup'] = df.apply(
            lambda row: seq_times.get(row['file'], 0) / row['sa_time'] if row['sa_time'] > 0 else 0,
            axis=1
        )
        df['efficiency'] = df.apply(
            lambda row: row['speedup'] / row['processes'] if row['processes'] > 0 else 0,
            axis=1
        )

        # Salva i risultati
        os.makedirs("results/csv", exist_ok=True)
        output_file = "results/csv/mpi_results.csv"
        df.to_csv(output_file, index=False)
        
        print("\n" + "=" * 60)
        print("BENCHMARK MPI COMPLETATO")
        print(f"Risultati salvati in: {output_file}")
        print("=" * 60)
        
        print("\nRIEPILOGO RISULTATI:")
        print("-" * 65)
        print(f"{'File':<25} {'Proc':>5} {'Tempo SA':>10} {'Speedup':>10} {'Efficienza':>12}")
        print("-" * 65)
        for _, row in df.iterrows():
            efficiency_str = f"{row['efficiency'] * 100:.1f}%"
            print(f"{row['file']:<25} {row['processes']:>5} {format_time(row['sa_time']):>10} {row['speedup']:>9.2f}x {efficiency_str:>12}")
        print("-" * 65)

if __name__ == "__main__":
    main()