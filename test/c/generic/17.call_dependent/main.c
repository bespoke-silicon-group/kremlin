#include <stdlib.h>
#include <stdio.h>

__attribute__ ((noinline)) void foo(int* x) {
	*x += rand();
	return;
}

__attribute__ ((noinline)) void bar() {
	int x = 0;
	foo(&x);
	foo(&x);

	printf("x (bar) = %d\n",x);

	return;
}



int main() {
	bar();

	int array1[10];

	int i;
	int x = 0;
	for(i = 0; i < 10; ++i) {
		foo(&x);
		foo(&x);
	}

	printf("x (main) = %d\n",x);

	return 0;
}
