
CC = kremlin-gcc
LIB = lib.a
LIB_SOURCES = lib.c

$(LIB_SOURCES): CC=gcc

build: lib.a main.c
	$(CC) $^

check:
	@echo "TEST PASSED"

clean::
	-make clean -f kremlin.mk
	-rm $(LIB) kremlin.mk sregions.txt kremlin.a

$(LIB): $(patsubst %, $(LIB)(%), $(addsuffix .o, $(basename $(LIB_SOURCES))))
	ranlib $@
