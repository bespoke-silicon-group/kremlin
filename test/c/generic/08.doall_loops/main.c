#include <stdlib.h>
#include <stdio.h>

int main() {
	int array1[10];
	int array2[10];
	int result[10];

	// for loop should be DOALL
	int i;
	for(i = 0; i < 10; ++i) {
		array1[i] = rand();
		array2[i] = rand();
	}

	// This while loop should be DOALL
	// This should be a good test of induction variable dection.
	int j = 0;
	while(j < 10) {
		result[j] = array1[j] + array2[j];
		printf("result[%d] = %d\n",j,result[j]);
		j++;
	}

	return 0;
}
