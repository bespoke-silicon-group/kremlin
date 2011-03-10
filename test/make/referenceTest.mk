ifndef REFERENCE_TEST_MK

REFERENCE_TEST_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../paths.mk
include $(MAKE_DIR)/genericTest.mk

EXPECTED_OUT = kremlin.bin
REFERENCE_OUT = $(EXPECTED_OUT).reference

DEPENDENCIES := $(RUN_TASK)

define REFERENCE_BUILD_COMMANDS
	cp $(EXPECTED_OUT) $(REFERENCE_OUT)
endef

define REFERENCE_CHECK_COMMANDS
	diff --brief $(EXPECTED_OUT) $(REFERENCE_OUT)
endef

$(call GENERIC_TEST, referenceBuild, $(DEPENDENCIES), $(REFERENCE_BUILD_COMMANDS))
$(call GENERIC_TEST, referenceCheck, $(DEPENDENCIES), $(REFERENCE_CHECK_COMMANDS))

endif # REFERENCE_TEST_MK
