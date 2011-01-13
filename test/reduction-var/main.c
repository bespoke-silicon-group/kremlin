#include <stdio.h>

int sumZeroToX(int x) {
	int i;
	int sum = 0;
	for (i = 0; i < x; i++) {
		sum += i;	
	}
	return sum;
}

int main() {

#ifdef PYRPROF
	turnOnProfiler();
#endif /* PYRPROF */

	int x = sumZeroToX(3);
	int y = sumZeroToX(5);
	printf("%d\n", x + y);

#ifdef PYRPROF
	turnOffProfiler();
#endif /* PYRPROF */

	return 0;
}
