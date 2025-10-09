# Makefile - HPC Suffix Array Project

# Compilatori
CC = gcc
MPICC = mpicc
NVCC = nvcc

# Flag di compilazione
CFLAGS = -Wall -Wextra -std=c99 -O2
MPIFLAGS = -Wall -Wextra -O2
NVCCFLAGS = -arch=sm_75 -O3 -std=c++17
LDFLAGS = -lm

# Directory
SRC_DIR = src
SEQ_DIR = $(SRC_DIR)/sequential
MPI_DIR = $(SRC_DIR)/mpi
CUDA_DIR = $(SRC_DIR)/cuda
COMMON_DIR = $(SRC_DIR)/common
BIN_DIR = bin
TESTS_DIR = tests

# File sorgenti
COMMON_SRCS = $(COMMON_DIR)/utils.c
SEQ_SRCS = $(SEQ_DIR)/manber_myers.c $(SEQ_DIR)/main_sequential.c

# Target principali
.PHONY: all sequential mpi cuda clean test

all: sequential mpi

sequential: $(BIN_DIR)/suffix_array_seq

mpi: $(BIN_DIR)/suffix_array_mpi

cuda: $(BIN_DIR)/suffix_array_cuda

# Crea directory bin se non esiste
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# === VERSIONE SEQUENZIALE ===
$(BIN_DIR)/suffix_array_seq: $(COMMON_SRCS) $(SEQ_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/suffix_array_seq $(COMMON_SRCS) $(SEQ_SRCS) $(LDFLAGS)
	@echo "✅ Versione sequenziale compilata: $(BIN_DIR)/suffix_array_seq"

# === VERSIONE MPI (placeholder per ora) ===
$(BIN_DIR)/suffix_array_mpi: | $(BIN_DIR)
	@echo "📝 MPI target - da implementare"
	$(CC) $(CFLAGS) -o $(BIN_DIR)/mpi_placeholder $(COMMON_DIR)/utils.c $(LDFLAGS)
	@echo "⚠️  MPI non ancora implementato - creato placeholder"

# === VERSIONE CUDA (placeholder per ora) ===
$(BIN_DIR)/suffix_array_cuda: | $(BIN_DIR)
	@echo "📝 CUDA target - da implementare"
	@echo "⚠️  CUDA non ancora implementato"

# === TEST E UTILITY ===

# Test di base
test_basic: $(BIN_DIR)/suffix_array_seq
	$(CC) $(CFLAGS) -o $(BIN_DIR)/test_basic $(TESTS_DIR)/test_basic.c $(COMMON_SRCS) $(SEQ_DIR)/manber_myers.c $(LDFLAGS)
	@echo "✅ Test basic compilato"

# Esecuzione test automatici
test: $(BIN_DIR)/suffix_array_seq
	@echo "🧪 === TEST SUFFIX ARRAY SEQUENZIALE ==="
	@echo ""
	@echo "🧪 Test con stringa 'banana':"
	./$(BIN_DIR)/suffix_array_seq "banana"
	@echo ""
	@echo "🧪 Test con stringa 'mississippi':"
	./$(BIN_DIR)/suffix_array_seq "mississippi"
	@echo ""
	@echo "🧪 Test con stringa 'cabbage':"
	./$(BIN_DIR)/suffix_array_seq "cabbage"
	@echo ""
	@echo "🧪 Test con stringa 'abracadabra':"
	./$(BIN_DIR)/suffix_array_seq "abracadabra"

# Test di correttezza
test_correctness: $(BIN_DIR)/suffix_array_seq
	@echo "🔍 === TEST CORRETTEZZA ==="
	@echo "Test 1 - banana (atteso: 'ana' LRS=3)"
	./$(BIN_DIR)/suffix_array_seq "banana" | grep "Sottostringa"
	@echo ""
	@echo "Test 2 - mississippi (atteso: 'issi' LRS=4)"
	./$(BIN_DIR)/suffix_array_seq "mississippi" | grep "Sottostringa"
	@echo ""
	@echo "Test 3 - aaaa (atteso: 'aaa' LRS=3)"
	./$(BIN_DIR)/suffix_array_seq "aaaa" | grep "Sottostringa"

# Benchmark prestazioni
benchmark: $(BIN_DIR)/suffix_array_seq
	@echo "📊 === BENCHMARK PRESTAZIONI ==="
	@echo "Test con stringa di 1000 caratteri casuali:"
	@python3 -c "import random; import string; print(''.join(random.choices(string.ascii_lowercase, k=1000)))" > /tmp/test_1k.txt
	@time ./$(BIN_DIR)/suffix_array_seq "$$(cat /tmp/test_1k.txt)" > /dev/null
	@echo ""
	@echo "Test con stringa di 5000 caratteri casuali:"
	@python3 -c "import random; import string; print(''.join(random.choices(string.ascii_lowercase, k=5000)))" > /tmp/test_5k.txt
	@time ./$(BIN_DIR)/suffix_array_seq "$$(cat /tmp/test_5k.txt)" > /dev/null

# Pulizia
clean:
	rm -rf $(BIN_DIR)
	@echo "✅ Pulizia completata"

# Help
help:
	@echo "=== HPC SUFFIX ARRAY PROJECT ==="
	@echo "Target disponibili:"
	@echo "  all          - Compila sequential e MPI"
	@echo "  sequential   - Compila solo versione sequenziale"
	@echo "  mpi          - Compila versione MPI (placeholder)"
	@echo "  cuda         - Compila versione CUDA (placeholder)"
	@echo "  test         - Esegui test automatici"
	@echo "  test_correctness - Test di correttezza"
	@echo "  benchmark    - Test prestazioni"
	@echo "  clean        - Pulizia file compilati"
	@echo ""
	@echo "Esempi:"
	@echo "  make sequential && ./bin/suffix_array_seq \"banana\""
	@echo "  make test"
	@echo "  make benchmark"