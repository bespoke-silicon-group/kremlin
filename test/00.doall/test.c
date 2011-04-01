#include <stdio.h>
#include <stdlib.h>

int* loopTest(int x) __attribute__ ((noinline));

int* pointer;


int* loopTest(int x) {
	int i;
	pointer = malloc(x * sizeof(int));
	for (i=0; i<x; i++) {
		pointer[i] = pointer[i] + i;
	}
	return pointer;
	
}


int main() {
	loopTest(100); // sp should be close to 100
	loopTest(1000); // sp should be close to 1000
	return 0;
}
