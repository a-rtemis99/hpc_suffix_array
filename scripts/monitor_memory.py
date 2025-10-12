#!/usr/bin/env python3
"""
Script per monitorare l'utilizzo di memoria durante l'esecuzione
"""
import os
import time
import subprocess
import psutil
import threading

def get_memory_usage():
    """Restituisce l'utilizzo di memoria in MB"""
    process = psutil.Process()
    return process.memory_info().rss / (1024 * 1024)  # MB

def monitor_memory(interval=0.1, duration=3600):
    """Monitora l'utilizzo di memoria per un periodo"""
    start_time = time.time()
    max_memory = 0
    
    print("Monitoring memory usage...")
    print("Time(s)\tMemory(MB)\tPeak(MB)")
    
    while time.time() - start_time < duration:
        current_memory = get_memory_usage()
        max_memory = max(max_memory, current_memory)
        
        elapsed = time.time() - start_time
        print(f"{elapsed:.1f}\t{current_memory:.1f}\t\t{max_memory:.1f}")
        
        time.sleep(interval)
    
    return max_memory

def run_with_memory_monitoring(command, timeout=3600):
    """Esegue un comando monitorando la memoria"""
    print(f"Executing: {' '.join(command)}")
    print("-" * 50)
    
    # Avvia il monitoraggio in un thread separato
    peak_memory = [0]
    stop_monitoring = [False]
    
    def monitor_thread():
        while not stop_monitoring[0]:
            current_memory = get_memory_usage()
            peak_memory[0] = max(peak_memory[0], current_memory)
            time.sleep(0.1)
    
    monitor_thread = threading.Thread(target=monitor_thread)
    monitor_thread.start()
    
    try:
        # Esegui il comando
        start_time = time.time()
        result = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
        execution_time = time.time() - start_time
        
        # Ferma il monitoraggio
        stop_monitoring[0] = True
        monitor_thread.join()
        
        print("-" * 50)
        print(f"Execution time: {execution_time:.2f} seconds")
        print(f"Peak memory usage: {peak_memory[0]:.1f} MB")
        
        if result.returncode == 0:
            print("✓ Execution successful")
            # Estrai risultato LRS se presente
            for line in result.stdout.split('\n'):
                if "Longest repeated substring" in line:
                    print(f"Result: {line}")
        else:
            print(f"✗ Execution failed: {result.stderr}")
            
        return result.returncode == 0, execution_time, peak_memory[0]
        
    except subprocess.TimeoutExpired:
        stop_monitoring[0] = True
        monitor_thread.join()
        print("✗ Execution timed out")
        return False, timeout, peak_memory[0]

def main():
    """Test su file di diverse dimensioni con monitoraggio memoria"""
    test_files = [
        "test_data/banana.txt",
        "test_data/mississippi.txt",
        "test_data/large/random_1MB.txt",
        "test_data/large/random_50MB.txt",
        # Aggiungi gradualmente file più grandi
    ]
    
    print("HPC Suffix Array - Memory Monitoring Test")
    print("=" * 60)
    
    results = []
    
    for test_file in test_files:
        if not os.path.exists(test_file):
            print(f"File not found: {test_file}")
            continue
            
        file_size = os.path.getsize(test_file)
        file_size_mb = file_size / (1024 * 1024)
        
        print(f"\n{'='*60}")
        print(f"Testing: {os.path.basename(test_file)}")
        print(f"File size: {file_size:,} bytes ({file_size_mb:.1f} MB)")
        print(f"{'='*60}")
        
        success, exec_time, peak_memory = run_with_memory_monitoring(
            ["./bin/sequential_suffix_array", test_file],
            timeout=7200  # 2 ore di timeout
        )
        
        if success:
            results.append({
                'file': os.path.basename(test_file),
                'size_mb': file_size_mb,
                'time_seconds': exec_time,
                'peak_memory_mb': peak_memory,
                'throughput_chars_per_sec': file_size / exec_time if exec_time > 0 else 0
            })
        
        # Pausa tra i test per non sovraccaricare il sistema
        time.sleep(2)
    
    # Stampa riepilogo
    print(f"\n{'='*60}")
    print("MEMORY USAGE SUMMARY")
    print(f"{'='*60}")
    for result in results:
        print(f"\n{result['file']}:")
        print(f"  Size: {result['size_mb']:.1f} MB")
        print(f"  Time: {result['time_seconds']:.2f} s") 
        print(f"  Peak Memory: {result['peak_memory_mb']:.1f} MB")
        print(f"  Throughput: {result['throughput_chars_per_sec']:,.0f} chars/s")
        print(f"  Memory/Size Ratio: {result['peak_memory_mb']/result['size_mb']:.1f}x")

if __name__ == "__main__":
    main()