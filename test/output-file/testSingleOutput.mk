REGRESSION_TEST_ROOT = $(OUTPUT_FILE_ROOT)/..

include $(REGRESSION_TEST_ROOT)/buildTasks.mk

CC = kremlin-gcc
export CFLAGS += -Wall
TARGET ?= customOutputName
SOURCES ?= $(shell ls *.c)

$(BUILD_TASK): $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SOURCES)

$(RUN_TASK):
	@echo "Skipping running"

$(CHECK_TASK):
	# If the expected output file does not exist or is 0 in size, print that
	# it failed.
	@if [ ! -s $(TARGET) ]; \
	then \
		echo "TEST_RESULT: FAILED"; \
	else \
		echo "TEST_RESULT: PASSED"; \
	fi \

$(REFERENCE_CHECK_TASK):
	@echo "Skipping check."

$(REFERENCE_BUILD_TASK):
	@echo "Skipping check."


$(CLEAN_TASK)::
	-make clean -f kremlin.mk
	-@rm -rf *.s *.log *.bc *.o $(TARGET)
