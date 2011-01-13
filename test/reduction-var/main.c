#include <stdio.h>

int sumArray(int* a, int num_elems) {
	int i;
	int sum = 0;
	for (i = 0; i < num_elems; i++) {
		sum += a[i];	
	}
	return sum;
}

int main() {

#ifdef PYRPROF
	turnOnProfiler();
#endif /* PYRPROF */

	int a[3];

	int i;
	for(i = 0; i < 3; ++i) {
		printf("enter an integer for a[%d]: ",i);
		scanf("%d",&a[i]);
	}

	int sum = sumArray(a,3);
	printf("sum of entered values: %d\n", sum);

#ifdef PYRPROF
	turnOffProfiler();
#endif /* PYRPROF */

	return 0;
}
