# Any: type-erasing polymorphic object wrapper used to build heterogeneous containers


The *any* module provides the `seq::hold_any` class and related functions to provide a type-erasing polymorphic object wrapper.

`seq::hold_any` is a `std::any` like class optimized to create heterogeneous containers of any types (vectors, deques, hash tables, sorted containers...).

The seq library provides the following aliases:
-	`seq::any`: equivalent to `seq::hold_any<seq::any_default_interface>`
-	`seq::r_any`: relocatable version of `seq::any`
-	`seq::nh_any`: equivalent to  `seq::hold_any<seq::any_no_hash_interface>`
-	`seq::r_nh_any`: relocatable version of `seq::nh_any`


## Interface


`hold_any` offers a similar interface to `std::any` and supports additional features:
-	Support for comparison operators <, <=, >, >=, == and !=
-	Support for standard stream operators to/from std::ostream/std::istream
-	`hold_any` is hashable based on std::hash
-	`hold_any` can be formatted using the [format](format.md) module of seq library
-	`hold_any` interface can be extended

`hold_any` is very similar to boost.TypeErasure or folly.Poly, but its default implementation provides more features in order to be used within most containers (sorted containers, hash tables,...).

These features do not modify the requirements of held types: they must be at least move only contructible.
`hold_any` uses type erasure to provide custom behavior based on held object type, and the additional functions like streaming or comparison support are implemented if the type supports them. 
If not, the corresponding functions will throw a std::bad_function_call exception:

```cpp

std::cout << seq::any(2);					// works as expected, print '2'
std::cout << seq::any(std::vector<bool>()); // std::vector<bool> does not define stream operator to std::ostream, throw a std::bad_function_call

```

The only exception is the hashing support. Indeed, there is no way in C++11 to tell at compile time if a type is hashable or not, as most implementations of `std::hash` use static_assert() in the operator() member (undetectable through SFINAE). 
`hold_any` optimistically assumes that all types are hashable. In order to store non hashable types, you must specialize the `seq::is_hashable` type trait for this type:

```cpp

struct test{};

template<>
struct seq::is_hashable<test> : std::false_type {};

//...

seq::any a = test(); // compile as is_hashable is specialized for test class

```

Otherwise, you can completely disable the hashing support using `seq::nh_any` (equivalent to `seq::hold_any<seq::any_no_hash_interface>`) :

```cpp

struct test{};

seq::nh_any a = test();

```


## Casting


`hold_any` can be casted to another type using either `hold_any::cast()` function or `seq::any_cast()` (similar to `std::any_cast`).
When casting to the same type as the underlying object, it is possible to retrieve a reference to the object:

```cpp

seq::any a = 2;
int &value1 = a.cast<int&>();
int &value2 = seq::any_cast<int&>(a);
int *value3 = seq::any_cast<int>(&a);

```

`hold_any` supports casting to another type. By default, the following conversion are valid:
-	A pointer can be casted to void* or const void*
-	All arithmetic types can be casted between each other
-	Arithmetic types can be casted to `std::string` or `seq::tiny_string`
-	All string types can be casted between each other (std::string, seq::tstring, seq::tstring_view, std::string_view, char* and const char*)
-	All string types can be casted to arithmetic types (which can throw a std::bad_cast exception if the string does not represent an arithmetic type)

Example:

```cpp

using namespace seq;

any a = "1.2"; // holds a const char*
	
char* c = a.cast<char*>(); // const char * to char* is valid (although not safe)
void* v = a.cast<void*>(); //conversion to void* is valid (although not safe)
	
// conversion to the different string types
tstring str = a.cast<tstring>();
tstring_view view = a.cast<tstring_view>();
std::string str2 = a.cast<std::string>();
	
// conversion to arithmetic
double d = a.cast<double>(); // d holds 1.2
int i = a.cast<int>(); // i holds 1
	
a = 1.2;
i = a.cast<int>(); //valid, cast double to int
str2 = a.cast<std::string>(); //valid, str2 holds "1.2"
	
```

It is possible to register additional conversion functions for custom types using seq::register_any_conversion():

