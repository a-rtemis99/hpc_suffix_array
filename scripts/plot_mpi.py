import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd

# DATI MANUALI BENCHMARK
# Tempo sequenziale per 200MB (SOLO sa_time in secondi)
seq_time_200mb = 3.8 * 60

# Tempi MPI per 200MB (SOLO sa_time in secondi)
mpi_scaling_data = {
    'processes': [2, 4, 8],
    'sa_time': [2.2*60, 2.2*60, 3.4*60] # Tempi per 2, 4, 8 processi su 200MB
}
df_mpi_scale = pd.DataFrame(mpi_scaling_data)

# Calcolo Speedup e Efficienza
df_mpi_scale['speedup'] = seq_time_200mb / df_mpi_scale['sa_time']
df_mpi_scale['efficiency'] = df_mpi_scale['speedup'] / df_mpi_scale['processes']

output_dir = "results/charts/mpi"
os.makedirs(output_dir, exist_ok=True)

# Grafici di Scalabilità MPI (per 200MB) 
fig, axes = plt.subplots(3, 1, figsize=(8, 15)) 
fig.suptitle('Scalabilità Implementazione MPI (Input: 200MB)', fontsize=16, y=0.99) 

# Grafico 1: Tempo SA vs Processi (asse 0)
axes[0].plot(df_mpi_scale['processes'], df_mpi_scale['sa_time'], marker='o', linestyle='-')
axes[0].set_xlabel('Numero di Processi MPI')
axes[0].set_ylabel('Tempo Costruzione SA (secondi)')
axes[0].set_title('Tempo vs Processi')
axes[0].grid(True, ls="--")
axes[0].set_xticks(df_mpi_scale['processes'])

# Grafico 2: Speedup vs Processi (asse 1)
axes[1].plot(df_mpi_scale['processes'], df_mpi_scale['speedup'], marker='s', linestyle='--')
axes[1].plot(df_mpi_scale['processes'], df_mpi_scale['processes'], marker='', linestyle=':', color='gray', label='Speedup Ideale') # Linea ideale
axes[1].set_xlabel('Numero di Processi MPI')
axes[1].set_ylabel('Speedup (T_seq / T_mpi)')
axes[1].set_title('Speedup vs Processi')
axes[1].legend()
axes[1].grid(True, ls="--")
axes[1].set_xticks(df_mpi_scale['processes'])
axes[1].set_ylim(bottom=0)

# Grafico 3: Efficienza vs Processi (asse 2)
axes[2].plot(df_mpi_scale['processes'], df_mpi_scale['efficiency'], marker='^', linestyle='-.')
axes[2].axhline(1, color='gray', linestyle=':', label='Efficienza Ideale (100%)') # Linea ideale
axes[2].set_xlabel('Numero di Processi MPI')
axes[2].set_ylabel('Efficienza Parallela (Speedup / Processi)')
axes[2].set_title('Efficienza vs Processi')
axes[2].legend()
axes[2].grid(True, ls="--")
axes[2].set_xticks(df_mpi_scale['processes'])
axes[2].set_ylim(0, 1.1)
axes[2].yaxis.set_major_formatter(plt.FuncFormatter(lambda y, _: '{:.0%}'.format(y)))

plt.tight_layout(rect=[0, 0.03, 1, 0.97])
plt.subplots_adjust(hspace=0.35)

plt.savefig(os.path.join(output_dir, "mpi_scalability_200mb_vertical.png")) 
plt.close()

print(f"Grafico 'mpi_scalability_200mb_vertical.png' salvato in {output_dir}")