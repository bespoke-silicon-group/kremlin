#include <stdio.h>
#include "add.h"

int main(int argc, char* argv[])
{
	int otherNum = 5;
	printf("%d + %d = %d\n", argc, otherNum, add(argc, otherNum));
	return 0;
}
