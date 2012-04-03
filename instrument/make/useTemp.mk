# ---------------------------------------------------------------------------
# Uses temporary files to build.
# ---------------------------------------------------------------------------

ifndef USE_TEMP_MK
USE_TEMP_MK = 1

# Function:          MAKE_ALL_USING_TMP
# Description:       Adds rules to create files using temporary intermediates.
#
# Parameters:        $(1)   The list of all the sources.
#                    $(2)   The extension of the output files. Their default
#                           name from source.c will be source$(2)
#                    $(3)   The name of the function that returns the makefile
#                           text that defines the rules for converting the 
#                           source file to output file.
# Return:            Nothing.
MAKE_ALL_USING_TMP = $(foreach SOURCE, $(1), $(eval $(call MAKE_USING_TMP,$(SOURCE),$(2),$(3))))

# Function:          MAKE_USING_TMP
# Description:       Adds rules to create files using temporary intermediates
#                    for a single file.
#
# Parameters:        $(1)   The list of all the sources.
#                    $(2)   The extension of the output files. Their default
#                           name from source.c will be source$(2)
#                    $(3)   The name of the function that returns the makefile
#                           text that defines the rules for converting the 
#                           source file to output file.
# Return:            Nothing.
define MAKE_USING_TMP

# Files
# -----

# The source file.
$(eval SOURCE_FILE := $$(1))

# The source dir.
$(eval SOURCE_DIR := $$(dir $(SOURCE_FILE)))

# The source without the directory.
$(eval SOURCE_NO_DIR := $$(notdir $(SOURCE_FILE)))

# The base name of the source file.
$(eval SOURCE_BASE := $$(basename $(SOURCE_NO_DIR)))

# Create a temporary file to write the compiled file out to.
$(eval OUTPUT_FILE := $$(shell mktemp -t $(SOURCE_BASE).XXXXXXXX))

# Create a temporary file to write the compiled file out to.
$(eval SOURCE_TO_OUTPUT_FUNC := $$(3))

# The default name of the assembly file.
$(eval USER_FILE := $$(addsuffix $(2), $(SOURCE_BASE)))

# Rules (alpha order)
# -------------------

ifdef KREMLIN_VERBOSE_BUILD
$$(info Creating rule for $(SOURCE_FILE) -> $(OUTPUT_FILE) -> $(USER_FILE))
endif

# Mark the output file as phony because we just created it. Consequently,
# it'll always be newer than the dependencies and never be made. Furthermore,
# GCC always overwrites destination files, so mark those as phony so they will
# always be remade.
.PHONY: $(OUTPUT_FILE) $(USER_FILE)

# Mark the output file as an intermediate file so that it's always deleted
# after make completes.
.INTERMEDIATE: $(OUTPUT_FILE)

# If required, this also can make the file usable by the user. It always is
# put in the current directory even if we compile a source file in another
# directory.
$(USER_FILE): $(OUTPUT_FILE)
	mv $$< $$@

# Add in the rules to create SOURCE_FILE -> OUTPUT_FILE
$$(eval $$(call $(SOURCE_TO_OUTPUT_FUNC),$(SOURCE_FILE),$(OUTPUT_FILE)))

endef # MAKE_USING_TMP

endif # USE_TEMP_MK
