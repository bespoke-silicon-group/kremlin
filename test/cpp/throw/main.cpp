#include <cstdio>

using namespace std;

__attribute__ ((noinline)) int foo(int x) {
	if(x)
		throw x;
	return x;
}

int main(int argc, char* argv[])
{
	try 
	{
		fprintf(stderr,"Foo returning: %d\n", foo(argc));
	}
	catch(const int& err)
	{
		fprintf(stderr, "Caught: %d\n", err);
	}
	return 0;
}
