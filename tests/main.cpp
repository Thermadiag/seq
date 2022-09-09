
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

using namespace seq;

template<class ...Args>
void test(Args&&... args)
{
	// Returns a formatting object (multi_ostream_format or ostream_format) for given input values
	using return_type = typename detail::BuildFormat< sizeof...(Args), void, Args...>::return_type;
	std::cout << typeid(return_type).name() << std::endl;
	//std::cout << typeid(std::tuple<Args...> ).name() << std::endl;
	return_type tt( std::forward<Args>(args)...);
}

template<class Args>
void print(Args&& args)
{
	std::cout << typeid(decltype(args)).name() << std::endl;
}

auto  main  (int  /*unused*/, char**  /*unused*/) -> int
{
	using namespace seq;

	std::cout << fmt(fmt("Hash table name").l(20), "|", fmt("Insert").c(20), "|", fmt("Insert(reserve)").c(20), "|", fmt("Find (success)").c(15), "|",
		fmt("Find (failed)").c(15), "|", fmt("Iterate").c(15), "|", fmt("Erase").c(20), "|", fmt("Find again").c(15), "|") << std::endl;
	std::cout << fmt(str().c(20).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|", str().c(15).f('-'), "|", str().c(15).f('-'), "|", str().c(15).f('-'), "|", str().c(20).f('-'), "|", str().c(15).f('-'), "|") << std::endl;

	auto f = fmt( pos<0,2,4,6,8,10,12,14>(), 
		fmt("").l(20), "|",  //name
		fmt(pos<0,2>(),fmt<int>(), " ms/", fmt<int>(), " MO").c(20), "|", //insert
		fmt(pos<0, 2>(), fmt<int>(), " ms/", fmt<int>(), " MO").c(20), "|", //insert(reserve)
		fmt(pos<0>(), fmt<int>(), " ms").c(15), "|", //find
		fmt(pos<0>(), fmt<int>(), " ms").c(15), "|", //find(fail)
		fmt(pos<0>(), fmt<int>(), " ms").c(15), "|", //iterate
		fmt(pos<0, 2>(), fmt<int>(), " ms/", fmt<int>(), " MO").c(20), "|",  //erase
		fmt(pos<0>(), fmt<int>(), " ms").c(15), "|"); //find again
	
	std::cout << f("test", fmt(1, 2), fmt(3, 4), 5, 6, 7, fmt(8,9), 10) << std::endl;
	//std::cout << fmt(fmt(1, 2).l(10));

	//test(3, fmt(1, 2));
	//test(3,fmt(1, 2).ll(10));
	//std::cout << l << std::endl;
	
	//auto tmp = fmt(l);
	//auto tmp = test( f1);
	//auto f2 = fmt("|", f1);
	//std::cout << f2 << std::endl;


	//auto f = fmt(fmt("").l(20), "|", fmt(fmt<int>(), " ms/", fmt<int>(), " MO").l(20), "|", fmt(fmt<int>(), " ms/", fmt<int>(), " MO").l(20), "|", fmt<int>().l(15), "|",
	//	fmt<int>().l(15), "|", fmt<int>().l(15), "|", fmt(fmt<int>(), " ms/", fmt<int>(), " MO").l(20), "|", fmt<int>().l(15));

	//std::cout << f("test", 1, 2, 3, 4, 5, 6, 7, 8, 9) << std::endl;

	/*SEQ_TEST_MODULE(format, test_format());
	SEQ_TEST_MODULE(any, test_any());
	SEQ_TEST_MODULE(tiered_vector< seq::OptimizeForMemory>, test_tiered_vector<size_t, seq::OptimizeForMemory>(500000));
	SEQ_TEST_MODULE(tiered_vector< seq::OptimizeForSpeed>, test_tiered_vector<size_t, seq::OptimizeForSpeed>(500000));
	SEQ_TEST_MODULE(sequence<OptimizeForMemory>, test_sequence<size_t,seq::OptimizeForMemory>(1000000));
	SEQ_TEST_MODULE(sequence<OptimizeForSpeed>, test_sequence<size_t, seq::OptimizeForSpeed>(1000000));
	SEQ_TEST_MODULE(devector<OptimizeForBothEnds>, test_devector_logic<size_t, seq::OptimizeForBothEnds>());
	SEQ_TEST_MODULE(devector<OptimizeForPushBack>, test_devector_logic<size_t, seq::OptimizeForPushBack>());
	SEQ_TEST_MODULE(devector<OptimizeForPushFront>, test_devector_logic<size_t, seq::OptimizeForPushFront>());
	SEQ_TEST_MODULE(flat_map,test_flat_map_logic());
	SEQ_TEST_MODULE(flat_multimap,test_flat_multimap_logic());
	SEQ_TEST_MODULE(flat_set,test_flat_set_logic());
	SEQ_TEST_MODULE(flat_multiset,test_flat_multiset_logic());
	SEQ_TEST_MODULE(ordered_map,test_ordered_map_logic());
	SEQ_TEST_MODULE(ordered_set,test_ordered_set_logic());
	SEQ_TEST_MODULE(tiny_string,test_tstring_logic());
	SEQ_TEST_MODULE(charconv,test_charconv(10000, 30));
	SEQ_TEST_MODULE(memory,test_object_pool(1000000));
	std::cout << "FINISHED TESTS SUCCESSFULLY" << std::endl;*/
	return 0;
}
