#include <math.h>
#include <stdlib.h>


#define NUM_POINTERS	100

int* pointers[NUM_POINTERS];

int main() {
	int i;
	for (i=0; i<NUM_POINTERS; i++) {
		pointers[i] = malloc(20);
	}

	
	for (i=0; i<NUM_POINTERS; i++) {
		free(pointers[i]);
	}
	
	return 0;
	
}