```cpp

// Dumy pair of int defining an implicit conversion operator to std::string
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

// Conversion function from std::pair<int, int> to std::string
std::string pair_to_string(const std::pair<int, int>& p)
{
		std::string res;
		seq::fmt(p.first).append(res);
		seq::fmt(p.second).append(res);
		return res;
}
	


// Register implicit conversion (from my_int_pair to std::string)
seq::register_any_conversion<my_int_pair, std::string>();

// Register explicit conversion function (from std::pair<int, int> to std::string)
seq::register_any_conversion<std::pair<int, int>, std::string>(pair_to_string);
	
// Disable hashing interface
using my_any = seq::nh_any;
	
// Create any object containing a std::pair<int, int>
my_any a = std::pair<int, int>(1, 2);
// Cast a to std::string
std::cout << a.cast<std::string>() << std::endl;

// Create any object containing a my_int_pair
my_any b = my_int_pair(1, 2);
// Cast a to std::string
std::cout << b.cast<std::string>() << std::endl;

```


## Hashing


As said previously, `seq::hold_any` is hashable and a specialization for `std::hash` is provided.
This feature is mandatory when inserting any objects into hash table based containers like std::unordered_set.
In order to store non hashable types (that do not specialize std::hash), you must specialize `seq::is_hashable` type traits for this type to have seq::is_hashable<Type>::value == false.

Another way to store non hashable types is to use seq::nh_any instead of seq::any which disable the hashing support. 
Attempting to call `seq::hold_any::hash()` member on a non hashable type will throw a std::bad_function_call.

The specialization of std::hash for `hold_any` is transparent to support heterogeneous lookup.

Example:

```cpp

#include <iostream>
#include <seq/ordered_set.hpp>
#include <seq/tiny_string.hpp>
#include <seq/any.hpp>

// For transparent equal_to in C++11
#include <seq/utils.hpp>

//...

using namespace seq;

// build an ordered set than supports heterogeneous lookup 
seq::ordered_set<any,std::hash<any> , seq::equal_to<void> > set;

set.insert(3);						// insert integer
set.insert(2.5);					// insert double
set.insert(tstring("hello"));		// insert seq::tstring
set.insert(1);						// insert integer
set.insert(std::string("world"));	// insert std::string
set.insert("ok");					// insert const char*
	
// print the set content
for (auto val : set)
	std::cout << val << std::endl;
	
assert(set.find(any(3)) != set.end());				// standard lookup 
assert(set.find(3) != set.end());					// use heterogeneous lookup 
assert(set.find(2.5) != set.end());					// use heterogeneous lookup 
assert(set.find("hello")  != set.end());			// use heterogeneous lookup 
assert(set.find(tstring("world")) != set.end());	// use heterogeneous lookup 
assert(set.find(tstring_view("ok")) != set.end()) ;	// use heterogeneous lookup 
assert(set.find("ok") == set.end()) ;				// "ok" not found has we compare 2 const char* -> pointer comparison, not string comparison
assert(set.find("no") == set.end());				// failed lookup

```



## Equality comparison


`hold_any` objects are equally comparable. This feature is mandatory when inserting any objects into hash table based containers like `std::unordered_set`.
Two `hold_any` are considered equal if:
-	They are both empty.
-	They hold the same type and both underlying objects compare equals. If the type does not provide a comparison operator, the operator==() always return false.
-	They both hold an arithmetic value of possibly different types, and these values compare equals.
-	They both hold a string like object (std::string, seq::tstring, seq::tstring_view, std::string_view, char*, const char*) that compare equals.
	Note that a const char* can be compared to another string object (like std::string) using string comparison, but comparing two const char* will result in a pointer comparison!

It is possible to register a comparison function for unrelated types using `seq::register_any_equal_comparison()` function.

Example:

