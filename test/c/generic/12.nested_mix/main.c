#include <stdlib.h>
#include <stdio.h>

int main() {
	int array[10][10];

	// DOALL
	int i,j;
	// fill in first row of array
	for(i = 0; i < 10; ++i) {
		array[0][i] = rand();
	}

	// outer loop is DOACROSS
	// inner loop is DOALL
	for(i = 1; i < 10; ++i) {
		for(j = 0; j < 10; ++j) {
			array[i][j] = array[i-1][j] / 2;
		}
	}

	// outer loop is DOALL
	// inner loop is DOACROSS
	for(i = 0; i < 10; ++i) {
		for(j = 1; j < 10; ++j) {
			array[i][j] -= array[i][j-1];
		}
	}

	return 0;
}
