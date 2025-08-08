

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <functional>
#include <type_traits>

#include "seq/any.hpp"
#include "seq/tiny_string.hpp"
#include "seq/hash.hpp"
#include "seq/testing.hpp"
#include "seq/format.hpp"
#include "seq/ordered_map.hpp"


 namespace seq
 {
	template<class R, class T, class... As>
	 SEQ_ALWAYS_INLINE typename std::enable_if<std::is_invocable<T, As...>::value, R>::type call_fun(const T& fun, As... as)
	 {
		return fun(std::forward<As>(as)...);
	}
	template<class R, class T, class... As>
	SEQ_ALWAYS_INLINE typename std::enable_if<!std::is_invocable<T, As...>::value, R>::type call_fun(const T& fun, As... as)
	{
		(void)fun;		
		throw std::bad_function_call(); return R(); 
	}	


 	template<class Fun>
 	struct FunInterface;

 	// Provide custom interface to hold_any in order to be callable like std::function
 	template<class R, class... As>
 	struct FunInterface< R(As...)>
 	{
 		// The base type info class, must inherit seq::any_type_info virtually.
 		struct type_info : virtual any_type_info
 		{
 			// Add virtual members() call and target_type()
 			virtual R call(const void* data, As... as) const = 0;
 			virtual const std::type_info& target_type() const = 0;
 		};
 
 		// Concrete implementation for given type, must inherit type_info and any_typed_type_info<T> (for base implementation of common functions)
 		template<class T>
 		struct typed_type_info : type_info, any_typed_type_info<T, false> //disable hashing support
 		{
 			virtual R call(const void* data, As... as) const override
 			{
 				// Make sure that this interface is still suitable for non invocable types
				return call_fun<R>(* static_cast<const T*>(data), std::forward<As>(as)...);
 				
 			}
 			virtual const std::type_info& target_type() const override
 			{
 				return typeid(T);
 			}
 		};

 		// Extended interface to seq::hold_any
 		template<class Base>
 		struct any_interface : Base
 		{
 			//add operator() to hold_any interface
 			R operator()(As... as) const
 			{
 				if (this->empty())
 					throw std::bad_function_call();
 				return this->type()->call(this->data(), std::forward<As>(as)...);
 			}
 			//add member target_type() to hold_any interface
 			const std::type_info& target_type() const
 			{
 				return this->empty() ? typeid(void) : this->type()->target_type();
 			}
 		};
 	};

		// Define the seq::function type
 	template<class Signature>
 	using function = hold_any< FunInterface< Signature> >;

 }
 
 
 // dumy function 
 inline int divide(int a, int b) {return a / b;}

 template<class T>
 struct multiplies
 {
	 decltype(T()*T()) operator()(const T& a, const T& b) const {
		 return a * b;
	 }
 };
 template<class T>
 struct plus
 {
	 decltype(T()+ T()) operator()(const T& a, const T& b) const {
		 return a + b;
	 }
 };





template<size_t S>
struct padding
{
	size_t d_padd[S];
};
template<>
struct padding<0>
{
};

inline void my_strcpy(char* dst, const char* src)
{
	while ( *src) {
		*dst = *src;
		++dst;
		++src;
	} 
	*dst = 0;
}


/// @brief String type which could be small/big , relocatable or not
template<size_t S, bool Reloc>
struct Str : padding<S>
{
	char* d_data;

	Str()
		:d_data(nullptr) {}
	Str(const char* str)
		:d_data(nullptr) {
		d_data = new char[strlen(str)+1];
		my_strcpy(d_data, str);
	}
	Str(const Str& other)
		:d_data(nullptr) {
		if (other.d_data) {
			d_data = new char[strlen(other.d_data)+1];
			my_strcpy(d_data, other.d_data);
		}
	}
	Str( Str&& other) noexcept
		:d_data(other.d_data) {
		other.d_data = nullptr;
	}
	~Str() {
		if (d_data)
			delete[] d_data;
	}

	char* c_str() const { return d_data; }
	bool empty() const { return !d_data; }
	Str& operator=(const Str& other) {
		if (d_data) {
			delete[] d_data;
			d_data = nullptr;
		}
		if (other.d_data) {
			d_data = new char[strlen(other.d_data)+1];
			my_strcpy(d_data, other.d_data);
		}
		return *this;
	}
	Str& operator=(Str&& other) noexcept {
		std::swap(d_data, other.d_data);
		return *this;
	}

	
	bool operator==(const Str& other) const {
		if (!d_data && !other.d_data)
			return true;
		if (!d_data || !other.d_data)
			return false;
		return strcmp(d_data, other.d_data) == 0;
	}
	bool operator<(const Str& other) const {
		if (!d_data && !other.d_data)
			return false;
		if (!d_data )
			return true;
		if (!other.d_data)
			return false;
		int r= strcmp(d_data, other.d_data) ;
		return r < 0;
	}
};

