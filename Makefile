# Makefile - HPC Suffix Array Project
# Compilatori
CC = gcc
MPICC = mpicc
NVCC = nvcc

# Flag di compilazione
CFLAGS = -Wall -Wextra -std=c99 -O2
MPIFLAGS = -Wall -Wextra -O2
NVCCFLAGS = -arch=sm_75 -O3 -std=c++17

# Directory
SRC_DIR = src
SEQ_DIR = $(SRC_DIR)/sequential
MPI_DIR = $(SRC_DIR)/mpi
CUDA_DIR = $(SRC_DIR)/cuda
COMMON_DIR = $(SRC_DIR)/common
BIN_DIR = bin

# Target principali
.PHONY: all sequential mpi clean

all: sequential mpi

sequential: $(BIN_DIR)/suffix_array_seq

mpi: $(BIN_DIR)/suffix_array_mpi

# Crea directory bin se non esiste
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Pulizia
clean:
	rm -rf $(BIN_DIR)

# I target specifici verranno definiti dopo
$(BIN_DIR)/suffix_array_seq: 
	@echo "Sequential target - da implementare"

$(BIN_DIR)/suffix_array_mpi:
	@echo "MPI target - da implementare"
