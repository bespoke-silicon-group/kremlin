#include <stdio.h>
#include <stdlib.h>

#define MAX 10

int main(int argc, char* argv[])
{
    char next_arg[BUFSIZ];
    char* next_argv[MAX];
    int i;

    for(i = 0; i < argc && i < MAX; i++)
        next_argv[i] = argv[i];

    if(i < MAX)
    {
        snprintf(next_arg, BUFSIZ, "arg%d", i);
        next_argv[i] = next_arg;
        main(i + 1, next_argv);
    }
    for(i = 0; i < argc; i++)
        printf("%s ", argv[i]);
    printf("\n");
    return 0;
}
