# ---------------------------------------------------------------------------
# Creates the kremlin runtime library.
# ---------------------------------------------------------------------------

# Kremlin library
KREMLIN_LIB = kremlin.a

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
include $(dir $(lastword $(MAKEFILE_LIST)))/../../common/make/paths.mk

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# All the common source files.
COMMON_SOURCES = deque.c hash_map.c vector.c
COMMON_SOURCES_WITH_PATH = $(addprefix $(KREMLIN_COMMON_SRC)/, $(COMMON_SOURCES))

# All the kremlin runtime source files.
KREMLIN_SOURCES = debug.c log.c kremlin.c table.c udr.c MemMapAllocator.c Pool.c
KREMLIN_SOURCES_WITH_PATH = $(addprefix $(KREMLIN_RUNTIME_SRC_DIR)/, $(KREMLIN_SOURCES))

# All the sources required to build the kremlin library.
ALL_KREMLIN_SOURCES_WITH_PATH = $(COMMON_SOURCES_WITH_PATH) $(KREMLIN_SOURCES_WITH_PATH)
ALL_KREMLIN_OBJECTS_WITH_PATH = $(ALL_KREMLIN_SOURCES_WITH_PATH:.c=.o)

ALL_KREMLIN_OBJECTS_IN_LIB = $(patsubst %, $(KREMLIN_LIB)(%), $(ALL_KREMLIN_OBJECTS_WITH_PATH))

# ---------------------------------------------------------------------------
# Rules (alpha order)
# ---------------------------------------------------------------------------

# Compiles the kremlin runtime
$(ALL_KREMLIN_OBJECTS_WITH_PATH): CFLAGS += -I$(KREMLIN_COMMON_SRC) $(KREMLIB_EXTRA_FLAGS)
$(ALL_KREMLIN_OBJECTS_WITH_PATH): CC = gcc
$(ALL_KREMLIN_OBJECTS_WITH_PATH): %.o: %.c
	@echo "YAY!"
	$(CC) $(CFLAGS) -c -o $@ $<

# Archives the kremlin objects into a static library
$(KREMLIN_LIB): $(ALL_KREMLIN_OBJECTS_IN_LIB)
	ranlib $@

clean::
	-@$(RM) $(KREMLIN_LIB)
