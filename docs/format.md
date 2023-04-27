# Format: Type safe formatting module

The *format* module provides fast routines for object formatting to string/streams using C++11 only. It is strongly typed and does not rely on string parsing to find the output format. Therefore, almost all possible formatting errors are detected at compilation instead of runtime.

There are already several great C++ formatting libraries available in <a href="https://abseil.io/">Abseil</a>, <a href="https://github.com/facebook/folly">Folly</a> or the <a href="https://fmt.dev/latest/index.html">fmt</a> library.
Furtheremore, C++20 will provide a new text formatting library similar to the {fmt} one. The *format* module is an attempt to provide (yet) another formatting library which does not rely on string parsing, is compatible with c++ streams and is <b>fast</b>.

It was designed first to output huge matrices or tables to files and strings. *Format* module is based on the [charconv](charconv.md) module to format numerical values.

## Formatting single values

*Format* module heavily relies on the `seq::fmt` function to format single or several values.

When formatting a single value, `seq::fmt` returns a `seq::ostream_format` object providing several members to modify the formatting options:
-	<b>base(int)</b>: specify the base for integral types, similar to `b(int)`
-	<b>format(char)</b>: specify the format ('e', 'E', 'g', 'G', 'f') for floating point types, similar to `t(char)`
-	<b>precision(int)</b>: specify the maximum precision for floating point types, similar to `p(int)`
-	<b>dot(char)</b>: specify the dot character for floating point types, similar to `d(char)`
-	<b>hex_prefix()</b>: add trailing '0x' for hexadecimal format, similar to `h()`
-	<b>upper()</b>: output hexadecimal value in upper case, similar to `u()`
-	<b>as_char()</b>: output integral value as an ascii character, similar to `c()`
-	<b>left(int)</b>: align output to the left for given width, similar to `l(int)`
-	<b>right(int)</b>: align output to the right for given width, similar to `r(int)`
-	<b>center(int)</b>: center output for given width, similar to `c(int)`
-	<b>fill(char)</b>: specify the filling character used for aligned output (default to space character), similar to `f(char)`

Usage:

```cpp

// Usage for formatting one value

using namespace seq;

const double PI = 3.14159265358979323846;

std::cout << fmt(PI) << std::endl;										//default double formatting
std::cout << fmt(PI,'E') << std::endl;									//scientific notation, equivalent to fmt(PI).format('E') or fmt(PI).t('E')
std::cout << fmt(PI,'E').precision(12) << std::endl;					//scientific notation with maximum precision, equivalent to fmt(PI).t('E').precision(12) or fmt(PI).t('E').p(12)
std::cout << fmt(PI).dot(',') << std::endl;								//change dot, equivalent to fmt(PI).d(',')
std::cout << fmt(PI).right(10).fill('-') << std::endl;					//align to the right and pad with '-', equivalent to fmt(PI).r(10).f('-')
std::cout << fmt(PI).left(10).fill('-') << std::endl;					//align to the left and pad with '-', equivalent to fmt(PI).l(10).f('-')
std::cout << fmt(PI).center(10).fill('-') << std::endl;					//align to the center and pad with '-', equivalent to fmt(PI).c(10).f('-')
std::cout << fmt(123456).base(16).hex_prefix().upper() << std::endl;	//hexadecimal upper case with '0x' prefix. equivalent to fmt(123456).b(16).h().u() or hex(123456).h().u()
std::cout << fmt("hello").c(10).f('*') << std::endl;					//center string and pad with '*', equivalent to fmt("hello").center(10).fill('*')
std::cout << fmt("hello").c(3) << std::endl;							//center and truncate string

// Direct string conversion
std::string str = fmt(PI);
// Direct string conversion using .str()
std::string str2 = "PI value is " + fmt(PI).str();
std::cout << str2 << std::endl;

```

The *format* module provides additional convenient functions to shorten the syntax even more:

