#include <stdio.h>
#include <stdlib.h>

int add(int x) {
	if (x == 1)
		return 1;

	return 2 * add(x-1) + 1;
}

int main(int argc, char** argv) {
	printf("add(5) = %d\n", add(5));
	printf("add(10) = %d\n", add(10));
	//printf("add(40) = %d\n", add(40));
	//printf("add(256) = %d\n", add(256));
	return 0;
}
