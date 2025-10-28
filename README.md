# Costruzione Parallela di Suffix Array tramite Algoritmo Manber-Myers (MPI & CUDA)

## Panoramica del Progetto

Questo progetto implementa e analizza versioni parallele dell'algoritmo di Manber-Myers per la costruzione di Suffix Array, finalizzate a risolvere efficientemente il problema della Sottostringa Ripetuta Più Lunga (Longest Repeated Substring - LRS) per stringhe di input di grandi dimensioni. L'obiettivo primario è accelerare la fase computazionalmente intensiva della costruzione del Suffix Array utilizzando due distinti paradigmi dell'High Performance Computing (HPC): Message Passing Interface (MPI) per il parallelismo a memoria distribuita e CUDA per il parallelismo massivo su GPU NVIDIA.

Il progetto include:
1.  Un' **implementazione sequenziale di baseline** basata sulla strategia "doubling" di Manber-Myers, che utilizza `qsort` per l'ordinamento, risultando in una complessità O(n log^2 n).
2.  Un' **implementazione parallela MPI** che impiega una strategia Master-Worker ottimizzata con un algoritmo K-Way Merge sul processo master per ridurre il collo di bottiglia seriale da O(n log n) a O(n log P).
3.  Un' **implementazione parallela CUDA** che utilizza un approccio "quasi GPU-Resident", sfruttando la libreria Thrust (`thrust::stable_sort`) per l'ordinamento accelerato su GPU e gestendo parzialmente il flusso dati e il calcolo dei rank sull'host.
4.  Una suite di script per la **generazione dei dati**, il **benchmarking** e l'**analisi delle prestazioni**.

Le performance di queste implementazioni sono valutate su dataset che variano da 1MB a 500MB, confrontando i tempi di esecuzione, lo speedup e identificando i colli di bottiglia computazionali.

## Dettagli dell'Algoritmo

### Algoritmo Sequenziale di Baseline 

L'implementazione sequenziale di riferimento (`src/sequential/manber_myers.c`) segue direttamente l'algoritmo "doubling" di Manber-Myers. Affina iterativamente l'ordine dei suffissi considerando prefissi di lunghezza esponenzialmente crescente (k=2, 4, 8, ...). Ad ognuna delle O(log n) iterazioni, utilizza la funzione `qsort` della libreria standard C per ordinare un array di n strutture `Suffix` basandosi su coppie di rank calcolate nell'iterazione precedente. Poiché `qsort` ha una complessità temporale media di O(n log n), la complessità totale di questa implementazione di baseline è O(n log^2 n). La successiva costruzione dell'array LCP utilizza l'algoritmo lineare (O(n)) di Kasai.

### Algoritmo Parallelo MPI (Master-Worker con K-Way Merge)

