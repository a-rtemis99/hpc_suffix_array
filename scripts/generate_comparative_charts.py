#!/usr/bin/env python3
"""
Genera grafici comparativi per tutti i backend (Sequential, MPI, CUDA)
Versione aggiornata per analisi multi-backend
"""
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os
from datetime import datetime

# Configurazione styling
plt.style.use('seaborn-v0_8-whitegrid')
sns.set_palette("husl")
plt.rcParams['figure.figsize'] = (12, 8)
plt.rcParams['font.size'] = 12

def load_combined_results():
    """Carica e combina tutti i risultati dei benchmark"""
    results_files = [
        "results/benchmarks/sequential_results.csv",
        "results/benchmarks/mpi_results.csv", 
        "results/benchmarks/cuda_results.csv"
    ]
    
    all_data = []
    for file in results_files:
        if os.path.exists(file):
            df = pd.read_csv(file)
            # Filtra solo test riusciti
            df = df[df['success'] == True] if 'success' in df.columns else df
            all_data.append(df)
    
    if all_data:
        return pd.concat(all_data, ignore_index=True)
    else:
        print("Nessun dato di benchmark trovato!")
        return None

def create_comparative_analysis(output_dir):
    """Crea analisi comparativa multi-backend"""
    df = load_combined_results()
    if df is None or len(df) == 0:
        print("Nessun dato valido per l'analisi")
        return
    
    print("Creando analisi comparativa multi-backend...")
    
    # 1. GRAFICO: Confronto prestazioni assolute
    plt.figure(figsize=(14, 10))
    
    # Subplot 1: Tempi di esecuzione comparati
    plt.subplot(2, 2, 1)
    
    backends = df['backend'].unique() if 'backend' in df.columns else ['sequential']
    colors = plt.cm.Set3(np.linspace(0, 1, len(backends)))
    
    for i, backend in enumerate(backends):
        backend_data = df[df['backend'] == backend]
        if len(backend_data) > 0:
            plt.plot(backend_data['size_mb'], backend_data['time_seconds'], 
                    'o-', linewidth=3, markersize=8, label=backend, color=colors[i])
    
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Dimensione File (MB) - Scala Logaritmica')
    plt.ylabel('Tempo di Esecuzione (secondi) - Scala Logaritmica')
    plt.title('CONFRONTO PRESTAZIONI: Sequential vs MPI vs CUDA')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Subplot 2: Throughput comparato
    plt.subplot(2, 2, 2)
    
    for i, backend in enumerate(backends):
        backend_data = df[df['backend'] == backend]
        if len(backend_data) > 0 and 'throughput_mb_s' in backend_data.columns:
            plt.plot(backend_data['size_mb'], backend_data['throughput_mb_s'], 
                    's-', linewidth=2, markersize=6, label=backend, color=colors[i])
    
    plt.xscale('log')
    plt.xlabel('Dimensione File (MB) - Scala Logaritmica')
    plt.ylabel('Throughput (MB/s)')
    plt.title('CONFRONTO THROUGHPUT')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Subplot 3: Speedup rispetto al sequenziale
    plt.subplot(2, 2, 3)
    
    if 'sequential' in backends and len(backends) > 1:
        sequential_times = df[df['backend'] == 'sequential'].set_index('file')['time_seconds']
        
        for backend in backends:
            if backend != 'sequential':
                backend_data = df[df['backend'] == backend].set_index('file')
                common_files = sequential_times.index.intersection(backend_data.index)
                if len(common_files) > 0:
                    speedup = sequential_times[common_files] / backend_data.loc[common_files]['time_seconds']
                    plt.plot(backend_data.loc[common_files]['size_mb'], speedup, 
                            '^-', linewidth=2, markersize=6, label=f'{backend} speedup')
        
        plt.axhline(y=1, color='red', linestyle='--', alpha=0.7, label='Baseline (sequential)')
        plt.xlabel('Dimensione File (MB)')
        plt.ylabel('Speedup (vs Sequential)')
        plt.title('ANALISI SPEEDUP MPI/CUDA')
        plt.legend()
        plt.grid(True, alpha=0.3)
    else:
        plt.text(0.5, 0.5, 'Dati insufficienti\nper analisi speedup', 
                ha='center', va='center', transform=plt.gca().transAxes)
        plt.title('ANALISI SPEEDUP (dati insufficienti)')
    
    # Subplot 4: Efficienza parallela
    plt.subplot(2, 2, 4)
    
    if 'mpi_2' in backends and 'mpi_4' in backends:
        # Calcola efficienza MPI
        mpi_2_data = df[df['backend'] == 'mpi_2'].set_index('file')
        mpi_4_data = df[df['backend'] == 'mpi_4'].set_index('file')
        common_files = mpi_2_data.index.intersection(mpi_4_data.index)
        
        if len(common_files) > 0:
            efficiency = (mpi_2_data.loc[common_files]['time_seconds'] / 
                         mpi_4_data.loc[common_files]['time_seconds']) / 2 * 100
            plt.plot(mpi_4_data.loc[common_files]['size_mb'], efficiency, 
                    'D-', linewidth=2, markersize=6, label='Efficienza MPI', color='purple')
            plt.axhline(y=100, color='red', linestyle='--', alpha=0.5, label='Efficienza ideale')
            plt.xlabel('Dimensione File (MB)')
            plt.ylabel('Efficienza Parallela (%)')
            plt.title('EFFICIENZA PARALLELA MPI')
            plt.legend()
            plt.grid(True, alpha=0.3)
        else:
            plt.text(0.5, 0.5, 'Dati MPI insufficienti\nper analisi efficienza', 
                    ha='center', va='center', transform=plt.gca().transAxes)
            plt.title('EFFICIENZA PARALLELA (dati insufficienti)')
    else:
        plt.text(0.5, 0.5, 'Dati MPI insufficienti\nper analisi efficienza', 
                ha='center', va='center', transform=plt.gca().transAxes)
        plt.title('EFFICIENZA PARALLELA (dati insufficienti)')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/comparative_analysis.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print("Grafico comparativo generato: comparative_analysis.png")

