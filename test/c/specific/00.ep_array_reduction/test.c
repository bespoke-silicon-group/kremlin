// This benchmark is based off one of the main loops in the ep benchmark
// (from NAS Parallel Bench).

#include <stdlib.h>
#include <math.h>

#define NK 10
#define NQ 10
#define S       271828183.0

#define       pow2(a) ((a)*(a))
#define max(a,b) (((a) > (b)) ? (a) : (b))

int main() {
	int i, k, kk, l;
	double t1, t2, t3, t4, x1, x2;
	double sx, sy;

	sx = sy = 0.0;

	double x[2*NK];
	double qq[NQ];
	
    for (i = 0; i < NQ; i++) qq[i] = 0.0;

	for (k = 1; k <= 100; k++) {
		for(i = 0; i < 2*NK; ++i) {
			x[i] = drand48();
		}

		/*
		   c       Compute Gaussian deviates by acceptance-rejection method and 
		   c       tally counts in concentric square annuli.  This loop is not 
		   c       vectorizable.
		 */
		for ( i = 0; i < NK; i++) {
			x1 = 2.0 * x[2*i] - 1.0;
			x2 = 2.0 * x[2*i+1] - 1.0;
			t1 = pow2(x1) + pow2(x2);
			if (t1 <= 1.0) {
				t2 = sqrt(-2.0 * log(t1) / t1);
				t3 = (x1 * t2);             /* Xi */
				t4 = (x2 * t2);             /* Yi */
				l = max(fabs(t3), fabs(t4));
				qq[l] += 1.0;               /* counts */
				sx = sx + t3;               /* sum of Xi */
				sy = sy + t4;               /* sum of Yi */
			}
		}
	}

	return 0;
}
