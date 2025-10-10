#!/usr/bin/env python3
# scripts/generate_charts.py

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os

def plot_execution_time(csv_file):
    """Genera grafico tempi di esecuzione"""
    df = pd.read_csv(csv_file)
    
    # Calcola medie solo per le colonne numeriche
    numeric_cols = ['string_length', 'total_time', 'sa_time', 'lcp_time', 'lrs_time', 'memory_used']
    df_numeric = df[numeric_cols]
    df_avg = df_numeric.groupby('string_length').mean().reset_index()
    
    plt.figure(figsize=(12, 8))
    
    # Grafico 1: Tempi totali (scala log-log)
    plt.subplot(2, 2, 1)
    plt.plot(df_avg['string_length'], df_avg['total_time'], 
             'bo-', linewidth=2, markersize=8, label='Tempo totale')
    plt.plot(df_avg['string_length'], df_avg['sa_time'], 
             'ro-', linewidth=2, markersize=6, label='Costruzione SA')
    plt.plot(df_avg['string_length'], df_avg['lcp_time'], 
             'go-', linewidth=2, markersize=6, label='Costruzione LCP')
    plt.plot(df_avg['string_length'], df_avg['lrs_time'], 
             'mo-', linewidth=2, markersize=6, label='Ricerca LRS')
    
    plt.xlabel('Lunghezza Stringa')
    plt.ylabel('Tempo (secondi)')
    plt.title('Tempo di Esecuzione vs Dimensione Input\n(Sequenziale)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    plt.yscale('log')
    
    # Grafico 2: Breakdown percentuale
    plt.subplot(2, 2, 2)
    time_components = ['sa_time', 'lcp_time', 'lrs_time']
    component_labels = ['Costruzione SA', 'Costruzione LCP', 'Ricerca LRS']
    colors = ['red', 'green', 'purple']
    
    percentages = []
    for component in time_components:
        percent = (df_avg[component] / df_avg['total_time'] * 100).mean()
        percentages.append(percent)
    
    plt.pie(percentages, labels=component_labels, colors=colors, autopct='%1.1f%%', startangle=90)
    plt.title('Distribuzione Tempo per Fase\n(Media su tutte le dimensioni)')
    
    # Grafico 3: Utilizzo memoria
    plt.subplot(2, 2, 3)
    plt.plot(df_avg['string_length'], df_avg['memory_used'] / 1024 / 1024, 
             'co-', linewidth=2, markersize=8)
    plt.xlabel('Lunghezza Stringa')
    plt.ylabel('Memoria (MB)')
    plt.title('Utilizzo Memoria vs Dimensione Input')
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    plt.yscale('log')
    
    # Grafico 4: Throughput
    plt.subplot(2, 2, 4)
    df_avg['throughput'] = df_avg['string_length'] / df_avg['total_time']
    plt.plot(df_avg['string_length'], df_avg['throughput'], 
             'yo-', linewidth=2, markersize=8)
    plt.xlabel('Lunghezza Stringa')
    plt.ylabel('Throughput (caratteri/secondo)')
    plt.title('Throughput Computazionale')
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    plt.yscale('log')
    
    plt.tight_layout()
    plt.savefig('results/charts/execution_analysis.png', dpi=300, bbox_inches='tight')
    plt.close()
    print("✅ Grafico execution_analysis.png generato")

def plot_scaling_analysis(csv_file):
    """Analisi di scaling e prestazioni"""
    df = pd.read_csv(csv_file)
    
    # Calcola medie solo per le colonne numeriche
    numeric_cols = ['string_length', 'total_time', 'sa_time', 'lcp_time', 'lrs_time', 'memory_used']
    df_numeric = df[numeric_cols]
    df_avg = df_numeric.groupby('string_length').mean().reset_index()
    
    plt.figure(figsize=(10, 6))
    
    # Calcola operazioni per secondo
    df_avg['ops_per_second'] = df_avg['string_length'] / df_avg['total_time']
    
    plt.plot(df_avg['string_length'], df_avg['ops_per_second'], 
             'bo-', linewidth=2, markersize=8, label='Sequenziale')
    
    plt.xlabel('Lunghezza Stringa')
    plt.ylabel('Operazioni al Secondo')
    plt.title('Scalabilità - Throughput Computazionale')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    plt.yscale('log')
    
    plt.tight_layout()
    plt.savefig('results/charts/scaling_analysis.png', dpi=300, bbox_inches='tight')
    plt.close()
    print("✅ Grafico scaling_analysis.png generato")

def plot_detailed_breakdown(csv_file):
    """Breakdown dettagliato per dimensione"""
    df = pd.read_csv(csv_file)
    
    # Calcola medie solo per le colonne numeriche
    numeric_cols = ['string_length', 'total_time', 'sa_time', 'lcp_time', 'lrs_time']
    df_numeric = df[numeric_cols]
    df_avg = df_numeric.groupby('string_length').mean().reset_index()
    
    # Prepara dati per stacked bar chart
    sizes = df_avg['string_length']
    sa_times = df_avg['sa_time']
    lcp_times = df_avg['lcp_time'] 
    lrs_times = df_avg['lrs_time']
    
    plt.figure(figsize=(12, 6))
    
    bar_width = 0.6
    indices = np.arange(len(sizes))
    
    plt.bar(indices, sa_times, bar_width, label='Costruzione SA', color='red', alpha=0.8)
    plt.bar(indices, lcp_times, bar_width, bottom=sa_times, label='Costruzione LCP', color='green', alpha=0.8)
    plt.bar(indices, lrs_times, bar_width, bottom=sa_times + lcp_times, label='Ricerca LRS', color='purple', alpha=0.8)
    
    plt.xlabel('Dimensione Stringa')
    plt.ylabel('Tempo (secondi)')
    plt.title('Breakdown Dettagliato del Tempo di Esecuzione')
    plt.xticks(indices, [f'{size//1000}K' if size<1000000 else f'{size//1000000}M' 
                        for size in sizes], rotation=45)
    plt.legend()
    plt.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    
    plt.savefig('results/charts/detailed_breakdown.png', dpi=300, bbox_inches='tight')
    plt.close()
    print("✅ Grafico detailed_breakdown.png generato")

def generate_comprehensive_report(csv_file):
    """Genera report completo con statistiche"""
    df = pd.read_csv(csv_file)
    
    # Calcola medie solo per le colonne numeriche
    numeric_cols = ['string_length', 'total_time', 'sa_time', 'lcp_time', 'lrs_time', 'memory_used']
    df_numeric = df[numeric_cols]
    df_avg = df_numeric.groupby('string_length').mean().reset_index()
    
    # Informazioni sulle stringhe
    implementation = df['implementation'].iloc[0] if 'implementation' in df.columns else 'sequential'
    input_type = df['input_type'].iloc[0] if 'input_type' in df.columns else 'random'
    
    print("=" * 60)
    print("           RAPPORTO BENCHMARK COMPLETO")
    print("=" * 60)
    
    print(f"\nDATASET:")
    print(f"  • Implementazione: {implementation}")
    print(f"  • Tipo input: {input_type}")
    print(f"  • Numero totale di esecuzioni: {len(df)}")
    print(f"  • Dimensioni testate: {len(df_avg)}")
    print(f"  • Range dimensioni: {df_avg['string_length'].min():,} - {df_avg['string_length'].max():,} caratteri")
    
    print(f"\nPRESTAZIONI TEMPORALI:")
    total_time_1k = df_avg[df_avg['string_length'] == 1000]['total_time'].iloc[0]
    total_time_1m = df_avg[df_avg['string_length'] == 1000000]['total_time'].iloc[0]
    print(f"  • Tempo 1K caratteri: {total_time_1k:.6f}s")
    print(f"  • Tempo 1M caratteri: {total_time_1m:.3f}s")
    print(f"  • Fattore di crescita: {total_time_1m/total_time_1k:.1f}x")
    
    print(f"\nBREAKDOWN COMPUTAZIONALE (media):")
    avg_sa_percent = (df_avg['sa_time'] / df_avg['total_time'] * 100).mean()
    avg_lcp_percent = (df_avg['lcp_time'] / df_avg['total_time'] * 100).mean()
    avg_lrs_percent = (df_avg['lrs_time'] / df_avg['total_time'] * 100).mean()
    print(f"  • Costruzione SA: {avg_sa_percent:.1f}%")
    print(f"  • Costruzione LCP: {avg_lcp_percent:.1f}%")
    print(f"  • Ricerca LRS: {avg_lrs_percent:.1f}%")
    
    print(f"\nUTILIZZO MEMORIA:")
    max_memory = df_avg['memory_used'].max() / 1024 / 1024
    memory_per_char = (df_avg['memory_used'] / df_avg['string_length']).mean() / 1024
    print(f"  • Memoria massima: {max_memory:.1f} MB")
    print(f"  • Memoria per carattere: {memory_per_char:.1f} KB")
    
    print(f"\nTHROUGHPUT:")
    df_avg['throughput'] = df_avg['string_length'] / df_avg['total_time']
    min_throughput = df_avg['throughput'].min()
    max_throughput = df_avg['throughput'].max()
    avg_throughput = df_avg['throughput'].mean()
    print(f"  • Throughput minimo: {min_throughput:,.0f} char/s")
    print(f"  • Throughput massimo: {max_throughput:,.0f} char/s")
    print(f"  • Throughput medio: {avg_throughput:,.0f} char/s")
    
    print(f"\nANALISI SCALABILITÀ:")
    size_ratio = df_avg['string_length'].max() / df_avg['string_length'].min()
    time_ratio = df_avg['total_time'].max() / df_avg['total_time'].min()
    scalability = size_ratio / time_ratio
    print(f"  • Rapporto dimensioni: {size_ratio:.1f}x")
    print(f"  • Rapporto tempo: {time_ratio:.1f}x")
    print(f"  • Fattore scalabilità: {scalability:.3f}")
    
    # Identifica outlier
    outlier_threshold = df_avg['total_time'].median() * 2
    outliers = df_avg[df_avg['total_time'] > outlier_threshold]
    if len(outliers) > 0:
        print(f"\n⚠️  OUTLIER RILEVATI:")
        for _, outlier in outliers.iterrows():
            print(f"  • {outlier['string_length']:,} char: {outlier['total_time']:.3f}s")
    
    print("=" * 60)

def main():
    """Funzione principale"""
    # Crea directory se non esistono
    os.makedirs('results/charts', exist_ok=True)
    os.makedirs('results/csv', exist_ok=True)
    
    csv_file = 'results/csv/benchmark_results_sequential.csv'
    
    if not os.path.exists(csv_file):
        print(f"ERRORE: File {csv_file} non trovato!")
        print("Esegui prima: make run-benchmark")
        return
    
    print("Generazione grafici e analisi in corso...")
    
    # Genera tutti i grafici
    plot_execution_time(csv_file)
    plot_scaling_analysis(csv_file)
    plot_detailed_breakdown(csv_file)
    
    # Genera report
    generate_comprehensive_report(csv_file)
    
    print(f"\n✅ Tutti i grafici sono stati generati in 'results/charts/'")
    print("📊 I grafici includono:")
    print("   • execution_analysis.png - Panoramica completa")
    print("   • scaling_analysis.png - Analisi scalabilità") 
    print("   • detailed_breakdown.png - Breakdown dettagliato")

if __name__ == "__main__":
    main()