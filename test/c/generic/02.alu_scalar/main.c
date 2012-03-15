#include <stdlib.h>
#include <stdio.h>

int main() {
	int x = rand();
	x += rand();
	x -= rand();
	x = x * rand();
	x = x / rand();
	x = x % rand();
	x = ~x;
	x = x | rand();
	x = x & rand();
	x = x ^ rand();
	x = x >> (rand() % 10);
	x = x << (rand() % 10);

	printf("Result: %d\n",x);

	return 0;
}
