# === COMPILER AND FLAGS ===
CC = gcc
MPICC = mpicc
NVCC = nvcc
CFLAGS = -Wall -Wextra -O3 -std=c99
# Flags specifici per NVCC (usa ottimizzazione -O3, standard C++11 è comune per Thrust)
NVCCFLAGS = -O3 -std=c++11 -arch=sm_60 # Architettura Pascal (P100 su Kaggle è sm_60)
LDFLAGS =

# === DIRECTORIES ===
SRC_DIR = src
BIN_DIR = bin
COMMON_DIR = $(SRC_DIR)/common
SEQ_DIR = $(SRC_DIR)/sequential
MPI_DIR = $(SRC_DIR)/mpi
CUDA_DIR = $(SRC_DIR)/cuda # Aggiunto CUDA_DIR
BENCH_DIR = $(SRC_DIR)/benchmark

# === SOURCE AND OBJECT FILES ===
# Common
COMMON_SRC = $(COMMON_DIR)/utils.c
COMMON_OBJ = $(COMMON_SRC:.c=.o)

# Sequential
SEQ_SRC = $(SEQ_DIR)/manber_myers.c
SEQ_MAIN_SRC = $(SEQ_DIR)/main_sequential.c
# Oggetti specifici per l'eseguibile sequenziale
SEQ_TARGET_OBJS = $(COMMON_OBJ) $(SEQ_SRC:.c=.o) $(SEQ_MAIN_SRC:.c=.o)

# MPI
MPI_SRC = $(MPI_DIR)/manber_myers_mpi.c
MPI_MAIN_SRC = $(MPI_DIR)/main_mpi.c
# Oggetti specifici per l'eseguibile MPI (include il manber_myers.o sequenziale per build_lcp etc.)
MPI_TARGET_OBJS = $(COMMON_OBJ) $(SEQ_DIR)/manber_myers.o $(MPI_SRC:.c=.o) $(MPI_MAIN_SRC:.c=.o)

# CUDA
CUDA_SRC = $(CUDA_DIR)/manber_myers.cu
CUDA_MAIN_SRC = $(CUDA_DIR)/main_cuda.cu
# Oggetti specifici per l'eseguibile CUDA (include il manber_myers.o sequenziale per build_lcp etc.)
# Gli oggetti CUDA hanno suffisso .cu.o
CUDA_TARGET_OBJS = $(COMMON_OBJ) $(SEQ_DIR)/manber_myers.o $(CUDA_SRC:.cu=.cu.o) $(CUDA_MAIN_SRC:.cu=.cu.o)

# Benchmark (usa la logica sequenziale di manber_myers.c)
BENCH_SRC = $(BENCH_DIR)/main_benchmark.c
BENCH_OBJ = $(BENCH_SRC:.c=.o)
BENCH_TARGET_OBJS = $(COMMON_OBJ) $(SEQ_DIR)/manber_myers.o $(BENCH_OBJ)

# === TARGETS ===
TARGET_SEQ = $(BIN_DIR)/main_sequential
TARGET_MPI = $(BIN_DIR)/main_mpi
TARGET_CUDA = $(BIN_DIR)/main_cuda # Aggiunto TARGET_CUDA
TARGET_BENCH = $(BIN_DIR)/suffix_array_benchmark

# Aggiunto 'cuda' e 'run-benchmark-cuda' ai target .PHONY
.PHONY: all sequential mpi cuda benchmark charts clean distclean run-benchmark run-benchmark-mpi run-benchmark-cuda run-mpi test test-mpi test-correctness env-setup help generate-data

# === PRIMARY TARGETS ===
# Aggiunto 'cuda' al target 'all'
all: sequential mpi cuda benchmark

sequential: $(TARGET_SEQ)

mpi: $(TARGET_MPI)

cuda: $(TARGET_CUDA) # Aggiunto target cuda

benchmark: $(TARGET_BENCH)