```cpp

using namespace seq;

// Convenient shortcut functions

std::cout << ch('u') << std::endl;	//equivalent to fmt('u').as_char() or fmt('u').c()
std::cout << e(1.2) << std::endl;	//equivalent to fmt(1.2,'e') or fmt(1.2).format('e') or fmt(1.2).t('e')
std::cout << E(1.2) << std::endl;	//equivalent to fmt(1.2,'E') or fmt(1.2).format('E') or fmt(1.2).t('E')
std::cout << f(1.2) << std::endl;	//equivalent to fmt(1.2,'f') or fmt(1.2).format('f') or fmt(1.2).t('f')
std::cout << F(1.2) << std::endl;	//equivalent to fmt(1.2,'F') or fmt(1.2).format('F') or fmt(1.2).t('F')
std::cout << g(1.2) << std::endl;	//equivalent to fmt(1.2,'g') or fmt(1.2).format('g') or fmt(1.2).t('g')
std::cout << G(1.2) << std::endl;	//equivalent to fmt(1.2,'G') or fmt(1.2).format('G') or fmt(1.2).t('G')
std::cout << d(1) << std::endl;		//equivalent to fmt(1)
std::cout << u(1) << std::endl;		//equivalent to fmt(1)
std::cout << hex(100) << std::endl;	//equivalent to fmt(100).base(16) or fmt(100).b(16)
std::cout << x(100) << std::endl;	//equivalent to fmt(100).base(16) or fmt(100).b(16)
std::cout << X(100) << std::endl;	//equivalent to fmt(100).base(16).upper() or fmt(100).b(16).u()
std::cout << oct(100) << std::endl;	//equivalent to fmt(100).base(8) or fmt(100).b(8)
std::cout << o(100) << std::endl;	//equivalent to fmt(100).base(8) or fmt(100).b(8)
std::cout << bin(100) << std::endl;	//equivalent to fmt(100).base(2) or fmt(100).b(2)
std::cout << str("hello world") << std::endl;	//equivalent to fmt("hello world")

```

`seq::fmt` calls can also be nested:

```cpp

using namespace seq;

// Nested formatting
std::cout << fmt(fmt(fmt(fmt("surrounded text")).c(20).f('*')).c(30).f('#')).c(40).f('-') << std::endl;

```
Output:

> -----#####**surrounded text***#####-----


## Formatting several values


The `seq::fmt` function can be used to format any number of values at once.
The following example displays a few possibilities of multiple formatting: 

```cpp

// Formatting multiple values

// Stream a formatting object composed of multiple arguments
std::cout << fmt("The answer is ", 42 ," ...") << std::endl;

// Stream a formatting object composed of multiple arguments with nested formatting
std::cout << fmt("...Or it could be", fmt(42.3,'e').c(10) ) << std::endl;

std::cout << std::endl;


// Create and use a formatting object (basically a functor) built without arguments
auto f = fmt<int, tstring_view, double, tstring_view, double>();
// use the functor to stream values
std::cout << f(1, " + ", 2.2, " = ", 3.2) << std::endl;

std::cout << std::endl;


// Create a formatting object built without arguments and use seq::null to only update some arguments
auto f2 = fmt(int(), " + ", fmt<double>().format('g'), " = ", fmt<double>().format('e'));
std::cout << f2(1, null, 2.2, null, 3.2) << std::endl;

std::cout << std::endl;


// Convert to std::string or seq::tstring
std::string s1 = f2(1, null, 2.2, null, 3.2);	//equivalent to s1 = f2(1, null, 2.2, null, 3.2).str();
tstring s2 = f2(1, null, 2.2, null, 3.2);		//equivalent to s2 = f2(1, null, 2.2, null, 3.2).str<tstring>();


// Append to existing string
s2 += ", repeat-> ";
f2(1, null, 2.2, null, 3.2).append(s2);			// append formatted result to s2
std::cout << s2 <<std::endl;

std::cout << std::endl;


// Modify formatting object using get() and/or set()
f2.set<0>(fmt<int>().base(16).h().u()); // reset the formatting object at position 0
f2.get<2>().format('e');				// modifiy the formatting object at position 2
std::cout << f2(1, null, 2.2, null, 3.2) << std::endl;

std::cout << std::endl;


// Use positional argument with seq::pos function
std::cout << f2(pos<0, 2, 4>(), 1, 2.2, 3.2) << std::endl; // provided arguments are used for positions 0, 2 and 4

// Use positional directly in the seq::fmt call
auto f3 = fmt(pos<0, 2, 4>(), int(), " + ", seq::g<double>(), " = ", seq::e<double>());
std::cout << f3(1, 2.2, 3.2) << std::endl;

std::cout << std::endl;



// Use formatting module to build tables

// Create header/trailer functor, 2 columns of width 20 centered, separated by a '|'
auto header = fmt(pos<1, 3>(),"|", seq::str().c(20), "|", seq::str().c(20), "|");

// Create the line functor, 2 columns of width 20 centered, separated by a '|'
auto line = fmt(pos<1, 3>(),"|", seq::fmt<double>().c(20), "|", seq::fmt<double>().c(20), "|");

// Write very simple table composed of a 2 columns header, 2 lines of actual data, and & 2 columns trailer
std::cout << header( "Header 1", "Header 2") << std::endl;
std::cout << line( 1.1, 2.2) << std::endl;
std::cout << line( 3.3, 4.4) << std::endl;
std::cout << header( "Trailer 1", "Trailer 2") << std::endl;

std::cout << std::endl;

```


