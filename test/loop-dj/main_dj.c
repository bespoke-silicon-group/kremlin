#include <stdio.h>
#include <stdlib.h>

int* pointer;


int* loopTest(int x) {
	int i;
	pointer = malloc(x * sizeof(int));
	for (i=0; i<x; i++) {
		pointer[i] = i + i*3;
	}
	return pointer;
	
}

int main() {


	loopTest(1000);


	return 0;
}
