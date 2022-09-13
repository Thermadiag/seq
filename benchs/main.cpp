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

 
#include "bench_hash.hpp"
#include "bench_map.hpp"
#include "bench_text_stream.hpp"
#include "bench_tiny_string.hpp"
#include "bench_tiered_vector.hpp"
#include "bench_sequence.hpp"

using namespace seq;

 

int  main  (int , char** )
{
	
	//test_sort_strings(2000000);
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
