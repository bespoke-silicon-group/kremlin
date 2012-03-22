#include <stdlib.h>
#include <stdio.h>

__attribute__ ((noinline)) int foo() {
	return rand() / rand();
}

__attribute__ ((noinline)) void bar() {
	int x = foo();
	int y = foo();

	printf("x = %d; y = %d\n",x,y);

	return;
}



int main() {
	bar();

	int array1[10];
	int array2[10];

	int i;
	for(i = 0; i < 10; ++i) {
		array1[i] = foo();
		array2[i] = foo();
	}

	int x = 0;
	for(i = 0; i < 10; ++i) {
		x += array1[i] - array2[i];
	}

	printf("x = %d\n",x);

	return 0;
}
