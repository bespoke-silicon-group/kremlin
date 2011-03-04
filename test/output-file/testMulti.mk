REGRESSION_TEST_ROOT = $(OUTPUT_FILE_ROOT)/..

include $(REGRESSION_TEST_ROOT)/buildTasks.mk

CC = kremlin-gcc
export CFLAGS += -Wall
SOURCES ?= $(shell ls *.c)
EXPECTED_OUT_EXT ?= .s
EXPECTED_OUT = $(addsuffix $(EXPECTED_OUT_EXT), $(basename $(SOURCES)))

$(BUILD_TASK):
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES)

$(RUN_TASK):
	@echo "Skipping running"

$(CHECK_TASK):
	# If the expected output file does not exist or is 0 in size, print that
	# it failed.
	@failed=0; \
		for file in $(EXPECTED_OUT); \
		do \
			if [ ! -s $$file ]; \
			then \
				failed=1; \
				break; \
			fi \
		done; \
		if [ "$$failed" -eq "1" ]; \
		then \
			echo "TEST_RESULT: FAILED"; \
		else \
			echo "TEST_RESULT: PASSED"; \
		fi; \

$(REFERENCE_CHECK_TASK):
	@echo "Skipping check."

$(REFERENCE_BUILD_TASK):
	@echo "Skipping check."

$(CLEAN_TASK)::
	-make clean -f kremlin.mk
	-@rm -rf *.s *.log *.bc *.o $(TARGET)