```cpp

seq::any a = 1.0;
seq::any b = 1;

seq::any c = "hello";
seq::any d = std::string("hello");

assert(a == b);  // compare any(double) and any(int)
assert(a == 1.); // compare any(double) to a double
assert(a == 1);   // compare any(double) to int
assert(a != 1.2); // compare any(double) to a double
assert(a != c); // compare any(double) to any(const char*)
assert(c == d); //compare any(const char*) to any(std::string)
assert(d == "hello"); // compare any(std::string) to const char*


// define a comparison function between std::pair<int,int> and int
struct equal_pair
{
bool operator()(const std::pair<int,int> & a, int b) const
{
	return a.first == b && a.second == b;
}
};

seq::register_any_equal_comparison<std::pair<int,int>, int>(equal_pair{});

seq::nh_any pair = std::pair<int,int>(2,2);
seq::nh_any integer = 2;
assert(pair == integer);


```


## Less comparison


`hold_any` object can be compared using operators <, >, <=, >=, all based on the <i>less than</i> operator. This is mandatory when inserting any objects into a sorted container like std::set.
A `hold_any` object A is considered less than another `hold_any` B if:
-	A is empty and not B.
-	A and B hold the same type and object(A) < object(B). If the type does not provide a less than operator, throw std::bad_function_call.
-	They both hold an arithmetic value of possibly different types, and object(A) < object(B).
-	They both hold a string like object (std::string, seq::tstring, seq::tstring_view, std::string_view, char*, const char*) and object(A) < object(B).
-	For totally unrelated types, returns get_type_id(A) < get_type_id(B).
 
It is possible to register a comparison function for unrelated types using `seq::register_any_less_comparison()` function.

Example:

```cpp

seq::any a = 1;
seq::any b = 2.3;

seq::any c = "hello";
seq::any d = std::string("world");

assert(a < b);  // compare any(int) and any(double)
assert(a < 2.3); // compare any(int) to a double
assert(a <= 1);   // compare any(int) to int
assert(b >= a);   // compare any(double) to any(int)
assert(c < d); //compare any(const char*) to any(std::string)
assert(d > c); // compare any(std::string) to const char*


// define a dumy comparison function between std::pair<int,int> and int
struct less_pair
{
bool operator()(const std::pair<int,int> & a, int b) const
{
	return a.first < b && a.second < b;
}
};

seq::register_any_less_comparison<std::pair<int,int>, int>(less_pair{});

seq::nh_any pair = std::pair<int,int>(1,2);
seq::nh_any integer = 3;
assert(pair < integer); 

``` 


## Small Buffer Optimization


`hold_any` provides a customizable Small Buffer Optimization (SBO) in order to avoid costly memory allocation for small types.
The alignment of the small buffer can also be modified.

By default, the small buffer size is equal to sizeof(double) and its alignment is alignof(double). Therefore, on 64 bits machines, sizeof(seq::any) is 16 bytes.

The small buffer size and alignment can be changed through `hold_any` template parameters:

```cpp

// define an any type that can hold a tstring object on the stack
using my_any = seq::hold_any<seq::any_default_interface, sizeof(seq::tstring), alignof(seq::tstring) >;

// the tstring object is stored on the stack using the small buffer. This line does not trigger a memory allocation as tstring itself uses Small String Optimization.
my_any a = seq::tstring("hello");

``` 


## Type information and various optimizations


Internally, `hold_any` uses a tagged pointer to store 3 additional information on the held type object:
-	Tells if the type is relocatable (`seq::is_relocatable<Type>::value == true`)
-	Tells if the type is trivially copyable (`std::is_trivially_copyable<Type>::value == true`)
-	Tells if the type is trivially destructible (`std::is_trivially_destructible<Type>::value == true`)

These information are used to optimize the copy, move assignment and destruction of `hold_any` objects.

Note that, by default, `hold_any` itself is NOT relocatable as it might hold a non relocatable type inside its small buffer.
It is possible to force `hold_any` type to be relocatable by setting the template parameter *Relocatable* to true.
In this case, small but non relocatable types will be allocated on the heap instead of being stored in the small buffer.
The global typedef `seq::r_any` and `seq::r_nh_any` are defined to provide relocatable alternatives to `seq::any` and `seq::nh_any`.
Note that some containers (`seq::tiered_vector`, `seq::devector`, `seq::flat_set/map`) are faster with relocatable types. Furthermore, `seq::cvector` only works with relocatable types.

