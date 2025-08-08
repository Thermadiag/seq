#include <random>
#include <vector>

#include <seq/algorithm.hpp>
#include <seq/testing.hpp>
#include <seq/tiny_string.hpp>
#include <seq/format.hpp>

#ifdef BOOST_FOUND
#include <boost/sort/spinsort/spinsort.hpp>
#endif

#include "pdqsort.hpp"

template<class T, class Cmp>
void indisort(T start, size_t size, Cmp c)
{
	using value_type = typename std::iterator_traits<T>::value_type;

	std::vector<uintptr_t> buffer(size * 3u);
	uintptr_t* sort_array = buffer.data();
	uintptr_t* indexes = buffer.data() + size;
	value_type** ptrs = (value_type**)(buffer.data() + size * 2);
	size_t index = 0;
	for (auto it = start; index != size; ++it, ++index) {

		sort_array[index] = (index);
		indexes[index] = (index);
		ptrs[index] = (&(*it));
	}
	// pdqsort(sort_array, sort_array + size, [&ptrs,c](size_t l, size_t r) { return c(*ptrs[l] , *ptrs[r]); });
	auto le = [&ptrs, c](size_t l, size_t r) noexcept { return c(*ptrs[l], *ptrs[r]); };
	seq::net_sort(sort_array, sort_array + size, le);

	index = 0;

	for (auto current_index = sort_array; current_index != sort_array + size; ++current_index, ++index) {

		size_t idx = indexes[*current_index];
		while (idx != indexes[idx])
			idx = indexes[idx];
		value_type tmp = std::move(*ptrs[index]);
		*ptrs[index] = std::move(*ptrs[idx]);
		*ptrs[idx] = std::move(tmp);

		indexes[index] = idx;
	}
}




template<class Arithmetic>
std::vector<Arithmetic> generate_random_numbers(size_t count, Arithmetic max = std::numeric_limits<Arithmetic>::max())
{
	std::mt19937 rng(0);
	std::vector<Arithmetic> vec(count);
	if constexpr(std::is_integral_v<Arithmetic>) {
		std::uniform_int_distribution<Arithmetic> dist(0, max);
		for (size_t i = 0; i < vec.size(); ++i) 
			vec[i] = (dist(rng));
	}
	else {
		std::uniform_real_distribution<Arithmetic> dist(0, max);
		for (size_t i = 0; i < vec.size(); ++i)
			vec[i] = (dist(rng));
	}
	return vec;
}

template<class String>
std::vector<String> generate_random_strings(size_t count, size_t max_size)
{
	std::vector<String> res(count);
	for (String& s : res)
		s = seq::generate_random_string<String>((int)max_size, true);
	return res;
}

template<class Type>
std::vector<Type> generate_random(size_t count, size_t max_size_or_val)
{
	if constexpr (std::is_arithmetic_v<Type>)
		return generate_random_numbers<Type>(count, max_size_or_val);
	else
		return generate_random_strings<Type>(count, max_size_or_val);
}


template<class Type>
std::vector<Type> generate_waves(size_t count, size_t max_wave_len, size_t max_val)
{
	std::mt19937 rng(0);
	std::uniform_int_distribution<size_t> dist(0, max_wave_len);

	std::vector<Type> res;
	while (res.size() < count) {
		std::vector<Type> tmp;
		size_t size = dist(rng);
		tmp = generate_random<Type>(size, max_val);

		// Sort and potentially reverse
		std::sort(tmp.begin(), tmp.end());
		if (dist(rng) & 1)
			std::reverse(tmp.begin(), tmp.end());

		res.insert(res.end(), tmp.begin(), tmp.end());
	}

	res.resize(count);
	return res;
}


enum Method
{
	StdSort,
	StdStableSort,
	Pdqsort,
	BoostSpinSort,
	NetSort,
	NetSortTiny
};

