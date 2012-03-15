#include <stdlib.h>
#include <stdio.h>

extern void f1();
extern void f2(int x);
extern void f3(int* x);
extern void f4(int x, int y);
extern int f5();

int main() {
	f1();

	int x = rand();
	f2(x);
	f2(3);

	f3(&x);

	int y = rand();
	f4(x,y);

	x += f5();

	printf("x = %d\n",x);

	return 0;
}
