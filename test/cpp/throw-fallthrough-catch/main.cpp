#include <cstdio>

using namespace std;

__attribute__ ((noinline)) int foo(int x) {
	if(x < 0)
		throw 1;
	else if (x > 1000)
		throw 'a';

	return x+1;
}

int main(int argc, char* argv[])
{
	int q = 0;
	try 
	{
		q = foo(2000);
	}
	catch(const int& err) {
		fprintf(stderr,"Caught %d\n",err);
	}
	catch(const char& err) {
		fprintf(stderr,"Caught %c\n",err);
	}
	return 0;
}