## Using formatting objects as functor

As seen above, a formatting object can be used as a functor. This way, the formatting object arguments are replaced by new values, but the formatting options are preserved:

```cpp

// build formatting functor
auto f = seq::fmt("PI is: ", E(3.14159265359));

// print the formatting functor with its default arguments :'PI is: 3.141593E+00'
std::cout<< f << std::endl;

// Print 'PI is not 3E+00'
std::cout<< f("PI is not ", 3) <<std::endl;

```

An argument can be skipped using `seq::none` argument:

```cpp

auto f = seq::fmt("PI is: ", E(3.14159265359));

// Print 'PI is: 3E+00'
std::cout<< f(seq::none, 3) <<std::endl;

```

Optionally, the functor can be built with a positional object as first parameter to only accept modifying some of its arguments:

```cpp

auto f = seq::fmt(seq::pos<1>(), "PI is: ", E(3.14159265359));

// Print 'PI is: 3E+00'
std::cout<< f(3) <<std::endl;

```

The format module provides an alternative solution to build custom formatting objects: slot arguments. A slot argument is defined with the function `_fmt`, and tells to the functor that only slot arguments can be modified.
All convient functions seen above provide a version starting with an underscore to build slots. Example:

```cpp

using namespace seq;

// in this example, _E() is equivalent to _fmt<double>().format('E').
auto f = fmt("The sum of ", _E(), " and ", _E(), " is equal to ",_E() );

// print 'The sum of 1.1E+00 and 2.2E+00 is equal to 3.3E+00'
std::cout<< f(1.1,2.2,3.3) <<std::endl;

// print 'The sum of 4E+00 and 5E+00 is equal to 9E+00'
std::cout<< f(4,5,9) <<std::endl;


// another example with string and custom string formatting
auto f2 = fmt("Hi, my name is ", _str().c(20).f('-'));

// print 'Hi, my name is -------Victor-------'
std::cout<<f2("Victor")<<std::endl;

```

The slot mechanism supports dynamically typed arguments using  the `seq::_any()` slot (or `seq::_a()`):

```cpp

auto f = seq::fmt("The result is :", _a());

// print 'The result is : 1'
std::cout<< f(1) <<std::endl;

// print 'The result is : 1.3'
std::cout<< f(1.3) <<std::endl;

// print 'The result is : this'
std::cout<< f("this") <<std::endl;

// print 'The result is : 1.3E00'
std::cout<< f(E(1.3)) <<std::endl;


```


