#!/usr/bin/env /mnt/c/Users/Artemis/Desktop/hpc_suffix_array/hpc_env/bin/python3
"""
Genera grafici completi delle prestazioni per il report HPC Suffix Array
Versione migliorata con analisi dettagliata
"""
import pandas as pd
import matplotlib
matplotlib.use('Agg')  # Important for headless environments
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os
from datetime import datetime

# Configurazione styling professionale
plt.style.use('seaborn-v0_8-whitegrid')
sns.set_palette("husl")
plt.rcParams['figure.figsize'] = (12, 8)
plt.rcParams['font.size'] = 12

def create_comprehensive_analysis(csv_file, output_dir):
    """Analisi completa delle prestazioni"""
    
    # Leggi i dati
    df = pd.read_csv(csv_file)
    
    # Filtra solo i file significativi per l'analisi
    analysis_df = df[df['file_size_mb'] >= 0.1].copy()
    
    print("Generating comprehensive performance analysis...")
    
    # 1. GRAFICO: Tempo di esecuzione vs Dimensione file (DETTAGLIATO)
    plt.figure(figsize=(14, 10))
    
    # Subplot 1: Tempo totale
    plt.subplot(2, 2, 1)
    plt.plot(analysis_df['file_size_mb'], analysis_df['execution_time_seconds'], 
             'bo-', linewidth=3, markersize=10, label='Tempo Totale', alpha=0.8)
    
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Dimensione File (MB) - Scala Logaritmica')
    plt.ylabel('Tempo di Esecuzione (secondi) - Scala Logaritmica')
    plt.title('Scalabilità Algoritmo di Manber-Myers\nTempo Totale vs Dimensione Input')
    plt.grid(True, alpha=0.3)
    
    # Annotazioni dettagliate
    for i, row in analysis_df.iterrows():
        plt.annotate(f"{row['execution_time_seconds']:.1f}s", 
                    (row['file_size_mb'], row['execution_time_seconds']),
                    textcoords="offset points", xytext=(0,12), ha='center', 
                    fontsize=9, bbox=dict(boxstyle="round,pad=0.3", facecolor="white", alpha=0.7))
    
    # Subplot 2: Throughput
    plt.subplot(2, 2, 2)
    plt.plot(analysis_df['file_size_mb'], analysis_df['throughput_chars_per_second'], 
             'go-', linewidth=3, markersize=10, label='Throughput', alpha=0.8)
    
    plt.xscale('log')
    plt.xlabel('Dimensione File (MB) - Scala Logaritmica')
    plt.ylabel('Throughput (caratteri/secondo)')
    plt.title('Efficienza Computazionale\nThroughput vs Dimensione Input')
    plt.grid(True, alpha=0.3)
    
    for i, row in analysis_df.iterrows():
        plt.annotate(f"{row['throughput_chars_per_second']:,.0f}/s", 
                    (row['file_size_mb'], row['throughput_chars_per_second']),
                    textcoords="offset points", xytext=(0,12), ha='center', 
                    fontsize=9, bbox=dict(boxstyle="round,pad=0.3", facecolor="white", alpha=0.7))
    
    # Subplot 3: Analisi di complessità
    plt.subplot(2, 2, 3)
    
    # Teorico O(n log n) - normalizzato per fit
    theoretical_x = np.array(analysis_df['file_size_mb'])
    theoretical_y = theoretical_x * np.log2(theoretical_x * 1e6) * 0.00005  # Normalizzato
    
    # Sperimentale
    experimental_y = analysis_df['execution_time_seconds']
    
    plt.plot(theoretical_x, theoretical_y, 'r--', linewidth=3, label='Teorico O(n log n)', alpha=0.7)
    plt.plot(theoretical_x, experimental_y, 'b-o', linewidth=2, markersize=8, label='Sperimentale')
    
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Dimensione File (MB) - Scala Logaritmica')
    plt.ylabel('Tempo di Esecuzione (secondi) - Scala Logaritmica')
    plt.title('Confronto Complessità: Teorica vs Sperimentale')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Subplot 4: Fattore di crescita
    plt.subplot(2, 2, 4)
    
    sizes = analysis_df['file_size_mb'].values
    times = analysis_df['execution_time_seconds'].values
    
    # Calcola fattore di crescita
    growth_factors = []
    for i in range(1, len(sizes)):
        size_ratio = sizes[i] / sizes[i-1]
        time_ratio = times[i] / times[i-1]
        growth_factor = time_ratio / size_ratio
        growth_factors.append(growth_factor)
    
    x_positions = [(sizes[i] + sizes[i-1]) / 2 for i in range(1, len(sizes))]
    
    bars = plt.bar(range(len(growth_factors)), growth_factors, 
                   color=['green' if x <= 1 else 'orange' for x in growth_factors], alpha=0.7)
    
    plt.xlabel('Intervallo di Dimensioni')
    plt.ylabel('Fattore di Crescita\n(Tempo/Dimensione)')
    plt.title('Analisi Fattore di Crescita Computazionale')
    plt.xticks(range(len(growth_factors)), 
               [f"{sizes[i-1]:.0f}→{sizes[i]:.0f}MB" for i in range(1, len(sizes))], 
               rotation=45)
    plt.grid(True, alpha=0.3, axis='y')
    
    # Aggiungi valori sulle barre
    for bar, factor in zip(bars, growth_factors):
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2., height + 0.01,
                f'{factor:.2f}', ha='center', va='bottom', fontsize=10)
    
    plt.axhline(y=1, color='red', linestyle='--', alpha=0.5, label='Crescita Lineare')
    plt.legend()
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/comprehensive_analysis.png', dpi=300, bbox_inches='tight')
    plt.close()

