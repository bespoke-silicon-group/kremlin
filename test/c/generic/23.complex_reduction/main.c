#include <stdlib.h>
#include <stdio.h>

/**
 * Test for a reduction variable used in a conditional within a nested loop.
 *
 * @note Test was inspired by ljForce function in CoMD (ExMatEx benchmark)
 */

int main() {
	int sum = 3; // sum is a reduction variable

	int i, j;
	// Both loops should be DOALL
	for (i = 0; i < 10; i++) {
		for (j = 0; j < 10; j++) {
			if (i > j)
				sum += 1;
			else
				sum += 9;
		}
	}

	printf("sum = %d\n", sum);

	int blah = 0; // NOT a reduction variable

	// Both loops should be DOALL
	for (i = 0; i < 10; i++) {
		for (j = 0; j < 10; j++) {
			if (i > j)
				blah += 1;
			else
				blah *= 9;
		}
	}

	printf("blah = %d\n", blah);

	return 0;
}
