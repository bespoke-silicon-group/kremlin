#include <iostream>
#include <cstdio>
#include <string>

using namespace std;

class ThrowingObj {
	public:
	ThrowingObj() throw() {}

	void willThrow() throw(std::string) {
		fprintf(stderr, "Throwing obj is about to throw!\n");
		throw std::string("Throwing obj threw!");
	}
};

int main(int argc, char* argv[])
{
	ThrowingObj o;
	try
	{
		o.willThrow();
	}
	catch(const std::string& msg) 
	{
		fprintf(stderr, "main caught: %s\n", msg.c_str());
	}
	return 0;
}
