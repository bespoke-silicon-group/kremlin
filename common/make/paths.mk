# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
KREMLIN_ROOT := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
KREMLIN_COMMON = $(KREMLIN_ROOT)/common
KREMLIN_COMMON_SRC = $(KREMLIN_COMMON)/src
KREMLIN_COMMON_MAKE = $(KREMLIN_COMMON)/make
KREMLIN_ANALYZE_DIR = $(KREMLIN_ROOT)/analyze
KREMLIN_BIN_DIR = $(KREMLIN_ROOT)/bin
KREMLIN_INSTRUMENT_DIR = $(KREMLIN_ROOT)/instrument
KREMLIN_INSTRUMENT_MAKE_DIR = $(KREMLIN_INSTRUMENT_DIR)/make
KREMLIN_INSTRUMENT_LLVM_DIR = $(KREMLIN_INSTRUMENT_DIR)/llvm
KREMLIN_INSTRUMENT_LLVM_BIN_DIR = $(KREMLIN_INSTRUMENT_LLVM_DIR)/install/bin
KREMLIN_INSTRUMENT_SRC_DIR = $(KREMLIN_INSTRUMENT_DIR)/src
KREMLIN_RUNTIME_DIR = $(KREMLIN_ROOT)/runtime
KREMLIN_RUNTIME_SRC_DIR = $(KREMLIN_RUNTIME_DIR)/src

SOD_ROOT = $(realpath $(KREMLIN_ROOT)/../..)