La versione MPI (`src/mpi/manber_myers_mpi.c`) parallelizza la baseline usando una strategia Master-Worker.
1.  **Inizializzazione:** Il processo master (rank 0) distribuisce le strutture `Suffix` iniziali (basate sui primi due caratteri) equamente tra tutti i P processi usando `MPI_Scatterv`.
2.  **Raddoppio Iterativo (O(log n) iterazioni):**
    * **Ordinamento Locale (Parallelo):** Ogni processo ordina il proprio chunk locale di strutture `Suffix` usando `qsort` (O(n/P log(n/P))).
    * **Raccolta (Comunicazione):** Tutti i processi inviano i loro chunk localmente ordinati al master usando `MPI_Gatherv`.
    * **Merge Globale (Seriale sul Master - Ottimizzato):** Il processo master fonde i P chunk ordinati usando un **algoritmo K-Way Merge** implementato con un Min-Heap. Questo passo sostituisce un `qsort` globale e ha una complessità di O(n log P), riducendo significativamente il collo di bottiglia seriale rispetto a O(n \og n).
    * **Calcolo Rank (Seriale sul Master):** Il master calcola il nuovo array dei rank globale basandosi sulla lista fusa e ordinata (O(n)).
    * **Broadcast (Comunicazione):** Il master invia il nuovo array dei rank a tutti i processi usando `MPI_Bcast`.
    * **Aggiornamento Locale (Parallelo):** Ogni processo aggiorna le coppie di rank (`rank[0]`, `rank[1]`) all'interno delle proprie strutture `Suffix` locali usando l'array dei rank globale ricevuto (O(n/P).
3.  **Finalizzazione:** Il master invia tramite broadcast il Suffix Array finale (indici) a tutti i processi.

Viene impiegata una **strategia ibrida**: per input inferiori a 5MB, solo il processo master esegue l'algoritmo sequenziale di baseline per evitare l'overhead di MPI.

### Algoritmo Parallelo CUDA ("Quasi GPU-Resident" con Thrust)

La versione CUDA (`src/cuda/manber_myers.cu`) sfrutta la libreria Thrust per l'accelerazione GPU all'interno di un flusso di lavoro ibrido Host-Device.
1.  **Inizializzazione (Host & HtoD):** L'host inizializza le strutture `Suffix` e calcola i rank iniziali (per k=2). Queste strutture e l'array dei rank iniziale vengono copiati nella memoria globale della GPU (`thrust::device_vector`) una sola volta.
2.  **Raddoppio Iterativo (O(log n) iterazioni, orchestrato dall'Host):**
    * **Aggiornamento Rank (Kernel GPU):** Un kernel CUDA custom (`kernel_update_suffixes`) viene lanciato per aggiornare i campi `rank[0]` e `rank[1]` delle strutture `Suffix` direttamente sulla GPU, usando l'array dei rank dell'iterazione precedente (anch'esso residente sulla GPU).
    * **Ordinamento (GPU via Thrust):** Viene chiamato `thrust::stable_sort` per ordinare le strutture `Suffix` sulla GPU basandosi sulle coppie di rank aggiornate.
    * **Trasferimento Dati Ordinati (DtoH - Bottleneck):** L'intero array ordinato di strutture `Suffix` viene copiato indietro sull'host usando `cudaMemcpy`.
    * **Calcolo Rank (CPU Seriale - Bottleneck):** L'host calcola il nuovo array dei rank globale scansionando le strutture ordinate ricevute (O(n)).
    * **Trasferimento Nuovi Rank (HtoD - Bottleneck):** Il nuovo array dei rank calcolato viene copiato indietro sulla GPU usando `cudaMemcpy` per l'iterazione successiva.
3.  **Finalizzazione (DtoH):** Le strutture `Suffix` finali ordinate (contenenti gli indici corretti) sono disponibili sull'host dall'ultimo trasferimento DtoH dell'iterazione e vengono usate per popolare il Suffix Array di output.

Simile a MPI, una **strategia ibrida** bypassa l'accelerazione GPU per input inferiori a 5MB. Questo approccio "quasi GPU-Resident" riduce significativamente il traffico HtoD rispetto a un modello Host-Centric, ma mantiene i trasferimenti DtoH e il calcolo seriale dei rank come colli di bottiglia residui all'interno del ciclo.

## Struttura dei File

Hai assolutamente ragione, perdonami! Ho continuato in inglese per abitudine con il codice. Ecco il README completo tradotto in italiano, pronto per essere inserito nel tuo progetto.

Markdown

# Costruzione Parallela di Suffix Array tramite Algoritmo Manber-Myers (MPI & CUDA)

## Panoramica del Progetto

Questo progetto implementa e analizza versioni parallele dell'algoritmo di Manber-Myers per la costruzione di Suffix Array, finalizzate a risolvere efficientemente il problema della Sottostringa Ripetuta Più Lunga (Longest Repeated Substring - LRS) per stringhe di input di grandi dimensioni. L'obiettivo primario è accelerare la fase computazionalmente intensiva della costruzione del Suffix Array utilizzando due distinti paradigmi dell'High Performance Computing (HPC): Message Passing Interface (MPI) per il parallelismo a memoria distribuita e CUDA per il parallelismo massivo su GPU NVIDIA.

Il progetto include:
1.  Una **implementazione sequenziale di baseline** basata sulla strategia "doubling" di Manber-Myers, che utilizza `qsort` per l'ordinamento, risultando in una complessità $O(n \log^2 n)$.
2.  Una **implementazione parallela MPI** che impiega una strategia Master-Worker ottimizzata con un algoritmo K-Way Merge sul processo master per ridurre il collo di bottiglia seriale da $O(n \log n)$ a $O(n \log P)$.
3.  Una **implementazione parallela CUDA** che utilizza un approccio "Quasi GPU-Resident", sfruttando la libreria Thrust (`thrust::stable_sort`) per l'ordinamento accelerato su GPU e gestendo parzialmente il flusso dati e il calcolo dei rank sull'host.
4.  Una suite di script per la **generazione dei dati**, il **benchmarking** e l'**analisi delle prestazioni**.

Le performance di queste implementazioni sono valutate su dataset che variano da 1MB a 500MB, confrontando i tempi di esecuzione, lo speedup e identificando i colli di bottiglia computazionali.

## Dettagli dell'Algoritmo

### Algoritmo Sequenziale di Baseline ($O(n \log^2 n)$)

L'implementazione sequenziale di riferimento (`src/sequential/manber_myers.c`) segue direttamente l'algoritmo "doubling" di Manber-Myers. Affina iterativamente l'ordine dei suffissi considerando prefissi di lunghezza esponenzialmente crescente ($k=2, 4, 8, \dots$). Crucialmente, ad ognuna delle $O(\log n)$ iterazioni, utilizza la funzione `qsort` della libreria standard C per ordinare un array di $n$ strutture `Suffix` basandosi su coppie di rank calcolate nell'iterazione precedente. Poiché `qsort` ha una complessità temporale media di $O(n \log n)$, la complessità totale di questa implementazione di baseline è $O(n \log^2 n)$. La successiva costruzione dell'array LCP utilizza l'algoritmo lineare ($O(n)$) di Kasai.

### Algoritmo Parallelo MPI (Master-Worker con K-Way Merge)

La versione MPI (`src/mpi/manber_myers_mpi.c`) parallelizza la baseline $O(n \log^2 n)$ usando una strategia Master-Worker.
1.  **Inizializzazione:** Il processo master (rank 0) distribuisce le strutture `Suffix` iniziali (basate sui primi due caratteri) equamente tra tutti i $P$ processi usando `MPI_Scatterv`.
2.  **Raddoppio Iterativo ($O(\log n)$ iterazioni):**
    * **Ordinamento Locale (Parallelo):** Ogni processo ordina il proprio chunk locale di strutture `Suffix` usando `qsort` ($O(\frac{n}{P} \log \frac{n}{P})$).
    * **Raccolta (Comunicazione):** Tutti i processi inviano i loro chunk localmente ordinati al master usando `MPI_Gatherv`.
    * **Merge Globale (Seriale sul Master - Ottimizzato):** Il processo master **fonde** i $P$ chunk ordinati usando un **algoritmo K-Way Merge** implementato con un Min-Heap. Questo passo sostituisce un `qsort` globale ingenuo e ha una complessità di **$O(n \log P)$**, riducendo significativamente il collo di bottiglia seriale rispetto a $O(n \log n)$.
    * **Calcolo Rank (Seriale sul Master):** Il master calcola il nuovo array dei rank globale basandosi sulla lista fusa e ordinata ($O(n)$).
    * **Broadcast (Comunicazione):** Il master invia il nuovo array dei rank a tutti i processi usando `MPI_Bcast`.
    * **Aggiornamento Locale (Parallelo):** Ogni processo aggiorna le coppie di rank (`rank[0]`, `rank[1]`) all'interno delle proprie strutture `Suffix` locali usando l'array dei rank globale ricevuto ($O(\frac{n}{P})$).
3.  **Finalizzazione:** Il master invia tramite broadcast il Suffix Array finale (indici) a tutti i processi.

Viene impiegata una **strategia ibrida**: per input inferiori a 5MB, solo il processo master esegue l'algoritmo sequenziale di baseline per evitare l'overhead di MPI.

### Algoritmo Parallelo CUDA ("Quasi GPU-Resident" con Thrust)

La versione CUDA (`src/cuda/manber_myers.cu`) sfrutta la libreria Thrust per l'accelerazione GPU all'interno di un flusso di lavoro ibrido Host-Device.
1.  **Inizializzazione (Host & HtoD):** L'host inizializza le strutture `Suffix` e calcola i rank iniziali (per $k=2$). Queste strutture e l'array dei rank iniziale vengono copiati nella memoria globale della GPU (`thrust::device_vector`) **una sola volta**.
2.  **Raddoppio Iterativo ($O(\log n)$ iterazioni, orchestrato dall'Host):**
    * **Aggiornamento Rank (Kernel GPU):** Un kernel CUDA custom (`kernel_update_suffixes`) viene lanciato per aggiornare i campi `rank[0]` e `rank[1]` delle strutture `Suffix` direttamente sulla GPU, usando l'array dei rank dell'iterazione precedente (anch'esso residente sulla GPU).
    * **Ordinamento (GPU via Thrust):** Viene chiamato `thrust::stable_sort` per ordinare le strutture `Suffix` sulla GPU basandosi sulle coppie di rank aggiornate.
    * **Trasferimento Dati Ordinati (DtoH - Bottleneck):** L'intero array ordinato di strutture `Suffix` viene copiato **indietro sull'host** usando `cudaMemcpy`.
    * **Calcolo Rank (CPU Seriale - Bottleneck):** L'host calcola il nuovo array dei rank globale scansionando le strutture ordinate ricevute ($O(n)$).
    * **Trasferimento Nuovi Rank (HtoD - Bottleneck):** Il nuovo array dei rank calcolato viene copiato **indietro sulla GPU** usando `cudaMemcpy` per l'iterazione successiva.
3.  **Finalizzazione (DtoH):** Le strutture `Suffix` finali ordinate (contenenti gli indici corretti) sono disponibili sull'host dall'ultimo trasferimento DtoH dell'iterazione e vengono usate per popolare il Suffix Array di output.

Simile a MPI, una **strategia ibrida** bypassa l'accelerazione GPU per input inferiori a 5MB. Questo approccio "Quasi GPU-Resident" riduce significativamente il traffico HtoD rispetto a un modello Host-Centric ingenuo, ma mantiene i trasferimenti DtoH e il calcolo seriale dei rank come colli di bottiglia residui all'interno del ciclo.

## Struttura dei File

│ 
├── Makefile # Orchestrazione della compilazione 
├── README.md  
├── .gitignore # Regole per Git ignore 
│ 
├── bin/ # Eseguibili compilati (ignorati da git) 
│ ├── main_sequential 
│ ├── main_mpi 
│ └── main_cuda 
│ 
├── src/ # Codice sorgente 
│ ├── common/ # Codice condiviso (struct, utility) 
│ │ ├── suffix_array.h 
│ │ ├── utils.c 
│ │ └── utils.h 
│ ├── sequential/ # Implementazione sequenziale baseline 
│ │ ├── main_sequential.c 
│ │ └── manber_myers.c 
│ ├── mpi/ # Implementazione parallela MPI 
│ │ ├── main_mpi.c 
│ │ └── manber_myers_mpi.c # (Master-Worker + K-Way Merge) 
│ └── cuda/ # Implementazione parallela CUDA 
│ ├── main_cuda.cu 
│ └── manber_myers.cu # ("quasi GPU-Resident" + Thrust) 
│ 
├── scripts/ # Script di automazione e analisi 
│ ├── generate_large_datasets.py # Generazione dati di test (con seme fisso) 
│ ├── benchmark_sequential.py # Esegue benchmark sequenziale 
│ ├── benchmark_mpi.py # Esegue benchmark MPI 
│ ├── benchmark_cuda.py # Esegue benchmark CUDA 
│ ├── plot_sequential.py # Genera grafici sequenziali 
│ ├── plot_mpi.py # Genera grafici MPI 
│ ├── plot_cuda.py # Genera grafici CUDA 
│ └── plot_comparison.py # Genera grafici comparativi 
│ 
├── results/ # Output dei benchmark (ignorati da git) 
│ ├── csv/ # Dati grezzi dei benchmark 
│ └── charts/ # Grafici generati 
│ 
└── test_data/ # File di input per i test (ignorati da git) 
  ├── banana.txt 
  ├── mississippi.txt 
  ├── ... (altri file piccoli) 
  └── large/ # Dataset grandi 
    └── random_1MB.txt # ... fino a random_500MB.txt


## Prerequisiti

* **Compilatori:**
    * GCC (>= 7.0, con supporto C99)
    * Implementazione OpenMPI (es. OpenMPI >= 4.1, che fornisce `mpicc`)
    * NVIDIA CUDA Toolkit (>= 11.0, testato con 12.2, che fornisce `nvcc`)
* **Strumenti di Build:** `make`, `git`
* **Ambiente Python (per gli script):**
    * Python 3 (>= 3.8 raccomandato)
    * Librerie: `pandas`, `matplotlib`, `numpy`
    * È raccomandato un ambiente virtuale (es. `hpc_env`).

## Istruzioni per la Compilazione

Il `Makefile` fornito gestisce il processo di compilazione.

1.  **Compila tutte le versioni (Sequenziale, MPI, CUDA):**
    make all
    *(NB: Questo presume che `nvcc` sia disponibile. Altrimenti, la compilazione CUDA fallirà.)*

2.  **Compila solo la versione sequenziale:**
    make sequential

3.  **Compila solo la versione MPI:**
    make mpi

4.  **Compila solo la versione CUDA:**
    make cuda

Gli eseguibili verranno creati nella directory `bin/`.


## Generazione dei Dati

Dataset di test di grandi dimensioni (1MB, 50MB, 100MB, 200MB, 500MB) composti da caratteri pseudo-casuali possono essere generati usando lo script fornito (`scripts/generate_large_dataset.py`). Assicurarsi che Python sia disponibile.

# Genera i dati
python3 scripts/generate_large_datasets.py

I file generati verranno salvati in `test_data/large/`. Questi file usano un seme fisso (42) per la riproducibilità.


## Esecuzione dei Benchmark
Script Python sono forniti per automatizzare l'esecuzione dei benchmark su tutti i file di test e raccogliere i dati prestazionali. Assicurarsi che l'ambiente Python sia configurato e che gli eseguibili siano compilati.

1.  **Esegui Benchmark Sequenziale:**
    python3 scripts/benchmark_sequential.py

    I risultati verranno salvati in `results/csv/sequential_results.csv`.

2.  **Esegui Benchmark MPI:**
    python3 scripts/benchmark_mpi.py

    I risultati verranno salvati in `results/csv/mpi_results.csv`.

3.  **Esegui Benchmark CUDA:**
    python3 scripts/benchmark_cuda.py

    I risultati verranno salvati in `results/csv/cuda_results.csv`.


## Esecuzione Manuale delle Versioni Specifiche
È anche possibile eseguire direttamente gli eseguibili compilati:

* **Sequenziale:**
    ./bin/main_sequential test_data/large/random_1MB.txt
    # o con una stringa diretta
    ./bin/main_sequential "banana"

* **MPI:**
    mpirun -np 4 ./bin/main_mpi test_data/large/random_100MB.txt

* **CUDA:**
    ./bin/main_cuda test_data/large/random_50MB.txt


## Generazione dei Grafici
Dopo aver eseguito i benchmark e generato i file `.csv` con i risultati, è possibile generare i grafici prestazionali usando gli script Python forniti. Assicurarsi che l'ambiente Python sia attivo.

# Genera grafici specifici per l'esecuzione sequenziale
python3 scripts/plot_sequential.py
# Genera grafici specifici per l'esecuzione MPI (scalabilità)
python3 scripts/plot_mpi.py
# Genera grafici specifici per l'esecuzione CUDA (breakdown)
python3 scripts/plot_cuda.py
# Genera grafici comparativi (Seq vs MPI vs CUDA)
python3 scripts/plot_comparison.py


I grafici verranno salvati come file PNG nelle rispettive sottodirectory sotto `results/charts/`.


## Risultati
* I dati grezzi delle performance (tempi di esecuzione, ecc.) dai benchmark sono salvati in formato CSV nella directory `results/csv/`.
* I grafici prestazionali generati (formato PNG) sono salvati nella directory `results/charts/`, organizzati in sottocartelle: `sequential`, `mpi`, `cuda` e `comparison`.


## Ambiente Target
L'ambiente primario di sviluppo e test per l'analisi prestazionale è stato:

* **Piattaforma:** Istanza Kaggle Notebook
* **CPU:** Intel Xeon (Multi-core)
* **GPU:** NVIDIA Tesla P100 (16GB, Compute Capability 6.0)
* **OS:** Ubuntu 22.04 LTS (all'interno dell'ambiente Kaggle)
* **Compilatori:** GCC 11.4.0, OpenMPI 4.1.x, CUDA Toolkit 12.2

I test MPI sono stati eseguiti su un singolo nodo, simulando un piccolo cluster usando processi multipli, potenzialmente in modalità oversubscription.