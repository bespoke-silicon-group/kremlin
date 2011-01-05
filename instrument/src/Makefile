##===- lib/Transforms/Hello/Makefile -----------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##

LEVEL = ../llvm/2.8-obj/
LIBRARYNAME = KremlinInstrument
LOADABLE_MODULE = 1
USEDLIBS = 
LINK_COMPONENTS = support
REQUIRES_RTTI = 1

LLVM_INSTALL_DIR = ../llvm/install
LLVM_INSTALL_BIN_DIR = $(LLVM_INSTALL_DIR)/bin
LLVM_INSTALL_LIB_DIR = $(LLVM_INSTALL_DIR)/lib
override PROJ_SRC_DIR = .
override LLVMToolDir = $(LLVM_INSTALL_BIN_DIR)
CXX.Flags += -I../../common/include
LD.Flags += -L$(LLVM_INSTALL_LIB_DIR)

ifndef ($(strip $(DEBUG)),)
override ENABLE_OPTIMIZED = 0
else # DEBUG
override ENABLE_OPTIMIZED = 1
endif # DEBUG

# If we don't need RTTI or EH, there's no reason to export anything
# from the hello plugin.
ifneq ($(REQUIRES_RTTI), 1)
ifneq ($(REQUIRES_EH), 1)
EXPORTED_SYMBOL_FILE = $(PROJ_SRC_DIR)/Kremlin.exports
endif
endif

include $(LEVEL)/Makefile.common

