#include <iostream>
#include <string>

using namespace std;

int foo(int x) {
	if(x)
		throw std::string("thrown!");
	return x;
}

int main(int argc, char* argv[])
{
	try 
	{
		cerr << "Foo returning: " << foo(argc) << endl;
	}
	catch(const std::string& err)
	{
		cerr << "Caught: " << err << endl;
	}
	return 0;
}
