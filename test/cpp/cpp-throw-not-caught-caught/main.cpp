#include <cstdio>
#include <cassert>

int throws()
{
	fprintf(stderr, "throws() - throwing 5\n");
	throw 5;
}

int not_catching()
{
	try
	{
		throws();
	}
	catch(const char& err)
	{
		fprintf(stderr, "not_catching() - caught %c?!?\n", err);
		assert(0);
	}
}

int main()
{
	try
	{
		not_catching();
	}
	catch(const int& err)
	{
		fprintf(stderr, "main() - caught %d\n", err);
	}
	return 0;
}
