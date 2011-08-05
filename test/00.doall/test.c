#include <stdio.h>
#include <stdlib.h>

int* loopTest(int x, int count) __attribute__ ((noinline));
int getSum(int x, int count) __attribute__ ((noinline));

int* pointer;


int* loopTest(int x, int count) {
	int i;
	pointer = malloc(100 * sizeof(int));
	for (i=0; i<x; i++) {
		pointer[i] = i + count;
	}
	return pointer;
	
}

int getSum(int x, int y) {
	return x + y;
}


int main(int argc, char** argv) {
	int x = argc + 3;
	printf("argc = %d\n", argc);
	int ret = getSum(x, 3);
	ret += getSum(x, 5);
	ret += getSum(x, 8);
	int* ret2 = loopTest(10, x); // sp should be close to 100
	//loopTest(1000); // sp should be close to 1000
	printf("addr = 0x%x 0x%x\n", ret, ret2);
	return 0;
}
