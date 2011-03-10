TOP_LEVEL := $(dir $(lastword $(MAKEFILE_LIST)))

include $(TOP_LEVEL)/paths.mk
include $(MAKE_DIR)/defaultTest.mk
