/**
 * MIT License
 *
 * Copyright (c) 2022 Victor Moncada <vtr.moncada@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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

#ifdef TEST_CVECTOR
#include "test_cvector.hpp"
#endif


using namespace seq;


auto  main  (int  /*unused*/, char**  /*unused*/) -> int
{

	SEQ_TEST_MODULE(format, test_format());
	SEQ_TEST_MODULE(any, test_any());
#ifdef TEST_CVECTOR
	SEQ_TEST_MODULE(cvector, test_cvector<size_t>(50000));
#endif
	SEQ_TEST_MODULE(tiered_vector< seq::OptimizeForMemory>, test_tiered_vector<size_t, seq::OptimizeForMemory>(100000));
	SEQ_TEST_MODULE(tiered_vector< seq::OptimizeForSpeed>, test_tiered_vector<size_t, seq::OptimizeForSpeed>(100000));
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