template<size_t S, bool R>
std::ostream& operator <<(std::ostream& oss, const Str<S,R> & s) {
	if (!s.empty())
		oss << s.c_str();
	return oss;
}


namespace std
{
	template<size_t S, bool R>
	struct hash<Str<S, R> >
	{
		size_t operator()(const Str<S, R>& s) const
		{
			return s.empty() ? 0 : std::hash<seq::tstring>{}(s.c_str());
		}
	};
}

namespace seq
{
	template<size_t S, bool Reloc>
	struct is_relocatable<Str<S, Reloc> >
	{
		static constexpr bool value = Reloc;
	};
}

using small_pod = Str<0,true>;
using big_pod = Str<4, true>;

using small_non_pod = Str<0, false>;
using big_non_pod = Str<4, false>;




 struct my_int_pair
{
	int a, b;
	my_int_pair(int _a = 0, int _b = 0)
		:a(_a), b(_b) {}
	
	// define conversion operator to std::string
	operator std::string() const {
		std::string res;
		seq::fmt(a).append(res);
		seq::fmt(b).append(res);
		return res;
	}
};
	
inline std::string pair_to_string(const std::pair<int, int>& p)
{
	std::string res;
	seq::fmt(p.first).append(res);
	seq::fmt(p.second).append(res);
	return res;
}



