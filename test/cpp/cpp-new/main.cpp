#include <cstdio>

class Obj
{
	int x;

	public:
	Obj() throw() : x(0) {}

	int getX() throw () { return x; }
};

int main()
{
	Obj* o = new Obj();
	printf("Hello %d\n", o->getX());
	delete o;
	return 0;
}
