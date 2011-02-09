#include <stdio.h>

#define MAX_ELEMENTS 3

int sneaky_sum;

int sumArray(int* a, int num_elems) {
	int i;
	int sum = 0;
	for (i = 0; i < num_elems; i++) {
		sum += a[i];	
	}
	return sum;
}

int sneakyReducer(int x) {
	int i;
	sneaky_sum = 0;
	int tmp;
	for (i = 0; i < x; i++) {
		printf("enter an integer: ");
		scanf("%d",&tmp);
		sneaky_sum += tmp;	
	}
	return sneaky_sum;
}

int main() {

#ifdef PYRPROF
	turnOnProfiler();
#endif /* PYRPROF */

	int a[MAX_ELEMENTS];

	int i;
	for(i = 0; i < MAX_ELEMENTS; ++i) {
		printf("enter an integer for a[%d]: ",i);
		scanf("%d",&a[i]);
	}

	int sum = sumArray(a,3);
	printf("sum of entered values: %d\n", sum);

	int red = sneakyReducer(3);
	printf("sum of entered values: %d\n", red);

#ifdef PYRPROF
	turnOffProfiler();
#endif /* PYRPROF */

	return 0;
}
