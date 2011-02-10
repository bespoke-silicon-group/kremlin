REGRESSION_TEST_ROOT = ../../..

SOURCES = main.c add.c

add.o: CC = gcc
add.o: CFLAGS := $(filter-out --krem-%, $(CFLAGS))

include $(REGRESSION_TEST_ROOT)/defaultTest.mk
