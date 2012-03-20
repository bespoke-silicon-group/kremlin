#include <stdlib.h>
#include <stdio.h>

int main() {
	int array[10][10];

	// DOALL
	int i,j;
	// fill in first row of array
	for(i = 0; i < 10; ++i) {
		array[i][0] = rand();
	}

	// DOALL
	// fill in first col of array
	for(i = 1; i < 10; ++i) {
		array[0][i] = rand();
	}

	// nested DOACROSS loops
	for(i = 1; i < 10; ++i) {
		for(j = 1; j < 10; ++j) {
			array[i][j] = (array[i][j-1] + array[i-1][j-1] + array[i-1][j])/3;
		}
	}

	return 0;
}
