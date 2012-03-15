#include <stdlib.h>
#include <stdio.h>

int main() {
	int array1d[2] = {1,5};
	int array2d[2][2] = {{100,101},{200,201}};

	int i = rand() % 2;
	int* p1 = array1d;
	p1 += i;

	int j = rand() % 2;
	int x = *p1 + array2d[i][j];

	printf("array1d[%d] + array2d[%d][%d]: %d\n",i,i,j,x);

	return 0;
}
