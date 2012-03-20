#include <stdlib.h>
#include <stdio.h>

int main() {
	int* array = malloc(2*sizeof(int));
	array[0] = rand();
	array[1] = rand();

	array = realloc(array,3*sizeof(int));
	array[2] = array[0] + array[1];

	printf("array[2]: %d\n",array[2]);

	free(array);

	return 0;
}
