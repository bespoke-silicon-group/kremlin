#include <stdlib.h>
#include <stdio.h>

typedef struct st {
	int field1;
	int field2;
	int* field3;
} my_struct;


__attribute__ ((noinline)) int foo(my_struct x) {
	int result = x.field1 + x.field2;
	result = result & *(x.field3);
	return result;
}

int main() {
	my_struct x;
	
	x.field1 = rand();
	x.field2 = rand();

	int *p = malloc(sizeof(int));
	*p = rand();

	x.field3 = p;

	int y = foo(x);

	free(x.field3);

	printf("y = %d\n",y);

	return 0;
}
