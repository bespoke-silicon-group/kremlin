ifndef REFERENCE_MK

REFERENCE_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../paths.mk
include $(MAKE_DIR)/genericTest.mk

EXPECTED_OUT = kremlin.bin
REFERENCE_OUT = $(EXPECTED_OUT).reference

REFERENCE_DEPENDENCIES := $(RUN_TASK)

define REFERENCE_BUILD_COMMANDS
	cp $(EXPECTED_OUT) $(REFERENCE_OUT)
endef

define REFERENCE_CHECK_COMMANDS
	diff --brief $(EXPECTED_OUT) $(REFERENCE_OUT)
endef

$(call GENERIC_TEST, referenceBuild, $(REFERENCE_DEPENDENCIES), $(REFERENCE_BUILD_COMMANDS))
$(call GENERIC_TEST, referenceCheck, $(REFERENCE_DEPENDENCIES), $(REFERENCE_CHECK_COMMANDS))

endif # REFERENCE_MK
