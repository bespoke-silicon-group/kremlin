#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h> // for vararg funcs

__attribute__ ((noinline)) int sumArgs(unsigned int num_args, ...) {
	va_list args;
	va_start(args, num_args);

	int x = 0;
	int i;
	for (i = 0; i < num_args; ++i) { 
		x += va_arg(args, int);
	}

	va_end(args);

	return x;
}


int main() {
	int r = rand();
	int x = sumArgs(5,1,2,3,4,r);

	printf("x = %d\n",x);

	return 0;
}
