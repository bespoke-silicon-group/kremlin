#include <stdlib.h>
#include <stdio.h>

int main() {
	int array[2];
	array[0] = rand();
	array[1] = array[0] + rand();

	printf("array[1]: %d\n",array[1]);

	return 0;
}
