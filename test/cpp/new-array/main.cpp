#include <cstdio>

#define MAX 10

class Obj
{
	int x;

	public:
	Obj() throw() : x(0) {}

	int getX() throw () { return x; }
	void setX(const int x) throw () { this->x = x; }
};

int main()
{
	Obj* o = new Obj[MAX];

	for(int i = 0; i < MAX; i++) {
		o[i].setX(i);
	}

	for(int i = 0; i < MAX; i++)
		printf("Hello front obj %d\n", o[i].getX());

	delete[] o;
	return 0;
}
