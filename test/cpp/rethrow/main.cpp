#include <iostream>
#include <cstdio>

using namespace std;

void nop() 
{
	fprintf(stderr, "nop\n");
}

int throw_if(int x) {
	fprintf(stderr, "throw_if(x=%d) called\n", x);
	if(x)
	{
		fprintf(stderr, "throw_if - throwing: %d\n", x);
		throw x;
	}
	return x;
}

int rethrow_if(int x) {
	fprintf(stderr, "rethrow_if(x=%d) called\n", x);
	if(x)
	{
		try 
		{
			throw_if(x);
		}
		catch(const int& err)
		{
			fprintf(stderr, "rethrow_if - caught: %d..rethrowing\n", err);
			throw;
		}
	}
	return x;
}

int main(int argc, char* argv[])
{
	fprintf(stderr, "main(argc=%d,...) called\n", argc);
	try
	{
		rethrow_if(argc);
	}
	catch(const int& err)
	{
			fprintf(stderr, "main - caught: %d\n", err);
	}

	return 0;
}

