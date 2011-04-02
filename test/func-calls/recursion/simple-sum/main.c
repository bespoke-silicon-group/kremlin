#include <stdio.h>

/**
 * Sums n + (n - 1) + ... + 1 + 0
 */
int sumToZero(int n)
{
    return n > 0 ? n + sumToZero(n - 1) : 0;
}

int main()
{
    int n = 100;
    printf("%d + %d + ... + 1 + 0 = %d\n", n, n - 1, sumToZero(n));

    return 0;
}
