# ---------------------------------------------------------------------------
# The test suite runner.
# ---------------------------------------------------------------------------

ifndef RUN_MK
RUN_MK = 1

# All the directories to run tests for.
# They're expected to have their own makefile that can clean, build and run
# the test.
#
# Looks for any directory with a makefile in it. This is considered to be a
# test directory.
# mindepth 2 to ignore the makefile in the current directory
TEST_DIRECTORIES = $(sort $(abspath $(dir $(shell find $(REGRESSION_TEST_ROOT) -mindepth 2 -name Makefile))))


# Rule to clean the directories
CLEAN_RESULTS = $(addsuffix $(CLEAN_TASK), $(TEST_DIRECTORIES))

$(CLEAN_RESULTS):
	make -C $(dir $@) $(CLEAN_TASK)

.DEFAULT_GOAL = referenceCheckAll

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../paths.mk
include $(MAKE_DIR)/buildTasks.mk
include $(MAKE_DIR)/tests.mk

endif # RUN_MK
