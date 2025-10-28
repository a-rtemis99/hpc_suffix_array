import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd

# DATI MANUALI CUDA (500MB)
cuda_data_500mb = {
    'sa_time': 2.4*60,
    'lcp_time': (4.1-2.4)*60,
    'total_time': 4.1*60
}

output_dir = "results/charts/cuda"
os.makedirs(output_dir, exist_ok=True)

# Grafico Breakdown Tempo CUDA (500MB) 
sa_perc = (cuda_data_500mb['sa_time'] / cuda_data_500mb['total_time']) * 100
lcp_perc = (cuda_data_500mb['lcp_time'] / cuda_data_500mb['total_time']) * 100
total_time_500 = cuda_data_500mb['total_time']

fig, ax = plt.subplots(figsize=(6, 6))
bar_width = 0.5
labels = ['CUDA (500MB)']
bars1 = ax.bar(labels, [sa_perc], bar_width, label='Tempo Costruzione SA (fp)')
bars2 = ax.bar(labels, [lcp_perc], bar_width, bottom=[sa_perc], label='Tempo LCP + LRS (fs)')
ax.set_ylabel('Quota Percentuale del Tempo Totale (%)')
ax.set_title('CUDA: Breakdown Tempo (500MB)')
ax.legend(loc='lower right')
ax.set_ylim(0, 100) 

h1 = bars1[0].get_height()
h2 = bars2[0].get_height()
if h1 > 5:
    ax.text(bars1[0].get_x() + bars1[0].get_width() / 2., h1 / 2.,
            f"{h1:.1f}%", ha='center', va='center', color='white', fontweight='bold')
if h2 > 5:
    ax.text(bars2[0].get_x() + bars2[0].get_width() / 2., h1 + (h2 * 0.4), 
            f"{h2:.1f}%", ha='center', va='center', color='white', fontweight='bold')

ax.set_ylim(0, 105)
time_str = f"{total_time_500/60:.1f} min"
ax.text(0, 101, f"Tot: {time_str}", ha='center', va='bottom', color='black', fontsize=9)


plt.tight_layout()
plt.subplots_adjust(top=0.92) 

plt.savefig(os.path.join(output_dir, "cuda_time_breakdown_amdahl_500mb.png"))
plt.close()
print(f"Grafico 'cuda_time_breakdown_amdahl_500mb.png' salvato in {output_dir}")