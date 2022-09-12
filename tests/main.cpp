
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


auto  main  (int  /*unused*/, char**  /*unused*/) -> int
{
	flat_map<int, std::string, std::less<void>> ff;
	ff.emplace( 2, std::string("toto"));
	ff.emplace( 3, "ok");
	for (auto i = ff.begin(); i != ff.end(); ++i)
		std::cout << i->first << " " << i->second << std::endl;
	ff.find(3);
	
	SEQ_TEST_MODULE(format, test_format());
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
	std::cout << "FINISHED TESTS SUCCESSFULLY" << std::endl;
	return 0;
}
