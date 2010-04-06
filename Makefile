CC=gcc
#CFLAGS=-O3
override CFLAGS+=-g -DDEBUG -pg

SRC=skeleton.o main.o table.o

all: $(SRC)
	$(CC) $(SRC) -o test

clean:
	-rm *.o test
