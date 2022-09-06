

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <functional>

#include "any.hpp"
#include "tiny_string.hpp"
#include "hash.hpp"
#include "testing.hpp"
#include "format.hpp"
#include "ordered_map.hpp"


 namespace seq
 {
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
 			virtual R call(const void* data, As... as) const
 			{
 				// C++11 emulation of if constexpr
 				// Make sure that this interface is still suitable for non invocable types
 				return constexpr_if<is_invocable<T, As ...>::value>(
 					[&as...](const auto& fun) {return fun(std::forward<As>(as)...); },
 					[](const auto&) {throw std::bad_function_call(); return R(); },
 					* static_cast<const T*>(data));
 			}
 			virtual const std::type_info& target_type() const
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
 int divide(int a, int b) {return a / b;}

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



using namespace seq;

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
		:d_data(NULL) {}
	Str(const char* str)
		:d_data(NULL) {
		d_data = new char[strlen(str)+1];
		my_strcpy(d_data, str);
	}
	Str(const Str& other)
		:d_data(NULL) {
		if (other.d_data) {
			d_data = new char[strlen(other.d_data)+1];
			my_strcpy(d_data, other.d_data);
		}
	}
	Str( Str&& other) noexcept
		:d_data(other.d_data) {
		other.d_data = NULL;
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
			d_data = NULL;
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
		size_t operator()(const Str<S, R>& s)
		{
			return s.empty() ? 0 : std::hash<tstring>{}(s.c_str());
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
	my_int_pair(int a = 0, int b = 0)
		:a(a), b(b) {}
	
	// define conversion operator to std::string
	operator std::string() const {
		std::string res;
		seq::fmt(a).append(res);
		seq::fmt(b).append(res);
		return res;
	}
};
	
std::string pair_to_string(const std::pair<int, int>& p)
{
	std::string res;
	seq::fmt(p.first).append(res);
	seq::fmt(p.second).append(res);
	return res;
}



void test_any()
{
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
		SEQ_TEST_ASSERT(a == b);
		SEQ_TEST_ASSERT(a.empty());

		//test emplace with comparison
		a.emplace<small_pod>("toto");
		b.emplace<big_pod>("toto");
		c.emplace<small_non_pod>("toto");
		d.emplace<big_non_pod>("toto");

		SEQ_TEST_ASSERT(a == small_pod("toto"));
		SEQ_TEST_ASSERT(a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(b == big_pod("toto"));
		SEQ_TEST_ASSERT(b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(d == any(big_non_pod("toto")));
	}

	{
		//test construct from value
		any a = small_pod("toto");
		any b = big_pod("toto");
		any c = small_non_pod("toto");
		any d = big_non_pod("toto");

		SEQ_TEST_ASSERT(a == small_pod("toto"));
		SEQ_TEST_ASSERT(a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(b == big_pod("toto"));
		SEQ_TEST_ASSERT(b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(d == any(big_non_pod("toto")));

		a.reset();
		b.reset();
		c.reset();
		d.reset();
		//test copy to null
		a = small_pod("toto");
		b = big_pod("toto");
		c = small_non_pod("toto");
		d = big_non_pod("toto");
		SEQ_TEST_ASSERT(a == small_pod("toto"));
		SEQ_TEST_ASSERT(a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(b == big_pod("toto"));
		SEQ_TEST_ASSERT(b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(d == any(big_non_pod("toto")));

	}
	{
		//test construct from any
		any a = any(small_pod("toto"));
		any b = any(big_pod("toto"));
		any c = any(small_non_pod("toto"));
		any d = any(big_non_pod("toto"));

		SEQ_TEST_ASSERT(a == small_pod("toto"));
		SEQ_TEST_ASSERT(a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(b == big_pod("toto"));
		SEQ_TEST_ASSERT(b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(d == any(big_non_pod("toto")));

		// test move construct
		any _a = std::move(a);
		any _b = std::move(b);
		any _c = std::move(c);
		any _d = std::move(d);

		SEQ_TEST_ASSERT(_a == small_pod("toto"));
		SEQ_TEST_ASSERT(_a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(_b == big_pod("toto"));
		SEQ_TEST_ASSERT(_b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(_c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(_c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(_d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(_d == any(big_non_pod("toto")));

		//test copy to non null
		a = _a;
		b = _b;
		c = _c;
		d = _d;
		SEQ_TEST_ASSERT(a == small_pod("toto"));
		SEQ_TEST_ASSERT(a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(b == big_pod("toto"));
		SEQ_TEST_ASSERT(b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(d == any(big_non_pod("toto")));

		//test move assignment to non null
		_a = std::move(a);
		_b = std::move(b);
		_c = std::move(c);
		_d = std::move(d);
		SEQ_TEST_ASSERT(_a == small_pod("toto"));
		SEQ_TEST_ASSERT(_a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(_b == big_pod("toto"));
		SEQ_TEST_ASSERT(_b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(_c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(_c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(_d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(_d == any(big_non_pod("toto")));

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

		SEQ_TEST_ASSERT(a.empty());
		SEQ_TEST_ASSERT(b.empty());
		SEQ_TEST_ASSERT(c.empty());
		SEQ_TEST_ASSERT(d.empty());

		SEQ_TEST_ASSERT(_a == small_pod("toto"));
		SEQ_TEST_ASSERT(_a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(_b == big_pod("toto"));
		SEQ_TEST_ASSERT(_b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(_c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(_c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(_d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(_d == any(big_non_pod("toto")));


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

		SEQ_TEST_ASSERT(a.empty());
		SEQ_TEST_ASSERT(b.empty());
		SEQ_TEST_ASSERT(c.empty());
		SEQ_TEST_ASSERT(d.empty());

		SEQ_TEST_ASSERT(_a == small_pod("toto"));
		SEQ_TEST_ASSERT(_a == any(small_pod("toto")));
		SEQ_TEST_ASSERT(_b == big_pod("toto"));
		SEQ_TEST_ASSERT(_b == any(big_pod("toto")));
		SEQ_TEST_ASSERT(_c == small_non_pod("toto"));
		SEQ_TEST_ASSERT(_c == any(small_non_pod("toto")));
		SEQ_TEST_ASSERT(_d == big_non_pod("toto"));
		SEQ_TEST_ASSERT(_d == any(big_non_pod("toto")));
	}
	{
		//test operators
		any a = small_pod("toto");
		any b = small_pod("tutu");

		SEQ_TEST_ASSERT(small_pod("toto") < b);
		SEQ_TEST_ASSERT(small_pod("toto") <= b);
		SEQ_TEST_ASSERT(b > small_pod("toto"));
		SEQ_TEST_ASSERT(b >= small_pod("toto"));
		SEQ_TEST_ASSERT(b != small_pod("toto"));

		SEQ_TEST_ASSERT(a < b);
		SEQ_TEST_ASSERT(a <= b);
		SEQ_TEST_ASSERT(b > a);
		SEQ_TEST_ASSERT(b >= a);
		SEQ_TEST_ASSERT(b != a);
	}
	{
		//test operators between different arithmetic types
		any a = 1;
		any b = 1.2;

		SEQ_TEST_ASSERT(a < b);
		SEQ_TEST_ASSERT(a <= b);
		SEQ_TEST_ASSERT(b > a);
		SEQ_TEST_ASSERT(b >= a);
		SEQ_TEST_ASSERT(b != a);

		SEQ_TEST_ASSERT(1 < b);
		SEQ_TEST_ASSERT(1 <= b);
		SEQ_TEST_ASSERT(b > 1);
		SEQ_TEST_ASSERT(b >= 1);
		SEQ_TEST_ASSERT(b != 1);

	}
	{
		//test operators between different string types
		any a = "toto";
		any b = tstring("tutu");

		SEQ_TEST_ASSERT(a < b);
		SEQ_TEST_ASSERT(a <= b);
		SEQ_TEST_ASSERT(b > a);
		SEQ_TEST_ASSERT(b >= a);
		SEQ_TEST_ASSERT(b != a);

		SEQ_TEST_ASSERT(std::string("toto") < b);
		SEQ_TEST_ASSERT(std::string("toto") <= b);
		SEQ_TEST_ASSERT(b > std::string("toto"));
		SEQ_TEST_ASSERT(b >= std::string("toto"));
		SEQ_TEST_ASSERT(b != std::string("toto"));
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
	 
		 nh_any pair = std::pair<int,int>(2,2);
		 nh_any integer = 2;
		 SEQ_TEST_ASSERT(pair == integer);
	 
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
	 
		 nh_any pair = std::pair<int,int>(1,2);
		 nh_any integer = 3;
		 SEQ_TEST_ASSERT(pair < integer);
	}
	{
		//test cast
		any a = small_pod("toto");
		small_pod b = a.cast< small_pod>();
		small_pod &c = a.cast< small_pod&>();

		SEQ_TEST_ASSERT(a == b);
		SEQ_TEST_ASSERT(a == c);

		small_pod d = any_cast<small_pod>(a);
		small_pod &e = any_cast<small_pod&>(a);
		small_pod* f = any_cast<small_pod>(&a);

		SEQ_TEST_ASSERT(a == d);
		SEQ_TEST_ASSERT(a == e);
		SEQ_TEST_ASSERT(a == *f);
	}
	{
		//test make_any
		any a = make_any<any, small_pod>("toto");
		SEQ_TEST_ASSERT(a == small_pod("toto"));

		//test print
		std::cout << a << std::endl;
	}
	{
		//test istream
		std::ofstream out("test_any_file");
		out << 1.2;
		out.close();

		any a = 3.;
		std::ifstream in("test_any_file");
		in >> a;

		SEQ_TEST_ASSERT(a == 1.2);
	}
	{
		//test bad_function_call throw
		bool has_thrown = false;

		try {
			std::cout << nh_any(std::vector<bool>()); // std::vector<bool> does not define stream operator to std::ostream, throw a std::bad_function_call
		}
		catch (const std::bad_function_call& )
		{
			has_thrown = true;
		}
		SEQ_TEST_ASSERT(has_thrown);
	}
	{
		// test other conversions
		const char* s = "1.2";
		 any a = s; // holds a const char*
		 char* c = a.cast<char*>(); // const char * to char* is valid (although not safe)
		 void* v = a.cast<void*>(); //conversion to void* is valid (although not safe)

		 SEQ_TEST_ASSERT(a == c); //compare pointer
		 SEQ_TEST_ASSERT(s == c);//compare pointer
		 SEQ_TEST_ASSERT(s == v);//compare pointer
		 SEQ_TEST_ASSERT(a == std::string("1.2"));//compare string
	
		 // conversion to the different string types
		 tstring str = a.cast<tstring>();
		 tstring_view view = a.cast<tstring_view>();
		 std::string str2 = a.cast<std::string>();
		 SEQ_TEST_ASSERT(a == str);
		 SEQ_TEST_ASSERT(a == view);
		 SEQ_TEST_ASSERT(a == str2);
	
		 // conversion to arithmetic
		 double d = a.cast<double>(); // d holds 1.2
		 SEQ_TEST_ASSERT(d == 1.2);

		 int i = a.cast<int>(); // i holds 1
		 SEQ_TEST_ASSERT(i == 1);
	
		 a = 1.2;
		 i = a.cast<int>(); //valid, cast double to int
		 SEQ_TEST_ASSERT(i == 1);

		 str2 = a.cast<std::string>(); //valid, str2 holds "1.2"
		 SEQ_TEST_ASSERT(str2 == "1.2");
	}
	{
		 // register conversion that already exists 
		 register_any_conversion<my_int_pair, std::string>();
		 // register conversion function
		 register_any_conversion<std::pair<int, int>, std::string>(pair_to_string);
		
		 nh_any a = std::pair<int, int>(1, 2);
		 nh_any b = my_int_pair(1, 2);
	
		 std::cout << a.cast<std::string>() << std::endl;
		 std::cout << b.cast<std::string>() << std::endl;
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
		 for (auto val : set)
				std::cout << val << std::endl;
	
		 SEQ_TEST_ASSERT(set.find(3) != set.end());			// use heterogeneous lookup 
		 SEQ_TEST_ASSERT(set.find(2.5) != set.end());			// use heterogeneous lookup 
		 SEQ_TEST_ASSERT(set.find("hello")  != set.end());	// use heterogeneous lookup 
		 SEQ_TEST_ASSERT(set.find(tstring("world")) != set.end());	// use heterogeneous lookup 
		 SEQ_TEST_ASSERT(set.find("ok") == set.end()) ;			//"ok" not found has we compare 2 const char* -> pointer comparison, not string comparison
		 SEQ_TEST_ASSERT(set.find("no") == set.end());			//failed lookup
	 
	}
	{
		// test move-only type
		// 
		 // create any from unique_ptr
		 any a = std::unique_ptr<int>(new int(3));
		 // retrieve a reference to the unique_ptr
		 std::unique_ptr<int>& val1 = a.cast< std::unique_ptr<int>&>();
		 SEQ_TEST_ASSERT(*val1 == 3);
	
		 // move any object
		 any b = std::move(a);
		 std::unique_ptr<int>& val2 = b.cast< std::unique_ptr<int>&>();
		 SEQ_TEST_ASSERT(*val2 == 3);
	
		 // try to copy: throw std::bad_function_call as copy operator is disabled for unique_ptr
		 bool has_thrown = false;
		 try {
	 		any c = b;
		 }
		 catch (const std::bad_function_call&)
		 {
			 has_thrown = true;
		 }
		 SEQ_TEST_ASSERT(has_thrown);
	}
	{
		//test extend interface

		 seq::function<int(int,int)> plus_fun = plus<int>{};					// affect functor
		 seq::function<int(int,int)> minus_fun = [](int a, int b){return a - b;}	;	// affect lambda
		 seq::function<int(int,int)> multiplies_fun = multiplies<int>{};		// affect functor
		 seq::function<int(int,int)> divide_fun = divide;							// affect function
		
		 SEQ_TEST_ASSERT( plus_fun(1,2) == 3);
		 SEQ_TEST_ASSERT( minus_fun(2,1) == 1);
		 SEQ_TEST_ASSERT( multiplies_fun(2,3) == 6);
		 SEQ_TEST_ASSERT( divide_fun(9,3) == 3);
		 
	}
}