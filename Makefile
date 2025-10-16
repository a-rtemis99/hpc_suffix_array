# === COMPILER AND FLAGS ===
CC = gcc
MPICC = mpicc
CFLAGS = -Wall -Wextra -O3 -std=c99
LDFLAGS =

# === DIRECTORIES ===
SRC_DIR = src
BIN_DIR = bin
COMMON_DIR = $(SRC_DIR)/common
SEQ_DIR = $(SRC_DIR)/sequential
MPI_DIR = $(SRC_DIR)/mpi
BENCH_DIR = $(SRC_DIR)/benchmark

# === SOURCE AND OBJECT FILES ===
# Common
COMMON_SRC = $(COMMON_DIR)/utils.c
COMMON_OBJ = $(COMMON_SRC:.c=.o)

# Sequential
SEQ_SRC = $(SEQ_DIR)/manber_myers.c
SEQ_MAIN_SRC = $(SEQ_DIR)/main_sequential.c
SEQ_OBJ = $(COMMON_OBJ) $(SEQ_SRC:.c=.o) $(SEQ_MAIN_SRC:.c=.o)

# MPI
MPI_SRC = $(MPI_DIR)/manber_myers_mpi.c
MPI_MAIN_SRC = $(MPI_DIR)/main_mpi.c
MPI_OBJ = $(COMMON_OBJ) $(SEQ_DIR)/manber_myers.o $(MPI_SRC:.c=.o) $(MPI_MAIN_SRC:.c=.o)

# Benchmark
BENCH_SRC = $(BENCH_DIR)/main_benchmark.c
BENCH_OBJ = $(BENCH_SRC:.c=.o)

# === TARGETS ===
TARGET_SEQ = $(BIN_DIR)/main_sequential
TARGET_MPI = $(BIN_DIR)/main_mpi
TARGET_BENCH = $(BIN_DIR)/suffix_array_benchmark

.PHONY: all sequential mpi benchmark charts clean distclean run-benchmark run-benchmark-mpi run-mpi test test-mpi test-correctness env-setup help generate-data

# === PRIMARY TARGETS ===
all: sequential mpi benchmark

sequential: $(TARGET_SEQ)

mpi: $(TARGET_MPI)

benchmark: $(TARGET_BENCH)

# === LINKING RULES ===
# Linking Sequential Target
$(TARGET_SEQ): $(SEQ_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Linking MPI Target
$(TARGET_MPI): $(MPI_OBJ) | $(BIN_DIR)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Linking Benchmark Target (uses sequential manber_myers.c)
$(TARGET_BENCH): $(BENCH_OBJ) $(SEQ_DIR)/manber_myers.o $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# === COMPILATION RULES (PATTERN RULES) ===
# Compile common source files
$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile sequential source files
$(SEQ_DIR)/%.o: $(SEQ_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile MPI source files
$(MPI_DIR)/%.o: $(MPI_DIR)/%.c
	$(MPICC) $(CFLAGS) -c -o $@ $<

# Compile benchmark source files
$(BENCH_DIR)/%.o: $(BENCH_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<


# === UTILITY & TESTING TARGETS ===
# Setup Python virtual environment
env-setup:
	@echo "Setting up Python virtual environment..."
	sudo apt-get update && sudo apt-get install -y python3-full python3-venv
	python3 -m venv hpc_env
	./hpc_env/bin/pip install pandas matplotlib seaborn numpy
	@echo "Python environment configured in ./hpc_env/"

# Generate test data
generate-data:
	@echo "Generating large test datasets..."
	@python3 scripts/generate_large_datasets.py

# Generate charts from benchmark results
charts:
	@echo "Generating charts..."
	./hpc_env/bin/python3 scripts/generate_charts.py

# Run sequential benchmark and generate charts
run-benchmark: benchmark
	./$(TARGET_BENCH)
	make charts

# Run MPI benchmark and save results
run-benchmark-mpi: mpi
	@echo "Running MPI benchmark..."
	./hpc_env/bin/python3 scripts/benchmark_mpi.py

# Run MPI version on a large file for a quick test
run-mpi: mpi
	@echo "Running MPI version on 500MB file with 4 processes..."
	mpirun -np 4 ./$(TARGET_MPI) test_data/random_500MB.txt

# Basic sequential tests
test: sequential
	@echo "=== TESTING SEQUENTIAL VERSION ==="
	./$(TARGET_SEQ) "banana"
	@echo ""
	./$(TARGET_SEQ) "mississippi"

# Basic MPI tests
test-mpi: mpi
	@echo "=== TESTING MPI VERSION (4 processes) ==="
	mpirun -np 4 ./$(TARGET_MPI) test_data/banana.txt

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
	@echo "Cleaning build files..."
	rm -f $(SEQ_DIR)/*.o $(MPI_DIR)/*.o $(COMMON_DIR)/*.o $(BENCH_DIR)/*.o
	rm -f $(TARGET_SEQ) $(TARGET_MPI) $(TARGET_BENCH)
	rm -rf results/csv/*.csv results/charts/*.png

distclean: clean
	@echo "Performing deep clean (removes Python venv and bin)..."
	rm -rf hpc_env $(BIN_DIR)

# === HELP TARGET ===
help:
	@echo "=== HPC SUFFIX ARRAY MAKEFILE TARGETS ==="
	@echo "--- Build Targets ---"
	@echo "  make all                - Compila tutte le versioni (sequential, mpi, benchmark)"
	@echo "  make sequential         - Compila solo la versione sequenziale"
	@echo "  make mpi                - Compila solo la versione MPI"
	@echo "  make benchmark          - Compila l'eseguibile per il benchmark sequenziale"
	@echo ""
	@echo "--- Execution & Testing ---"
	@echo "  make run-benchmark      - Esegue il benchmark sequenziale e genera i grafici"
	@echo "  make run-benchmark-mpi  - Esegue il benchmark MPI e salva i risultati"
	@echo "  make test               - Esegue test di base sulla versione sequenziale"
	@echo "  make test-mpi           - Esegue test di base sulla versione MPI"
	@echo "  make test-correctness   - Test di correttezza con output atteso"
	@echo ""
	@echo "--- Utility Targets ---"
	@echo "  make generate-data      - Genera i file di test di grandi dimensioni"
	@echo "  make charts             - Genera i grafici dai risultati dei benchmark"
	@echo "  make env-setup          - Configura l'ambiente virtuale Python per i grafici"
	@echo "  make clean              - Rimuove i file oggetto e gli eseguibili"
	@echo "  make distclean          - Rimuove tutto, incluso l'ambiente Python"
	@echo "  make help               - Mostra questo messaggio di aiuto"