# === LINKING RULES ===
# Linking Sequential Target
$(TARGET_SEQ): $(SEQ_TARGET_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Linking MPI Target
$(TARGET_MPI): $(MPI_TARGET_OBJS) | $(BIN_DIR)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Linking CUDA Target (usa nvcc per il linking finale)
$(TARGET_CUDA): $(CUDA_TARGET_OBJS) | $(BIN_DIR)
	$(NVCC) $(NVCCFLAGS) -o $@ $^ $(LDFLAGS) # Linka usando nvcc

# Linking Benchmark Target
$(TARGET_BENCH): $(BENCH_TARGET_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# === COMPILATION RULES (PATTERN RULES) ===
# Compile common source files (.c -> .o)
$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c $(COMMON_DIR)/utils.h $(COMMON_DIR)/suffix_array.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile sequential source files (.c -> .o)
$(SEQ_DIR)/%.o: $(SEQ_DIR)/%.c $(COMMON_DIR)/suffix_array.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile MPI source files (.c -> .o, usa mpicc)
$(MPI_DIR)/%.o: $(MPI_DIR)/%.c $(COMMON_DIR)/suffix_array.h
	$(MPICC) $(CFLAGS) -c -o $@ $<

# Nuova regola CORRETTA per compilare file .cu -> .cu.o
$(CUDA_DIR)/%.cu.o: $(CUDA_DIR)/%.cu $(COMMON_DIR)/suffix_array.h $(COMMON_DIR)/utils.h
	@echo "Compiling CUDA file: $<"
	$(NVCC) $(NVCCFLAGS) -c -o $@ $< # $< ora punta correttamente al file .cu

# Compile benchmark source files (.c -> .o)
$(BENCH_DIR)/%.o: $(BENCH_DIR)/%.c $(COMMON_DIR)/suffix_array.h
	$(CC) $(CFLAGS) -c -o $@ $<

# === UTILITY & TESTING TARGETS ===
# Setup Python virtual environment
env-setup:
	@echo "🐍 Setting up Python virtual environment..."
	sudo apt-get update && sudo apt-get install -y python3-full python3-venv
	python3 -m venv hpc_env
	./hpc_env/bin/pip install pandas matplotlib seaborn numpy
	@echo "✅ Python environment configured in ./hpc_env/"

# Generate test data
generate-data:
	@echo "💾 Generating large test datasets..."
	@python3 scripts/generate_large_datasets.py

# Generate charts from benchmark results
charts:
	@echo "📊 Generating charts..."
	# Assicurati che lo script esista e usi l'env corretto
	./hpc_env/bin/python3 scripts/generate_charts.py

# Run sequential benchmark and generate charts (obsoleto?)
# run-benchmark: benchmark
#	./$(TARGET_BENCH) # Questo eseguibile benchmark cosa fa? Potrebbe essere superfluo
#	make charts

# Run MPI benchmark and save results
run-benchmark-mpi: mpi
	@echo "🚀 Running MPI benchmark..."
	# Assicurati che lo script esista e usi l'env corretto
	./hpc_env/bin/python3 scripts/benchmark_mpi.py

# Aggiunto target per benchmark CUDA
run-benchmark-cuda: cuda
	@echo "🚀 Running CUDA benchmark..."
	# Assicurati che lo script esista e usi l'env corretto
	./hpc_env/bin/python3 scripts/benchmark_cuda.py

# Run MPI version on a large file for a quick test
run-mpi: mpi
	@echo "🚀 Running MPI version on 500MB file with 4 processes..."
	mpirun --allow-run-as-root --oversubscribe -np 4 ./$(TARGET_MPI) test_data/large/random_500MB.txt

# Basic sequential tests
test: sequential
	@echo "=== TESTING SEQUENTIAL VERSION ==="
	./$(TARGET_SEQ) "banana"
	@echo ""
	./$(TARGET_SEQ) "mississippi"

# Basic MPI tests
test-mpi: mpi
	@echo "=== TESTING MPI VERSION (4 processes) ==="
	mpirun --allow-run-as-root --oversubscribe -np 4 ./$(TARGET_MPI) test_data/banana.txt

# Correctness tests for sequential version
test-correctness: sequential
	@echo "=== CORRECTNESS TESTS ==="
	@echo "Test 1: 'banana' (expected: 'ana')"
	@./$(TARGET_SEQ) "banana" | grep "Longest repeated substring"
	@echo "Test 2: 'mississippi' (expected: 'issi')"
	@./$(TARGET_SEQ) "mississippi" | grep "Longest repeated substring"
	@echo "Test 3: 'abcabcabc' (expected: 'abcabc')"
	@./$(TARGET_SEQ) "abcabcabc" | grep "Longest repeated substring"

# === CLEANING TARGETS ===
clean:
	@echo "🧹 Cleaning build files..."
	# Rimuove tutti i file oggetto e gli eseguibili
	rm -f $(SEQ_DIR)/*.o $(MPI_DIR)/*.o $(CUDA_DIR)/*.cu.o $(COMMON_DIR)/*.o $(BENCH_DIR)/*.o
	rm -f $(TARGET_SEQ) $(TARGET_MPI) $(TARGET_CUDA) $(TARGET_BENCH)
	# Rimuove i risultati generati
	rm -rf results/csv/*.csv results/charts/*.png

distclean: clean
	@echo "🔥 Performing deep clean (removes Python venv and bin)..."
	rm -rf hpc_env $(BIN_DIR)

# === HELP TARGET ===
help:
	@echo "=== HPC SUFFIX ARRAY MAKEFILE TARGETS ==="
	@echo "--- Build Targets ---"
	@echo "  make all           - Compila tutte le versioni (sequential, mpi, cuda, benchmark)"
	@echo "  make sequential    - Compila solo la versione sequenziale"
	@echo "  make mpi           - Compila solo la versione MPI"
	@echo "  make cuda          - Compila solo la versione CUDA"
	@echo "  make benchmark     - Compila l'eseguibile per il benchmark (sequenziale?)"
	@echo ""
	@echo "--- Execution & Testing ---"
	@echo "  make run-benchmark-mpi  - Esegue il benchmark MPI e salva i risultati"
	@echo "  make run-benchmark-cuda - Esegue il benchmark CUDA e salva i risultati"
	@echo "  make test          - Esegue test di base sulla versione sequenziale"
	@echo "  make test-mpi      - Esegue test di base sulla versione MPI"
	@echo "  make test-correctness - Test di correttezza con output atteso"
	@echo ""
	@echo "--- Utility Targets ---"
	@echo "  make generate-data - Genera i file di test di grandi dimensioni"
	@echo "  make charts        - Genera i grafici dai risultati dei benchmark (richiede CSV)"
	@echo "  make env-setup     - Configura l'ambiente virtuale Python per i grafici"
	@echo "  make clean         - Rimuove i file oggetto e gli eseguibili"
	@echo "  make distclean     - Rimuove tutto, incluso l'ambiente Python e la cartella bin"
	@echo "  make help          - Mostra questo messaggio di aiuto"