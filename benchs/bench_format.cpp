#include <seq/testing.hpp>
#include <seq/format.hpp>
#include <vector>
#include <iostream>

#ifdef SEQ_HAS_CPP_20
#include <format>
#endif

int bench_format(int, char ** const)
{
	using namespace seq;

	// Generate 4M double values
	using ftype = double;
	random_float_genertor<ftype> rgn;
	std::vector<ftype> vec_d;
	for (int i = 0; i < 4000000; ++i)
		vec_d.push_back(rgn());

	// Null ostream object
	nullbuf n;
	std::ostream oss(&n);
	oss.sync_with_stdio(false);

	// Build a table of 4 * 1000000 double values separated by a '|'. All values are centered on a 20 characters space
	tick();
	oss << std::setprecision(6);
	for (size_t i = 0; i < vec_d.size() / 4; ++i)
	{
		oss << std::left << std::setw(20) << vec_d[i * 4] << "|";
		oss << std::left << std::setw(20) << vec_d[i * 4 + 1] << "|";
		oss << std::left << std::setw(20) << vec_d[i * 4 + 2] << "|";
		oss << std::left << std::setw(20) << vec_d[i * 4 + 3] << "|";
		oss << std::endl;
	}
	size_t el = tock_ms();
	std::cout << "Write table with streams: " << el << " ms" << std::endl;
	

	// Build the same table with format module

	// Create the format object
	auto slot = _g<ftype>().p(6).l(20); // floating point slot with a precision of 6 and left-aligned on a 20 characters width
	auto f = join("|",slot,  slot, slot,  slot, "");
	tick();
	for (size_t i = 0; i < vec_d.size() / 4; ++i)
		oss << f(vec_d[i * 4], vec_d[i * 4 + 1], vec_d[i * 4 + 2], vec_d[i * 4 + 3]) << std::endl;
	el = tock_ms();
	std::cout << "Write table with seq formatting module: " << el << " ms" << std::endl;

	
	// Compare to std::format for C++20 compilers
#ifdef SEQ_HAS_CPP_20
	tick();
	for (size_t i = 0; i < vec_d.size() / 4; ++i)
	 	std::format_to(
			std::ostreambuf_iterator<char>(oss), 
			"{:^20.6g} | {:^20.6g} | {:^20.6g} | {:^20.6g}\n", 
			vec_d[i * 4], vec_d[i * 4 + 1], vec_d[i * 4 + 2], vec_d[i * 4 + 3]);
	el = tock_ms();
	std::cout << "Write table with std::format : " << el << " ms" << std::endl;
#endif

	// Just for comparison, directly dump the double values without the '|' character (but keeping alignment)

	tick();
	auto f2 = g<ftype>().l(20);
	for (size_t i = 0; i < vec_d.size(); ++i)
		oss << f2(vec_d[i]);
	el = tock_ms();
	std::cout << "Write left-aligned double with seq::fmt: " << el << " ms" << std::endl;
	


	// use std::ostream::bad() to make sure the above tests are not simply ignored by the compiler
	if (oss.bad())
		std::cout << "error" << std::endl;


	return 0;
}