def generate_multi_backend_report(output_dir):
    """Genera report dettagliato multi-backend"""
    df = load_combined_results()
    if df is None:
        return
    
    with open(f'{output_dir}/multi_backend_report.txt', 'w') as f:
        f.write("HPC SUFFIX ARRAY - RAPPORTO MULTI-BACKEND\n")
        f.write("=" * 70 + "\n")
        f.write(f"Generato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        
        # Statistiche per backend
        f.write("STATISTICHE PER BACKEND\n")
        f.write("-" * 40 + "\n")
        
        backends = df['backend'].unique() if 'backend' in df.columns else ['sequential']
        
        for backend in backends:
            backend_data = df[df['backend'] == backend]
            if len(backend_data) > 0:
                f.write(f"\n{backend.upper()}:\n")
                f.write(f"  • Test completati: {len(backend_data)}\n")
                f.write(f"  • Tempo medio: {backend_data['time_seconds'].mean():.2f}s\n")
                if 'throughput_mb_s' in backend_data.columns:
                    f.write(f"  • Throughput medio: {backend_data['throughput_mb_s'].mean():.1f} MB/s\n")
        
        # Analisi speedup
        f.write("\nANALISI SPEEDUP\n")
        f.write("-" * 40 + "\n")
        
        if 'sequential' in backends and len(backends) > 1:
            sequential_avg = df[df['backend'] == 'sequential']['time_seconds'].mean()
            
            for backend in backends:
                if backend != 'sequential':
                    backend_avg = df[df['backend'] == backend]['time_seconds'].mean()
                    speedup = sequential_avg / backend_avg if backend_avg > 0 else 0
                    f.write(f"  • {backend}: {speedup:.2f}x speedup\n")

def main():
    """Funzione principale aggiornata"""
    output_dir = "results/charts"
    os.makedirs(output_dir, exist_ok=True)
    
    print("HPC Suffix Array - Multi-Backend Performance Analysis")
    print("=" * 60)
    
    # Genera analisi comparativa
    create_comparative_analysis(output_dir)
    generate_multi_backend_report(output_dir)
    
    print("\nANALISI MULTI-BACKEND COMPLETATA!")
    print("=" * 50)
    print("GRAFICI GENERATI:")
    print("   • comparative_analysis.png - Confronto completo 4 quadranti")
    print("REPORT:")
    print("   • multi_backend_report.txt - Statistiche comparative")
    print("BACKEND ANALIZZATI: Sequential, MPI, CUDA")

if __name__ == "__main__":
    main()