static void test_hold_any()
{
	using namespace seq;

	static_assert(sizeof(small_pod) <= sizeof(double),"");
	static_assert(sizeof(small_non_pod) <= sizeof(double),"");
	static_assert(sizeof(big_pod) > sizeof(double), "");
	static_assert(sizeof(big_non_pod) > sizeof(double), "");

	static_assert(is_relocatable<small_pod>::value == true, "");
	static_assert(is_relocatable<big_pod>::value == true, "");
	static_assert(is_relocatable<small_non_pod>::value == false, "");
	static_assert(is_relocatable<big_non_pod>::value == false, "");
	{
		// test default ctor
		any a, b, c, d;
		SEQ_TEST(a == b);
		SEQ_TEST(a.empty());

		//test emplace with comparison
		a.emplace<small_pod>("toto");
		b.emplace<big_pod>("toto");
		c.emplace<small_non_pod>("toto");
		d.emplace<big_non_pod>("toto");

		SEQ_TEST(a == small_pod("toto"));
		SEQ_TEST(a == any(small_pod("toto")));
		SEQ_TEST(b == big_pod("toto"));
		SEQ_TEST(b == any(big_pod("toto")));
		SEQ_TEST(c == small_non_pod("toto"));
		SEQ_TEST(c == any(small_non_pod("toto")));
		SEQ_TEST(d == big_non_pod("toto"));
		SEQ_TEST(d == any(big_non_pod("toto")));
	}

	{
		//test construct from value
		any a = small_pod("toto");
		any b = big_pod("toto");
		any c = small_non_pod("toto");
		any d = big_non_pod("toto");

		SEQ_TEST(a == small_pod("toto"));
		SEQ_TEST(a == any(small_pod("toto")));
		SEQ_TEST(b == big_pod("toto"));
		SEQ_TEST(b == any(big_pod("toto")));
		SEQ_TEST(c == small_non_pod("toto"));
		SEQ_TEST(c == any(small_non_pod("toto")));
		SEQ_TEST(d == big_non_pod("toto"));
		SEQ_TEST(d == any(big_non_pod("toto")));

		a.reset();
		b.reset();
		c.reset();
		d.reset();
		//test copy to null
		a = small_pod("toto");
		b = big_pod("toto");
		c = small_non_pod("toto");
		d = big_non_pod("toto");
		SEQ_TEST(a == small_pod("toto"));
		SEQ_TEST(a == any(small_pod("toto")));
		SEQ_TEST(b == big_pod("toto"));
		SEQ_TEST(b == any(big_pod("toto")));
		SEQ_TEST(c == small_non_pod("toto"));
		SEQ_TEST(c == any(small_non_pod("toto")));
		SEQ_TEST(d == big_non_pod("toto"));
		SEQ_TEST(d == any(big_non_pod("toto")));

	}
	{
		//test construct from any
		any a = any(small_pod("toto"));
		any b = any(big_pod("toto"));
		any c = any(small_non_pod("toto"));
		any d = any(big_non_pod("toto"));

		SEQ_TEST(a == small_pod("toto"));
		SEQ_TEST(a == any(small_pod("toto")));
		SEQ_TEST(b == big_pod("toto"));
		SEQ_TEST(b == any(big_pod("toto")));
		SEQ_TEST(c == small_non_pod("toto"));
		SEQ_TEST(c == any(small_non_pod("toto")));
		SEQ_TEST(d == big_non_pod("toto"));
		SEQ_TEST(d == any(big_non_pod("toto")));

		// test move construct
		any _a = std::move(a);
		any _b = std::move(b);
		any _c = std::move(c);
		any _d = std::move(d);

		SEQ_TEST(_a == small_pod("toto"));
		SEQ_TEST(_a == any(small_pod("toto")));
		SEQ_TEST(_b == big_pod("toto"));
		SEQ_TEST(_b == any(big_pod("toto")));
		SEQ_TEST(_c == small_non_pod("toto"));
		SEQ_TEST(_c == any(small_non_pod("toto")));
		SEQ_TEST(_d == big_non_pod("toto"));
		SEQ_TEST(_d == any(big_non_pod("toto")));

		//test copy to non null
		a = _a;
		b = _b;
		c = _c;
		d = _d;
		SEQ_TEST(a == small_pod("toto"));
		SEQ_TEST(a == any(small_pod("toto")));
		SEQ_TEST(b == big_pod("toto"));
		SEQ_TEST(b == any(big_pod("toto")));
		SEQ_TEST(c == small_non_pod("toto"));
		SEQ_TEST(c == any(small_non_pod("toto")));
		SEQ_TEST(d == big_non_pod("toto"));
		SEQ_TEST(d == any(big_non_pod("toto")));

		//test move assignment to non null
		_a = std::move(a);
		_b = std::move(b);
		_c = std::move(c);
		_d = std::move(d);
		SEQ_TEST(_a == small_pod("toto"));
		SEQ_TEST(_a == any(small_pod("toto")));
		SEQ_TEST(_b == big_pod("toto"));
		SEQ_TEST(_b == any(big_pod("toto")));
		SEQ_TEST(_c == small_non_pod("toto"));
		SEQ_TEST(_c == any(small_non_pod("toto")));
		SEQ_TEST(_d == big_non_pod("toto"));
		SEQ_TEST(_d == any(big_non_pod("toto")));

	}

	{
		small_pod a = small_pod("toto");
		big_pod b = big_pod("toto");
		small_non_pod c = small_non_pod("toto");
		big_non_pod d = big_non_pod("toto");

		//test move construct to any
		any _a = std::move(a);
		any _b = std::move(b);
		any _c = std::move(c);
		any _d = std::move(d);

		SEQ_TEST(a.empty());
		SEQ_TEST(b.empty());
		SEQ_TEST(c.empty());
		SEQ_TEST(d.empty());

		SEQ_TEST(_a == small_pod("toto"));
		SEQ_TEST(_a == any(small_pod("toto")));
		SEQ_TEST(_b == big_pod("toto"));
		SEQ_TEST(_b == any(big_pod("toto")));
		SEQ_TEST(_c == small_non_pod("toto"));
		SEQ_TEST(_c == any(small_non_pod("toto")));
		SEQ_TEST(_d == big_non_pod("toto"));
		SEQ_TEST(_d == any(big_non_pod("toto")));


		//test move to any
		_a.reset();
		_b.reset();
		_c.reset();
		_d.reset();

		a = small_pod("toto");
		b = big_pod("toto");
		c = small_non_pod("toto");
		d = big_non_pod("toto");

		_a = std::move(a);
		_b = std::move(b);
		_c = std::move(c);
		_d = std::move(d);

		SEQ_TEST(a.empty());
		SEQ_TEST(b.empty());
		SEQ_TEST(c.empty());
		SEQ_TEST(d.empty());

		SEQ_TEST(_a == small_pod("toto"));
		SEQ_TEST(_a == any(small_pod("toto")));
		SEQ_TEST(_b == big_pod("toto"));
		SEQ_TEST(_b == any(big_pod("toto")));
		SEQ_TEST(_c == small_non_pod("toto"));
		SEQ_TEST(_c == any(small_non_pod("toto")));
		SEQ_TEST(_d == big_non_pod("toto"));
		SEQ_TEST(_d == any(big_non_pod("toto")));
	}
	{
		//test operators
		any a = small_pod("toto");
		any b = small_pod("tutu");

		SEQ_TEST(small_pod("toto") < b);
		SEQ_TEST(small_pod("toto") <= b);
		SEQ_TEST(b > small_pod("toto"));
		SEQ_TEST(b >= small_pod("toto"));
		SEQ_TEST(b != small_pod("toto"));

		SEQ_TEST(a < b);
		SEQ_TEST(a <= b);
		SEQ_TEST(b > a);
		SEQ_TEST(b >= a);
		SEQ_TEST(b != a);
	}
	{
		//test operators between different arithmetic types
		any a = 1;
		any b = 1.2;

		SEQ_TEST(a < b);
		SEQ_TEST(a <= b);
		SEQ_TEST(b > a);
		SEQ_TEST(b >= a);
		SEQ_TEST(b != a);

		SEQ_TEST(1 < b);
		SEQ_TEST(1 <= b);
		SEQ_TEST(b > 1);
		SEQ_TEST(b >= 1);
		SEQ_TEST(b != 1);

	}
	{
		//test operators between different string types
		any a = "toto";
		any b = tstring("tutu");

		SEQ_TEST(a < b);
		SEQ_TEST(a <= b);
		SEQ_TEST(b > a);
		SEQ_TEST(b >= a);
		SEQ_TEST(b != a);

		SEQ_TEST(std::string("toto") < b);
		SEQ_TEST(std::string("toto") <= b);
		SEQ_TEST(b > std::string("toto"));
		SEQ_TEST(b >= std::string("toto"));
		SEQ_TEST(b != std::string("toto"));
	}
	{
		// test custom comparison
		 
		 // define a comparison function between std::pair<int,int> and int
		 struct equal_pair
		 {
				bool operator()(const std::pair<int,int> & a, int b) const
				{
					return a.first == b && a.second == b;
				}
		 };
	 
		 register_any_equal_comparison<std::pair<int,int>, int>(equal_pair{});
	 
		 nh_any pair = std::make_pair(2,2);
		 nh_any integer = 2;
		 SEQ_TEST(pair == integer);
	 
	}
	{
		// test custom comparison
		
		 // define a dumy comparison function between std::pair<int,int> and int
		 struct less_pair
		 {
				bool operator()(const std::pair<int,int> & a, int b) const
				{
					return a.first < b && a.second < b;
				}
		 };
	 
		 register_any_less_comparison<std::pair<int,int>, int>(less_pair{});
	 
		 nh_any pair = std::make_pair(1,2);
		 nh_any integer = 3;
		 SEQ_TEST(pair < integer);
	}
	{
		//test cast
		any a = small_pod("toto");
		small_pod b = a.cast< small_pod>();
		small_pod &c = a.cast< small_pod&>();

		SEQ_TEST(a == b);
		SEQ_TEST(a == c);

		small_pod d = any_cast<small_pod>(a);
		small_pod &e = any_cast<small_pod&>(a);
		small_pod* f = any_cast<small_pod>(&a);

		SEQ_TEST(a == d);
		SEQ_TEST(a == e);
		SEQ_TEST(a == *f);
	}
	{
		//test make_any
		any a = make_any<any, small_pod>("toto");
		SEQ_TEST(a == small_pod("toto"));

		//test print
		SEQ_TEST_TO_OSTREAM("toto", a);
	}
	{
		//test istream
		std::ofstream out("test_any_file");
		out << 1.2;
		out.close();

		any a = 3.;
		std::ifstream in("test_any_file");
		in >> a;

		SEQ_TEST(a == 1.2);
	}
	{
		//test bad_function_call throw
		SEQ_TEST_THROW(std::bad_function_call, std::cout << nh_any(std::vector<bool>()));
	}
	{
		// test other conversions
		const char* s = "1.2";
		 any a = s; // holds a const char*
		 char* c = a.cast<char*>(); // const char * to char* is valid (although not safe)
		 void* v = a.cast<void*>(); //conversion to void* is valid (although not safe)

		 SEQ_TEST(a == c); //compare pointer
		 SEQ_TEST(s == c);//compare pointer
		 SEQ_TEST(s == v);//compare pointer
		 SEQ_TEST(a == std::string("1.2"));//compare string
	
		 // conversion to the different string types
		 tstring str = a.cast<tstring>();
		 tstring_view view = a.cast<tstring_view>();
		 std::string str2 = a.cast<std::string>();
		 SEQ_TEST(a == str);
		 SEQ_TEST(a == view);
		 SEQ_TEST(a == str2);
	
		 // conversion to arithmetic
		 double d = a.cast<double>(); // d holds 1.2
		 SEQ_COMPARE_FLOAT(SEQ_TEST((d == 1.2));)

		 int i = a.cast<int>(); // i holds 1
		 SEQ_TEST(i == 1);
	
		 a = 1.2;
		 i = a.cast<int>(); //valid, cast double to int
		 SEQ_TEST(i == 1);

		 str2 = a.cast<std::string>(); //valid, str2 holds "1.2"
		 SEQ_TEST(str2 == "1.2");
	}
	{
		 // register conversion that already exists 
		 register_any_conversion<my_int_pair, std::string>();
		 // register conversion function
		 register_any_conversion<std::pair<int, int>, std::string>(pair_to_string);
		
		 nh_any a = std::make_pair(1, 2);
		 nh_any b = my_int_pair(1, 2);
	
		 SEQ_TEST_TO_OSTREAM("12", a.cast<std::string>());
		 SEQ_TEST_TO_OSTREAM("12", b.cast<std::string>());
	}
	{
		// build an ordered set than supports heterogeneous lookup 
		 ordered_set<any,std::hash<any> , std::equal_to<void> > set;
	 
		 set.insert(3);
		 set.insert(2.5);
		 set.insert(tstring("hello"));
		 set.insert(1);
		 set.insert(std::string("world"));
		 set.insert("ok");
	
		 // print the set content
		 auto it = set.begin();
		 SEQ_TEST_TO_OSTREAM("3", *it++);
		 SEQ_TEST_TO_OSTREAM("2.5", *it++);
		 SEQ_TEST_TO_OSTREAM("hello", *it++);
		 SEQ_TEST_TO_OSTREAM("1", *it++);
		 SEQ_TEST_TO_OSTREAM("world", *it++);
		 SEQ_TEST_TO_OSTREAM("ok", *it++);
		
	
		 SEQ_TEST(set.find(3) != set.end());			// use heterogeneous lookup 
		 SEQ_TEST(set.find(2.5) != set.end());			// use heterogeneous lookup 
		 SEQ_TEST(set.find("hello")  != set.end());	// use heterogeneous lookup 
		 SEQ_TEST(set.find(tstring("world")) != set.end());	// use heterogeneous lookup 
		 SEQ_TEST(set.find("ok") == set.end()) ;			//"ok" not found has we compare 2 const char* -> pointer comparison, not string comparison
		 SEQ_TEST(set.find("no") == set.end());			//failed lookup
	 
	}
	{
		// test move-only type
		// 
		 // create any from unique_ptr
		 any a = std::unique_ptr<int>(new int(3));
		 // retrieve a reference to the unique_ptr
		 std::unique_ptr<int>& val1 = a.cast< std::unique_ptr<int>&>();
		 SEQ_TEST(*val1 == 3);
	
		 // move any object
		 any b = std::move(a);
		 std::unique_ptr<int>& val2 = b.cast< std::unique_ptr<int>&>();
		 SEQ_TEST(*val2 == 3);
	
		 // try to copy: throw std::bad_function_call as copy operator is disabled for unique_ptr
		 SEQ_TEST_THROW(std::bad_function_call, any c = b);
		
	}
	{
		//test extend interface

		 seq::function<int(int,int)> plus_fun = plus<int>{};					// affect functor
		 seq::function<int(int,int)> minus_fun = [](int a, int b){return a - b;}	;	// affect lambda
		 seq::function<int(int,int)> multiplies_fun = multiplies<int>{};		// affect functor
		 seq::function<int(int,int)> divide_fun = divide;							// affect function
		
		 SEQ_TEST( plus_fun(1,2) == 3);
		 SEQ_TEST( minus_fun(2,1) == 1);
		 SEQ_TEST( multiplies_fun(2,3) == 6);
		 SEQ_TEST( divide_fun(9,3) == 3);
		 
	}
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif


int test_any(int, char*[])
{
	
	SEQ_TEST_MODULE_RETURN(any, 1, test_hold_any());
	return 0;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
