#!/usr/bin/env /mnt/c/Users/Artemis/Desktop/hpc_suffix_array/hpc_env/bin/python3
"""
Genera grafici comparativi tra Sequential vs MPI vs CUDA
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

def create_comparative_analysis(csv_file, output_dir):
    """Analisi comparativa tra implementazioni"""
    
    df = pd.read_csv(csv_file)
    
    # Separa per implementazione
    sequential_df = df[df['implementation'] == 'sequential']
    mpi_df = df[df['implementation'] == 'mpi']
    cuda_df = df[df['implementation'] == 'cuda'] if 'cuda' in df['implementation'].values else None
    
    print("Generating comparative analysis charts...")
    
    # 1. GRAFICO: Confronto tempi di esecuzione
    plt.figure(figsize=(12, 8))
    
    # Subplot 1: Tempi assoluti
    plt.subplot(2, 2, 1)
    
    if not sequential_df.empty:
        plt.plot(sequential_df['file_size_mb'], sequential_df['total_time_seconds'], 
                 'bo-', linewidth=3, markersize=8, label='Sequential', alpha=0.8)
    
    if not mpi_df.empty:
        # Prendi solo i test con più processi per chiarezza
        mpi_4procs = mpi_df[mpi_df['num_processes'] == 4]
        if not mpi_4procs.empty:
            plt.plot(mpi_4procs['file_size_mb'], mpi_4procs['total_time_seconds'], 
                     'ro-', linewidth=3, markersize=8, label='MPI (4 processes)', alpha=0.8)
    
    if cuda_df is not None and not cuda_df.empty:
        plt.plot(cuda_df['file_size_mb'], cuda_df['total_time_seconds'], 
                 'go-', linewidth=3, markersize=8, label='CUDA', alpha=0.8)
    
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Dimensione File (MB)')
    plt.ylabel('Tempo di Esecuzione (secondi)')
    plt.title('Confronto Prestazioni: Sequential vs MPI vs CUDA')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Subplot 2: Speedup MPI
    plt.subplot(2, 2, 2)
    
    if not sequential_df.empty and not mpi_df.empty:
        # Calcola speedup per ogni configurazione MPI
        speedup_data = []
        for num_procs in mpi_df['num_processes'].unique():
            mpi_proc_df = mpi_df[mpi_df['num_processes'] == num_procs]
            
            for size in mpi_proc_df['file_size_mb'].unique():
                mpi_time = mpi_proc_df[mpi_proc_df['file_size_mb'] == size]['total_time_seconds'].values
                seq_time = sequential_df[sequential_df['file_size_mb'] == size]['total_time_seconds'].values
                
                if len(mpi_time) > 0 and len(seq_time) > 0:
                    speedup = seq_time[0] / mpi_time[0]
                    speedup_data.append({
                        'processes': num_procs,
                        'size_mb': size,
                        'speedup': speedup
                    })
        
        if speedup_data:
            speedup_df = pd.DataFrame(speedup_data)
            
            for num_procs in speedup_df['processes'].unique():
                proc_data = speedup_df[speedup_df['processes'] == num_procs]
                plt.plot(proc_data['size_mb'], proc_data['speedup'], 
                         'o-', linewidth=2, markersize=6, 
                         label=f'MPI ({int(num_procs)} processes)')
            
            plt.axhline(y=1, color='black', linestyle='--', alpha=0.5, label='Baseline Sequential')
            plt.xscale('log')
            plt.xlabel('Dimensione File (MB)')
            plt.ylabel('Speedup')
            plt.title('Speedup MPI vs Sequential')
            plt.legend()
            plt.grid(True, alpha=0.3)
    
    # Subplot 3: Throughput comparativo
    plt.subplot(2, 2, 3)
    
    implementations = []
    if not sequential_df.empty:
        implementations.append(('Sequential', sequential_df, 'blue'))
    if not mpi_df.empty:
        implementations.append(('MPI (4 procs)', mpi_df[mpi_df['num_processes'] == 4], 'red'))
    if cuda_df is not None and not cuda_df.empty:
        implementations.append(('CUDA', cuda_df, 'green'))
    
    for label, data, color in implementations:
        if not data.empty:
            plt.plot(data['file_size_mb'], data['throughput_chars_per_second'], 
                     'o-', color=color, linewidth=2, markersize=6, label=label)
    
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Dimensione File (MB)')
    plt.ylabel('Throughput (caratteri/secondo)')
    plt.title('Throughput Comparativo')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Subplot 4: Efficienza parallela MPI
    plt.subplot(2, 2, 4)
    
    if not sequential_df.empty and not mpi_df.empty:
        efficiency_data = []
        for num_procs in mpi_df['num_processes'].unique():
            mpi_proc_df = mpi_df[mpi_df['num_processes'] == num_procs]
            
            for size in mpi_proc_df['file_size_mb'].unique():
                mpi_time = mpi_proc_df[mpi_proc_df['file_size_mb'] == size]['total_time_seconds'].values
                seq_time = sequential_df[sequential_df['file_size_mb'] == size]['total_time_seconds'].values
                
                if len(mpi_time) > 0 and len(seq_time) > 0:
                    speedup = seq_time[0] / mpi_time[0]
                    efficiency = speedup / num_procs
                    efficiency_data.append({
                        'processes': num_procs,
                        'size_mb': size,
                        'efficiency': efficiency
                    })
        
        if efficiency_data:
            efficiency_df = pd.DataFrame(efficiency_data)
            
            for num_procs in efficiency_df['processes'].unique():
                proc_data = efficiency_df[efficiency_df['processes'] == num_procs]
                plt.plot(proc_data['size_mb'], proc_data['efficiency'], 
                         's-', linewidth=2, markersize=6, 
                         label=f'{int(num_procs)} processes')
            
            plt.axhline(y=1.0, color='green', linestyle='--', alpha=0.7, label='Efficienza Ideale (100%)')
            plt.axhline(y=0.5, color='orange', linestyle='--', alpha=0.7, label='Soglia 50%')
            plt.xscale('log')
            plt.xlabel('Dimensione File (MB)')
            plt.ylabel('Efficienza Parallela')
            plt.title('Efficienza Parallela MPI')
            plt.legend()
            plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/comparative_analysis.png', dpi=300, bbox_inches='tight')
    plt.close()

def create_scaling_analysis(csv_file, output_dir):
    """Analisi strong/weak scaling MPI"""
    
    df = pd.read_csv(csv_file)
    mpi_df = df[df['implementation'] == 'mpi']
    
    if mpi_df.empty:
        print("No MPI data for scaling analysis")
        return
    
    plt.figure(figsize=(12, 5))
    
    # Strong scaling (problema fisso)
    plt.subplot(1, 2, 1)
    
    strong_scaling_data = []
    fixed_size = mpi_df['file_size_mb'].max()  # Prendi la dimensione più grande
    
    for num_procs in sorted(mpi_df['num_processes'].unique()):
        proc_data = mpi_df[(mpi_df['num_processes'] == num_procs) & 
                          (mpi_df['file_size_mb'] == fixed_size)]
        
        if not proc_data.empty:
            time = proc_data['total_time_seconds'].values[0]
            strong_scaling_data.append({
                'processes': num_procs,
                'time': time
            })
    
    if len(strong_scaling_data) > 1:
        strong_df = pd.DataFrame(strong_scaling_data)
        base_time = strong_df[strong_df['processes'] == 1]['time'].values[0]
        strong_df['speedup'] = base_time / strong_df['time']
        strong_df['efficiency'] = strong_df['speedup'] / strong_df['processes']
        
        plt.plot(strong_df['processes'], strong_df['speedup'], 'bo-', 
                 linewidth=2, markersize=8, label='Speedup Osservato')
        plt.plot(strong_df['processes'], strong_df['processes'], 'r--', 
                 linewidth=2, label='Speedup Ideale')
        
        plt.xlabel('Numero di Processi')
        plt.ylabel('Speedup')
        plt.title(f'Strong Scaling\n(File: {fixed_size}MB)')
        plt.legend()
        plt.grid(True, alpha=0.3)
    
    # Weak scaling (problema crescente)
    plt.subplot(1, 2, 2)
    
    weak_scaling_data = []
    for num_procs in sorted(mpi_df['num_processes'].unique()):
        # Per weak scaling, problema crescente con processi
        proc_data = mpi_df[mpi_df['num_processes'] == num_procs]
        if not proc_data.empty:
            # Prendi la dimensione più grande disponibile per questo numero di processi
            largest_size = proc_data['file_size_mb'].max()
            time = proc_data[proc_data['file_size_mb'] == largest_size]['total_time_seconds'].values[0]
            weak_scaling_data.append({
                'processes': num_procs,
                'size_mb': largest_size,
                'time': time
            })
    
    if len(weak_scaling_data) > 1:
        weak_df = pd.DataFrame(weak_scaling_data)
        base_time = weak_df[weak_df['processes'] == 1]['time'].values[0]
        weak_df['efficiency'] = base_time / weak_df['time']
        
        plt.plot(weak_df['processes'], weak_df['efficiency'], 'go-', 
                 linewidth=2, markersize=8, label='Efficienza Osservata')
        plt.axhline(y=1.0, color='r--', linewidth=2, label='Efficienza Ideale')
        
        plt.xlabel('Numero di Processi')
        plt.ylabel('Efficienza')
        plt.title('Weak Scaling Analysis')
        plt.legend()
        plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/scaling_analysis.png', dpi=300, bbox_inches='tight')
    plt.close()

def main():
    csv_file = "results/benchmarks/unified_benchmark_results.csv"
    output_dir = "results/charts"
    
    os.makedirs(output_dir, exist_ok=True)
    
    if not os.path.exists(csv_file):
        print(f"Error: File {csv_file} not found!")
        print("Please run unified benchmark first: make unified-benchmark")
        return
    
    print("HPC Suffix Array - Comparative Charts Generator")
    print("=" * 50)
    
    create_comparative_analysis(csv_file, output_dir)
    create_scaling_analysis(csv_file, output_dir)
    
    print("\nComparative charts generated successfully!")
    print(f"Charts location: {output_dir}/")
    print("   • comparative_analysis.png - Confronto completo")
    print("   • scaling_analysis.png - Analisi scaling MPI")

if __name__ == "__main__":
    main()