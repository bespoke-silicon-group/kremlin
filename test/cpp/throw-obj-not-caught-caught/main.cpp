#include <iostream>
#include <cstdio>
#include <string>
#include <cassert>

using namespace std;

class ThrowingObj {
	public:
	ThrowingObj() throw() {}

	void willThrow() throw(std::string) {
		fprintf(stderr, "Throwing obj is about to throw!\n");
		throw std::string("Throwing obj threw!");
	}
};

void willThrow()
{
    ThrowingObj o;
    o.willThrow();
}

void willNotCatch()
{
    try
    {
        fprintf(stderr, "willNotCatch calling willThrow\n");
        willThrow();
        fprintf(stderr, "willNotCatch returned willThrow\n");
        assert(0 && "Should not happen");
    }
    catch(int i)
    {
        fprintf(stderr, "willNotCatch caught: %d\n", i);
        assert(0 && "Should not happen");
    }
}

int main(int argc, char* argv[])
{
	try
	{
        fprintf(stderr, "main calling willNotCatch\n");
        willNotCatch();
        fprintf(stderr, "main returned from willNotCatch\n");
        assert(0 && "Should not happen");
	}
	catch(const std::string& msg) 
	{
		fprintf(stderr, "main caught: %s\n", msg.c_str());
	}
	return 0;
}