## Nested formatting

Nested formatting occurs when using *fmt* calls within other *fmt* calls. The complexity comes from the argument replacement when using formatting objects as functors.
The following example shows how to use nested *fmt* calls with multiple arguments and argument replacement:

```cpp

// Build a formatting functor used to display 2 couples animal/species
auto f = fmt(
		"We have 2 couples:\nAnimal/Species: ",
		_fmt(_str(),"/",_str()).c(20),		//A couple Animal/Species centered on a 20 characters width string
		"\nAnimal/Species: ",
		_fmt(_str(),"/",_str()).c(20)		//Another couple Animal/Species centered on a 20 characters width string
	);

// Use this functor with custom values.
// fmt calls are also used to replace arguments in a multi-formatting functor
	std::cout << f(
		fmt("Tiger", "P. tigris"),
		fmt("Panda", "A. melanoleuca")
	) << std::endl;

```
Output:

> We have 2 couples:
>
> Animal/Species:   Tiger/P. tigris
>
> Animal/Species: Panda/A. melanoleuca


## Formatting to string or buffer


A formatting object can be:
-	Printed to a `std::ostream` object
-	Converted to a string object
-	Added to an existing string object
-	Writed to a buffer

Example:

```cpp

// Print to std::cout
std::cout<< seq::fmt(1.123456789,'g') << std::endl;

// Convert to string
std::string str = seq::fmt(1.123456789,'g');
std::cout<< str << std::endl;

// Append to an existing string
std::string str2;
seq::fmt(1.123456789,'g').append(str2);
std::cout<< str2 << std::endl;

// write to buffer (to_chars(char*) returns past-the-end pointer)
char dst[100];
*seq::fmt(1.123456789,'g').to_chars(dst) = 0;
std::cout<< dst << std::endl;

// write to buffer with maximum size (to_chars(char*,size_t) returns a pair of past-the-end pointer and size without truncation)
char dst2[100];
*seq::fmt(1.123456789,'g').to_chars(dst2, sizeof(dst2)).first = 0;
std::cout<< dst2 << std::endl;

```


## Using std::to_chars


It is possible to use `std::to_chars` instead of `seq::to_chars` within the *format* module, mostly when exact round-trip guarantee is mandatory.
For that, you must define `SEQ_FORMAT_USE_STD_TO_CHARS` and enable C++17. If C++17 is not supported by the compiler, the *format* module will always fallback to `seq::to_chars`.


## Working with custom types


By default, the *format* module supports arithmetic types and string types. Not that `std::string`, `seq::tstring` or `const char*` arguments are represented internally as string views (`seq::tstring_view` class).
The *format* module is extendible to custom types by 2 means:
-	If the type is streamable to `std::ostream`, it will directly work with `seq::fmt` using internally a (slow) `std::ostringstream`. 
-	Otherwise, you need to specialize `seq::ostream_format` for your type.

Example of custom type formatting:

```cpp

#include <seq/format.hpp>
#include <iostream>
#include <utility>



namespace seq
{
	// Specialization of ostream_format for std::pair<T,T>

	template<class T, bool S>
	class ostream_format<std::pair<T, T>,S> : public base_ostream_format<std::pair<T, T>, ostream_format<std::pair<T, T>,S > >
	{
		using base_type = base_ostream_format<std::pair<T, T>, ostream_format<std::pair<T, T>,S > >;

	public:

		ostream_format() : base_type() {}
		ostream_format(const std::pair<T, T>& v) : base_type(v) {}

		// The specialization must provide this member:

		size_t to_string(std::string& out) const
		{
			size_t prev = out.size();

			out.append("(");
			// Format the first member of the pair using the internal numeric format
			ostream_format<T>(this->value().first, this->numeric_fmt()).append(out);
			out.append(", ");
			// Format the second member of the pair using the internal numeric format
			ostream_format<T>(this->value().second, this->numeric_fmt()).append(out);
			out.append(")");

			return out.size() - prev;
		}
	};
}


int main(int argc, char ** argv)
{
	using namespace seq;

	// Formatting custom types
	std::cout << fmt("Print a pair of float: ", std::make_pair(1.2f, 3.4f)) << std::endl;

	// Formatting custom types with custom format
	std::cout << fmt("Print a pair of double: ", fmt(std::make_pair(1.2, 3.4)).format('e')) << std::endl;

	// Formatting custom types with custom format and alignment
	std::cout << fmt("Print a pair of double centered: ", fmt(std::make_pair(1.2, 3.4)).t('e').c(30).f('*')) << std::endl;


	// Formatting custom types using a formatting functor
	auto f = fmt("Print a pair of float: ", _fmt<std::pair<float, float> >());
	std::cout << f(std::make_pair(1.2f, 3.4f)) << std::endl;

	return 0;
}

```

