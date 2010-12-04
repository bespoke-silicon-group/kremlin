# ---------------------------------------------------------------------------
# Assembles from .s to .o
# ---------------------------------------------------------------------------

ifndef ASSEMBLE_MK
ASSEMBLE_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/compile.mk

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# All the assembly files that have been instrumented or not.
SOURCES_ASM = $(filter %.s, $(SOURCES)) $(INSTRUMENTED_ASM_WITH_GCC_NAME)

# All the object files that have been instrumented or not.
SOURCES_OBJ_FROM_ASM = $(SOURCES_ASM:.s=.o)

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------

.PHONY: assemble

# Assembles from .s to .o
$(SOURCES_OBJ_FROM_ASM): %.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

# Creates all the instrumented assembly
assemble: $(SOURCES_OBJ_FROM_ASM)

clean::
	-@$(RM) $(SOURCES_OBJ_FROM_ASM)

endif # ASSEMBLE_MK