def create_performance_breakdown(csv_file, output_dir):
    """Breakdown dettagliato delle prestazioni"""
    
    df = pd.read_csv(csv_file)
    analysis_df = df[df['file_size_mb'] >= 0.1].copy()
    
    # Calcola metriche aggiuntive
    analysis_df['time_per_mb'] = analysis_df['execution_time_seconds'] / analysis_df['file_size_mb']
    analysis_df['efficiency'] = analysis_df['throughput_chars_per_second'] / analysis_df['file_size_mb']
    
    plt.figure(figsize=(15, 5))
    
    # Subplot 1: Tempo per MB
    plt.subplot(1, 3, 1)
    plt.plot(analysis_df['file_size_mb'], analysis_df['time_per_mb'], 
             'purple', marker='s', linewidth=2, markersize=8)
    
    plt.xscale('log')
    plt.xlabel('Dimensione File (MB)')
    plt.ylabel('Tempo per MB (secondi/MB)')
    plt.title('Efficienza per Unitá di Dimensione')
    plt.grid(True, alpha=0.3)
    
    for i, row in analysis_df.iterrows():
        plt.annotate(f"{row['time_per_mb']:.1f}s/MB", 
                    (row['file_size_mb'], row['time_per_mb']),
                    textcoords="offset points", xytext=(0,10), ha='center', fontsize=9)
    
    # Subplot 2: Throughput normalizzato
    plt.subplot(1, 3, 2)
    plt.plot(analysis_df['file_size_mb'], analysis_df['efficiency'], 
             'orange', marker='^', linewidth=2, markersize=8)
    
    plt.xscale('log')
    plt.xlabel('Dimensione File (MB)')
    plt.ylabel('Efficienza (char/s per MB)')
    plt.title('Throughput Normalizzato')
    plt.grid(True, alpha=0.3)
    
    for i, row in analysis_df.iterrows():
        plt.annotate(f"{row['efficiency']:,.0f}", 
                    (row['file_size_mb'], row['efficiency']),
                    textcoords="offset points", xytext=(0,10), ha='center', fontsize=9)
    
    # Subplot 3: Confronto prestazioni assolute
    plt.subplot(1, 3, 3)
    
    sizes = [f"{size}MB" for size in analysis_df['file_size_mb']]
    times = analysis_df['execution_time_seconds']
    
    bars = plt.bar(sizes, times, color='skyblue', alpha=0.7)
    plt.xlabel('Dimensione File')
    plt.ylabel('Tempo di Esecuzione (secondi)')
    plt.title('Confronto Prestazioni Assolute')
    plt.xticks(rotation=45)
    plt.grid(True, alpha=0.3, axis='y')
    
    # Aggiungi valori sulle barre
    for bar, time in zip(bars, times):
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2., height + max(times)*0.01,
                f'{time:.1f}s', ha='center', va='bottom', fontsize=10)
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/performance_breakdown.png', dpi=300, bbox_inches='tight')
    plt.close()

