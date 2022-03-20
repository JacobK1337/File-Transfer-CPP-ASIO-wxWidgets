
#include <iostream>
#include"include/request.h"
#include<filesystem>

struct Test
{
	int a;
	int b;
	std::string some_some_some;
	Test(int t_a, int t_b) : a(t_a), b(t_b){}
};

int main()
{
	Test first{ 1, 5 };
	first.some_some_some = "abc";

	std::cout << "Before moving: " << first.a << " " << first.b << first.some_some_some << "\n";

	Test second = std::move(first);

	std::cout << "After moving: " << first.a << " " << first.b << first.some_some_some << "\n";
	
}


