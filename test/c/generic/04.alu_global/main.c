#include <stdlib.h>
#include <stdio.h>

static int x;

int main() {
	x = rand();
	int z = x + rand();

	printf("Result: %d\n",z);

	return 0;
}
