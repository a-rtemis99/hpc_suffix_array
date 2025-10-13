#!/usr/bin/env python3
"""
Orchestratore - Esegue tutti i benchmark in sequenza
Versione intelligente che rileva automaticamente l'ambiente
"""
import subprocess
import sys
import os
import time
from datetime import datetime

def is_kaggle_environment():
    """Rileva se siamo in ambiente Kaggle"""
    return os.path.exists("/kaggle")

def run_script(script_name, description):
    """Esegue uno script e gestisce gli errori"""
    print(f"\n{'='*60}")
    print(f"{description}")
    print(f"Script: {script_name}")
    print(f"{'='*60}")
    
    start_time = time.time()
    
    try:
        result = subprocess.run(
            [sys.executable, script_name],
            capture_output=True,
            text=True
        )
        execution_time = time.time() - start_time
        
        if result.returncode == 0:
            print(f"SUCCESSO - {execution_time:.1f}s")
            return True
        else:
            print(f"FALLITO - {execution_time:.1f}s")
            if result.stderr:
                print(f"   Error: {result.stderr[:200]}")
            return False
            
    except Exception as e:
        print(f"ECCEZIONE: {e}")
        return False

def main():
    print("HPC SUFFIX ARRAY - BENCHMARK COMPLETO AUTOMATICO")
    print("=" * 60)
    print(f"Avviato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    # Rileva ambiente
    if is_kaggle_environment():
        print("🌐 Ambiente: KAGGLE (con GPU)")
        cuda_script = "scripts/benchmark_cuda_kaggle.py"
    else:
        print("💻 Ambiente: LOCALE (senza GPU)")  
        cuda_script = "scripts/benchmark_cuda_stub.py"
    
    # Lista script da eseguire
    scripts = [
        ("scripts/benchmark_sequential.py", "BENCHMARK SEQUENZIALE"),
        ("scripts/benchmark_mpi.py", "BENCHMARK MPI"),
        (cuda_script, "BENCHMARK CUDA")
    ]
    
    successful = 0
    total = len(scripts)
    
    for script_path, description in scripts:
        if run_script(script_path, description):
            successful += 1
        print()  # Linea vuota
    
    # Report finale
    print("=" * 60)
    print("BENCHMARK COMPLETATI!")
    print("=" * 60)
    print(f"Script eseguiti con successo: {successful}/{total}")
    print(f"Ora: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    if successful == total:
        print("Tutti i benchmark completati con successo!")
        print("Usa: python scripts/generate_performance_charts.py per i grafici")
    else:
        print("Alcuni benchmark hanno fallito, controlla i log sopra")
    
    print("\nI risultati sono in: results/benchmarks/")

if __name__ == "__main__":
    main()