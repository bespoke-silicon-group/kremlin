#include <stdio.h>

int multBy2(int x) {
	return x * 2;
}

int data[100];

int main() {

	int i;
	int* p;
	int sum;
	for (i=0; i<100; i++) {
		data[i] = i;
	}
	
#ifdef PYRPROF
	turnOnProfiler();
#endif /* PYRPROF */

	p = data;
	sum = 1000000;
	i = 0;
	for (i=0; i<100; i++) {
		if (*p++ == 50)
			goto finish;
		else
			sum += i;
	}
finish:
	printf("sum = %d\n", sum);

#ifdef PYRPROF
	turnOffProfiler();
#endif /* PYRPROF */

	return 0;
}
