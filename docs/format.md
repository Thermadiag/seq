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
std::cout << hex(100) << std::endl;	//equivalent to fmt(100).base(16) or fmt(100).b(16)
std::cout << oct(100) << std::endl;	//equivalent to fmt(100).base(8) or fmt(100).b(8)
std::cout << bin(100) << std::endl;	//equivalent to fmt(100).base(2) or fmt(100).b(2)

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


The `seq::fmt` function can be used to format any number of values at once:

```cpp

// Formatting multiple values

// Stream a formatting object composed of multiple arguments
std::cout << fmt("The answer is ", 43," ...") << std::endl;
// Stream a formatting object composed of multiple arguments with nested formatting
std::cout << fmt("...Or it could be", fmt(43.3,'e').c(10) ) << std::endl;

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


## Nested formatting

Nested formatting occurs when using *fmt* calls within other *fmt* calls. The complexity comes from the argument replacement when using formatting objects as functors.
The following example shows how to use nested *fmt* calls with multiple arguments and argument replacement:

```cpp

// Build a formatting functor used to display 2 couples animal/species
auto f = fmt(
		pos<1,3>(), //we can modifies positions 1 and 3 (the 2 couples animal/species)
		"We have 2 couples:\nAnimal/Species: ",
		fmt(pos<0,2>(),"","/","").c(20),	//A couple Animal/Species centered on a 20 characters width string
		"\nAnimal/Species: ",
		fmt(pos<0,2>(),"","/","").c(20)		//Another couple Animal/Species centered on a 20 characters width string
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
> Animal/Species:   Tiger/P. tigris
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
std::string str = seq::fmt(1.123456789,'g').str<std::string>();
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

	template<class T>
	class ostream_format<std::pair<T, T> >: public base_ostream_format<std::pair<T, T> , ostream_format<std::pair<T, T> > >
	{
		using base_type = base_ostream_format<std::pair<T, T>, ostream_format<std::pair<T, T> > >;

	public:

		ostream_format() : base_type() {}
		ostream_format(const std::pair<T, T>& v) : base_type(v) {}

		// The specialization must provide this member:

		size_t to_string(std::string & out) const
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


## Performances


The *format* module is relatively fast compared to C++ streams, mainly thanks to the [charconv](charconv.md) module.
Usually, using `seq::fmt` to output arithmetic values to streams should be around 8 times faster than directly writing the values to a std::ostream object.
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
