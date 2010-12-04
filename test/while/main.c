#include <stdio.h>

#define COUNT 100
#define WHILE_END 50
int data[COUNT];

// Sums the numbers 0 through WHILE_END
int main() {

	int i;
	int* p;
	int sum;
	for (i=0; i<COUNT; i++) {
		data[i] = i;
	}
	
#ifdef PYRPROF
	turnOnProfiler();
#endif /* PYRPROF */

	p = data;
	sum = 0;

	while (*p != WHILE_END) {
		sum += *p;
		p++;
	}

	printf("%d\n", sum);

#ifdef PYRPROF
	turnOffProfiler();
#endif /* PYRPROF */

	return 0;
}