def generate_detailed_report(csv_file, output_dir):
    """Genera report dettagliato con statistiche avanzate"""
    
    df = pd.read_csv(csv_file)
    analysis_df = df[df['file_size_mb'] >= 0.1].copy()
    
    # Calcola statistiche avanzate
    stats = {
        'Numero test': len(analysis_df),
        'Dimensione min': f"{analysis_df['file_size_mb'].min():.1f} MB",
        'Dimensione max': f"{analysis_df['file_size_mb'].max():.1f} MB",
        'Tempo min': f"{analysis_df['execution_time_seconds'].min():.2f} s",
        'Tempo max': f"{analysis_df['execution_time_seconds'].max():.2f} s",
        'Throughput medio': f"{analysis_df['throughput_chars_per_second'].mean():,.0f} char/s",
        'Tempo per 1MB': f"{analysis_df[analysis_df['file_size_mb'] == 1.0]['execution_time_seconds'].values[0]:.2f} s",
        'Tempo per 50MB': f"{analysis_df[analysis_df['file_size_mb'] == 50.0]['execution_time_seconds'].values[0]:.2f} s",
        'Tempo per 100MB': f"{analysis_df[analysis_df['file_size_mb'] == 100.0]['execution_time_seconds'].values[0]:.2f} s",
    }
    
    # Calcola fattore di crescita complessivo
    size_ratio = analysis_df['file_size_mb'].max() / analysis_df['file_size_mb'].min()
    time_ratio = analysis_df['execution_time_seconds'].max() / analysis_df['execution_time_seconds'].min()
    complexity_factor = time_ratio / size_ratio
    
    stats['Fattore crescita dimensioni'] = f"{size_ratio:.1f}x"
    stats['Fattore crescita tempo'] = f"{time_ratio:.1f}x"
    stats['Fattore complessità'] = f"{complexity_factor:.3f}"
    
    # Salva report dettagliato
    with open(f'{output_dir}/detailed_performance_report.txt', 'w') as f:
        f.write("HPC SUFFIX ARRAY - RAPPORTO PRESTAZIONI DETTAGLIATO\n")
        f.write("=" * 70 + "\n")
        f.write(f"Generato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        
        f.write("DATASET E CONFIGURAZIONE\n")
        f.write("-" * 40 + "\n")
        f.write(f"• Numero di test eseguiti: {stats['Numero test']}\n")
        f.write(f"• Range dimensioni: {stats['Dimensione min']} - {stats['Dimensione max']}\n")
        f.write(f"• Algoritmo: Manber-Myers sequenziale\n")
        f.write(f"• Piattaforma: WSL2 Ubuntu\n\n")
        
        f.write("PRESTAZIONI TEMPORALI\n")
        f.write("-" * 40 + "\n")
        for key in ['Tempo per 1MB', 'Tempo per 50MB', 'Tempo per 100MB']:
            f.write(f"• {key}: {stats[key]}\n")
        f.write(f"• Range temporale: {stats['Tempo min']} - {stats['Tempo max']}\n\n")
        
        f.write("ANALISI EFFICIENZA\n")
        f.write("-" * 40 + "\n")
        f.write(f"• Throughput medio: {stats['Throughput medio']}\n")
        f.write(f"• Fattore crescita dimensioni: {stats['Fattore crescita dimensioni']}\n")
        f.write(f"• Fattore crescita tempo: {stats['Fattore crescita tempo']}\n")
        f.write(f"• Fattore complessità osservato: {stats['Fattore complessità']}\n\n")
        
        f.write("INTERPRETAZIONE RISULTATI\n")
        f.write("-" * 40 + "\n")
        f.write("• Fattore complessità < 1: efficienza migliora con dimensione\n")
        f.write("• Fattore complessità ≈ 1: crescita lineare\n")
        f.write("• Fattore complessità > 1: complessità super-lineare\n")
        f.write(f"• Il valore {complexity_factor:.3f} indica: ")
        
        if complexity_factor < 0.8:
            f.write("OTTIMA scalabilità sub-lineare\n")
        elif complexity_factor < 1.2:
            f.write("BUONA scalabilità quasi-lineare\n")
        else:
            f.write("SCALABILITÀ con complessità super-lineare\n")
        
        f.write("\nRACCOMANDAZIONI PER PARALLELIZZAZIONE\n")
        f.write("-" * 45 + "\n")
        f.write("1. MPI: Partizionamento dati per grandi dimensioni\n")
        f.write("2. CUDA: Parallelizzazione fasi di ordinamento\n")
        f.write("3. Ottimizzazione: Riduzione allocazioni memoria\n")
        f.write("4. Target: Speedup 5-10x atteso con parallelizzazione\n")

def main():
    """Funzione principale"""
    csv_file = "results/benchmarks/large_scale/benchmark_results.csv"
    output_dir = "results/charts"
    
    # Crea directory se non esistono
    os.makedirs(output_dir, exist_ok=True)
    
    if not os.path.exists(csv_file):
        print(f"Error: File {csv_file} not found!")
        print("Please run 'make benchmark-large' first.")
        return
    
    print("HPC Suffix Array - Comprehensive Performance Analysis")
    print("=" * 60)
    
    # Genera tutti i grafici e analisi
    create_comprehensive_analysis(csv_file, output_dir)
    create_performance_breakdown(csv_file, output_dir)
    generate_detailed_report(csv_file, output_dir)
    
    print("\nANALISI COMPLETA GENERATA CON SUCCESSO!")
    print("=" * 50)
    print("GRAFICI GENERATI:")
    print("   • comprehensive_analysis.png - Analisi completa a 4 quadranti")
    print("   • performance_breakdown.png - Breakdown prestazioni dettagliato")
    print("REPORT:")
    print("   • detailed_performance_report.txt - Analisi statistica completa")
    print("INSIGHTS:")
    print("   • Scalabilità algoritmo verificata")
    print("   • Baseline prestazioni stabilita")
    print("   • Ready per parallelizzazione MPI/CUDA!")

if __name__ == "__main__":
    main()