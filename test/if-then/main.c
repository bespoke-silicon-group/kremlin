int multBy2(int x) {
	return x * 2;
}

int main() {
	int x, y, z = 0;

	x = multBy2(2);
	y = multBy2(3);

	if(y > 10) {
		x = multBy2(5);
	}

	do {
		z = x+1;

		if(y > 5) {
			x = y*10;
		}
		else {
			x = y*2;
		}
	} while(z < 10);

	printf("result: %d\n", z);

	return 0;
}
