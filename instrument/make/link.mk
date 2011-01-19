# ---------------------------------------------------------------------------
# Links from .o to an executable
# ---------------------------------------------------------------------------

ifndef LINK_MK
LINK_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/assemble.mk
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/kremlinLib.mk

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# All the files to link.
#
# We filter out all the C sources and the assembly sources since we've run
# them possibly through instrumentation. We add the remainder sources with
# unknown extensions because that is what gcc does. It passes any unknown file
# extensions to the linker.
#
# We also add the objects that we've assembled.
SOURCES_OBJ = $(filter-out $(SOURCES_ASM) $(SOURCES_C), $(SOURCES)) $(SOURCES_OBJ_FROM_ASM)

# The name of the executable to produce.
EXECUTABLE_OUTPUT_NAME = a.out
DEBUG_INFO_FILE = sregions.txt

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------

# Creates all the instrumented assembly
link: $(EXECUTABLE_OUTPUT_NAME) $(DEBUG_INFO_FILE)

# Compiles and links the source with the kremlin library
$(EXECUTABLE_OUTPUT_NAME): $(SOURCES_OBJ) $(KREMLIN_LIB)
	$(CC) $(LDFLAGS) $(LOADLIBES) $(LDLIBS) $(CFLAGS) $(SOURCES_OBJ) $(KREMLIN_LIB) -o $@

$(DEBUG_INFO_FILE): $(EXECUTABLE_OUTPUT_NAME)
	objdump $< -t | grep "_krem_" | sed 's/^.*krem_region//g' | sed 's/_krem_/\t/g' > $@

clean::
	$(RM) $(EXECUTABLE_OUTPUT_NAME)

endif # LINK_MK
