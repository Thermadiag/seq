
#include "bench_hash.hpp"
#include "bench_map.hpp"
#include "bench_text_stream.hpp"
#include "bench_tiny_string.hpp"
#include "bench_tiered_vector.hpp"
#include "bench_sequence.hpp"

using namespace seq;

 

int  main  (int , char** )
{

	test_sort_strings(2000000);
	test_tstring_members(20000000);
	test_tstring_operators<25>(5000000, 14);
	test_sequence_vs_colony<size_t>(5000000);
	
	test_tiered_vector_algorithms<size_t>(5000000);
	test_tiered_vector<size_t>();
	test_hash<double, std::hash<double> >(10000000, [](size_t i) { return (i * UINT64_C(0xc4ceb9fe1a85ec53)); });
	test_map<double>(1000000, [](size_t i) { return (i * UINT64_C(0xc4ceb9fe1a85ec53)); });
	
	test_write_numeric<std::int64_t>(1000000);
	test_write_numeric<float,seq::general>(1000000,12);
	test_write_numeric<double, seq::general>(1000000, 12);
	test_write_numeric<long double, seq::general>(1000000, 12);
	test_read_numeric<std::int64_t>(1000000);
	test_read_numeric<float>(1000000);
	test_read_numeric<double>(1000000);

	// last benchmark, as strtold is buggy on gcc
	test_read_numeric<long double>(1000000);

	return 0;

	
}
