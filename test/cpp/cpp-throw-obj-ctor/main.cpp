#include <iostream>
#include <cstdio>
#include <string>

using namespace std;

class ThrowingObj {
	public:
	ThrowingObj() { 
		fprintf(stderr, "Throwing obj is about to throw!\n");
		throw std::string("Throwing obj threw!");
	}
};

class Obj {
	ThrowingObj o;

	public:
	Obj()  
		try
			: o() 
		{
			{
				fprintf(stderr, "in Obj constructor\n");
			}
		} 
		catch(const std::string& msg) 
		{
			fprintf(stderr, "Obj() - %p - caught: %s..throwing again!\n", this, msg.c_str());
			throw;
		}
		catch(...) 
		{
			fprintf(stderr, "Obj() - %p - caught an exception..throwing again!\n", this);
			throw;
		}
};

int main(int argc, char* argv[])
{
	try
	{
		Obj o;
	}
	catch(const std::string& msg) 
	{
		fprintf(stderr, "main caught: %s\n", msg.c_str());
	}
	return 0;
}
