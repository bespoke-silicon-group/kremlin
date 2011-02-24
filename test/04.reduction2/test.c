#include <stdio.h>
#include <stdlib.h>

int* pointer1;
int* offset;
int* offset2;


double loopTest(int x) {
	int i, j;
	double  sum;
	sum = 0.0;
	for (i=0; i<x; i++) {
		sum += offset[i] * offset2[i];
	}
	return sum;
	
}


int main() {
	int size = 1000;
	offset = calloc(sizeof(int), size+1);
	offset2 = calloc(sizeof(int), size+1);
	double res = loopTest(size); // sp should be close to 100
	printf("res = %.2f\n", res);
	return 0;
}