For arithmetic types, a `seq::ostream_format` internally stores a copy of the value passed as argument of `seq::fmt`. 
Therefore, the ostream_format object can be stored and formatted afterward. 
However, for custom types as well as strings, it is unsafe to store a ostream_format object and format it afterward as it internally stores a <i>pointer</i> to the actual data.

Example:

```cpp

// Format arithmetic type

auto f = seq::fmt(1.2);
std::cout << f << std::endl;		// Safe: the ostream_format stores a plain double value

// Format string type

std::cout << seq::fmt("format a string")<< std::endl;					// Safe: lifetime of string literals is the lifetime of the program
std::cout << seq::fmt(std::string("format a string")) << std::endl;		// Safe: the temporay string is valid when the actual formatting occurs

auto f2 = seq::fmt(std::string("format a string"));
std::cout << f2(std::string("another string")) <<std::endl;		//Safe: the first string is replace by a new temporary one
std::cout << f2 << std::endl;									//UNSAFE: attempt to format the temporay std::string holding "another string" which was already destroyed

```


## Thread safety


The *format* module is thread safe: formatting objects in different threads is allowed, as the module only uses (few) global variables with the <i>thread_local</i> specifier.
However, a formatting object returned by `seq::fmt` is not thread safe and you must pass copies of this object to other threads.


## Merging arguments


The *format* module provides the `seq::join` function to merge several arguments with a string delimiter:

```cpp

// print '1, 2, 3'
std::cout<< seq::join(", ", 1,2,3) <<std::endl;

```

`seq::join` can be used to merge any iterable object:

```cpp

std::vector<int> vec = {1,2,3};

// print '1, 2, 3'
std::cout<< seq::join(", ", vec) <<std::endl;

// join a sub-part only
// print '1, 2'
std::cout<< seq::join(", ", seq::range(vec.begin(),vec.begin()+1) ) <<std::endl;

```

The object returned by `seq::join` is of the same type as the one returned by `seq::fmt`. Therefore it provides the same functionalities: positional arguments, slots, conversion to string...

```cpp

using namespace seq;

// Build functor with slots
auto f = join(", ", _d(), _d(), _d());

// print '1, 2, 3'
std::cout<< f(1,2,3) <<std::endl;


// conversion to string
std::string str = f(1,2,3);
// print '1, 2, 3'
std::cout<< str << std::endl;


// using _join as a slot argument
auto f2 = fmt( "Here is a list of ", _d() ," numbers: ", _join("," ,_d(), _d(), _d() ) );
// print 'Here is a list of 3 numbers: 1,2,3'
std::cout<< f2( 3, fmt(1,2,3) ) <<std::endl;

```


## Build tables

The *format* module provides ways to simply the process of building a markdown table. Note that all tables displayed in benchmarks are built using the format module.
The following complete example builds a table that displays the performances of std::vector, std::deque and std::list for back insertion and iteration.

