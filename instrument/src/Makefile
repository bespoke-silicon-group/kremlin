##===- lib/Transforms/Hello/Makefile -----------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##

KREMLIN_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# TODO: don't hardwire obj dir in here (will change when we update llvm
# versions)
LEVEL = ../llvm/llvm-3.2-obj/
LIBRARYNAME = KremlinInstrument
LOADABLE_MODULE = 1
USEDLIBS = 
LINK_COMPONENTS = support
REQUIRES_RTTI = 1
REQUIRES_EH = 1

# Paths for LLVM.
LLVM_INSTALL_DIR = ../llvm/install
LLVM_INSTALL_BIN_DIR = $(LLVM_INSTALL_DIR)/bin
LLVM_INSTALL_LIB_DIR = $(LLVM_INSTALL_DIR)/lib

# Override the sources
SOURCES = $(shell find . -name '*.cpp')

# Override the project directory to this one.
override PROJ_SRC_DIR = .

# Override the tool directory to the one we installed.
override LLVMToolDir = $(LLVM_INSTALL_BIN_DIR)

UNAME := $(shell uname)

# Add the common directory to the list of includes.
#ifneq ($(UNAME), Darwin)
#CXX.Flags += -I../../common/include
#endif

LD.Flags += -L$(LLVM_INSTALL_LIB_DIR)


# librt doesn't exist for OSX so we're careful to not link against it if we
# detect OSX.
ifneq ($(UNAME), Darwin)
LD.Flags += -lrt
endif

ifdef DEBUG
override ENABLE_OPTIMIZED=0
else
ENABLE_OPTIMIZED=1
endif

#uncomment the following line if you want to build with support for gprof profiling
#ENABLE_PROFILING=1

# If we don't need RTTI or EH, there's no reason to export anything
# from the hello plugin.
ifneq ($(REQUIRES_RTTI), 1)
ifneq ($(REQUIRES_EH), 1)
EXPORTED_SYMBOL_FILE = $(PROJ_SRC_DIR)/Kremlin.exports
endif
endif

include $(LEVEL)/Makefile.common

BUILD_DIRS = $(addprefix $(ObjDir)/, $(dir $(SOURCES)))
BUILD_DIRS_RULE = build-dirs
$(BUILD_DIRS_RULE): 
	@echo "kremlin makefile: $(KREMLIN_MAKEFILE)"
	mkdir -p $(BUILD_DIRS)
	make -f $(KREMLIN_MAKEFILE) $(LibName.SO)

.DEFAULT_GOAL = build-dirs
