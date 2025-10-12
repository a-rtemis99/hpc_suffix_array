# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c99
LDFLAGS = 

# Directories
SRC_DIR = src
SEQ_DIR = $(SRC_DIR)/sequential
COMMON_DIR = $(SRC_DIR)/common
BENCH_DIR = $(SRC_DIR)/benchmark
BIN_DIR = bin

# Source files
COMMON_SRC = $(COMMON_DIR)/utils.c
SEQ_SRC = $(SEQ_DIR)/manber_myers.c
BENCH_SRC = $(BENCH_DIR)/suffix_array_benchmark.c $(BENCH_DIR)/main_benchmark.c

# Object files
COMMON_OBJ = $(COMMON_SRC:.c=.o)
SEQ_OBJ = $(SEQ_SRC:.c=.o)
BENCH_OBJ = $(BENCH_SRC:.c=.o)

# Targets
TARGET_SEQ = $(BIN_DIR)/sequential_suffix_array
TARGET_BENCH = $(BIN_DIR)/suffix_array_benchmark

.PHONY: all sequential benchmark charts clean run-benchmark test env-setup

all: sequential benchmark

sequential: $(TARGET_SEQ)

benchmark: $(TARGET_BENCH)

benchmark-large:
	@echo "Running large-scale benchmark..."
	@python3 scripts/run_large_benchmark.py


memory-test:
	@echo "Running memory usage tests..."
	@python3 scripts/monitor_memory.py

performance-charts:
	@echo "Generating performance charts..."
	@./hpc_env/bin/python scripts/generate_performance_charts.py

full-benchmark: benchmark-large performance-charts
	@echo "Full benchmark completed!"
	@echo "Charts generated in results/charts/"

test-large: sequential
	@echo "Testing on large datasets..."
	@echo "1MB test:"
	@./$(TARGET_SEQ) test_data/large/random_1MB.txt | grep -E "(Longest repeated substring|Time|Length)"
	@echo ""
	@echo "50MB test:" 
	@timeout 300 ./$(TARGET_SEQ) test_data/large/random_50MB.txt | grep -E "(Longest repeated substring|Time|Length)" || echo "Timeout or error"

generate-data:
	@echo "Generating test datasets..."
	@python3 scripts/generate_large_datasets.py

# Target sequenziale include main_sequential.o
$(TARGET_SEQ): $(SEQ_OBJ) $(COMMON_OBJ) src/sequential/main_sequential.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Target benchmark NON include main_sequential.o
$(TARGET_BENCH): $(BENCH_OBJ) $(COMMON_OBJ) $(SEQ_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Regole di compilazione
$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SEQ_DIR)/%.o: $(SEQ_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BENCH_DIR)/%.o: $(BENCH_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Regola specifica per main_sequential.o
src/sequential/main_sequential.o: src/sequential/main_sequential.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Setup ambiente Python
env-setup:
	sudo apt update
	sudo apt install -y python3-full python3-venv
	python3 -m venv hpc_env
	./hpc_env/bin/pip install pandas matplotlib seaborn numpy
	@echo "✅ Ambiente Python configurato!"

# Genera grafici usando il virtual environment
charts:
	./hpc_env/bin/python scripts/generate_charts.py

# Esegui tutto il benchmark (compilazione + esecuzione + grafici)
run-benchmark: benchmark
	./$(TARGET_BENCH)
	./hpc_env/bin/python scripts/generate_charts.py

# Test base della versione sequenziale
test: sequential
	@echo "=== TESTING SEQUENTIAL VERSION ==="
	./$(TARGET_SEQ) "banana"
	@echo ""
	./$(TARGET_SEQ) "mississippi"
	@echo ""
	./$(TARGET_SEQ) "abcabcabc"

# Test di correttezza
test-correctness: sequential
	@echo "=== CORRECTNESS TESTS ==="
	@echo "Test 1: 'banana' (expected: 'ana')"
	./$(TARGET_SEQ) "banana" | grep "Longest repeated substring"
	@echo "Test 2: 'mississippi' (expected: 'issi')" 
	./$(TARGET_SEQ) "mississippi" | grep "Longest repeated substring"
	@echo "Test 3: 'abcabcabc' (expected: 'abcabc')"
	./$(TARGET_SEQ) "abcabcabc" | grep "Longest repeated substring"

# Pulizia completa
clean:
	rm -f $(COMMON_OBJ) $(SEQ_OBJ) $(BENCH_OBJ) src/sequential/main_sequential.o $(TARGET_SEQ) $(TARGET_BENCH)
	rm -rf results/csv/*.csv results/charts/*.png

# Pulizia estrema (include virtual environment)
distclean: clean
	rm -rf hpc_env bin

# Help
help:
	@echo "=== HPC SUFFIX ARRAY MAKEFILE TARGETS ==="
	@echo "make all              - Compila tutto (sequential + benchmark)"
	@echo "make sequential       - Compila solo la versione sequenziale"
	@echo "make benchmark        - Compila solo il benchmark"
	@echo "make run-benchmark    - Esegue benchmark completo + grafici"
	@echo "make test             - Test di base con stringhe note"
	@echo "make test-correctness - Test di correttezza dettagliato"
	@echo "make charts           - Genera solo i grafici (dopo benchmark)"
	@echo "make env-setup        - Configura ambiente Python"
	@echo "make clean            - Pulizia file oggetto e eseguibili"
	@echo "make distclean        - Pulizia completa (include virtual env)"
	@echo "make help             - Mostra questo help"