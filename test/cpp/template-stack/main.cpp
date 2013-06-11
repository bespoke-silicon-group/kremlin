#include <cstdio>

#ifndef NULL
#define NULL (void*)0
#endif

template <typename Element>
class Stack
{
	enum { DEFAULT_SIZE = 16 };
	typedef size_t SizeTy;

	Element* stack;
	SizeTy occupancy;

	public:
	Stack();
	~Stack();

	void push(const Element& e);
	Element& top();
	void pop();

	size_t size() const;
};

template <typename Element>
Stack<Element>::Stack() : stack(NULL), occupancy(0)
{
	stack = new Element[DEFAULT_SIZE];
}

template <typename Element>
Stack<Element>::~Stack()
{
	delete[] stack;
	stack = NULL;
}

template <typename Element>
void Stack<Element>::push(const Element& e)
{
	stack[occupancy++] = e;
}

template <typename Element>
Element& Stack<Element>::top()
{
	return stack[occupancy - 1];
}

template <typename Element>
void Stack<Element>::pop()
{
	occupancy--;
}

template <typename Element>
size_t Stack<Element>::size() const
{
	return occupancy;
}

int main()
{
	Stack<int> s;

	printf("Pushing %d\n", 0);
	s.push(0);

	printf("Pushing %d\n", 1);
	s.push(1);

	printf("Pushing %d\n", 2);
	s.push(2);

	printf("Pushing %d\n", 3);
	s.push(3);

	printf("Pushing %d\n", 4);
	s.push(4);

	printf("Pushing %d\n", 5);
	s.push(5);

	printf("Pushing %d\n", 6);
	s.push(6);

	printf("Pushing %d\n", 7);
	s.push(7);

	printf("Pushing %d\n", 8);
	s.push(8);

	printf("Pushing %d\n", 9);
	s.push(9);

	printf("Popping %d\n", s.top()); // 9
	s.pop();

	printf("Popping %d\n", s.top()); // 8
	s.pop();

	printf("Popping %d\n", s.top()); // 7
	s.pop();

	printf("Popping %d\n", s.top()); // 6
	s.pop();

	printf("Popping %d\n", s.top()); // 5
	s.pop();

	printf("Popping %d\n", s.top()); // 4
	s.pop();

	printf("Popping %d\n", s.top()); // 3
	s.pop();

	printf("Popping %d\n", s.top()); // 2 
	s.pop();

	printf("Popping %d\n", s.top()); // 1
	s.pop();

	printf("Popping %d\n", s.top()); // 0
	s.pop();
}
