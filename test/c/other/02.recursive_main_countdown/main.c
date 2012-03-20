#include <stdio.h>
#include <stdlib.h>

#define MAX 10

int main(int argc, char* argv[])
{
	printf("%d\n", argc);
	if(argc > 0)
		return main(argc - 1, argv);
	return 0;
}
