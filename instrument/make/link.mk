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
include $(KREMLIN_INSTRUMENT_MAKE_DIR)/useTemp.mk
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
OBJ_SOURCES += $(filter %.o, $(SOURCES))

# The name of the executable to produce.
LINK_OUTPUT_FILE ?= a.out
DEBUG_INFO_FILE = sregions.txt

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------
#

# Creates all the instrumented assembly
link: $(LINK_OUTPUT_FILE)

# Compiles and links the source with the kremlin library
$(LINK_OUTPUT_FILE): $(OBJ_SOURCES) $(KREMLIN_LIB)
	$(LD) $(LDFLAGS) $(LOADLIBES) $(LDLIBS) $(CFLAGS) $(OBJ_SOURCES) $(KREMLIN_LIB) -o $@

	# XXX: Side effect of making the executable!
	# TODO: This only magically works because the names line up. Fix the
	# robustness!
	objdump $@ -t | grep "_krem_" | sed 's/^.*krem_prefix//g; s/_krem_/\t/g' > sregions.txt

clean::
	$(RM) $(LINK_OUTPUT_FILE)

endif # LINK_MK