In some situations, a relocatable hold_any will be faster than a standard hold_any as its move copy/assignement operators are faster, as well as its swap member. For instance, a `seq::flat_set` of `seq::r_any` is WAY faster than for `seq::any`.
 

## Move only types


`hold_any` supports holding move only types like std::unique_ptr. In this case, the `hold_any` will silently become a move only object, and trying to copy the `hold_any` object will throw a std::bad_function_call exception.
Likewise, the `hold_any` can only be casted to a reference to this type.

Example:

```cpp

// create any from unique_ptr
seq::any a = std::unique_ptr<int>(new int(3));
// retrieve a reference to the unique_ptr
std::unique_ptr<int>& val1 = a.cast< std::unique_ptr<int>&>();
std::cout << *val1 << std::endl;
	
// move any object
seq::any b = std::move(a);
std::unique_ptr<int>& val2 = b.cast< std::unique_ptr<int>&>();
std::cout << *val2 << std::endl;
	
// try to copy: throw std::bad_function_call as copy operator is disabled for unique_ptr
try {
	seq::any c = b;
}
catch (const std::bad_function_call&)
{
	std::cout << "cannot copy any object containing a unique_ptr" << std::endl;
}

``` 


## Extending hold_any interface



`hold_any` interface is easily extendable to provide additional members or modify its standard behavior.

Below is an example of extension that adds the info() member returning a `std::type_info` object and modify the hash function:

```cpp

using namespace seq;

// Define a new interface for hold_any
struct NewInterface
{
	// The base type info class, must inherit seq::any_type_info virtually
	struct type_info : virtual any_type_info
	{
		virtual const std::type_info& info() const = 0;
	};
	
	// Concrete implementation for given type, must inherit type_info and any_typed_type_info<T> (for base implementation of common functions)
	template<class T>
	struct typed_type_info : type_info, any_typed_type_info<T>
	{
		// implementation of additional info() member
		virtual const std::type_info& info() const { return typeid(T); }

		// new implementation for the hash function based on the old one
		virtual size_t hash_any(const void* in) const {
			return any_typed_type_info<T>{}.hash_any(in)* UINT64_C(0xc4ceb9fe1a85ec53);
		}
	};

	// Extended interface to seq::hold_any
	template<class Base>
	struct any_interface : Base
	{
		//add function to `hold_any` interface
		const std::type_info& info() const { return this->type()->info(); }
	};
};


using my_any = hold_any< NewInterface>;

my_any a  = 2;

// print the type name
std::cout<< a.info().name() << std::endl;
// print the hash value
std::cout<< a.hash() << std::endl;


``` 


Below is a more complex example that transform `hold_any` into a `std::function` equivalent:

```cpp

// for std::plus and std::multiplies (C++14)
#include <functional>

#include <seq/any.hpp>

// for seq::is_invocable
#include <seq/type_traits.hpp>




namespace seq
{
	template<class R, class T, class... As>
	SEQ_ALWAYS_INLINE typename std::enable_if< is_invocable<T, As ...>::value,R>::type call_fun(const T & fun,  As... as){
		return fun(std::forward<As>(as)...);
	}
	template<class R, class T, class... As>
	SEQ_ALWAYS_INLINE typename std::enable_if< !is_invocable<T, As ...>::value,R>::type call_fun(const T & fun,  As... as){
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
 


// Divide function 
int divide(int a, int b) {return a / b;}



// Usage

seq::function<int(int,int)> plus_fun = std::plus<int>{};					// affect functor
seq::function<int(int,int)> minus_fun = [](int a, int b){return a - b;}	;	// affect lambda
seq::function<int(int,int)> multiplies_fun = std::multiplies<int>{};		// affect functor
seq::function<int(int,int)> divide_fun = divide;							// affect function
	
assert( plus_fun(1,2) == 3);
assert( minus_fun(2,1) == 1);
assert( multiplies_fun(2,3) == 6);
assert( divide_fun(9,3) == 3);


``` 
