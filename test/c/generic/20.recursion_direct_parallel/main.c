#include <stdlib.h>
#include <stdio.h>

// serial recursive call
unsigned int fib(unsigned int x) {
	if(x < 2) return 1;
	else return fib(x-1)+fib(x-2);
}


int main() {
	unsigned int x = (unsigned int)(rand() % 16);
	unsigned int result = fib(x);

	printf("result = %u\n",result);

	return 0;
}
