#include <stdio.h>

int nestFunc(int x) {
	return x*x + 100;
}

int sumZeroToX(int x) {
	int i;
	int sum = 0;
	for (i = 0; i < x; i++) {
		sum += nestFunc(i+1);
	}
	return sum;
}

int main() {

#ifdef PYRPROF
	turnOnProfiler();
#endif /* PYRPROF */

	int x = sumZeroToX(3);
	int y = sumZeroToX(5);
	int z = sumZeroToX(3);
	printf("%d\n", x + y + z);

#ifdef PYRPROF
	turnOffProfiler();
#endif /* PYRPROF */

	return 0;
}
