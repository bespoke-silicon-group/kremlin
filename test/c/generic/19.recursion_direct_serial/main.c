#include <stdlib.h>
#include <stdio.h>

// serial recursive call
int foo(unsigned int x) {
	if(x < 1) return rand();
	else return foo(x-1)+1;
}


int main() {
	unsigned int x = (unsigned int)(rand() % 16);
	int result = foo(x);

	printf("result = %d\n",result);

	return 0;
}