```cpp


#include <list>
#include <deque>
#include <vector>

#include <seq/testing.hpp> // For SEQ_TEST, tick(), tock_ms(), reset_memory_usage() and get_memory_usage() 
#include <seq/format.hpp> // Obvious
#include <seq/any.hpp> // For the _a() slot


int main(int, char ** const)
{

	using namespace seq;

	// Build the line format. Use join() to add a '|' character in between columns. Use _a() to format anything with the supplied width modifiers.
	// The first column is left aligned with a width of 20 characters. The 3 remaining columns are centered on a 15 characters width.
	auto line = join("|", _a().l(20), _a().c(15), _a().c(15), _a().c(15), "");
	// Slot argument passed to line object. Displays a time measurement as unsigned integer followed by " ms" string
	auto slot = fmt(_u(), "ms");
	// Another slot argument passed to line object. Displays a time measurment as unsigned integer followed by " ms" string, and a memory measurement as unsigned integer followed by " MO" string.
	auto slot2 = fmt(_u(), "ms / ", _u(), "MO");

	// Output table header using the 'line' format object
	std::cout << line("Operation type", "std::vector", "std::deque", "std::list") << std::endl;

	// Output the  separator between table header and actual table content (something like ----|----|----|). 
	// Use reset() to clear the line content and set the fill character to '-'
	std::cout << line.reset('-') << std::endl;

	// Reset the fill character to ' ' (blank space)
	line.reset(' ');



	// Containers to benchmark
	std::vector<size_t> vec;
	std::deque<size_t> deq;
	std::list<size_t> lst;



	// Benchmark back insertion for std::vector, std::deque, std::list.
	// We measure the time spent and the program memory footprint afterward.

	reset_memory_usage();
	tick();
	for (size_t i = 0; i < 10000000; ++i)
		vec.push_back(i);
	size_t tvec = tock_ms(); //measure elapsed time
	size_t mvec = get_memory_usage() / 1000000; // measure program memory usage

	reset_memory_usage();
	tick();
	for (size_t i = 0; i < 10000000; ++i)
		deq.push_back(i);
	size_t tdeq = tock_ms();
	size_t mdeq = get_memory_usage() / 1000000; // measure program memory usage

	reset_memory_usage();
	tick();
	for (size_t i = 0; i < 10000000; ++i)
		lst.push_back(i);
	size_t tlst = tock_ms();
	size_t mlst = get_memory_usage() / 1000000;


	// Output measurments using the 'line' format object.
	// Note that in this situation, we must use the operator*() of the slot objects to create copies.
	// Indeed, passing values to the slot will modify it and return a reference. By calling slot2(...) several times
	// in the same instruction, the line object will only receive the last set values in slot2 (depending on function evaluation order).
	std::cout << line(
		"push_back",		// type of operation (left aligned on 20 characters)
		*slot2(tvec, mvec), // time and memory (centered on 15 characters)
		*slot2(tdeq, mdeq), // time and memory (centered on 15 characters)
		*slot2(tlst, mlst)	// time and memory (centered on 15 characters)
	) << std::endl;


	// Benchmark iteration

	tick();
	for (auto it = vec.begin(); it != vec.end(); ++it)
		SEQ_TEST(*it != 10000000); // Use SEQ_TEST to make sure the compiler wont 'optimize' the loop (and remove it)
	tvec = tock_ms();

	tick();
	for (auto it = deq.begin(); it != deq.end(); ++it)
		SEQ_TEST(*it != 10000000);
	tdeq = tock_ms();

	tick();
	for (auto it = lst.begin(); it != lst.end(); ++it)
		SEQ_TEST(*it != 10000000);
	tlst = tock_ms();

	// Output measurments
	std::cout << line(
		"iterate",		// type of operation (left aligned on 20 characters)
		*slot(tvec),	// time (centered on 15 characters)
		*slot(tdeq),	// time (centered on 15 characters)
		*slot(tlst)		// time (centered on 15 characters)
	) << std::endl;


	return 0;
}

```

This displays the following table (tested with msvc):

