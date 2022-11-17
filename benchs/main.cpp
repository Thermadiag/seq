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
#include "bench_mem_pool.hpp"




using namespace seq;

// Test structure for maps and hash maps
template<size_t N>
struct test
{
	size_t data[N];
	test() {}
	test(size_t v) { 
		data[0] = v;
		for (size_t i = 1; i < N; ++i)
			data[i] = v * (i + 1);
	}
	test(const test& other)
	{
		memcpy(data, other.data, sizeof(data));
	}
	operator size_t() const { return data[0]; }
};

template<size_t N>
bool operator < (const test<N>& a, const test<N>& b)
{
	return a.data[0] < b.data[0];
}
template<size_t N>
bool operator == (const test<N>& a, const test<N>& b)
{
	//return a.data[0] == b.data[0];
	return memcmp(a.data, b.data, sizeof(a.data)) == 0;
}

template<size_t N>
struct seq::is_relocatable<test<N>> : std::true_type {};

namespace std
{
	template<size_t N>
	struct hash<test<N> >
	{
		size_t operator()(const test<N>& t) const noexcept
		{
			return std::hash<size_t>{}(t.data[0]);
		}
	};
}

// Statefull allocator to test constructors of the form container(container &&, const Allocator & )
template<class T>
struct statefull_alloc : public std::allocator<T>
{
	using value_type = T;
	using pointer = T*;
	using const_pointer = const T*;
	using reference = T&;
	using const_reference = const T&;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using propagate_on_container_swap = typename std::allocator_traits<std::allocator<T> >::propagate_on_container_swap;
	using propagate_on_container_copy_assignment = typename std::allocator_traits<std::allocator<T> >::propagate_on_container_copy_assignment;
	using propagate_on_container_move_assignment = typename std::allocator_traits<std::allocator<T> >::propagate_on_container_move_assignment;
	template< class U > struct rebind { using other = statefull_alloc<U>; };

	auto select_on_container_copy_construction() const noexcept -> statefull_alloc { return *this; }

	int my_val;
	statefull_alloc() : my_val(rand()) {}
	statefull_alloc(const statefull_alloc& other) : my_val(other.my_val) {}
	template<class U>
	statefull_alloc(const statefull_alloc<U> & other) : my_val(other.my_val) {}

	bool operator==(const statefull_alloc& other) const { return my_val == other.my_val; }
	bool operator!=(const statefull_alloc& other) const { return my_val != other.my_val; }
};




int  main  (int , char** )
{ 
	// Build a formatting functor used to display 2 couples animal/species
	auto f = fmt(
		pos<1, 3>(), //we can modifies positions 1 and 3 (the 2 couples animal/species)
		"We have 2 couples:\nAnimal/Species: ",
		fmt(pos<0, 2>(), "", "/", "").c(20),	//A couple Animal/Species centered on a 20 characters width string
		"\nAnimal/Species: ",
		fmt(pos<0, 2>(), "", "/", "").c(20)		//Another couple Animal/Species centered on a 20 characters width string
	);

	// Use this functor with custom values.
	// fmt calls are also used to replace arguments in a multi-formatting functor
	std::cout << f(
		fmt("Tiger", "P. tigris"),
		fmt("Panda", "A. melanoleuca")
	) << std::endl;


	test_tiered_vector_algorithms<size_t>(5000000);
	test_tiered_vector<size_t>();


	test_tstring_members(20000000);
	test_sort_strings(2000000);
	test_tstring_operators<25>(5000000, 14);

	test_sequence_vs_colony<size_t>(5000000);
	
	
	test_map<double>(1000000, [](size_t i) { return (i * UINT64_C(0xc4ceb9fe1a85ec53)); });
	test_map<tiny_string<>>(1000000, [](size_t i) { return generate_random_string<tiny_string<>>(14, true); });


	test_hash<std::string, std::hash<tstring> >(5000000, [](size_t i) { return generate_random_string<std::string>(14, true); });
	test_hash<tstring, std::hash<tstring> >(5000000, [](size_t i) { return generate_random_string<tstring>(14, true); });
	test_hash<double, std::hash<double> >(10000000, [](size_t i) { return (i * UINT64_C(0xc4ceb9fe1a85ec53)); });

	test_object_pool(1000000);

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
