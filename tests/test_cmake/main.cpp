#include <seq/cvector.hpp>
#include <iostream>

int main(int argc, char**)
{
	seq::cvector<int> vec;
	for (int i = 0; i < argc; ++i)
		vec.push_back(argc);
	std::cout << vec.size() << std::endl;
	return 0;
}