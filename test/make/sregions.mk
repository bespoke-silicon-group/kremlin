ifndef SREGIONS_MK

SREGIONS_MK = 1

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../paths.mk
include $(MAKE_DIR)/genericTest.mk

SREGIONS_EXPECTED_OUT = sregions.txt.normalized
SREGIONS_REFERENCE_OUT = $(SREGIONS_EXPECTED_OUT).reference

SREGIONS_DEPENDENCIES := $(SREGIONS_EXPECTED_OUT)

# XXX: Hack to remove UUID
$(SREGIONS_EXPECTED_OUT): $(BUILD_TASK)
	sed 's/[a-z0-9]\{16\}/0000000000000000/g' sregions.txt | sort > $@

define SREGIONS_BUILD_COMMANDS
	cp $(SREGIONS_EXPECTED_OUT) $(SREGIONS_REFERENCE_OUT)
endef

define SREGIONS_CHECK_COMMANDS
	diff --brief $(SREGIONS_EXPECTED_OUT) $(SREGIONS_REFERENCE_OUT)
endef

$(call GENERIC_TEST, sregionsBuild, $(SREGIONS_DEPENDENCIES), $(SREGIONS_BUILD_COMMANDS))
$(call GENERIC_TEST, sregionsCheck, $(SREGIONS_DEPENDENCIES), $(SREGIONS_CHECK_COMMANDS))

endif # SREGIONS_MK

