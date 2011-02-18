ifndef LLVM_GENERIC_RULES_MK
LLVM_GENERIC_RULES_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk

# ---------------------------------------------------------------------------
# Paths to LLVM
# ---------------------------------------------------------------------------

LLVM_OBJ_DIR = $(KREMLIN_INSTRUMENT_LLVM_DIR)/2.8-obj
LLVM_BIN_DIR = $(KREMLIN_INSTRUMENT_LLVM_BIN_DIR)

ifeq ($(DEBUG), 1)
	RELEASE_OR_DEBUG = Debug+Asserts
else
	RELEASE_OR_DEBUG = Release+Asserts
endif # $(DEBUG) == 1
LLVM_LIB_DIR = $(LLVM_OBJ_DIR)/$(RELEASE_OR_DEBUG)/lib

# ---------------------------------------------------------------------------
# LLVM Compilers, assemblers, etc.
# ---------------------------------------------------------------------------

# The LLVM C compiler
LLVM_CC ?= $(LLVM_BIN_DIR)/llvm-gcc
LLVM_CFLAGS ?= $(CFLAGS)

# The LLVM C++ compiler
LLVM_CXX ?= $(LLVM_BIN_DIR)/llvm-g++
LLVM_CXXFLAGS ?= $(CXXFLAGS)

# THe LLVM fortran compiler
LLVM_FORTRAN ?= $(LLVM_BIN_DIR)/llvm-gfortran
LLVM_FORTRANFLAGS ?= $(FORTRANFLAGS)

# The LLVM dissassembler. Converts LLVM byte code to LLVM assembly.
LLVM_DIS ?= $(LLVM_BIN_DIR)/llvm-dis

# The LLVM static compiler. Converts LLVM byte code to assembly for a
# particular architecture
LLC ?= $(LLVM_BIN_DIR)/llc
LLC_FLAGS ?=

# The architecture to compile to
LLC_MARCH ?= x86-64

# The LLVM byte code optimizer
OPT ?= $(LLVM_BIN_DIR)/opt
OPT_FLAGS ?= -f

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

# Whether to print debug statements for this makefile
DEBUG_MAKE ?= 1

# Options to pass while building the shared object
ifdef DEBUG
	SO_BUILD_OPTIONS = DEBUG=1
else
	SO_BUILD_OPTIONS = 
endif # $(DEBUG) == 1

# ---------------------------------------------------------------------------
# Functions (alpha order)
# ---------------------------------------------------------------------------

# Function:          OPT_PASS_RULE
# Description:       Adds a rule to make the file through opt into the
#                    makefile.
#
#                    Let $(EXTENSION) = the name of the pass without the
#                    leading '-'.
#
#                    The resulting file will be %.$(EXTENSION).bc and take in
#                    %.bc
#
#                    If $(1) is empty, then the empty string is returned.
#
# Parameters:        $(1)   The name of the shared object. If $(1) is empty,
#                           then no load is performed.
#                    $(2)   The command to run the pass.
#                    $(3)   Any additional flags to append to the opt command.
# Return:            The rule to make a particular pass.
OPT_PASS_RULE = $(eval $(call OPT_PASS_RULE_TEXT,$(1),$(2),$(3)))

# Function:          OPT_PASS_RULE_TEXT
# Description:       Returns the makefile rule to make a particular opt pass.
#
#                    Let $(EXTENSION) = the name of the pass without the
#                    leading '-'.
#
#                    The resulting file will be %.$(EXTENSION).bc and take in
#                    %.bc
#
#                    If $(1) is empty, then the empty string is returned.
#
# Parameters:        $(1)   The name of the shared object. If $(1) is empty,
#                           then no load is performed.
#                    $(2)   The command to run the pass.
#                    $(3)   Any additional flags to append to the opt command.
# Return:            The rule to make a particular pass.
define OPT_PASS_RULE_TEXT

# The name of the pass. This must be evaluated in an eval now or else it will
# not be available in the rest of this eval body. That is, if PASS_NAME was
# not wrapped in an eval, it would only be available after all of this text
# has been evaluated.
$(eval PASS_NAME := $$(patsubst -%,%,$(2)))

# Print args
ifneq ($(strip $(DEBUG_MAKE)),)
$$(info beginning rule defs for $(PASS_NAME). Args: $$$$(1): $$(1), $$$$(2): $$(2), $$$$(3): $$(3))
endif # DEBUG_MAKE

