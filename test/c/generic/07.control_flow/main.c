#include <stdlib.h>
#include <stdio.h>

int main() {

	int x = 0;
	int r = rand();
	if(r > 1000) {
		x += rand();
	}

	if(r % 2 == 0) {
		x += 1;
	} else {
		x -= 1;
	}

	r = r % 16;

	if(r < 8) {
		if(r == 2) {
			x += 1;
		}
		else if(r == 4) {
			x += 3;
		}
		else {
			x += 7;
		}
	}

	x = (r == 9) ? x+1 : x+2;

	printf("x = %d\n",x);

	return 0;
}
