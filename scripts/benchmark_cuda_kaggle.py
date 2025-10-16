#!/usr/bin/env python3
"""
CUDA Benchmark - DA ESEGUIRE SOLO SU KAGGLE CON GPU
"""
import subprocess
import time
import os
import pandas as pd
from datetime import datetime
import re

def parse_output(output):
    """Estrae informazioni dettagliate dall'output del programma CUDA"""
    result = {
        'lrs_length': 0,
        'lrs_string': 'N/A',
        'suffix_array_length': 0,
        'execution_details': 'N/A',
        'total_time': 0.0,
        'sa_time': 0.0,
        'lcp_time': 0.0,
        'gpu_memory_used': 0,
        'gpu_utilization': 0.0,
        'cuda_kernel_times': 'N/A',
        'speedup_vs_cpu': 0.0
    }
    
    lines = output.split('\n')
    
    for line in lines:
        # Cerca informazioni GPU
        if 'GPU memory used:' in line or 'Memory used:' in line:
            match = re.search(r'(\d+\.?\d*)\s*([KMG]?B)', line, re.IGNORECASE)
            if match:
                value = float(match.group(1))
                unit = match.group(2).upper()
                # Converti in MB
                if unit == 'KB':
                    result['gpu_memory_used'] = value / 1024
                elif unit == 'GB':
                    result['gpu_memory_used'] = value * 1024
                else:  # MB
                    result['gpu_memory_used'] = value
        
        # Cerca utilizzo GPU
        if 'GPU utilization:' in line or 'Utilization:' in line:
            match = re.search(r'(\d+\.?\d*)%', line)
            if match:
                result['gpu_utilization'] = float(match.group(1))
        
        # Cerca lunghezza LRS - formato: "Longest repeated substring: 'ana' (length: 3)"
        if 'Longest repeated substring:' in line and 'length:' in line:
            # Estrai la lunghezza
            length_match = re.search(r'length:\s*(\d+)', line)
            if length_match:
                result['lrs_length'] = int(length_match.group(1))
            
            # Estrai la stringa LRS
            string_match = re.search(r"substring:\s*'([^']*)'", line)
            if string_match:
                result['lrs_string'] = string_match.group(1)
            else:
                string_match = re.search(r'substring:\s*"([^"]*)"', line)
                if string_match:
                    result['lrs_string'] = string_match.group(1)
        
        # Cerca informazioni sul suffix array
        if 'Actual string length:' in line:
            match = re.search(r'Actual string length:\s*(\d+)', line)
            if match:
                result['suffix_array_length'] = int(match.group(1))
        
        # Estrai tempi di esecuzione
        if 'Total execution time:' in line:
            match = re.search(r'Total execution time:\s*([\d.]+)', line)
            if match:
                result['total_time'] = float(match.group(1))
                result['execution_details'] = line.strip()
        
        # Estrai tempi specifici dalla sezione STRUCTURED_RESULTS
        if 'TOTAL_TIME:' in line:
            match = re.search(r'TOTAL_TIME:([\d.]+)', line)
            if match:
                result['total_time'] = float(match.group(1))
        if 'SA_TIME:' in line:
            match = re.search(r'SA_TIME:([\d.]+)', line)
            if match:
                result['sa_time'] = float(match.group(1))
        if 'LCP_TIME:' in line:
            match = re.search(r'LCP_TIME:([\d.]+)', line)
            if match:
                result['lcp_time'] = float(match.group(1))
        
        # Estrai dettagli di esecuzione CUDA
        if 'CUDA kernel time:' in line or 'Kernel time:' in line:
            result['cuda_kernel_times'] = line.strip()
        
        # Cerca speedup rispetto alla CPU
        if 'Speedup vs CPU:' in line or 'Speedup:' in line:
            match = re.search(r'(\d+\.\d+)', line)
            if match:
                result['speedup_vs_cpu'] = float(match.group(1))
    
    return result

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
            'gpu_memory_used_mb': parsed_info['gpu_memory_used'],
            'gpu_utilization': parsed_info['gpu_utilization'],
            'cuda_kernel_times': parsed_info['cuda_kernel_times'],
            'speedup_vs_cpu': parsed_info['speedup_vs_cpu']
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
            'gpu_memory_used_mb': 0,
            'gpu_utilization': 0.0,
            'cuda_kernel_times': 'TIMEOUT',
            'speedup_vs_cpu': 0.0
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
            'gpu_memory_used_mb': 0,
            'gpu_utilization': 0.0,
            'cuda_kernel_times': 'ERROR',
            'speedup_vs_cpu': 0.0
        }