# Create a rule to run the pass.
%.$(PASS_NAME).bc: %.bc $(call OPT_PASS_SHARED_OBJ, $(1))
	$(OPT) $(OPT_FLAGS) $(call OPT_LOAD_PASS_SHARED_OBJ,$(1)) $(2) $(value $(3)) -o $$@ $$<  &>$$*.$(PASS_NAME).log

# Create a rule to make the shared object if required.
ifneq ($(strip $(call OPT_PASS_SHARED_OBJ) $(1)),)
ifeq ($$(DEFINED_$(PASS_NAME)_RULE),)

DEFINED_$(PASS_NAME)_RULE := 1

# Mark the rule as phony because the recursive call to make will check if it
# needs to rebuild.
.PHONY: $(call OPT_PASS_SHARED_OBJ, $(1))

# The rule to create the shared object
#
# TODO: All the shared objects are not generated from the same directory. This
# should take in the name of the shared object and find the directory with the
# source and compile it.
$(call OPT_PASS_SHARED_OBJ, $(1)): $(ALWAYS-MAKE)
	$(MAKE) -C $(KREMLIN_INSTRUMENT_SRC_DIR) $(SO_BUILD_OPTIONS)

endif # DEFINED_PASS_SO_ALREADY
endif # PASS_SO

$$(info DEFINED_$(PASS_NAME)_RULE: $$(DEFINED_$(PASS_NAME)_RULE))
$$(info $$(and $(strip $(call OPT_PASS_SHARED_OBJ) $(1)), $$(DEFINED_$(PASS_NAME)_RULE)))
$$(info sod obj dir: $$(LLVM_OBJ_DIR))
$$(info kremlin instrument source dir: $$(KREMLIN_INSTRUMENT_SRC_DIR))

endef # RULES_FOR_SO

# Function:          OPT_LOAD_PASS_SHARED_OBJ
# Description:       Returns the load command for a shared object.
#
#                    If $(1) is empty, then the empty string is returned.
#
# Parameters:        $(1)   The name of the shared object. If $(1) is empty,
#                           then the empty string is returned.
# Return:            The load command for the shared object or the empty
#                    string.
OPT_LOAD_PASS_SHARED_OBJ = $(if $(strip $(1)), -load $(call OPT_PASS_SHARED_OBJ,$(1)))

# Function:          OPT_PASS_SHARED_OBJ
# Description:       Returns the name of the shared object with the directory
#                    prepended. The shared object is expected to be in
#                    $(LLVM_OBJ_DIR)
#
#                    If $(1) is empty, then the empty string is returned.
#
# Parameters:        $(1)   The name of the shared object. If $(1) is empty,
#                           then the empty string is returned.
# Return:            The full path of the shared object. It may be relative
#                    or absolute.
OPT_PASS_SHARED_OBJ = $(if $(strip $(1)), $(LLVM_LIB_DIR)/$(strip $(1)))

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------

# Mark everything as phony since GCC remakes everything every time.
.PHONY: %.bc

# Converts c to LLVM byte code.
%.bc: %.c
	$(LLVM_CC) $(LLVM_CFLAGS) --emit-llvm -c -o $@ $<

# Converts c to LLVM byte code.
%.bc: %.C
%.bc: %.cxx
%.bc: %.cc
%.bc: %.cpp
	$(LLVM_CXX) $(LLVM_CXXFLAGS) --emit-llvm -c -o $@ $<

# Converts fortran source code to LLVM byte code
%.bc: %.f
%.bc: %.f95
	$(LLVM_FORTRAN) $(LLVM_FORTRANFLAGS) --emit-llvm -c -o $@ $<

# Converts LLVM byte code to assembly.
%.bc.s: %.bc
	$(LLC) $(LLC_FLAGS) -march=$(LLC_MARCH) -o $@ $<

# Converts LLVM byte code to human readable LLVM assembly.
%.ll: %.bc
	$(LLVM_DIS) -o $@ $<

clean::
	-rm -rf *.bc *.ll *.s *.log
	$(RM) bb.info loop-source_line.txt Makefile.kremlin nesting.dot region.graph region_id-to-func_name.txt regions.info sregions.txt

# ---------------------------------------------------------------------------
# Required files
# ---------------------------------------------------------------------------
REQUIRED_MAKEFILES := paths.mk
ifdef CHECK_REQUIRED_MAKEFILES_INCLUDED
    $(call CHECK_REQUIRED_MAKEFILES_INCLUDED, $(REQUIRED_MAKEFILES))
else
    $(error Include checkRequiredFiles.mk first)
endif # CHECK_REQUIRED_MAKEFILES_INCLUDED

endif # LLVM_GENERIC_RULES_MK
