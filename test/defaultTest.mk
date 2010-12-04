include $(REGRESSION_TEST_ROOT)/buildTasks.mk

CC = kremlin-gcc
EXPECTED_BIN = cpInfo.bin
export CFLAGS += -Wall
TARGET ?= a.out
SOURCES ?= $(shell ls *.c)
OBJECTS = $(SOURCES:.c=.o)

$(BUILD_TASK): $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

$(RUN_TASK): $(TARGET)
	./$(TARGET) $(ARGS)

$(EXPECTED_BIN): $(RUN_TASK)

$(CHECK_TASK):
	# If the expected output file does not exist or is 0 in size, print that
	# it failed.
	@if [ ! -a $(EXECTED_BIN) ] || [ ! -s $(EXPECTED_BIN) ]; \
	then \
		echo "TEST_RESULT: FAILED"; \
	else \
		echo "TEST_RESULT: PASSED"; \
	fi \

$(CLEAN_TASK)::
	-make clean -f kremlin.mk
	-@rm -rf *.s *.log *.bc *.o $(TARGET) $(EXPECTED_BIN)
