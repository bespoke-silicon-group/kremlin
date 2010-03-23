CC=gcc
#CFLAGS=-O3
override CFLAGS+=-g -DDEBUG -pg

SRC=skeleton.o

all: $(SRC)

clean:
	-rm *.o
