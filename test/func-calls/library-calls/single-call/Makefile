include topLevel.mk

SOURCES = main.c add.c

add.o: CC = gcc
add.o: CFLAGS := $(filter-out --krem-%, $(CFLAGS))
