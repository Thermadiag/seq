#pragma once

#include <cstdlib>
#include <sstream>
#include <limits>
#include <random>

#include "tiny_string.hpp"
#include "charconv.hpp"
#include "format.hpp"
#include "utils.hpp"
#include "testing.hpp"


using namespace seq;


inline std::vector<std::int64_t> generate_random_integers(size_t count)
{
	// Seed with a real random value, if available
	std::random_device r1,r2,r3,r4;
	std::default_random_engine e1(r1()), e2(r2()), e3(r3()), e4(r4());
	// Choose a random mean between 1 and 6
	
	std::uniform_int_distribution<short> uniform_dist_very_small(std::numeric_limits<short>::min(), std::numeric_limits<short>::max());
	std::uniform_int_distribution<short> uniform_dist_small(std::numeric_limits<short>::min(), std::numeric_limits<short>::max());
	std::uniform_int_distribution<int> uniform_dist_medium(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
	std::uniform_int_distribution<std::int64_t> uniform_dist_big(std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max());
	std::vector<std::int64_t> vec(count);
	for (size_t i = 0; i < count; ++i) {
		std::int64_t val;
		if ((i & 3) == 0) val = uniform_dist_very_small(e1) & 255;
		else if ((i & 3) == 1) val = uniform_dist_small(e2);
		else if ((i & 3) == 2) val = uniform_dist_medium(e3);
		else  val = uniform_dist_big(e4);
		vec[i] = (val);
	}
	seq::random_shuffle(vec.begin(), vec.end());
	return vec;
}

template<class T>
std::vector<T> generate_random_float(size_t count)
{
	std::vector<T> res(count);

	// Half is the result of casting random integers to float
	std::vector<std::int64_t> tmp = generate_random_integers(count / 2);
	for (size_t i = 0; i < tmp.size(); ++i)
		res[i] = static_cast<T>(tmp[i]);

	// Remaining half
	random_float_genertor<T> rng;
	for (size_t i = count / 2; i < count; ++i)
	{
		res[i] = rng();

		// remove infinit values as std::istream is not able to read them
		while(std::isinf(res[i]))
			res[i] = rng();
	}

	seq::random_shuffle(res.begin(), res.end());
	return res;
}

template<class T, bool IsIntegral = std::is_integral<T>::value>
struct Generator
{
	using type = std::int64_t;
	static std::vector<std::int64_t> generate(size_t count)
	{
		return generate_random_integers(count);
	}
};
template<class T>
struct Generator<T,false>
{
	using type = T;
	static std::vector<T> generate(size_t count)
	{
		return generate_random_float<T>(count);
	}
};




/// @brief Compare reading numeric values using seq::from_stream, strto* and std::istream
template<class Type>
void test_read_numeric(size_t count)
{
	using type = typename Generator<Type>::type;
	auto values = Generator<Type>::generate(count);

	std::cout << std::endl;
	std::cout << "Test reading " << count << " values of type " << typeid(type).name() << std::endl;
	std::cout << std::endl;
	std::cout << fmt(fmt("Method").l(30), "|", fmt("Read").c(20), "|") << std::endl;
	std::cout << fmt(str().c(30).f('-'), "|", str().c(20).f('-'), "|") << std::endl;

	auto f = fmt(pos<0,2>(), fmt("Method").l(30), "|", fmt<size_t>().c(20), "|");

	

	type sum = 0;

	//generate big string of values
	std::ostringstream sout;
	for (int i = 0; i < count; ++i)
		sout << values[i] << " ";
	std::string str = sout.str();

	//test buffer_input_stream
	tick();
	sum = 0;
	seq::buffer_input_stream in(str);
	while (in) {
		type i;
		seq::from_stream(in, i);
		sum += i;
	}
	size_t b_i_s = tock_ms();
	print_null(sum);

	std::cout << f("seq::buffer_input_stream", b_i_s) << std::endl;

	//test std_input_stream
	tick();
	sum = 0;
	std::istringstream sin(str);
	seq::std_input_stream<> seqin(sin);
	while (seqin) {
		type i;
		seq::from_stream(seqin, i);
		sum += i;
	}
	size_t s_i_s = tock_ms();
	print_null(sum);
	
	std::cout << f("seq::std_input_stream", s_i_s) << std::endl;
	
	//test strto* familly
	tick();
	sum = 0;
	const char* pos = str.data();
	while (true) {
		char* end = (char*)str.data() + str.size();
		type i=0;
		if (std::is_integral<type>::value) 
			i = static_cast<type>(strtoll(pos, &end, 10));
		else if (std::is_same<long double, type>::value) {
			i = static_cast<type>(strtold(pos, &end));
		}
		else if (std::is_same<double, type>::value)
			i = static_cast<type>(strtod(pos, &end));
		else if (std::is_same<float, type>::value)
			i = static_cast<type>(strtof(pos, &end));
		if (end == NULL || end == pos)
			break;
		sum += i;
		pos = end;
	}
	size_t strto = tock_ms();
	print_null(sum);

	std::cout << f("strto* familly", strto) << std::endl;
	
	//test istringstream
	tick();
	sum = 0;
	std::istringstream istr(str);
	istr.sync_with_stdio(false);
	while (istr) {
		type i;
		istr >> i;
		sum += i;
	}
	size_t istream = tock_ms();
	print_null(sum);

	std::cout << f("std::istringstream", istream) << std::endl;
}




template<class T, seq::chars_format format, bool IsIntegral = std::is_integral<T>::value>
struct Writer
{
	static seq::to_chars_result write(char * first, char * last, const T& v, int )
	{
		return seq::to_chars(first, last, v);
	}
};
template<class T, seq::chars_format format>
struct Writer<T,format,false>
{
	static seq::to_chars_result write(char* first, char* last, const T& v, int precision)
	{
		return seq::to_chars(first, last, v, format, precision);
	}
};

/// @brief Compare writing numeric values using seq::to_chars, seq::fmt, printf and std::ostream
template<class Type, seq::chars_format format = seq::general>
void test_write_numeric(size_t count, int precision = 6)
{
	using type = typename Generator<Type>::type;
	auto values = Generator<Type>::generate(count);

	std::cout << std::endl;
	if(std::is_integral<Type>::value)
		std::cout << "Test writing " << count << " values of type " << typeid(type).name() << std::endl;
	else
		std::cout << "Test writing " << count << " values of type " << typeid(type).name() <<
		" with format " << (format == seq::general ? "'general'" : format == seq::scientific ? "'scientific'" : "'fixed'") << " and precision " << precision << std::endl;
	std::cout << std::endl;

	std::cout << fmt(fmt("Method").l(30), "|", fmt("Write").c(20), "|") << std::endl;
	std::cout << fmt(str().c(30).f('-'), "|", str().c(20).f('-'), "|") << std::endl;

	auto f = fmt(pos<0, 2>(), fmt("Method").l(30), "|", fmt<size_t>().c(20), "|");
	size_t sum = 0;
	char buff[5000];

	std::string pfmt;
	{
		std::ostringstream oss;
		if (std::is_integral<type>::value)
			oss << "%lld";
		else if (std::is_same<float, type>::value)
			oss << "%." << precision << (format == seq::general ? "g" : format == seq::scientific ? "e" : "f");
		else if (std::is_same<double, type>::value)
			oss << "%." << precision << (format == seq::general ? "g" : format == seq::scientific ? "e" : "f");
		else if (std::is_same<long double, type>::value)
			oss << "%." << precision << (format == seq::general ? "Lg" : format == seq::scientific ? "Le" : "Lf");
		pfmt = oss.str();
	}

	
	tick();
	sum = 0;
	for (size_t i = 0; i < count; ++i)
	{
		*Writer<type, format>::write(buff, buff + sizeof(buff), values[i], precision).ptr = 0;
		sum += buff[0];
	}
	size_t t_c = tock_ms();

	tick();
	sum = 0;
	for (size_t i = 0; i < count; ++i)
	{
		if (std::is_integral<type>::value)
			fmt(values[i]).to_chars(buff);
		else
			fmt(values[i]).t (format == seq::general ? 'g' : format == seq::scientific ? 'e' : 'f').p(precision).to_chars(buff);
		sum += buff[0];
	}
	size_t f_m = tock_ms();
	
	
	tick();
	sum = 0;
	for (size_t i = 0; i < count; ++i)
	{
		snprintf(buff,sizeof(buff), pfmt.c_str(), values[i]);
		sum += buff[0];
	}
	size_t prtf = tock_ms();

	
	tick();
	nullbuf n;
	std::ostream oss(&n);
	for (int i = 0; i < count; ++i)
	{
		oss << values[i];
	}
	size_t ostr = tock_ms();

	std::cout << f("seq::to_chars", t_c) << std::endl;
	std::cout << f("seq::fmt", f_m) << std::endl;
	std::cout << f("snprintf", prtf) << std::endl;
	std::cout << f("std::ostream", ostr) << std::endl;
	
}

