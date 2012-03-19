#include <stdlib.h>
#include <stdio.h>

static int global_sum;

int main() {
	int array_1d[10];
	int array_2d[10][10];

	int i,j;
	for(i = 0; i < 10; ++i) {
		array_1d[i] = rand();

		for(j = 0; j < 10; ++j) {
			array_2d[i][j] = rand();
		}
	}

	// sum_1d is reduction var over 1d array
	int sum_1d = 0;

	// sum_2d is reduction var over 2d array
	int sum_2d = 0;

	// Both these loops should be DOALL if reduction variables are working
	// correctly
	for(i = 0; i < 10; ++i) {
		sum_1d += array_1d[i];

		for(j = 0; j < 10; ++j) {
			sum_2d += array_2d[i][j];
		}
	}

	// diff_1d should not be a reduction var (sub isn't commutative)
	// Therefore, this loop should be DOACROSS if things are working
	// correctly.
	int diff_1d = 0;
	for(i = 0; i < 10; ++i) {
		diff_1d -= array_1d[i];
	}

	// global_sum is a global reduction variable over 1d array
	global_sum = 0;
	for(i = 0; i < 10; i=i+2) {
		global_sum += (array_1d[i] - array_1d[i-1]);
	}

	return 0;
}
