import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd

# DATI MANUALI SEQUENZIALI
seq_data = {
    'size_mb': [1.00, 50.00, 100.00, 200.00, 500.00],
    'sa_time': [0.512, 51.78, 1.8*60, 3.8*60, 10.5*60],
    'lcp_time': [0.540-0.512, 57.87-51.78, 2.0*60-1.8*60, 4.3*60-3.8*60, 11.8*60-10.5*60],
    'total_time': [0.540, 57.87, 2.0*60, 4.3*60, 11.8*60]
}
df_seq = pd.DataFrame(seq_data)

output_dir = "results/charts/sequential"
os.makedirs(output_dir, exist_ok=True)

# Grafico 1: Tempo Totale vs Dimensione (Scala Log) 
plt.figure(figsize=(10, 6))
plt.plot(df_seq['size_mb'], df_seq['total_time'], marker='o', linestyle='-', label='Sequenziale')
plt.xlabel('Dimensione Input (MB)')
plt.ylabel('Tempo Totale Esecuzione (secondi, scala log)')
plt.yscale('log')
plt.title('Baseline Sequenziale: Tempo Totale Esecuzione vs Dimensione Input')
plt.legend()
plt.grid(True, which="both", ls="--")
plt.xticks(df_seq['size_mb'])
plt.tight_layout()
plt.savefig(os.path.join(output_dir, "seq_total_time_vs_size_log.png"))
plt.close()
print(f"Grafico 'seq_total_time_vs_size_log.png' salvato in {output_dir}")


# Grafico 2: Breakdown Tempo Sequenziale (500MB)
df_seq_500 = df_seq[df_seq['size_mb'] == 500.0].iloc[0]
sa_perc = (df_seq_500['sa_time'] / df_seq_500['total_time']) * 100
lcp_perc = (df_seq_500['lcp_time'] / df_seq_500['total_time']) * 100
total_time_500 = df_seq_500['total_time']

fig, ax = plt.subplots(figsize=(6, 6)) 
bar_width = 0.5
labels = ['Sequenziale (500MB)']
bars1 = ax.bar(labels, [sa_perc], bar_width, label='Tempo Costruzione SA (fp)')
bars2 = ax.bar(labels, [lcp_perc], bar_width, bottom=[sa_perc], label='Tempo LCP + LRS (fs)')
ax.set_ylabel('Quota Percentuale del Tempo Totale (%)')
ax.set_title('Baseline Sequenziale: Breakdown Tempo (500MB)')
ax.legend(loc='lower right')
ax.set_ylim(0, 105) 

h1 = bars1[0].get_height()
h2 = bars2[0].get_height()
if h1 > 5: ax.text(bars1[0].get_x() + bars1[0].get_width() / 2., h1 / 2., f"{h1:.1f}%", ha='center', va='center', color='white', fontweight='bold')
if h2 > 5: ax.text(bars2[0].get_x() + bars2[0].get_width() / 2., h1 + h2 / 2., f"{h2:.1f}%", ha='center', va='center', color='white', fontweight='bold')

time_str = f"{total_time_500/60:.1f} min"
ax.text(0, 101, f"Tot: {time_str}", ha='center', va='bottom', color='black', fontsize=9)


plt.tight_layout() 
plt.subplots_adjust(top=0.92) 

plt.savefig(os.path.join(output_dir, "seq_time_breakdown_amdahl_500mb.png"))
plt.close()
print(f"Grafico 'seq_time_breakdown_amdahl_500mb.png' salvato in {output_dir}")