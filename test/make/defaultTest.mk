
# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../paths.mk
include $(MAKE_DIR)/buildTasks.mk
include $(MAKE_DIR)/tests.mk

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

# How to run the program
RUN_COMMAND = ./$(TARGET) $(ARGS)

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------
$(BUILD_TASK): $(TARGET)

# Cleans generated files.
$(CLEAN_TASK)::
	-make clean -f kremlin.mk
	-@rm -rf *.s *.log *.bc *.o $(TARGET) $(EXPECTED_BIN) gmon.out massif.out.* *.graph $(PROFILE_OUT)

# Run the profiler.
$(PROFILE_TASK): $(EXPECTED_BIN)
	$(PROFILER)

# Runs the program.
$(RUN_TASK): $(TARGET)
	$(RUN_COMMAND)

# Builds the executable image binary.
$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

.DEFAULT_GOAL = referenceCheck
