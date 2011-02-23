#include <stdio.h>
#include <stdlib.h>

int* pointer1;


int loopTest(int x) {
	int i, j;
	int sum = 0;
	pointer1 = malloc(x * sizeof(int));
	for (i=0; i<x; i++) {
		sum += pointer1[i];
	}

		
	
	return sum;
	
}


int main() {
	int res = loopTest(100); // sp should be close to 100
	printf("res = %d\n", res);
	return 0;
}
