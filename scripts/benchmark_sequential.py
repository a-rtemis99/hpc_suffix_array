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
        'execution_details': 'N/A',
        'total_time': 0.0,
        'sa_time': 0.0,
        'lcp_time': 0.0
    }
    
    # Cerca i dati leggibili
    lrs_match = re.search(r"Longest repeated substring:.*\(length: (\d+)\)", output)
    if lrs_match:
        result['lrs_length'] = int(lrs_match.group(1))

    lrs_str_match = re.search(r"Longest repeated substring: '([^']*)'", output)
    if lrs_str_match:
        result['lrs_string'] = lrs_str_match.group(1)

    # Estrae i dati precisi dal blocco strutturato
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
    
    # Fallback se il blocco strutturato non c'è (dovrebbe esserci)
    if result['total_time'] == 0.0:
        total_time_fallback = re.search(r"Total execution time: ([\d.]+)", output)
        if total_time_fallback:
            result['total_time'] = float(total_time_fallback.group(1))
            
    if result['sa_time'] == 0.0:
        sa_time_fallback = re.search(r"Suffix array construction time: ([\d.]+)", output)
        if sa_time_fallback:
            result['sa_time'] = float(sa_time_fallback.group(1))
            
    if result['lcp_time'] == 0.0:
        lcp_time_fallback = re.search(r"LCP construction \+ LRS search time: ([\d.]+)", output)
        if lcp_time_fallback:
            result['lcp_time'] = float(lcp_time_fallback.group(1))

    return result

def run_benchmark(input_file):
    """Esegue il benchmark per un singolo file"""
    cmd = ["./bin/main_sequential", input_file]
    
    try:
        result = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True, 
            timeout=7200  # 2 ore timeout
        )
        
        parsed_info = parse_output(result.stdout)
        
        # Se il parsing fallisce, usa il tempo totale esterno
        exec_time = parsed_info['total_time']
        if exec_time == 0.0:
             # Fallback estremo se lo stdout è rotto
             exec_time = (time.time() - subprocess.run(['date', '+%s'], capture_output=True, text=True).stdout.strip())
             
        
        return {
            'success': result.returncode == 0,
            'time': exec_time, # Ritorna il tempo totale per il logger principale
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
        return { 'success': False, 'time': 7200, 'error': 'TIMEOUT', 'lrs_length': 0, 'lrs_string': 'TIMEOUT', 'sa_time': 0.0, 'lcp_time': 0.0, 'total_time': 0.0, 'suffix_array_length': 0 }
    except Exception as e:
        return { 'success': False, 'time': 0, 'error': str(e), 'lrs_length': 0, 'lrs_string': 'ERROR', 'sa_time': 0.0, 'lcp_time': 0.0, 'total_time': 0.0, 'suffix_array_length': 0 }

def format_time(seconds):
    """Formatta il tempo in modo leggibile"""
    if seconds < 1:
        return f"{seconds*1000:.0f}ms"
    elif seconds < 60:
        return f"{seconds:.2f}s"
    else:
        return f"{seconds / 60:.1f}m"

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
            print(f"{os.path.basename(test_file):<25} - NON TROVATO")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"{os.path.basename(test_file):<25} ({file_size_mb:6.2f} MB)...", end=" ", flush=True)
        
        result = run_benchmark(test_file)
        
        if result['success']:
            time_str = format_time(result['total_time']) # Tempo totale per il log
            lrs_preview = result['lrs_string'][:20] if result['lrs_string'] != 'N/A' else 'N/A'
            print(f"OK ({time_str:>8}) - LRS: {result['lrs_length']:3} chars ('{lrs_preview}...')")
            successful_tests += 1
            
            sequential_results.append({
                'file': os.path.basename(test_file),
                'size_bytes': file_size,
                'size_mb': file_size_mb,
                'backend': 'sequential',
                'processes': 1,
                'time_seconds': result['total_time'],
                'sa_time': result['sa_time'],
                'lcp_time': result['lcp_time']
            })
        else:
            print("FAILED")
            if result['error']:
                print(f"      Error: {result['error'][:100]}")

    # Salva risultati
    if sequential_results:
        df_seq = pd.DataFrame(sequential_results)
        
        output_dir = "results/csv"
        os.makedirs(output_dir, exist_ok=True)
        output_file = os.path.join(output_dir, "sequential_results.csv")
        
        df_seq.to_csv(output_file, index=False)
        
        print("\n" + "=" * 60)
        print("BENCHMARK SEQUENZIALE COMPLETATO")
        print(f"Risultati salvati in: {output_file}")
        print(f"Test completati: {successful_tests}/{len(sequential_files)}")
        
        print("\nRIEPILOGO DETTAGLIATO:")
        print("-" * 75)
        print(f"{'File':<25} {'Size(MB)':>10} {'Tempo SA':>10} {'Tempo LCP/LRS':>14} {'Tempo Totale':>12}")
        print("-" * 75)
        
        for _, row in df_seq.iterrows():
            sa_time_str = format_time(row['sa_time'])
            lcp_time_str = format_time(row['lcp_time'])
            total_time_str = format_time(row['time_seconds'])
            print(f"{row['file']:<25} {row['size_mb']:10.2f} {sa_time_str:>10} {lcp_time_str:>14} {total_time_str:>12}")
        print("-" * 75)
            
    else:
        print("\nNessun test completato con successo!")
        sys.exit(1)

if __name__ == "__main__":
    main()