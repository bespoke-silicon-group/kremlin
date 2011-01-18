# ---------------------------------------------------------------------------
# Compiles from C to assembly.
# ---------------------------------------------------------------------------

ifndef COMPILE_MK
COMPILE_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/buildTools.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/llvmGenericRules.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/llvmRules.mk

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# Anything we must instrument.
SOURCES_C = $(filter %.c, $(SOURCES))

# All the C sources without the .c
SOURCES_C_NO_EXTENSION = $(basename $(SOURCES_C))

# Passes required as a chained rule. The code to instrument must go through
# all these passes.
#PASS_CHAIN = .simplifycfg.mem2reg.indvars.elimsinglephis.splitbbatfunccall.uniquify.criticalpath.regioninstrument
PASS_CHAIN = .simplifycfg.mem2reg.indvars.elimsinglephis.splitbbatfunccall.criticalpath.uniquify.regioninstrument

# Instrumented byte code
INSTRUMENTED_ASM = $(addsuffix $(PASS_CHAIN).bc.s, $(SOURCES_C_NO_EXTENSION))
INSTRUMENTED_ASM_WITH_GCC_NAME = $(addsuffix .s, $(SOURCES_C_NO_EXTENSION))

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------

.PHONY: compile

# Creates all the instrumented assembly
compile: $(INSTRUMENTED_ASM_WITH_GCC_NAME)
	@echo "sources: $(SOURCES)"
	@echo "sources_c: $(SOURCES_C)"
	@echo "instrumented asm: $(INSTRUMENTED_ASM)"
	@echo "desired asm: $(INSTRUMENTED_ASM_WITH_GCC_NAME)"

# Converts from our long chain name to the names produced by gcc.
$(INSTRUMENTED_ASM_WITH_GCC_NAME): %.s: %$(PASS_CHAIN).bc.s
	mv $< $@

clean::
	-@$(RM) $(INSTRUMENTED_ASM_WITH_GCC_NAME)

endif # COMPILE_MK
