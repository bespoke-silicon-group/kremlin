# ---------------------------------------------------------------------------
# Renames files as the last step. This handles the -o option.
# ---------------------------------------------------------------------------

ifndef OUTPUT_NAME_MK
OUTPUT_NAME_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/compile.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/assemble.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/link.mk

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------
# The base name of the file to move.
BASE_NAME = $(basename $(firstword $(SOURCES)))

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------
$(OUTPUT_NAME): 
	# Try to move the output files to the desired output name. The last stage
	# created will be the output name. This is kind of a hack.
	-cp $(BASE_NAME).ii $@
	-cp $(BASE_NAME).s $@
	-cp $(BASE_NAME).o $@
	-cp $(EXECUTABLE_OUTPUT_NAME) $@

endif # OUTPUT_NAME_MK
