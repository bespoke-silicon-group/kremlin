#include <stdio.h>
#include <stdlib.h>

int* pointer1;
int* pointer2;


int* loopTest(int x) {
	int i, j;
	pointer1 = malloc(x * sizeof(int));
	pointer2 = malloc(x * sizeof(int));
	for (i=0; i<x; i++) {
		for (j=0; j<x; j++) {
			pointer1[i] = pointer2[j] + i*j;
		}
	}
	return pointer1;
	
}


int main() {
	loopTest(100); // sp should be close to 100
	return 0;
}
