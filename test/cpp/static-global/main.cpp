#include <iostream>

using namespace std;

class meow {
  private:
  	int x;

  public:
	meow() : x(0) { cerr << "constructing meow" << std::endl; }
	meow(int x) : x(x) { cerr << "constructing meow" << std::endl; }
	~meow() { cerr << "deconstructing meow" << std::endl; }
  	void setX(int a) { x = a; }
	int getX() { return x; }
};

static meow kitty;
int main() {

	kitty.setX(5);

	cerr << "kitty's x = " << kitty.getX() << endl;

	return 0;
}
