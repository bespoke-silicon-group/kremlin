#include <math.h>

int foo(int x) {
	return x % 2;
}

#define X_SIZE 4
#define Y_SIZE 200000

int r_array0[X_SIZE][Y_SIZE];
int r_array1[X_SIZE][Y_SIZE];
int w_array[X_SIZE][Y_SIZE];

double delay(int factor) {
	double sum;
	int i;
	for (i=0; i<factor; i++) {
		sum += sqrt((double)(i+1));
	}
	return sum;
}
/*
void bw_light() {
	int i, j;
	for (i=0; i<X_SIZE; i++) {
		for (j=0; j<Y_SIZE; j++) {
			w_array[i][j] = 10 * r_array[i][j] + 100;
			delay(100);
		}
	}

}*/

void bw_heavy(int write_arr[X_SIZE][Y_SIZE], int read_arr[X_SIZE][Y_SIZE]) {
	int i, j;
	for (i=0; i<X_SIZE; i++) {
		for (j=0; j<Y_SIZE; j++) {
			write_arr[i][j] = 10 * read_arr[i][j]  + 100;
		}
	}
}

int main() {
	//turnOnProfiler();
	//bw_heavy(w_array, r_array0);
	bw_heavy(r_array1, r_array0);
	//bw_light();
	//turnOffProfiler();

	return 0;
	
}
