#include <stdlib.h>
#include <stdio.h>

int main() {
	int array1[100];

	array1[0] = rand();

	// doacross with self-parallelism = 1
	int i;
	for(i = 1; i < 100; ++i) {
		array1[i] = array1[i-1] + i;
	}

	int array2[100];
	array2[0] = rand();
	array2[1] = rand();

	// doacross with self-parallelism = 2
	i = 2;
	while(i < 100) {
		array2[i] = array2[i-2] + i;
		i++;
	}

	printf("array1[99] = %d\narray2[99] = %d\n",array1[99],array2[99]);

	return 0;
}
