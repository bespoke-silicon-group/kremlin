REGRESSION_TEST_ROOT = $(dir $(lastword $(MAKEFILE_LIST)))

include $(REGRESSION_TEST_ROOT)/buildTasks.mk

# ---------------------------------------------------------------------------
# Defines
# ---------------------------------------------------------------------------

# The C compiler.
CC = kremlin-gcc

# The C++ compiler.
CXX = kremlin-g++

# The fortran compiler.
FC = kremlin-gfortran

# The linker.
LD = $(CC)

# The profiler.
PROFILER = kremlin

# The expected binary output file.
EXPECTED_BIN = kremlin.bin

# The output of the profiler.
PROFILE_OUT = profile.out

# The reference output file.
PROFILE_OUT_REFERENCE = $(PROFILE_OUT).reference

# C compiler flags.
CFLAGS += -Wall --krem-debug

# C++ compiler flags.
CXXFLAGS += $(CFLAGS)

# The name of the executable.
TARGET ?= a.out

# All the source files.
SOURCES ?= $(shell ls *.c *.cpp *.f *.f95)

# All the object files.
OBJECTS = $(addsuffix .o, $(basename $(SOURCES)))

# If this looks like a C++ compilation
ifneq ($(filter %.cpp, $(SOURCES)),)
LD = $(CXX)
LDFLAGS += $(CXXFLAGS)
endif # cpp

# If this looks like a fortran compilation
ifneq ($(filter %.f %.f95, $(SOURCES)),)
LD = $(FC)
endif # cpp

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------
$(BUILD_TASK): $(TARGET)

# Cleans generated files.
$(CLEAN_TASK)::
	-make clean -f kremlin.mk
	-@rm -rf *.s *.log *.bc *.o $(TARGET) $(EXPECTED_BIN) gmon.out massif.out.* *.graph $(PROFILE_OUT)

# Checks that a run has completed successfully.
$(CHECK_TASK):
	# If the expected output file does not exist or is 0 in size, print that
	# it failed.
	@if [ ! -a $(EXECTED_BIN) ] || [ ! -s $(EXPECTED_BIN) ]; \
	then \
		echo "TEST_RESULT: FAILED"; \
	else \
		echo "TEST_RESULT: PASSED"; \
	fi \

# The expected binary file.
$(EXPECTED_BIN): $(RUN_TASK)
	@echo ""

# The output of the profiler.
$(PROFILE_OUT): $(PROFILE_TASK)
	@echo ""

# Builds a reference from the output of the profiler.
$(PROFILE_OUT_REFERENCE): $(PROFILE_OUT)
	cp $< $@

# Run the profiler.
$(PROFILE_TASK): $(EXPECTED_BIN)
	$(PROFILER) | tee $(PROFILE_OUT)

# Builds a reference output file.
$(REFERENCE_BUILD_TASK): $(PROFILE_OUT_REFERENCE)

# Checks the reference output file.
$(REFERENCE_CHECK_TASK): $(PROFILE_OUT)
	diff --brief $(PROFILE_OUT_REFERENCE) $(PROFILE_OUT)

# Runs the program.
$(RUN_TASK): $(TARGET)
	./$(TARGET) $(ARGS)

# Builds the executable image binary.
$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

