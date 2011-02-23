#include <stdio.h>
#include <stdlib.h>

int* pointer1;


int* loopTest(int x) {
	int i, j;
	pointer1 = malloc(x * sizeof(int));
	for (i=0; i<x; i++) {
		double temp1, temp2, temp3;
		temp3 = x + 0.1;
		temp1 = (i+1.0) * (1.34 + i) / (i- 0.01);
		temp2 = (i+1.11) * (1.35 + i) / (i- 0.02);
		temp3 = (temp2 + temp1) * x * temp1 * temp2;
		pointer1[i] = pointer1[i] + sin(temp3);
	}
	return pointer1;
	
}


int main() {
	int* res = loopTest(100); // sp should be close to 100
	printf("res = %.2f\n", res[0]);
	return 0;
}
