#include <iostream>

using namespace std;

int foo(int x) {
	if(x)
		throw x;
	return x;
}

int main(int argc, char* argv[])
{
	try 
	{
		cerr << "Foo returning: " << foo(argc) << endl;
	}
	catch(const int& err)
	{
		cerr << "Caught: " << err << endl;
	}
	return 0;
}
