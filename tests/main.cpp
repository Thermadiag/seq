

#define SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS


#include "test_hash.hpp"
#include "test_any.hpp"
#include "test_charconv.hpp"
#include "test_devector.hpp"
#include "test_format.hpp"
#include "test_map.hpp"
#include "test_mem_pool.hpp"
#include "test_sequence.hpp"
#include "test_tiered_vector.hpp"
#include "test_tiny_string.hpp"


auto  main  (int  /*unused*/, char**  /*unused*/) -> int
{
	std::cout << "test any" << std::endl;
	test_any();
	std::cout << "test tiered_vector" << std::endl;
	test_tiered_vector<size_t>(1000000);
	std::cout << "test sequence" << std::endl;
	test_sequence<size_t>(1000000);
	std::cout << "test devector" << std::endl;
	test_devector_logic<size_t, seq::OptimizeForBothEnds>();
	test_devector_logic<size_t, seq::OptimizeForPushBack>();
	test_devector_logic<size_t, seq::OptimizeForPushFront>();
	std::cout << "test flat_map" << std::endl;
	test_flat_map_logic();
	std::cout << "test flat_multimap" << std::endl;
	test_flat_multimap_logic();
	std::cout << "test flat_set" << std::endl;
	test_flat_set_logic();
	std::cout << "test flat_multiset" << std::endl;
	test_flat_multiset_logic();
	std::cout << "test ordered_map" << std::endl;
	test_ordered_map_logic();
	std::cout << "test ordered_set" << std::endl;
	test_ordered_set_logic();
	std::cout << "test tiny_string" << std::endl;
	test_tstring_logic();
	std::cout << "test format" << std::endl;
	test_format();
	std::cout << "test charconv" << std::endl;
	test_charconv(10000, 30);
	std::cout << "test memory" << std::endl;
	test_object_pool(1000000);
	std::cout << "FINISHED TESTS SUCCESSFULLY" << std::endl;
	return 0;
	
/*
test_map<double >(1000000, [&](size_t i) {return  i; });
return 0;
*/
	/*for (size_t s = 10000000; s <= 10000000; s += 1000000)
	{
		test_hash<std::string, std::hash<tstring>>(s, [](size_t i) { return generate_random_string<std::string>(28, true); });
		test_hash<tiny_string<28>, std::hash<tstring>>(s, [](size_t i) { return generate_random_string<tiny_string<28>>(28, true); });
		
		//test_hash<double>(s, [](size_t i) { return (i  * UINT64_C(0xc4ceb9fe1a85ec53)); });
	}*/
	
}
