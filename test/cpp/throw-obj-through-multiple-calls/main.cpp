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

void willThrow9()
{
	ThrowingObj o;
    o.willThrow();
}
void willThrow8()
{
    printf("Called willThrow8\n");
    willThrow9();
}
void willThrow7()
{
    printf("Called willThrow7\n");
    willThrow8();
}
void willThrow6()
{
    printf("Called willThrow6\n");
    willThrow7();
}
void willThrow5()
{
    printf("Called willThrow5\n");
    willThrow6();
}
void willThrow4()
{
    printf("Called willThrow4\n");
    willThrow5();
}
void willThrow3()
{
    printf("Called willThrow3\n");
    willThrow4();
}
void willThrow2()
{
    printf("Called willThrow2\n");
    willThrow3();
}
void willThrow1()
{
    printf("Called willThrow1\n");
    willThrow2();
}
void willThrow0()
{
    printf("Called willThrow0\n");
    willThrow1();
}

int main(int argc, char* argv[])
{
	try
	{
        willThrow0();
	}
	catch(const std::string& msg) 
	{
		fprintf(stderr, "main caught: %s\n", msg.c_str());
	}
	return 0;
}
