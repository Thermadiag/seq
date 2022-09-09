

#include <cstdlib>
#include <cstdio>
#include <sstream>
#include "charconv.hpp"
#include "utils.hpp"
#include "testing.hpp"


template<class T>
int __float_to_string_seq(T val, seq::chars_format fmt, int prec, char *dst, char * end)
{
	int size = (int)(seq::to_chars(dst, end, val, fmt, prec).ptr - dst);
	dst[size] = 0;
	return size;
}
template<class T>
int __float_to_string_printf(T val, seq::chars_format fmt, int prec, char* dst, char* end)
{
	std::ostringstream oss;
	if (fmt == seq::general) {
		if(std::is_same<T,long double>::value)
			oss <<"%." << prec << "Lg";
		else
			oss << "%." << prec << "g" ;
	}
	else if (fmt == seq::scientific) {
		if (std::is_same<T, long double>::value)
			oss << "%." << prec << "Le" ;
		else
			oss << "%." << prec << "e" ;
	}
	else {
		if (std::is_same<T, long double>::value)
			oss << "%." << prec<< "Lf" ;
		else
			oss << "%." << prec<< "f";
	}

	snprintf(dst,end-dst, oss.str().c_str(), val);
	return (int)strlen(dst);
}

template<class T>
bool __test_read_val(const char* src)
{
	std::istringstream iss(src);
	T v;
	iss >> v;
	if (!iss) {
		if (strcmp(src, "inf") == 0) return true;
		if (strcmp(src, "-inf") == 0) return true;
		if (strcmp(src, "nan") == 0) return true;
		return false;
	}
	return iss.peek() == EOF;
}

template<class T>
int exponent(T v)
{
	T exp = std::log10(std::abs(v));
	exp = std::floor(exp);
	return (int)exp;
}

template<class T>
bool __test_equal(const char* s1, const char* s2, seq::chars_format fmt, int prec)
{	
	int l1 = (int)strlen(s1);
	//int l2 = (int)strlen(s2);

	if (strcmp(s1, s2) == 0)
		return true;

	//check for inf and nan
	T v1;
	if (from_chars(s1, s1 + l1, v1).ec != std::errc())
		return false;
	T v2;
	{
		std::istringstream iss(s2);
		iss >> v2;
	}

	T saved1 = v1;
	T saved2 = v2;

	//equal values: no need to go further
	if (v1 == v2)
		return true;
	if (std::isnan(v1) && std::isnan(v2))
		return true;


	int exp1 = exponent(v1);
	int exp2 = exponent(v2);
	if (fmt == seq::fixed) {
		// for fixed specifier, harder to compare, for now just check the exponents
		return exp1 == exp2;
	}
	v1 *= (T)std::pow((T)10, (T)-exp1);
	v2 *= (T)std::pow((T)10, (T)-exp1);

	int p = prec;
	if (p > 14) p = 14;
	if (std::is_same<T, float>::value && p > 6)
		p = 6;
	if (p > 0) --p;
	T error = (T)std::pow((T)10., (T)-p) *4;
	T diff = std::abs(v1 - v2);
	if (diff <= error)
		return true;

	std::cout << std::setprecision(prec + 6) << "read vals: " << saved1 << " and " << saved2 << std::endl;
	std::cout << std::setprecision(prec + 6) << "normalized: " << v1 << " and " << v2 << std::endl;

	std::cout << std::setprecision(prec + 6) << "diff is " << diff << " and max error is " << error << std::endl;
	return false;

	/*int p = prec;
	if (p > 15) p = 15;
	if (std::is_same<T, float>::value && p > 6)
		p = 6;
	if (p > 0) --p;
	T error = (T)std::pow((T)10., (T)-p);
	T pow = std::log10(std::abs(v1));
	T mul = std::pow((T)10., pow < 0 ? std::ceil(pow) : std::floor(pow));
	error *= mul*4;

	T diff = std::abs(v1 - v2);
	if (diff <= error)
		return true;

	std::cout << std::setprecision(prec+6)<< "diff is " << diff << " and max error is " << error << std::endl;
	return false;*/
}





template<class T>
void test_to_chars(int count, seq::chars_format fmt, int p)
{
	std::cout << "test charconv for " << count << " random " << typeid(T).name() << " with precision " << p << " and type " << (fmt == seq::scientific ? "scientific" : (fmt == seq::fixed ? "fixed" : "general")) << std::endl;

	random_float_genertor<T> rgn;
	std::vector<T> vals;
	for (int i = 0; i < count; ++i)
		vals.push_back(rgn());

	char dst1[1000];
	char dst2[1000];
	
	for (int i = 0; i < count; ++i)
	{
		

		__float_to_string_seq(vals[i], fmt, p, dst1, dst1 + sizeof(dst1));
		__float_to_string_printf(vals[i], fmt, p, dst2, dst2 + sizeof(dst2));

		SEQ_TEST(__test_read_val<T>(dst1));
		SEQ_TEST(__test_read_val<T>(dst2));

		try {
			
			SEQ_TEST(__test_equal<T>(dst1, dst2,fmt,p));
		 }
		catch (...)
		{
			//SEQ_TEST(__test_equal<T>(dst1, dst2,fmt, p));
			std::cout << "error while parsing " << dst1 << " (seq) and " << dst2 << " (printf) for value " << std::setprecision(p+6) << vals[i]<<  std::endl;
			throw;
		}
		
	}
}


void test_charconv(int count = 100000, int max_precision = 50)
{

	for (int p = 0; p < max_precision; ++p)
		test_to_chars<float>(count, seq::general, p);
	for (int p = 0; p < max_precision; ++p)
		test_to_chars<float>(count, seq::scientific, p);


	for (int p = 0; p < max_precision; ++p)
		test_to_chars<double>(count, seq::general, p);
	for (int p = 0; p < max_precision; ++p)
		test_to_chars<double>(count, seq::scientific, p);

	for (int p = 0; p < max_precision; ++p)
		test_to_chars<long double>(count, seq::general, p);
	for (int p = 0; p < max_precision; ++p)
		test_to_chars<long double>(count, seq::scientific, p);

}
