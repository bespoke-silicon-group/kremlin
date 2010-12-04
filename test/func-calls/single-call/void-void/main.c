#include <stdio.h>

void foo()
{
	printf("foo called!\n");
}

int main()
{
	printf("main calling foo\n");
	foo();
	printf("main called foo\n");

	return 0;
}
