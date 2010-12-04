# ---------------------------------------------------------------------------
# Driver kremlin makefile.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Global options
# ---------------------------------------------------------------------------

# Debug mode.
export DEBUG = 1

# Output program name.
TARGET = a.out

# Source files.
SOURCES ?= 

# Flags to pass to all C compilers.
CFLAGS += -g -DDEBUG

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/buildTools.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/checkRequiredFiles.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/eliminateImplicitRules.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/eliminateOverrides.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/spaceAndEmpty.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/compile.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/assemble.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/link.mk


# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------

# Default to creating an executable
executable: $(TARGET)
