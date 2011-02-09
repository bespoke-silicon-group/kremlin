#include <cstdio>

int foo()
{
	printf("Throwing 5\n");
	throw 5;
}
int main()
{
	try
	{
		for(int i = 0; i < 10; i++)
			foo();
	}
	catch(const int& err)
	{
		printf("Caught %d\n", err);
	}
	return 0;
}
