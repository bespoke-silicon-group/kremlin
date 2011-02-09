#include <cstdio>

void throws();

int main()
{
	printf("main() - calling throws()\n");
	try
	{
		throws();
	}
	catch(const int& err)
	{
		printf("main() - caught %d\n", err);
	}
	return 0;
}