template<class Vec, class Cmp>
bool sort(Vec& v, Cmp c, Method m)
{
	switch (m) {
		case StdSort:
			std::sort(v.begin(), v.end(),c);
			break;
		case StdStableSort:
			std::stable_sort(v.begin(), v.end(),c);
			break;
		case Pdqsort:
			pdqsort(v.begin(), v.end(),c);
			break;
#ifdef BOOST_FOUND
		case BoostSpinSort:
			boost::sort::spinsort(v.begin(), v.end(),c);
			break;
#endif
		case NetSort:
			seq::net_sort(v.begin(), v.end(), c);
			break;
		case NetSortTiny:
			seq::net_sort(v.begin(), v.end(), c,seq::tiny_buffer);
			break;
	}
	return std::is_sorted(v.begin(), v.end());
}


template<class Vec, class F>
void test_pattern(const Vec & v , const char * name, F f)
{
	using namespace seq;

	auto h = join("|", _str().l(20), _str().c(20), _fmt("is_sorted").c(20));
	std::cout << std::endl;
	std::cout << h(name, "time", null) << std::endl;
	std::cout << h(seq::fill('-'), seq::fill('-'), seq::fill('-')) << std::endl;

	auto vec = v;
	size_t el;
	bool sorted;
	
	tick();
	sorted = sort(vec, std::less<>{}, StdSort);
	el = tock_ms();
	std::cout << f("std::sort", el, sorted) << std::endl;
	

	vec = v;
	tick();
	sorted = sort(vec, std::less<>{}, StdStableSort);
	el = tock_ms();
	std::cout << f("std::stable_sort", el, sorted) << std::endl;

	vec = v;
	tick();
	sorted = sort(vec, std::less<>{}, Pdqsort);
	el = tock_ms();
	std::cout << f("pdqsort", el, sorted) << std::endl;

#ifdef BOOST_FOUND
	vec = v;
	tick();
	sorted = sort(vec, std::less<>{}, BoostSpinSort);
	el = tock_ms();
	std::cout << f("boost::spinsort", el, sorted) << std::endl;
#endif

	vec = v;
	tick();
	sorted = sort(vec, std::less<>{}, NetSort);
	el = tock_ms();
	std::cout << f("seq::net_sort", el, sorted) << std::endl;

	vec = v;
	tick();
	sorted = sort(vec, std::less<>{}, NetSortTiny);
	el = tock_ms();
	std::cout << f("seq::net_sort_tiny", el, sorted) << std::endl;
}

template<class Type>
void test_patterns_for_type(size_t count, size_t max_val)
{
	auto f = seq::join("|", seq::_str().l(20), seq::_fmt(seq::_u(), " ms").c(20), seq::_fmt<bool>().c(20));
	

	auto v = generate_random<Type>(count, max_val);
	std::sort(v.begin(), v.end());


	test_pattern(v,"sorted",  f);

	std::sort(v.begin(), v.end(), std::greater<>{});
	test_pattern(v, "reverse", f);


	test_pattern(generate_random<Type>(count, max_val),"random", f);


	test_pattern(generate_waves<Type>(count, 1000, max_val), "wave" ,f);
}

int bench_sort(int, char** const)
{
	std::cout << "Test uint64_t" << std::endl;
	test_patterns_for_type<uint64_t>(10000000, std::numeric_limits<int>::max());

	std::cout << std::endl;
	std::cout << "Test uint64_t % 100" << std::endl;
	test_patterns_for_type<uint64_t>(10000000, 100);

	std::cout << "Test double" << std::endl;
	test_patterns_for_type<double>(10000000, std::numeric_limits<double>::max());
	
	using string = std::string;

	std::cout << std::endl;
	std::cout << "Test string length 4" << std::endl;
	test_patterns_for_type<string>(1000000, 4);

	std::cout << std::endl;
	std::cout << "Test string length 15" << std::endl;
	test_patterns_for_type<string>(1000000, 15);

	std::cout << std::endl;
	std::cout << "Test string length 70" << std::endl;
	test_patterns_for_type<string>(1000000, 70);
	

	return 0;
}