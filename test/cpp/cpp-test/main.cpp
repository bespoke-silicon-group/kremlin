#include <iostream>

using namespace std;

class meow {
  private:
  	int x;

  public:
  	void setX(int a) { x = a; }
	int getX() { return x; }
};

int main() {
	class meow kitty;

	kitty.setX(5);

	cerr << "kitty's x = " << kitty.getX() << endl;

	return 0;
}
