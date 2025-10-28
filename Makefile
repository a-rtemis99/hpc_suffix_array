# COMPILER AND FLAGS 
CC = gcc
MPICC = mpicc
NVCC = nvcc
CFLAGS = -Wall -Wextra -O3 -std=c99
# Flags specifici per NVCC (ottimizzazione -O3, standard C++11, architettura P100)
NVCCFLAGS = -O3 -std=c++11 -arch=sm_60
LDFLAGS = -lm 

# DIRECTORIES 
SRC_DIR = src
BIN_DIR = bin
COMMON_DIR = $(SRC_DIR)/common
SEQ_DIR = $(SRC_DIR)/sequential
MPI_DIR = $(SRC_DIR)/mpi
CUDA_DIR = $(SRC_DIR)/cuda
BENCH_DIR = $(SRC_DIR)/benchmark

# SOURCE AND OBJECT FILES 
# Common
COMMON_SRC = $(wildcard $(COMMON_DIR)/*.c) 
COMMON_OBJ = $(patsubst $(COMMON_DIR)/%.c,$(COMMON_DIR)/%.o,$(COMMON_SRC))

# Sequential
SEQ_SRC = $(wildcard $(SEQ_DIR)/*.c)
# Esclude main_sequential.o se esiste un main_benchmark.c
SEQ_OBJS_NO_MAIN = $(patsubst $(SEQ_DIR)/%.c,$(SEQ_DIR)/%.o,$(filter-out $(SEQ_DIR)/main_sequential.c,$(SEQ_SRC)))
SEQ_MAIN_OBJ = $(SEQ_DIR)/main_sequential.o
SEQ_TARGET_OBJS = $(COMMON_OBJ) $(SEQ_OBJS_NO_MAIN) $(SEQ_MAIN_OBJ)

# MPI
MPI_SRC = $(wildcard $(MPI_DIR)/*.c)
MPI_OBJS_NO_MAIN = $(patsubst $(MPI_DIR)/%.c,$(MPI_DIR)/%.o,$(filter-out $(MPI_DIR)/main_mpi.c,$(MPI_SRC)))
MPI_MAIN_OBJ = $(MPI_DIR)/main_mpi.o
# Dipende dagli oggetti comuni, oggetti sequenziali (per LCP etc), oggetti MPI
MPI_TARGET_OBJS = $(COMMON_OBJ) $(SEQ_DIR)/manber_myers.o $(MPI_OBJS_NO_MAIN) $(MPI_MAIN_OBJ)

# CUDA
CUDA_SRC = $(wildcard $(CUDA_DIR)/*.cu)
# Oggetti CUDA avranno suffisso .cu.o
CUDA_OBJS = $(patsubst $(CUDA_DIR)/%.cu,$(CUDA_DIR)/%.cu.o,$(CUDA_SRC))
# Dipende dagli oggetti comuni, oggetti sequenziali (per LCP etc), oggetti CUDA
CUDA_TARGET_OBJS = $(COMMON_OBJ) $(SEQ_DIR)/manber_myers.o $(CUDA_OBJS)

# Benchmark (usa la logica sequenziale di manber_myers.c)
BENCH_SRC = $(wildcard $(BENCH_DIR)/*.c)
BENCH_OBJS = $(patsubst $(BENCH_DIR)/%.c,$(BENCH_DIR)/%.o,$(BENCH_SRC))
BENCH_TARGET_OBJS = $(COMMON_OBJ) $(SEQ_DIR)/manber_myers.o $(BENCH_OBJS)

# TARGETS 
TARGET_SEQ = $(BIN_DIR)/main_sequential
TARGET_MPI = $(BIN_DIR)/main_mpi
TARGET_CUDA = $(BIN_DIR)/main_cuda
TARGET_BENCH = $(BIN_DIR)/suffix_array_benchmark

.PHONY: all sequential mpi cuda benchmark charts clean distclean run-benchmark run-benchmark-mpi run-benchmark-cuda run-mpi test test-mpi test-correctness env-setup help generate-data

# PRIMARY TARGETS 
all: sequential mpi cuda benchmark

sequential: $(TARGET_SEQ)

mpi: $(TARGET_MPI)

cuda: $(TARGET_CUDA)

benchmark: $(TARGET_BENCH)

# LINKING RULES
$(TARGET_SEQ): $(SEQ_TARGET_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_MPI): $(MPI_TARGET_OBJS) | $(BIN_DIR)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_CUDA): $(CUDA_TARGET_OBJS) | $(BIN_DIR)
	$(NVCC) $(NVCCFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_BENCH): $(BENCH_TARGET_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# COMPILATION RULES (PATTERN RULES)
# Regola generica per .c -> .o (usa CC)
$(COMMON_DIR)/%.o $(SEQ_DIR)/%.o $(BENCH_DIR)/%.o: %.c $(COMMON_DIR)/suffix_array.h $(COMMON_DIR)/utils.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Regola specifica per MPI .c -> .o (usa MPICC)
$(MPI_DIR)/%.o: $(MPI_DIR)/%.c $(COMMON_DIR)/suffix_array.h $(COMMON_DIR)/utils.h | $(BIN_DIR)
	$(MPICC) $(CFLAGS) -c -o $@ $<

# Regola specifica per CUDA .cu -> .cu.o (usa NVCC)
$(CUDA_DIR)/%.cu.o: $(CUDA_DIR)/%.cu $(COMMON_DIR)/suffix_array.h $(COMMON_DIR)/utils.h | $(BIN_DIR)
	@echo "Compiling CUDA file: $<"
	$(NVCC) $(NVCCFLAGS) -c -o $@ $<

# UTILITY & TESTING TARGETS 
env-setup:
	@echo "Setting up Python virtual environment..."
	sudo apt-get update && sudo apt-get install -y python3-full python3-venv
	python3 -m venv hpc_env
	./hpc_env/bin/pip install pandas matplotlib seaborn numpy
	@echo "Python environment configured in ./hpc_env/"

generate-data:
	@echo "Generating large test datasets..."
	@python3 scripts/generate_large_datasets.py

charts: env-check 
	@echo "Generating charts..."
	./hpc_env/bin/python3 scripts/generate_charts.py

run-benchmark-mpi: mpi env-check
	@echo "Running MPI benchmark..."
	./hpc_env/bin/python3 scripts/benchmark_mpi.py

run-benchmark-cuda: cuda env-check
	@echo "Running CUDA benchmark..."
	./hpc_env/bin/python3 scripts/benchmark_cuda.py

run-mpi: mpi
	@echo "Running MPI version on 500MB file with 4 processes..."
	mpirun --allow-run-as-root --oversubscribe -np 4 ./$(TARGET_MPI) test_data/large/random_500MB.txt

test: sequential
	@echo "=== TESTING SEQUENTIAL VERSION ==="
	./$(TARGET_SEQ) "banana"
	@echo ""
	./$(TARGET_SEQ) "mississippi"

test-mpi: mpi
	@echo "=== TESTING MPI VERSION (4 processes) ==="
	mpirun --allow-run-as-root --oversubscribe -np 4 ./$(TARGET_MPI) test_data/banana.txt

test-correctness: sequential
	@echo "=== CORRECTNESS TESTS ==="
	@echo "Test 1: 'banana' (expected: 'ana')"
	@./$(TARGET_SEQ) "banana" | grep "Longest repeated substring"
	@echo "Test 2: 'mississippi' (expected: 'issi')"
	@./$(TARGET_SEQ) "mississippi" | grep "Longest repeated substring"
	@echo "Test 3: 'abcabcabc' (expected: 'abcabc')"
	@./$(TARGET_SEQ) "abcabcabc" | grep "Longest repeated substring"

# Target per verificare se l'ambiente Python è attivo
.PHONY: env-check
env-check:
	@if [ ! -d "hpc_env" ]; then \
		echo "Python environment not found. Please run 'make env-setup' first."; \
		exit 1; \
	fi

# CLEANING TARGETS
clean:
	@echo "🧹 Cleaning build files..."
	rm -f $(COMMON_DIR)/*.o $(SEQ_DIR)/*.o $(MPI_DIR)/*.o $(CUDA_DIR)/*.cu.o $(BENCH_DIR)/*.o
	rm -f $(TARGET_SEQ) $(TARGET_MPI) $(TARGET_CUDA) $(TARGET_BENCH)
	rm -rf results/csv/*.csv results/charts/*.png

distclean: clean
	@echo "Performing deep clean (removes Python venv and bin)..."
	rm -rf hpc_env $(BIN_DIR)

# HELP TARGET
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