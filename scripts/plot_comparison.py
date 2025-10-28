import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd

# DATI MANUALI BENCHMARKS
seq_data = {
    'size_mb': [1.00, 50.00, 100.00, 200.00, 500.00],
    'sa_time': [0.512, 51.78, 1.8*60, 3.8*60, 10.5*60],
    'total_time': [0.540, 57.87, 2.0*60, 4.3*60, 11.8*60]
}

mpi_data = { # Solo 2 processi
    'size_mb': [1.00, 50.00, 100.00, 200.00, 500.00], 
    'sa_time': [0.535, 30.31, 1.0*60, 2.2*60, 5.6*60], 
    'total_time': [0.566, 36.44, 1.3*60, 2.6*60, 7.1*60] 
}

cuda_data = {
    'size_mb': [1.00, 50.00, 100.00, 200.00, 500.00],
    'sa_time': [0.528, 13.09, 27.32, 55.96, 2.4*60],
    'total_time': [0.558, 20.64, 43.33, 1.5*60, 4.1*60]
}
df_seq = pd.DataFrame(seq_data)
df_mpi = pd.DataFrame(mpi_data) 
df_cuda = pd.DataFrame(cuda_data)

output_dir = "results/charts/comparison"
os.makedirs(output_dir, exist_ok=True)

# Grafico 1: Tempo SA vs Dimensione (Comparativo)
plt.figure(figsize=(10, 6))
plt.plot(df_seq['size_mb'], df_seq['sa_time'], marker='o', linestyle='-', label='Sequenziale')
plt.plot(df_mpi['size_mb'], df_mpi['sa_time'], marker='s', linestyle='--', label='MPI (2 processi)') 
plt.plot(df_cuda['size_mb'], df_cuda['sa_time'], marker='^', linestyle=':', label='CUDA')
plt.xlabel('Dimensione Input (MB)')
plt.ylabel('Tempo Costruzione SA (secondi, scala log)')
plt.yscale('log')
plt.title('Comparativo: Tempo Costruzione SA vs Dimensione Input')
plt.legend()
plt.grid(True, which="both", ls="--")
plt.xticks(df_seq['size_mb'])
plt.tight_layout()
plt.savefig(os.path.join(output_dir, "comp_sa_time_vs_size_log.png"))
plt.close()
print(f"Grafico 'comp_sa_time_vs_size_log.png' salvato in {output_dir}")

# Grafico 2: Tempo Totale vs Dimensione (Comparativo)
plt.figure(figsize=(10, 6))
plt.plot(df_seq['size_mb'], df_seq['total_time'], marker='o', linestyle='-', label='Sequenziale')
plt.plot(df_mpi['size_mb'], df_mpi['total_time'], marker='s', linestyle='--', label='MPI (2 processi)') 
plt.plot(df_cuda['size_mb'], df_cuda['total_time'], marker='^', linestyle=':', label='CUDA')
plt.xlabel('Dimensione Input (MB)')
plt.ylabel('Tempo Totale Esecuzione (secondi, scala log)')
plt.yscale('log')
plt.title('Comparativo: Tempo Totale Esecuzione vs Dimensione Input')
plt.legend()
plt.grid(True, which="both", ls="--")
plt.xticks(df_seq['size_mb'])
plt.tight_layout()
plt.savefig(os.path.join(output_dir, "comp_total_time_vs_size_log.png"))
plt.close()
print(f"Grafico 'comp_total_time_vs_size_log.png' salvato in {output_dir}")


# Grafico 3: Speedup vs Dimensione (Comparativo)
df_seq_idx = df_seq.set_index('size_mb')
df_mpi_idx = df_mpi.set_index('size_mb') 
df_cuda_idx = df_cuda.set_index('size_mb')
df_mpi_idx['speedup'] = df_seq_idx['sa_time'].div(df_mpi_idx['sa_time'])
df_cuda_idx['speedup'] = df_seq_idx['sa_time'].div(df_cuda_idx['sa_time'])

plt.figure(figsize=(10, 6))
plt.plot(df_mpi_idx.index, df_mpi_idx['speedup'], marker='s', linestyle='--', label='MPI (2 processi)') 
plt.plot(df_cuda_idx.index, df_cuda_idx['speedup'], marker='^', linestyle=':', label='CUDA')
plt.axhline(1, color='gray', linestyle='-', linewidth=0.8, label='Baseline Sequenziale (Speedup=1)')
plt.xlabel('Dimensione Input (MB)')
plt.ylabel('Speedup (T_seq / T_par) [basato su Tempo SA]')
plt.title('Comparativo: Speedup Costruzione SA vs Dimensione Input')
plt.legend()
plt.grid(True, which="both", ls="--")
plt.xticks(df_seq['size_mb']) 
plt.ylim(bottom=0)
plt.tight_layout()
plt.savefig(os.path.join(output_dir, "comp_speedup_vs_size.png"))
plt.close()
print(f"Grafico 'comp_speedup_vs_size.png' salvato in {output_dir}")

# Grafico 4: Breakdown Tempo Comparativo (per file più grande)
breakdown_data_comp = {
    'Implementation': ['Sequenziale (500MB)', 'MPI (2 proc, 500MB)', 'CUDA (500MB)'], 
    'sa_time': [10.5*60, 5.6*60, 2.4*60], 
    'lcp_time': [1.3*60, (7.1-5.6)*60, (4.1-2.4)*60], 
}
df_breakdown_comp = pd.DataFrame(breakdown_data_comp)
df_breakdown_comp['total_time'] = df_breakdown_comp['sa_time'] + df_breakdown_comp['lcp_time']
df_breakdown_comp['sa_perc'] = (df_breakdown_comp['sa_time'] / df_breakdown_comp['total_time']) * 100
df_breakdown_comp['lcp_perc'] = (df_breakdown_comp['lcp_time'] / df_breakdown_comp['total_time']) * 100

fig, ax = plt.subplots(figsize=(10, 6))
bar_width = 0.5
bars1 = ax.bar(df_breakdown_comp['Implementation'], df_breakdown_comp['sa_perc'], bar_width, label='Tempo Costruzione SA (fp)')
bars2 = ax.bar(df_breakdown_comp['Implementation'], df_breakdown_comp['lcp_perc'], bar_width, bottom=df_breakdown_comp['sa_perc'], label='Tempo LCP + LRS (fs)')
ax.set_ylabel('Quota Percentuale del Tempo Totale (%)')
ax.set_title('Comparativo: Breakdown del Tempo di Esecuzione (Legge di Amdahl)')
ax.legend(loc='lower right')
ax.set_ylim(0, 105)

for r1, r2 in zip(bars1, bars2):
    h1 = r1.get_height()
    h2 = r2.get_height()
    if h1 > 5: ax.text(r1.get_x() + r1.get_width() / 2., h1 / 2., f"{h1:.1f}%", ha='center', va='center', color='white', fontweight='bold')
    if h2 > 5: ax.text(r2.get_x() + r2.get_width() / 2., h1 + h2 / 2., f"{h2:.1f}%", ha='center', va='center', color='white', fontweight='bold')

for i, total in enumerate(df_breakdown_comp['total_time']):
    time_str = f"{total/60:.1f} min" if total > 120 else f"{total:.1f} sec"
    ax.text(i, 101, f"Tot: {time_str}", ha='center', va='bottom', color='black', fontsize=9)

plt.tight_layout()
plt.subplots_adjust(top=0.92)

plt.savefig(os.path.join(output_dir, "comp_time_breakdown_amdahl.png"))
plt.close()
print(f"Grafico 'comp_time_breakdown_amdahl.png' salvato in {output_dir}")