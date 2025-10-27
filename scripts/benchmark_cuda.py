"""
Benchmark per la versione CUDA dell'algoritmo Suffix Array
"""
import subprocess
import time
import os
import pandas as pd
import sys
from datetime import datetime
import re

def parse_output(output):
    """Estrae informazioni dettagliate dall'output del programma CUDA."""
    result = {
        'lrs_length': 0, 'lrs_string': 'N/A', 'sa_time': 0.0, 'lcp_time': 0.0,
        'total_time': 0.0, 'mpi_processes': 0, 'suffix_array_length': 0
    }
    
    lrs_match = re.search(r"Longest repeated substring:.*\(length: (\d+)\)", output)
    if lrs_match:
        result['lrs_length'] = int(lrs_match.group(1))

    lrs_str_match = re.search(r"Longest repeated substring: '([^']*)'", output)
    if lrs_str_match:
        result['lrs_string'] = lrs_str_match.group(1)

    if '--- STRUCTURED_RESULTS ---' in output:
        structured_data = output.split('--- STRUCTURED_RESULTS ---')[1]
        
        sa_time_match = re.search(r"SA_TIME:([\d.]+)", structured_data)
        if sa_time_match: result['sa_time'] = float(sa_time_match.group(1))

        lcp_time_match = re.search(r"LCP_TIME:([\d.]+)", structured_data)
        if lcp_time_match: result['lcp_time'] = float(lcp_time_match.group(1))

        total_time_match = re.search(r"TOTAL_TIME:([\d.]+)", structured_data)
        if total_time_match: result['total_time'] = float(total_time_match.group(1))

        len_match = re.search(r"ACTUAL_STRING_LENGTH:(\d+)", structured_data)
        if len_match: result['suffix_array_length'] = int(len_match.group(1))
    
    # Fallback
    if result['total_time'] == 0.0:
        total_time_fallback = re.search(r"Total execution time.*: ([\d.]+)", output)
        if total_time_fallback:
            result['total_time'] = float(total_time_fallback.group(1))
            
    if result['sa_time'] == 0.0:
        sa_time_fallback = re.search(r"Suffix array construction time.*: ([\d.]+)", output)
        if sa_time_fallback:
            result['sa_time'] = float(sa_time_fallback.group(1))

    return result


def run_cuda_benchmark(input_file):
    """Esegue benchmark CUDA"""
    cmd = ["./bin/main_cuda", input_file] # Nome dell'eseguibile CUDA

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=3600  # 1 ora timeout
        )
        parsed_info = parse_output(result.stdout)

        return {
            'success': result.returncode == 0,
            'time': parsed_info['total_time'], # Usa il tempo interno CUDA
            'output': result.stdout,
            'error': result.stderr,
            'lrs_length': parsed_info['lrs_length'],
            'lrs_string': parsed_info['lrs_string'],
            'suffix_array_length': parsed_info['suffix_array_length'],
            'total_time': parsed_info['total_time'],
            'sa_time': parsed_info['sa_time'],
            'lcp_time': parsed_info['lcp_time']
        }

    except subprocess.TimeoutExpired:
        return {'success': False, 'time': 3600, 'error': 'TIMEOUT', 'lrs_length': 0, 'lrs_string': 'TIMEOUT', 'sa_time':0.0, 'lcp_time':0.0, 'total_time':0.0, 'suffix_array_length': 0}
    except Exception as e:
         return {'success': False, 'time': 0, 'error': str(e), 'lrs_length': 0, 'lrs_string': 'ERROR', 'sa_time':0.0, 'lcp_time':0.0, 'total_time':0.0, 'suffix_array_length': 0}

def format_time(seconds):
    """Formatta il tempo in modo leggibile"""
    if seconds < 1: return f"{seconds*1000:.0f}ms"
    elif seconds < 60: return f"{seconds:.2f}s"
    else: return f"{seconds / 60:.1f}m"

def main():
    print("BENCHMARK CUDA - Suffix Array")
    print("=" * 60)
    print(f"Avviato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()

    cuda_files = [
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

    cuda_results = []
    print("Testing versione CUDA...")
    print("-" * 60)

    for test_file in cuda_files:
        if not os.path.exists(test_file):
            print(f"{os.path.basename(test_file):<25} - NON TROVATO")
            continue

        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)

        print(f"{os.path.basename(test_file):<25} ({file_size_mb:6.2f} MB)...", end=" ", flush=True)

        result = run_cuda_benchmark(test_file)

        if result['success']:
            time_str = format_time(result['total_time'])
            print(f"OK ({time_str:>7}) - LRS Length: {result['lrs_length']}")
            cuda_results.append({
                'file': os.path.basename(test_file),
                'size_bytes': file_size,
                'size_mb': file_size_mb,
                'backend': 'cuda',
                'processes': 1, 
                'time_seconds': result['total_time'],
                'sa_time': result['sa_time'],
                'lcp_time': result['lcp_time']
            })
        else:
            print(f"FAILED - Error: {result['error']}")

    if cuda_results:
        df = pd.DataFrame(cuda_results)

        # Calcola speedup vs sequenziale
        seq_times = {}
        seq_csv_path = "results/csv/sequential_results.csv"
        try:
            df_seq = pd.read_csv(seq_csv_path)
            seq_times = pd.Series(df_seq.sa_time.values, index=df_seq.file).to_dict()
        except FileNotFoundError:
            print(f"\nAttenzione: file '{seq_csv_path}' non trovato. Speedup non calcolato.")

        if seq_times:
            df['speedup_vs_seq'] = df.apply(
                lambda row: seq_times.get(row['file'], 0) / row['sa_time'] if row['sa_time'] > 0 else 0,
                axis=1
            )
        else:
            df['speedup_vs_seq'] = 0.0

        output_dir = "results/csv" 
        os.makedirs(output_dir, exist_ok=True)
        output_file = os.path.join(output_dir, "cuda_results.csv")
        df.to_csv(output_file, index=False)

        print("\n" + "=" * 60)
        print("BENCHMARK CUDA COMPLETATO")
        print(f"Risultati salvati in: {output_file}")
        print("=" * 60)
        print("\nRIEPILOGO RISULTATI CUDA:")
        print("-" * 60)
        print(f"{'File':<25} {'Tempo SA':>10} {'Speedup Seq':>12}")
        print("-" * 60)
        for _, row in df.iterrows():
            speedup_str = f"{row['speedup_vs_seq']:.2f}x"
            if not seq_times: speedup_str = "N/A"
            print(f"{row['file']:<25} {format_time(row['sa_time']):>10} {speedup_str:>12}")
        print("-" * 60)

if __name__ == "__main__":
    main()