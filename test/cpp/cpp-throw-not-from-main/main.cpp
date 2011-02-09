#include <cstdio>

void throws()
{
	fprintf(stderr, "throws() - throwing 5\n");
	throw 5;
}

void throw_if(int x)
{
	if(x)
	{
		fprintf(stderr, "throw_if() - throwing %d\n", x);
		throw x;
	}
	fprintf(stderr, "throw_if() - not throwing\n");
}

void will_catch()
{
	try
	{
		throws();
	}
	catch(const int& err)
	{
		fprintf(stderr, "will_catch() - caught %d\n", err);
	}
}

int main(int argc, char* argv[])
{
	try
	{
		throw_if(argc - 1); // doesn't throw
		will_catch();       // doesn't throw
	}
	catch(const int& err)
	{
		fprintf(stderr, "main() - caught %d\n", err);
	}
	return 0;
}
