#include <cstdio>

using namespace std;

__attribute__ ((noinline)) int foo(int x) {
	if(x)
		throw x;
	return x;
}

int main(int argc, char* argv[])
{
	try {
		for (int i = foo(1); i < 10; ++i)
			fprintf(stderr,"ERROR: this shouldn't print!\n");
	}
	catch(const int& err) {
		fprintf(stderr, "Caught: %d\n",err);
	}

	return 0;
}
