#include <cstdio>

void throws();

int callThrows0();
int callThrows1();
int callThrows2();
int callThrows3();
int callThrows4();
int callThrows5();
int callThrows6();
int callThrows7();
int callThrows8();
int callThrows9();

int callThrows0() { printf("callThrows0() - calling callThrows1\n"); callThrows1(); return 0xdeadbeef; }
int callThrows1() { printf("callThrows1() - calling callThrows2\n"); callThrows2(); return 0xdeadbeef; }
int callThrows2() { printf("callThrows2() - calling callThrows3\n"); callThrows3(); return 0xdeadbeef; }
int callThrows3() { printf("callThrows3() - calling callThrows4\n"); callThrows4(); return 0xdeadbeef; }
int callThrows4() { printf("callThrows4() - calling callThrows5\n"); callThrows5(); return 0xdeadbeef; }
int callThrows5() { printf("callThrows5() - calling callThrows6\n"); callThrows6(); return 0xdeadbeef; }
int callThrows6() { printf("callThrows6() - calling callThrows7\n"); callThrows7(); return 0xdeadbeef; }
int callThrows7() { printf("callThrows7() - calling callThrows8\n"); callThrows8(); return 0xdeadbeef; }
int callThrows8() { printf("callThrows8() - calling callThrows9\n"); callThrows9(); return 0xdeadbeef; }
int callThrows9() { printf("callThrows9() - calling throws\n");      throws(); return 0xdeadbeef; }

void throw_if_not_zero(int x)
{
	if(x)
		throw "threw";
}
int main()
{
	printf("main() - calling callThrows0()\n");
	try
	{
		throw_if_not_zero(0);
		callThrows0();
	}
	catch(const int& err)
	{
		printf("main() - caught %d\n", err);
	}
	return 0;
}
