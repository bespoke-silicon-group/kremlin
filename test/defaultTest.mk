REGRESSION_TEST_ROOT = $(dir $(lastword $(MAKEFILE_LIST)))

include $(REGRESSION_TEST_ROOT)/buildTasks.mk

CC = kremlin-gcc
CXX = kremlin-g++
FC = kremlin-gfortran
LD = $(CC)
EXPECTED_BIN = kremlin.bin
CFLAGS += -Wall --krem-debug
CXXFLAGS += $(CFLAGS)
TARGET ?= a.out
SOURCES ?= $(shell ls *.c *.cpp *.f *.f95)
OBJECTS = $(addsuffix .o, $(basename $(SOURCES)))

ifneq ($(filter %.cpp, $(SOURCES)),)
LD = $(CXX)
LDFLAGS += $(CXXFLAGS)
endif # cpp

ifneq ($(filter %.f %.f95, $(SOURCES)),)
LD = $(FC)
endif # cpp

$(BUILD_TASK): $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

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
	-@rm -rf *.s *.log *.bc *.o $(TARGET) $(EXPECTED_BIN) gmon.out massif.out.*
