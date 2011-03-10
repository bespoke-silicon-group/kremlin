#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    int* p = (int*)malloc(sizeof(int));

    printf("Uninitialized: %d\n", *p);

    free(p);

    return 0;
}
