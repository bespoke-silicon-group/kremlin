#include <stdlib.h>
#include <stdio.h>

int main() {
	long long int array1[10][10];
	long long int array2[10][20];

	// directly nested loop
	int i,j;
	for(i = 0; i < 10; ++i) {
		for(j = 0; j < 10; ++j) {
			array1[i][j] = (long long)rand();
		}
	}

	// 2 directly nested loops
	for(i = 0; i < 10; ++i) {
		for(j = 0; j < 10; ++j) {
			array1[i][j] *= 2;
		}

		for(j = 0; j < 20; ++j) {
			int x = rand();
			printf("rand(): %d\n",x);
		}
	}

	return 0;
}