def format_time(seconds):
    """Formatta il tempo in modo leggibile"""
    if seconds < 0.001:
        return f"{seconds*1000:.2f}ms"
    elif seconds < 1:
        return f"{seconds*1000:.0f}ms"
    elif seconds < 60:
        return f"{seconds:.2f}s"
    elif seconds < 3600:
        minutes = seconds / 60
        return f"{minutes:.1f}m"
    else:
        hours = seconds / 3600
        return f"{hours:.1f}h"

def format_memory(mb):
    """Formatta la memoria in modo leggibile"""
    if mb < 1:
        return f"{mb*1024:.0f} KB"
    elif mb < 1024:
        return f"{mb:.0f} MB"
    else:
        return f"{mb/1024:.1f} GB"

def main():
    print("CUDA BENCHMARK - KAGGLE GPU")
    print("============================================================")
    print(f"Avviato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("ATTENZIONE: Questo script richiede GPU NVIDIA")
    print()
    
    # File di test per CUDA
    cuda_files = [
        "test_data/banana.txt",
        "test_data/mississippi.txt",
        "test_data/abcabcabc.txt",
        "test_data/aaaa.txt",
        "test_data/ababab.txt",
        "test_data/large/random_1MB.txt",
        "test_data/large/random_50MB.txt", 
        "test_data/large/random_100MB.txt",
        "test_data/large/random_200MB.txt"
    ]

    cuda_results = []
    successful_tests = 0

    print("Testing versione CUDA...")
    print("------------------------------------------------------------")
    
    for test_file in cuda_files:
        if not os.path.exists(test_file):
            print(f"{os.path.basename(test_file):25} - NON TROVATO")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"{os.path.basename(test_file):25} ({file_size_mb:6.1f} MB)...", end=" ", flush=True)
        
        result = run_cuda_benchmark(test_file)
        
        if result['success']:
            time_str = format_time(result['time'])
            speedup_str = f"{result['speedup_vs_cpu']:.2f}x" if result['speedup_vs_cpu'] > 0 else "N/A"
            lrs_preview = result['lrs_string'][:15] if result['lrs_string'] != 'N/A' else 'N/A'
            print(f"{time_str:>8} - LRS: {result['lrs_length']:3} chars - Speedup: {speedup_str:>6}")
            
            successful_tests += 1
            
            cuda_results.append({
                'file': os.path.basename(test_file),
                'size_bytes': file_size,
                'size_mb': file_size_mb,
                'backend': 'cuda',
                'time_seconds': result['time'],
                'throughput_mb_s': file_size_mb / result['time'] if result['time'] > 0 else 0,
                'throughput_chars_per_second': file_size / result['time'] if result['time'] > 0 else 0,
                'lrs_length': result['lrs_length'],
                'lrs_string': result['lrs_string'],
                'suffix_array_length': result['suffix_array_length'],
                'execution_details': result['execution_details'],
                'total_time': result['total_time'],
                'sa_time': result['sa_time'],
                'lcp_time': result['lcp_time'],
                'gpu_memory_used_mb': result['gpu_memory_used_mb'],
                'gpu_utilization': result['gpu_utilization'],
                'cuda_kernel_times': result['cuda_kernel_times'],
                'speedup_vs_cpu': result['speedup_vs_cpu'],
                'success': True,
                'timestamp': datetime.now()
            })
        else:
            print("FAILED")
            if result['error']:
                print(f"      Error: {result['error'][:100]}")

    # Calcola speedup rispetto alla versione sequenziale se disponibile
    if cuda_results:
        # Carica risultati sequenziali per confronto
        sequential_times = {}
        try:
            if os.path.exists("results/benchmarks/sequential_results.csv"):
                df_seq = pd.read_csv("results/benchmarks/sequential_results.csv")
                for _, row in df_seq.iterrows():
                    sequential_times[row['file']] = row['time_seconds']
        except:
            pass
        
        # Calcola speedup per ogni risultato CUDA
        for result in cuda_results:
            file_name = result['file']
            if file_name in sequential_times and sequential_times[file_name] > 0:
                calculated_speedup = sequential_times[file_name] / result['time_seconds']
                #