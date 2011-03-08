#include "MergeSort.h"
#include <stdio.h>
#include <stdlib.h>

#define MIN_ARGS 1
#define ARG_COUNT 1

#ifndef SEED
#define SEED time(NULL)
#endif /* SEED */

int main(int argc, char* argv[])
{
	int i;

	// Print the usage message
	printf("usage: %s <count>\n\nwhere count is the number of elements to sort and print\n", argv[0]);

	// The number of elements to sort
	int count = 10;
	if(argc > MIN_ARGS)
		count = atoi(argv[ARG_COUNT]);

	// Allocate the arrays.
	int* array = (int*)malloc(sizeof(int) * count);
	int* temp_array = (int*)malloc(sizeof(int) * count);

	// Set the random seed.
	srand(SEED);

	// Initialize to random numbers
	for(i = 0; i < count; i++)
		array[i] = rand();

	// Sort the array.
	merge_sort(array, temp_array, 0, count - 1);

	// Print the sorted data
	for(i = 0; i < count; i++)
		printf("%d\n", array[i]);

	// Free the arrays
	free(array);
	free(temp_array);

	return 0;
}