<!-- language: lang-none -->

	Operation type      |  std::vector  |  std::deque   |   std::list   |
	--------------------|---------------|---------------|---------------|
	push_back           |  55ms / 81MO  | 212ms / 228MO | 359ms / 322MO |
	iterate             |     19ms      |     58ms      |     72ms      |


## Performances


The *format* module is relatively fast compared to C++ streams, mainly thanks to the [charconv](charconv.md) module.
Usually, using `seq::fmt` to output floating point values to streams should be around 8 times faster than directly writing the values to a std::ostream object, less for integer types.
This will, of course, vary greatly depending on the considered scenario.

The following code is a simple benchmark on writing a 4 * 1000000 table of double values to a std::ostream object.

```cpp

#include <iostream>
#include <iomanip>
#include <vector>
#include <seq/testing.hpp>
#include <seq/format.hpp>

int main(int argc, char ** argv)
{
	using namespace seq;

	// Generate 4M double values
	using float_type = double;
	random_float_genertor<float_type> rgn;
	std::vector<float_type> vec_d;
	for (int i = 0; i < 4000000; ++i)
		vec_d.push_back(rgn());

	// Null ostream object
	nullbuf n;
	std::ostream oss(&n);
	oss.sync_with_stdio(false);

	// Build a table of 4 * 1000000 double values separated by a '|'. All values are centered on a 20 characters space
	tick();
	oss << std::setprecision(6);
	for (size_t i = 0; i < vec_d.size()/4; ++i)
	{
		oss << std::left << std::setw(20) << vec_d[i * 4] << "|";
		oss << std::left << std::setw(20) << vec_d[i * 4+1] << "|";
		oss << std::left << std::setw(20) << vec_d[i * 4+2] << "|";
		oss << std::left << std::setw(20) << vec_d[i * 4+3] << "|";
		oss << std::endl;
	}
	size_t el = tock_ms();
	std::cout << "Write table with streams: " <<el<<" ms"<< std::endl;


	// Build the same table with format module

	// Create the format object
	auto f = fmt(pos<0, 2, 4, 6>(), g<float_type>().p(6).c(20), "|", g<float_type>().p(6).c(20), "|", g<float_type>().p(6).c(20), "|", g<float_type>().p(6).c(20), "|");
	tick();
	for (size_t i = 0; i < vec_d.size() / 4; ++i)
		oss << f(vec_d[i * 4], vec_d[i * 4+1], vec_d[i * 4+2], vec_d[i * 4+3]) << std::endl;
	el = tock_ms();
	std::cout << "Write table with seq formatting module: " << el << " ms" << std::endl;


	// Compare to std::format for C++20 compilers
	// tick();
	// for (size_t i = 0; i < vec_d.size() / 4; ++i)
	// 	std::format_to(std::ostreambuf_iterator<char>(oss), "{:^20.6g} | {:^20.6g} | {:^20.6g} | {:^20.6g}\n", vec_d[i * 4], vec_d[i * 4 + 1], vec_d[i * 4 + 2], vec_d[i * 4 + 3]);
	// el = tock_ms();
	// std::cout << "Write table with std::format : " << el << " ms" << std::endl;


	// Just for comparison, directly dump the double values without the '|' character (but keeping centering)

	tick();
	auto f2 = seq::fmt(float_type(), 'g').c(20);
	for (size_t i = 0; i < vec_d.size(); ++i)
		oss << f2(vec_d[i]);
	el = tock_ms();
	std::cout << "Write centered double with seq::fmt: " << el << " ms" << std::endl;


	
	// use std::ostream::bad() to make sure the above tests are not simply ignored by the compiler
	if ((int)oss.bad())
		std::cout << "error" << std::endl;


	return 0;
}

```

Above example compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10 on a Intel(R) Core(TM) i7-10850H at 2.70GHz gives the following output:

> Write table with streams: 3469 ms
>
> Write table with seq formatting module: 413 ms
>
> Write centered double with seq::fmt: 366 ms
