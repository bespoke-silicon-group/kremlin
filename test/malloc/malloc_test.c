#include <math.h>
#include <stdlib.h>


#define NUM_POINTERS	100

int* pointers[NUM_POINTERS];

int main() {
	int i;
	turnOnProfiler();
	//bw_heavy(w_array, r_array0);
	for (i=0; i<NUM_POINTERS; i++) {
		pointers[i] = malloc(20);
	}

	
	for (i=0; i<NUM_POINTERS; i++) {
		free(pointers[i]);
	}
	
	//bw_light();
	turnOffProfiler();

	return 0;
	
}
