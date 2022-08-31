#pragma once

/** @file */

/**\defgroup format Format: Type safe formatting module

The format module provides fast routines for object formatting to string/streams. It is strongly typed and does not rely on string parsing to
find the output format. Therefore, almost all possible formatting errors are detected at compilation instead of runtime.

There are already several great C++ formatting libraries available in <a href="https://abseil.io/">Abseil</a>, <a href="https://github.com/facebook/folly">Folly</a> or the <a href="https://fmt.dev/latest/index.html">fmt</a> library.
Furtheremore, C++20 will provide a new text formatting library similar to the {fmt} one. The format module is an attempt to provide (yet) another formatting library which does not rely on string parsing,
is compatible with c++ streams and is <b>fast</b>.

It was designed first to output huge matrices or tables to files and strings.
Format module is based on the \ref charconv "charconv" module to format numerical values.

Formatting single values
------------------------

Format module heavily relies on the seq::fmt function to format single or several values.

When formatting a single value, seq::fmt returns a seq::ostream_format object providing several members to modify the formatting options:
	-	<b>base(int)</b>: specify the base for integral types, similar to b(int)
	-	<b>format(char)</b>: specify the format ('e', 'E', 'g', 'G', 'f') for floating point types, similar to t(char)
	-	<b>precision(int)</b>: specify the maximum precision for floating point types, similar to p(int)
	-	<b>dot(char)</b>: specify the dot character for floating point types, similar to d(char)
	-	<b>hex_prefix()</b>: add trailing '0x' for hexadecimal format, similar to h()
	-	<b>upper()</b>: output hexadecimal value in upper case, similar to u()
	-	<b>as_char()</b>: output integral value as an ascii character, similar to c()
	-	<b>left(int)</b>: align output to the left for given width, similar to l(int)
	-	<b>right(int)</b>: align output to the right for given width, similar to r(int)
	-	<b>center(int)</b>: center output for given width, similar to c(int)
	-	<b>fill(char)</b>: specify the filling character used for aligned output (default to space character), similar to f(char)

Usage:

\code{.cpp}

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

\endcode

The format module provides additional convenient functions to shorten the syntax even more:

\code{.cpp}

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

\endcode

seq::fmt calls can also be nested:

\code{.cpp}

using namespace seq;

// Nested formatting
std::cout << fmt(fmt(fmt(fmt("surrounded text")).c(20).f('*')).c(30).f('#')).c(40).f('-') << std::endl;

\endcode


Formatting several values
-------------------------

The seq::fmt function can be used to format any number of values at once:

\code{.cpp}

// Formatting multiple values

// Direct stream
std::cout << fmt("The answer is ", 43," ...") << std::endl;
// Direct stream with nested formatting
std::cout << fmt("...Or it could be", fmt(43.3,'e').c(10) ) << std::endl;

std::cout << std::endl;

// Reuse a formatting object built without arguments
auto f = fmt<int, tstring_view, double, tstring_view, double>();
std::cout << f(1, " + ", 2.2, " = ", 3.2) << std::endl;

std::cout << std::endl;

// Reuse a formatting object and use seq::null to only update some arguments
auto f2 = fmt(int(), " + ", fmt<double>().format('g'), " = ", fmt<double>().format('e'));
std::cout << f2(1, null, 2.2, null, 3.2) << std::endl;

std::cout << std::endl;

// Convert to string or tstring
std::string s1 = f2(1, null, 2.2, null, 3.2);	//equivalent to s1 = f2(1, null, 2.2, null, 3.2).str();
tstring s2 = f2(1, null, 2.2, null, 3.2);		//equivalent to s2 = f2(1, null, 2.2, null, 3.2).str<tstring>();

// Append to string
s2 += ", repeat-> ";
f2(1, null, 2.2, null, 3.2).append(s2);			// append formatted result to s2
std::cout << s2 <<std::endl;

std::cout << std::endl;

// Modify formatting object using get() and/or set()
f2.set<0>(fmt<int>().base(16).h().u()); // reset the formatting object at position 0
f2.get<2>().format('e');				// modifiy the formatting object at position 2
std::cout << f2(1, null, 2.2, null, 3.2) << std::endl;

std::cout << std::endl;

// Use positional argument
std::cout << f2(pos<0, 2, 4>(), 1, 2.2, 3.2) << std::endl; // provided arguments are used for positions 0, 2 and 4

// Positional directly in the fmt call
auto f3 = fmt(pos<0, 2, 4>(), int(), " + ", seq::g<double>(), " = ", seq::e<double>());
std::cout << f3(1, 2.2, 3.2) << std::endl;

std::cout << std::endl;

// Building tables

// header/trailer format, 2 columns of width 20 centered, separated by a '|'
auto header = fmt(pos<1, 3>(),"|", seq::str().c(20), "|", seq::str().c(20), "|");
//line format, 2 columns of width 20 centered, separated by a '|'
auto line = fmt(pos<1, 3>(),"|", seq::fmt<double>().c(20), "|", seq::fmt<double>().c(20), "|");
// write table
std::cout << header( "Header 1", "Header 2") << std::endl;
std::cout << line( 1.1, 2.2) << std::endl;
std::cout << line( 3.3, 4.4) << std::endl;
std::cout << header( "Trailer 1", "Trailer 2") << std::endl;

std::cout << std::endl;

\endcode


Formatting to string or buffer
------------------------------

A formatting object can be:
	-	Printed to a std::ostream object
	-	Converted to a string object
	-	Added to an existing string object
	-	Writed to a buffer

Example:

\code{.cpp}

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

\endcode


Using std::to_chars
-------------------

It is possible to use std::to_chars instead of seq::to_chars within the format module, mostly when exact round-trip guarantee is mandatory.
For that, you must define SEQ_FORMAT_USE_STD_TO_CHARS and enable C++17. If C++17 is not supported by the compiler, the format module will
always fallback to seq::to_chars.


Working with custom types
-------------------------

By default, the format library supports arithmetic types and string types. Not that std::string, seq::tstring or const char* arguments are represented internally as string views (tstring_view class).
The format module is extendible to custom types by 2 means:
	-	If the type is streamable to std::ostream, it will directly work with seq::fmt using internally a (slow) std::ostringstream. 
	-	Otherwise, you need to specialize seq::ostream_format for your type.

Example of custom type formatting:

\code{.cpp}

#include "format.hpp"
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

\endcode

For arithmetic types, a seq::ostream_format internally stores a copy of the value passed as argument of seq::fmt. Therefore, the ostream_format object
can be stored and formatted afterward. However, for custom types as well as strings, it is unsafe to store a ostream_format object and format it afterward
as it internally stores a <i>pointer</i> to the actual data.

Example:

\code{.cpp}

// Format arithmetic type

auto f = seq::fmt(1.2);
std::cout << f << std::endl;		// Safe: the ostream_format stores a plain double value

// Format string type

std::cout << seq::fmt("format a string")<< std::endl;					// Safe: lifetime of string literals is the lifetime of the program
std::cout << seq::fmt(std::string("format a string")) << std::endl;		// Safe: the temporay string is valid when the actual formatting occurs

auto f2 = seq::fmt(std::string("format a string"));
std::cout << f2(std::string("another string")) <<std::endl;		//Safe: the first string is replace by a new temporary one
std::cout << f2 << std::endl;									//UNSAFE: attempt to format the temporay std::string holding "another string" which was already destroyed

\endcode


Thread safety
-------------

The format module is thread safe: formatting objects in different threads is allowed, as the format module only uses (few) global variables with the <i>thread_local</i> specifier.
However, a formatting object returned by seq::fmt is not thread safe and you must pass copies of this object to other threads.


Performances
------------

The format module is relatively fast compared to C++ streams, mainly thanks to the \ref charconv "charconv" module.
Usually, using seq::fmt to output arithmetic values to streams should be around 8 times faster than directly writing the values to a std::ostream object.
This will, of course, vary greatly depending on the considered scenario.

The following code is a simple benchmark on writing a 4 * 1000000 table of double values to a std::ostream object.

\code{.cpp}

#include <iostream>
#include <iomanip>
#include <vector>
#include "testing.hpp"
#include "format.hpp"

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
	int el = tock_ms();
	std::cout << "Write table with streams: " <<el<<" ms"<< std::endl;


	// Build the same table with format module

	// Create the format object
	auto f = fmt(pos<0, 2, 4, 6>(), g<float_type>().p(6).c(20), "|", g<float_type>().p(6).c(20), "|", g<float_type>().p(6).c(20), "|", g<float_type>().p(6).c(20), "|");
	tick();
	for (size_t i = 0; i < vec_d.size() / 4; ++i)
		oss << f(vec_d[i * 4], vec_d[i * 4+1], vec_d[i * 4+2], vec_d[i * 4+3]) << std::endl;
	el = tock_ms();
	std::cout << "Write table with seq formatting module: " << el << " ms" << std::endl;


	// Use std::format
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

\endcode

Above example compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10 on a Intel(R) Core(TM) i7-10850H at 2.70GHz gives the following output:

> Write table with streams: 3469 ms
>
> Write table with seq formatting module: 413 ms
>
> Write centered double with seq::fmt: 366 ms


*/



/** \addtogroup format
 *  @{
 */


#include <type_traits>
#include <sstream>
#include <string>
#include <tuple>

#ifdef SEQ_HAS_CPP_17
#include <string_view>
#include <charconv>
#endif

#include "charconv.hpp"

#undef min
#undef max

namespace seq
{

	/// @brief Placehoder when reusing a formatting object
	struct null_format {};
	static const null_format null;

	/// @brief Class representing the width formatting for a any formatting object.
	///
	/// width_format controls how a string is aligned based on a width. The string can be aligned to the left, right or centered.
	/// If the given width is greater than the string length, the string is padded with a fill character. Otherwise, the string is truncated.
	/// Numerical values are never truncated.
	/// 
	struct width_format
	{
		/// @brief Possible alignment values
		enum Direction
		{
			align_none = 0,
			align_left = 1,
			align_right = 2,
			align_center = 3
		};

		// Width
		unsigned short width;
		// Alignment, one of Direction enum
		char alignment;
		// Fill character
		char pad;

		width_format(unsigned short w = 0, char align = 0, char f = ' ') noexcept : width(w), alignment(align), pad(f) {}

		/// @brief Align to the left with given width
		void left(unsigned short w) noexcept {
			width = w;
			alignment = align_left;
		}
		/// @brief Align to the right with given width
		void right(unsigned short w) noexcept {
			width = w;
			alignment = align_right;
		}
		/// @brief Align to the center with given width
		void center(unsigned short w) noexcept {
			width = w;
			alignment = align_center;
		}
		/// @brief Set the filling character
		void fill(char c) noexcept {
			pad = c;
		}
		/// @brief Reset alignment anf fill character
		void reset()noexcept {
			width = 0;
			alignment = 0;
			pad = ' ';
		}

		/// @brief Apply the alignment formatting to a portion of the string inplace. This might change the string size.
		/// @tparam String type
		/// @param str String in/out string object
		/// @param from format range [from,to)
		/// @param to format range [from,to)
		/// @param w formatting parameter
		template<class String>
		static void format(String& str, size_t from, size_t to, width_format w)
		{
			SEQ_ASSERT_DEBUG(to >= from, "");
			SEQ_ASSERT_DEBUG(from <= str.size(), "");
			SEQ_ASSERT_DEBUG(to <= str.size(), "");

			size_t f_size = to - from;
			if (f_size == w.width || w.alignment == width_format::align_none)
				return;

			if (f_size > w.width)
			{
				size_t diff = (f_size - w.width);
				//shrink string
				if (w.alignment == width_format::align_right) {
					memmove((char*)str.data() + from, str.data() + to - w.width, str.size() - to + w.width);
				}
				else if (w.alignment == width_format::align_left) {
					memmove((char*)str.data() + from + w.width, str.data() + to, str.size() - to);
				}
				else {
					memmove((char*)str.data() + from, str.data() + from + diff / 2, w.width);
					memmove((char*)str.data() + from + w.width, str.data() + to, str.size() - to);
				}
				str.resize(str.size() - diff);
			}
			else
			{
				//enlarge string
				size_t old_size = str.size();
				str.resize(str.size() - f_size + w.width);

				//move the right part
				if (to != str.size()) {
					memmove((char*)str.data() + from + w.width, str.data() + to, old_size - to);
				}

				if (w.alignment == width_format::align_right) {
					memmove((char*)str.data() + from + w.width - f_size, str.data() + from, f_size);
					memset((char*)str.data() + from, w.pad, w.width - f_size);

				}
				else if (w.alignment == width_format::align_center) {
					size_t s2 = (w.width - f_size) / 2;
					memmove((char*)str.data() + from + s2, str.data() + from, f_size);
					memset((char*)str.data() + from, w.pad, s2);
					memset((char*)str.data() + from + s2 + f_size, w.pad, w.width - (s2 + f_size));
				}
				else { //left
					memset((char*)str.data() + from + f_size, w.pad, w.width - f_size);
				}
			}
		}
	};


	namespace detail
	{
		enum FormatFlags
		{
			// Internal flag used by the numeric_format struct
			f_upper = 64,
			f_prefix = 128,
		};

		template<class String>
		static void format_width(String& str, size_t size, width_format w)
		{
			// Alignment formatting for numerical values

			if (w.alignment == width_format::align_none)
				return;

			if (w.alignment == width_format::align_right) {
				memmove((char*)str.data() + w.width - size, str.data(), size);
				memset((char*)str.data(), w.pad, w.width - size);

			}
			else if (w.alignment == width_format::align_center) {
				size_t s2 = (w.width - size) / 2;
				memmove((char*)str.data() + s2, str.data(), size);
				memset((char*)str.data(), w.pad, s2);
				memset((char*)str.data() + s2 + size, w.pad, w.width - (s2 + size));
			}
			else {
				memset((char*)str.data() + size, w.pad, w.width - size);
			}
		}



		static inline std::string& ostream_buffer()
		{
			// Returns buffer suitable to write values into a std::ostream
			static thread_local std::string tmp;
			return tmp;
		}
		static inline std::string& numeric_buffer()
		{
			// Returns buffer suitable to write numerical values to string
			static thread_local std::string tmp;
			return tmp;
		}
		static inline std::string& to_chars_buffer()
		{
			// Returns buffer suitable to write multi_ostream_format values into a std::ostream
			static thread_local std::string tmp;
			return tmp;
		}


		// Helper class to append ostream_format to a string
		template<class Ostream, bool IsArithmetic = std::is_arithmetic<typename Ostream::value_type>::value >
		struct AppendHelper
		{

			template<class String>
			static String& append(String& out, const Ostream& d)
			{
				// For all types except arithmetic ones:
				// - use to_string() to append formatted value to out
				// - apply the alignment formatting if not already done by the formatter

				size_t prev = out.size();
				d.to_string(out);
				if (!Ostream::auto_width_format) {
					// non default ostream_format, the width formatting must be applied
					if (d.alignment()) {
						width_format::format(out, prev, out.size(), d.width_fmt());
					}
				}
				return out;
			}
		};
		template<class Ostream >
		struct AppendHelper<Ostream, true>
		{
			template<class String>
			static String& append(String& out, const Ostream& d)
			{
				// Append arithmetic value: use a temporary buffer

				std::string& tmp = numeric_buffer();
				tmp.clear();
				size_t s = d.to_string(tmp);
				out.append(tmp.data(), s);
				return out;
			}

		};




		/// @brief Tells if T is one of format library basic type (arithmetic or tstring_view types)
		template<class T, bool IsBasicType = (std::is_arithmetic<T>::value || is_tiny_string<T>::value) >
		struct is_fmt_basic_type
		{
			static constexpr bool value = IsBasicType;
		};

		/// @brief Hold a value (basic type) or a const pointer (complex types)
		template<class T, bool IsBasicType = is_fmt_basic_type<T>::value >
		struct ValueHolder
		{
			T _value;
			ValueHolder() : _value() {}
			ValueHolder(const T& value) : _value(value) {}
			T& value() noexcept { return _value; }
			const T& value() const noexcept { return _value; }
			void set_value(const T& value) { _value = value; }
		};
		template<class T >
		struct ValueHolder<T, false>
		{
			const T * _value;
			ValueHolder() : _value(NULL) {}
			ValueHolder(const T& value) : _value(&value) {}
			T& value() noexcept { return *_value; }
			const T& value() const noexcept { return *_value; }
			void set_value(const T& value) { _value = &value; }
		};


	}// end detail



	/// @brief Formatting options for arithmetic types
	class numeric_format
	{

		using uchar = unsigned char;
		using ushort = unsigned short;
		using uint = unsigned int;

		char	_base_or_format;			// base for integral, format ('e', 'E', 'g', 'G', 'f' ) for floating point types
		uchar	_dot;						// dot character for floating point types, used to print char for integral types (if 'c')
		uchar	_precision_or_formatting;	// precision for float (limitted to 255), upper and prefix for integral

	public:

		numeric_format()
			:_base_or_format(10), _dot('.'), _precision_or_formatting(6) {}
		numeric_format(char base_or_format)
			:_base_or_format(base_or_format), _dot('.'), _precision_or_formatting(6) {}
		
		/// @brief Returns the base for integral types
		char base() const noexcept { return _base_or_format; }
		/// @brief Returns the format for integral types
		char format() const noexcept { return _base_or_format; }
		/// @brief Returns the format dot character for floating point types
		char dot() const noexcept { return (char)_dot; }
		/// @brief Returns the precision for floating point types
		unsigned char precision() const noexcept { return _precision_or_formatting; }
		/// @brief Returns the formatting options for integral types
		unsigned char formatting() const noexcept { return _precision_or_formatting; }

		/// @brief Set the base for integral types
		numeric_format& base(char b) noexcept {
			//static_assert(std::is_integral<T>::value, "'base' property only supported for integral types");
			_base_or_format = (uchar)b;
			return *this;
		}
		/// @brief Set the base for integral types
		numeric_format& b(char _b) noexcept { return base(_b); }

		/// @brief Set the format for floating point types
		numeric_format& format(char b) noexcept {
			//static_assert(std::is_floating_point<T>::value, "'format' property only supported for floating point types");
			_base_or_format = (uchar)b;
			return *this;
		}
		/// @brief Set the format for floating point types
		numeric_format& t(char f) noexcept { return format(f); }


		/// @brief Set the precision for floating point types, default to 6
		numeric_format& precision(int p) noexcept {
			//static_assert(std::is_floating_point<T>::value, "'precision' property only supported for floating point types");
			_precision_or_formatting = (uchar)p;
			return *this;
		}
		/// @brief Set the precision for floating point types, default to 6
		numeric_format& p(int _p) noexcept { return precision(_p); }

		/// @brief For integral types only and base > 10, output upper case characters
		numeric_format& upper() noexcept {
			//static_assert(std::is_integral<T>::value, "'upper' property only supported for integral types");
			_precision_or_formatting |= (uchar)detail::f_upper;
			return *this;
		}
		/// @brief For integral types only and base > 10, output upper case characters
		numeric_format& u() noexcept { return upper(); }

		/// @brief For integral types only and base == 16, output '0x' prefix
		numeric_format& hex_prefix() noexcept {
			//static_assert(std::is_integral<T>::value, "'hex_prefix' property only supported for integral types");
			_precision_or_formatting |= (uchar)detail::f_prefix;
			return *this;
		}
		/// @brief For integral types only and base == 16, output '0x' prefix
		numeric_format& h() noexcept { return hex_prefix(); }

		/// @brief For floating point types only, set the dot character (default to '.')
		numeric_format& dot(char d) noexcept {
			//static_assert(std::is_floating_point<T>::value, "'dot' property only supported for floating point types");
			_dot = (uchar)d;
			return *this;
		}
		/// @brief For floating point types only, set the dot character (default to '.')
		numeric_format& d(char _d) noexcept { return dot(_d); }

		/// @brief Print integral value as a character
		numeric_format& as_char() noexcept{return dot('c');}
		/// @brief Print integral value as a character
		numeric_format& c() noexcept { return dot('c'); }
	};


	
	/// @brief Base class for formatting objects
	/// @tparam T type to format
	/// @tparam Derived Curiously recurring template pattern 
	/// 
	/// This class can be derived to provide custom formatting
	/// 
	template<class T, class Derived>
	struct base_ostream_format
	{
	protected:

		// Store a T object by value or const pointer
		detail::ValueHolder<T> _value;
		
		// All ostream_format support the width formatting and arithmetic formatting
		width_format _width;
		numeric_format _format;


	public:

		static constexpr bool auto_width_format = false;
		static constexpr bool is_formattable = true;
		using value_type = T;

		base_ostream_format() : _value(), _width(), _format(std::is_floating_point<T>::value ? 'g' : (char)10) {}
		base_ostream_format(const T& val) : _value(val), _width(), _format(std::is_floating_point<T>::value ? 'g' : (char)10) {}
		base_ostream_format(const T& val, char base_or_format) : _value(val), _width(), _format(base_or_format) {}
		base_ostream_format(const T& val, const numeric_format& fmt) : _value(val), _width(), _format(fmt) {}


		//getters
		char base() const noexcept { return _format.base(); }
		char format() const noexcept { return _format.format(); }
		char dot() const noexcept { return (char)_format.dot(); }
		unsigned char precision() const noexcept { return _format.precision(); }
		unsigned char formatting() const noexcept { return _format.formatting(); }

		/// @brief Returns the value held by this base_ostream_format
		T& value() noexcept { return _value.value(); }
		/// @brief Returns the value held by this base_ostream_format
		const T& value() const noexcept { return _value.value(); }

		/// @brief Returns derived object
		Derived& derived() noexcept { return static_cast<Derived&>(*this); }
		/// @brief Returns derived object
		const Derived& derived() const noexcept { return static_cast<const Derived&>(*this); }

		/// @brief Returns the arithmetic format options
		numeric_format numeric_fmt() const { return _format; }
		/// @brief Returns the width format options
		width_format width_fmt() const noexcept { return _width; }

		void set_width_format(const width_format& f) { _width = f; }
		void set_numeric_format(const numeric_format& f) { _format = f; }

		/// @brief Returns width value of the width formatting options
		unsigned short width() const noexcept { return _width.width; }
		/// @brief Returns fill character of the width formatting options
		char fill_character() const noexcept { return _width.pad; }
		/// @brief Returns alignment value of the width formatting options
		char alignment() const noexcept { return _width.alignment; }

		/// @brief Set the exact width and the alignment
		Derived& left(int w)noexcept {
			_width.left((unsigned short)w);
			return derived();
		}
		/// @brief Set the exact width and the alignment
		Derived& l(int w) { return left(w); }

		/// @brief Set the exact width and the alignment
		Derived& right(int w)noexcept {
			_width.right((unsigned short)w);
			return derived();
		}
		/// @brief Set the exact width and the alignment
		Derived& r(int w) noexcept { return right(w); }

		/// @brief Set the exact width and the alignment
		Derived& center(int w)noexcept {
			_width.center((unsigned short)w);
			return derived();
		}
		/// @brief Set the exact width and the alignment
		Derived& c(int w) noexcept { return center(w); }

		/// @brief Reset alignment options
		Derived& no_align() noexcept {
			_width.reset();
			return derived();
		}
		/// @brief Set the fill character, used with left(), right() and center()
		Derived& fill(char f) noexcept {
			_width.fill(f);
			return derived();
		}
		/// @brief Set the fill character, used with left(), right() and center()
		Derived& f(char _f) noexcept { return fill(_f); }

		/// @brief Set the base for integral types
		Derived& base(char b) noexcept {
			_format.base(b);
			return derived();
		}
		/// @brief Set the base for integral types
		Derived& b(char _b) noexcept { return base(_b); }

		/// @brief Set the format for floating point types
		Derived& format(char f) noexcept {
			_format.format(f);
			return derived();
		}
		/// @brief Set the format for floating point types
		Derived& format(seq::chars_format f, bool upper = false) noexcept {
			if(!upper) _format.format(f == fixed ? 'f' : (f == general ? 'g' : 'e'));
			else  _format.format(f == fixed ? 'F' : (f == general ? 'G' : 'E'));
			return derived();
		}
		/// @brief Set the format for floating point types
		Derived& t(char f) noexcept { return format(f); }
		Derived& t(seq::chars_format f, bool upper = false) noexcept {return format(f, upper);}

		/// @brief Set the precision for floating point types, default to 6
		Derived& precision(int p) noexcept {
			_format.precision(p);
			return derived();
		}
		/// @brief Set the precision for floating point types, default to 6
		Derived& p(int _p) noexcept { return precision(_p); }

		/// @brief For integral types only and base > 10, output upper case characters
		Derived& upper() noexcept {
			_format.upper();
			return derived();
		}
		/// @brief For integral types only and base > 10, output upper case characters
		Derived& u() noexcept { return upper(); }

		/// @brief For integral types only and base == 16, output '0x' prefix
		Derived& hex_prefix() noexcept {
			_format.hex_prefix();
			return derived();
		}
		/// @brief For integral types only and base == 16, output '0x' prefix
		Derived& h() noexcept { return hex_prefix(); }

		/// @brief For floating point types only, set the dot character (default to '.')
		Derived& dot(char d) noexcept {
			_format.dot(d);
			return derived();
		}
		/// @brief For floating point types only, set the dot character (default to '.')
		Derived& d(char _d) noexcept { return dot(_d); }

		/// @brief For integral types, print the value as a character
		Derived& as_char() noexcept {
			_format.as_char();
			return derived();
		}
		/// @brief For integral types, print the value as a character
		Derived& c() noexcept { 
			return as_char();
		}


		/// @brief Copy v to this ostream_format
		/// @param v input value
		/// @return reference to *this
		Derived& operator()(const T& v) {
			_value.set_value(v);
			return derived();
		}

		/// @brief operator() using placeholder object, no-op
		Derived& operator()(const null_format&) {
			return derived();
		}
		/// @brief Equivalent to dervied() = other
		Derived& operator()(const Derived& other) {
			return derived() = other;
		}


		/// @brief Conversion operator to std::string
		operator std::string() const {
			return str();
		}
		/// @brief Conversion operator to tiny_string
		template<size_t Ss, class Al>
		operator tiny_string<Ss, Al>() const {
			return str<tiny_string<Ss, Al>>();
		}

		/// @brief Convert the formatting object to String
		template<class String = std::string>
		String str() const {
			String res;
			return append(res);
		}

		/// @brief Append the formatting object to a string-like object
		template<class String>
		String& append(String& out) const
		{
			return detail::AppendHelper<Derived>::append(out, derived());
		}

		char* to_chars(char* dst)
		{
			std::string& tmp = detail::to_chars_buffer();
			tmp.clear();
			this->append(tmp);
			memcpy(dst, tmp.data(), tmp.size());
			return dst + tmp.size();
		}

		std::pair<char*, size_t> to_chars(char* dst, size_t max)
		{
			std::string& tmp = detail::to_chars_buffer();
			tmp.clear();
			this->append(tmp);
			if (tmp.size() > max) {
				memcpy(dst, tmp.data(), max);
				return { dst + max,tmp.size() };
			}
			memcpy(dst, tmp.data(), tmp.size());
			return { dst + tmp.size(),tmp.size() };
		}

	};


	namespace detail
	{

		// Helper class to write a ostream_format to string
		template<class T, bool IsIntegral = std::is_integral<T>::value, bool IsFloat = std::is_floating_point<T>::value, bool IsString = is_tiny_string<T>::value, bool IsStreamable = is_ostreamable<T>::value>
		struct OstreamToString
		{
			template<class String, class Ostream>
			static size_t write(String& out, const Ostream& val)
			{
				// Default implementation: use ostringstream

				std::ostringstream oss;
				oss << val.value();
				size_t prev = out.size();
				out.append(oss.str());

				// Apply alignment
				if (val.alignment()) 
					width_format::format(out, prev, out.size(), val.width_fmt());
				
				return out.size() - prev;
			}
		};
		template<class T>
		struct OstreamToString<T,false,false,false,false>
		{};
		template<class T>
		struct OstreamToString<T, false, false, true, true>
		{
			template<class String, class Ostream>
			static size_t write(String& out, const Ostream& val)
			{
				// Specialization for tstring_view
				return val.write_string_to_string(out, val);
			}
		};
		template<class T>
		struct OstreamToString<T, true, false, false, true>
		{
			template<class String, class Ostream>
			static size_t write(String& out, const Ostream& val)
			{
				// Specialization for integral types
				return val.write_integral_to_string(out, val);
			}
		};
		template<class T>
		struct OstreamToString<T, false, true, false, true>
		{
			template<class String, class Ostream>
			static size_t write(String& out, const Ostream& val)
			{
				// Specialization for floating point types
				return val.write_float_to_string(out, val);
			}
		};


		// check if type T can be formatted based on default behavior
		template<class T, class Ostream>
		class is_default_formattable
		{
			template<class TT>
			static auto test(int)
				-> decltype(OstreamToString<TT>::write(std::declval<std::string&>(), std::declval<const Ostream&>()), std::true_type());

			template<class>
			static auto test(...)->std::false_type;
			
		public:
			static const bool value = decltype(test<T>(0))::value;
		};
	}


	


	/// @brief Number and string formatting class.
	/// @tparam T type of object to format
	/// 
	/// ostream_format is used in conjunction to fmt to format integers, floating point values and strings.
	/// An ostream_format object can be converted to std::string or tiny_string, and can be streamed to a std::ostream object.
	/// 
	/// Using ostream_format instead of std::to_string or streaming the value directly to std::ostream has 2 goals:
	///		-	Speed: streaming numerical values with fmt is an order of magnitude faster than the default std::ostream behavior.
	///			The integral and floating point formatting is obtained with to_chars.
	///		-	Local formatting: each call to fmt produces output with its own formatting options, without switching the std::ostream
	///			options like std::ostream::width() or std::ostream::left(). fmt does not use locale either. Note that you can still use a global ostream_format object
	///			to format several values with the same formatting options.
	/// 
	/// The following examples give a better idea of ostream_format usage.
	/// \code{.cpp}
	/// 
	/// std::cout << fmt(1.2) << std::endl;
	/// std::cout << fmt(1.2,'E') << std::endl;						//scientific notation
	/// std::cout << fmt(1.23456789,'E').precision(4) << std::endl;	//bounded maximum precision
	/// std::cout << fmt(1.2).dot(',') << std::endl;					//change dot
	/// std::cout << fmt(1.2).right(10).fill('-') << std::endl;		//align to the right
	/// std::cout << fmt(1.2).left(10).fill('-') << std::endl;			//align to the left
	/// std::cout << fmt(1.2).center(10).fill('-') << std::endl;		//align to the center
	/// std::cout << fmt(123456,16).hex_prefix().upper() << std::endl;	//hexadecimal upper case with '0x' prefix
	/// std::cout << fmt("hello").c(10).f('*') << std::endl;			//center string, use dense notation
	/// std::cout << fmt("hello").c(3) << std::endl;					//center and truncate string
	/// 
	/// std::string str = fmt(1.2);									//direct string conversion
	/// std::string str2 = "the value is " + fmt(1.2).str();			//direct string conversion
	/// \endcode
	/// 
	/// A ostream_format can be stored in order to format several values using the same formatting options:
	/// 
	/// \code{.cpp}
	/// auto f = fmt<int>().b(16).h().u();	//integer formatting : hexadecimal upper case with '0x' prefix
	/// 
	/// for(int i=0; i < 100; ++i)
	///		std::cout<<f(i)<<std::endl;		//use the same formatting options for all values
	/// 
	/// \endcode
	/// 
	/// A ostream_format object can be converted to a tstring_view. This is possible as the ostream_format object is internally 
	/// written inside a global thread_local string. Therefore, the tstring_view is only valid within the current thread, and should NOT
	/// be used in another thread.
	/// 
	/// 
	template<class T>
	class ostream_format : public base_ostream_format<T, ostream_format<T> >
	{
		using base_type = base_ostream_format<T, ostream_format<T> >;

		template<class U>
		friend class ostream_format;

		template<class U, bool IsIntegral, bool IsFloat, bool IsString, bool IsStreamable>
		friend struct detail::OstreamToString;
	public:


		static constexpr bool auto_width_format = true;

		// set flag telling if type T can be formatted with ostream_format
		static constexpr bool is_formattable = detail::is_default_formattable<T, ostream_format<T> >::value;

		/// @brief Default constructor. 
		///
		/// If the type is integral, initialize to base 10.
		/// If the type ia floating point, initialize the format with 'g'.
		ostream_format()
			: base_type() {}

		ostream_format(const T& value)
			:base_type(value) {}

		/// @brief Construct from a value and a base or format value
		/// @param value input value
		/// @param base_or_format the base for integral type (default to 10), the format for floating point types (like 'g')
		ostream_format(const T& value, char base_or_format)
			:base_type(value, base_or_format) {}

		ostream_format(const T& value, const numeric_format& fmt)
			:base_type(value, fmt) {}


		template<class String>
		size_t to_string(String& str) const
		{
			return detail::OstreamToString<T>::write(str, *this);
		}

	private:



		template<class String, class U>
		size_t write_integral_to_string(String& tmp, const ostream_format<U>& val) const
		{
			to_chars_result f;
			size_t size;
			int min_cap = std::max(14, (int)val.width());
			if (SEQ_UNLIKELY(tmp.capacity() < (size_t)min_cap)) {
				tmp.reserve(min_cap);
			}

			if (val.dot() == 'c') {
				//print as char
				(char&)*tmp.data() = (char)val.value();
				size = 1;
			}
			else {
				for (;;) {
					f = detail::write_integral(detail::char_range((char*)tmp.data(), (char*)tmp.data() + tmp.capacity()), val.value(),
						val.base(),
						integral_chars_format(0, val.formatting() & detail::f_prefix, val.formatting() & detail::f_upper)
					);
					if (SEQ_LIKELY(f.ec == std::errc()))
						break;
					tmp.reserve(tmp.capacity() * 2);
				}
				size = f.ptr - tmp.data();
			}

			
			if (val.width() > size) {
				detail::format_width(tmp, size, val.width_fmt());
				size = val.width();
			}
			return size;
		}

		template<class String, class U>
		size_t write_float_to_string(String& tmp, const ostream_format<U>& val) const
		{
			int min_cap = std::max(14, (int)val.width());
			if (SEQ_UNLIKELY(tmp.capacity() < min_cap)) {
				tmp.reserve(min_cap);
			}

			char fmt = detail::to_upper(val.format());
			bool upper = val.format() <= 'Z';

#if defined( SEQ_FORMAT_USE_STD_TO_CHARS) && defined(SEQ_HAS_CPP_17)
			std::to_chars_result f;
			std::chars_format format = fmt == 'E' ? std::chars_format::scientific : (fmt == 'F' ? std::chars_format::fixed : std::chars_format::general);
			for (;;) {
				f = to_chars((char*)tmp.data(), (char*)tmp.data() + tmp.capacity(), val.value(), format, val.precision());
				if (SEQ_LIKELY(f.ec == std::errc()))
					break;
				tmp.reserve(tmp.capacity() * 2);
			}
#else
			char exp = upper ? 'E' : 'e';
			chars_format format = fmt == 'E' ? scientific : (fmt == 'F' ? fixed : general);
			to_chars_result f;

			for (;;) {
				f = seq::to_chars((char*)tmp.data(), (char*)tmp.data() + tmp.capacity(), val.value() , format, val.precision(), val.dot(), exp, upper);
				if (SEQ_LIKELY(f.ec == std::errc()))
					break;
				tmp.reserve(tmp.capacity() * 2);
			}
#endif
			size_t size = f.ptr - tmp.data();
			if (val.width() > size) {
				detail::format_width(tmp, size, val.width_fmt());
				size = val.width();
			}
			return size;
		}

		template<class String, class U>
		size_t write_string_to_string(String& tmp, const ostream_format<U>& val) const
		{
			size_t prev = tmp.size();
			size_t size = val.value().size();
			const size_t w = val.width();
			if (w && w != size) {
				if (w > size) {
					size_t fill = w - size;
					if (val.width_fmt().alignment == width_format::align_right) {
						tmp.append(fill, val.width_fmt().pad);
						tmp.append(val.value().data(), size);
					}
					else if (val.width_fmt().alignment == width_format::align_center) {
						fill /= 2;
						tmp.append(fill, val.width_fmt().pad);
						tmp.append(val.value().data(), size);
						fill = w - size - fill;
						tmp.append(fill, val.width_fmt().pad);
					}
					else {
						tmp.append(val.value().data(), size);
						tmp.append(fill, val.width_fmt().pad);
					}
				}
				else if (w < size) {
					if (val.width_fmt().alignment == width_format::align_right) {
						tmp.append(val.value().data() + (size - w), w);
					}
					else if (val.width_fmt().alignment == width_format::align_center) {
						tmp.append(val.value().data() + (size - w) / 2, (w));
					}
					else {
						tmp.append(val.value().data(), w);
					}
				}
			}
			else
				tmp.append(val.value().data(), size);
			return tmp.size() - prev;
		}


	};

	
	
	/// @brief Nested ostream_format
	template<class T>
	class ostream_format<ostream_format<T> > : public base_ostream_format<ostream_format<T>, ostream_format<ostream_format<T> > >
	{
		using base_type = base_ostream_format<ostream_format<T>, ostream_format<ostream_format<T> > >;

	public:

		ostream_format()
			: base_type() {}

		ostream_format(const ostream_format<T>& value)
			:base_type(value) {}

		template<class String>
		size_t to_string(String& str) const
		{
			size_t prev = str.size();
			this->value().append(str);
			return str.size() - prev;
		}

	};

}



namespace std
{
	/// @brief Write a ostream_format object to a std::ostream 
	template<class Elem, class Traits, class T>
	static SEQ_ALWAYS_INLINE basic_ostream<Elem, Traits>& operator<<(basic_ostream<Elem, Traits>& oss, const seq::ostream_format<T>& val)
	{
		std::string& tmp = seq::detail::ostream_buffer();
		tmp.clear();
		size_t s = val.to_string(tmp);
		if (!seq::ostream_format<T>::auto_width_format) {
			// non default ostream_format, the width formatting must be applied
			if (val.alignment()) {
				seq::width_format::format(tmp, 0, tmp.size(), val.width_fmt());
				s = tmp.size();
			}
		}
		oss.rdbuf()->sputn(tmp.data(), (std::streamsize)s);
		return oss;
	}
}













namespace seq
{
	namespace detail
	{
		//
		// Helper structure used to build a ostream_format based on a type T
		//

		template<typename T>
		struct FormatWrapper
		{
			// Default behavior: use ostream_format<T>
			using type = ostream_format<T>;
		};
		template<typename T>
		struct FormatWrapper<const T&>
		{
			using type = ostream_format<T>;
		};
		template<typename T>
		struct FormatWrapper<T&>
		{
			using type = ostream_format<T>;
		};

		template<typename T>
		struct FormatWrapper<ostream_format<T> >
		{
			// Avoid nesting ostream_format
			using type = ostream_format<T>;
		};
		template<typename T>
		struct FormatWrapper<const ostream_format<T>&>
		{
			using type = ostream_format<T>;
		};
		template<typename T>
		struct FormatWrapper<ostream_format<T>&>
		{
			using type = ostream_format<T>;
		};

		template<>
		struct FormatWrapper<char*>
		{
			// Literal strings should produce ostream_format<tstring_view>
			using type = ostream_format<tstring_view>;
		};
		template<>
		struct FormatWrapper<const char*>
		{
			using type = ostream_format<tstring_view>;
		};
		template<>
		struct FormatWrapper<std::string>
		{
			// ALL string classes should produce ostream_format<tstring_view>
			using type = ostream_format<tstring_view>;
		};

#ifdef SEQ_HAS_CPP_17
		template<>
		struct FormatWrapper<std::string_view>
		{
			// ALL string classes should produce ostream_format<tstring_view>
			using type = ostream_format<tstring_view>;
		};
#endif

		template<size_t S, class Alloc>
		struct FormatWrapper<tiny_string<S, Alloc> >
		{
			using type = ostream_format<tstring_view>;
		};
		template<size_t N>
		struct FormatWrapper<char[N] >
		{
			using type = ostream_format<tstring_view>;
		};
		template<size_t N>
		struct FormatWrapper<const char[N] >
		{
			using type = ostream_format<tstring_view>;
		};
		template<size_t N>
		struct FormatWrapper<char const (&)[N] >
		{
			using type = ostream_format<tstring_view>;
		};

		// meta-function which yields FormatWrapper<Element>::type from Element
		template<class Element>
		struct apply_wrapper
		{
			using result = typename FormatWrapper<Element>::type;
		};

		namespace metafunction
		{
			template<class MetaFunction> using result_of = typename MetaFunction::result;


			template<class Tuple, template<class> class Function>
			struct transform_elements;

			// meta-function which takes a tuple and a unary metafunction
			// and yields a tuple of the result of applying the metafunction
			// to each element_type of the tuple.
			// type: binary metafunction
			// arg1 = the tuple of types to be wrapped
			// arg2 = the unary metafunction to apply to each element_type
			// returns tuple<result_of<arg2<element>>...> for each element in arg1

			template<class...Elements, template<class> class UnaryMetaFunction>
			struct transform_elements<std::tuple<Elements...>, UnaryMetaFunction>
			{
				template<class Arg> using function = UnaryMetaFunction<Arg>;
				using result = std::tuple
					<
					result_of<function<Elements>>...
					>;
			};
		}


		static inline std::string& multi_ostream_buffer()
		{
			// Returns buffer suitable to write multi_ostream_format values into a std::ostream
			static thread_local std::string tmp;
			return tmp;
		}



		/// @brief Convert a tuple of ostream_format to string 
		template<class Tuple, int N = std::tuple_size<Tuple>::value >
		struct Converter
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;

			template<class String>
			static void convert(String& out, const Tuple& t)
			{
				static constexpr int pos = tuple_size - N;
				std::get<pos>(t).append(out);
				Converter<Tuple, N - 1>::convert(out, t);
			}
		};
		template<class Tuple>
		struct Converter<Tuple, 0>
		{
			template<class String>
			static void convert(String&, const Tuple&)
			{}
		};

		/// @brief Write tuple of ostream_format to string 
		template<class Tuple, int N = std::tuple_size<Tuple>::value >
		struct ToOstream
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;

			template<class Stream>
			static void convert(Stream& out, const Tuple& t)
			{
				std::string& tmp = multi_ostream_buffer();
				tmp.clear();

				const auto& v = std::get<tuple_size - N>(t);
				size_t s = v.to_string(tmp);

				using ostream_type = typename std::tuple_element<tuple_size - N, Tuple>::type;
				if (!ostream_type::auto_width_format) {
					// non default ostream_format, the width formatting must be applied
					if (v.alignment()) {
						width_format::format(tmp, 0, tmp.size(), v.width_fmt());
						s = tmp.size();
					}
				}
				out.rdbuf()->sputn(tmp.data(), (std::streamsize)s);
				ToOstream<Tuple, N - 1>::convert(out, t);
			}
		};
		template<class Tuple>
		struct ToOstream<Tuple, 0>
		{
			template<class Stream>
			static void convert(Stream&, const Tuple&)
			{}
		};





		/// @brief Affect new values to all members of a tuple of ostream_format objects
		template<class Tuple, int N = std::tuple_size<Tuple>::value >
		struct AffectValues
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;

			template<class Out, class A, class ...Args>
			static void convert(Out& out, const A& a, Args&&... args)
			{
				std::get<tuple_size - N>(out)(a);
				AffectValues<Tuple, N - 1>::convert(out, std::forward<Args>(args)...);
			}
		};
		template<class Tuple>
		struct AffectValues<Tuple, 0>
		{
			template<class Out, class ...Args>
			static void convert(Out&, Args&&... args)
			{}
		};






		// Retrieve a constant value from  a Positional object
		template <size_t N, size_t... T>
		struct GetP;

		template <size_t N, size_t T1, size_t... Ts>
		struct GetP<N, T1, Ts...>
		{
			static constexpr size_t value = GetP<N - 1, Ts...>::value;
		};

		template <size_t T, size_t... Ts>
		struct GetP<0, T, Ts...>
		{
			static constexpr size_t value = T;
		};

		/// @brief Positinal type returned by seq::pos() function
		template<size_t... T>
		struct Positional
		{
			static constexpr size_t size = sizeof...(T);
			template<size_t Pos>
			struct get
			{
				static constexpr size_t value = GetP<Pos, T...>::value;
			};
		};



		/// @brief Affect new values to all members of a tuple of ostream_format objects.
		/// Use a Positional object to only updated some members of the tuple.
		/// 
		template<class Pos, class Tuple, int N = Pos::size >
		struct AffectValuesWithPos
		{
			static constexpr size_t pos_size = Pos::size;

			template<class Out, class A, class ...Args>
			static void convert(Out& out, const A& a, Args&&... args)
			{
				static constexpr size_t pos = pos_size - N;
				using get_type = typename Pos::template get<pos>;
				static constexpr size_t tuple_pos = get_type::value;
				std::get<tuple_pos>(out)(a);
				AffectValuesWithPos<Pos, Tuple, N - 1>::convert(out, std::forward<Args>(args)...);
			}
		};
		template<class Pos, class Tuple>
		struct AffectValuesWithPos<Pos, Tuple, 0>
		{
			template<class Out, class ...Args>
			static void convert(Out&, Args&&... args)
			{}
		};






		/// @brief Formatting class for multiple values, returned by seq::fmt(... , ...).
		/// @tparam Tuple tuple of ostream_format
		/// @tparam Pos possible Positional type
		template<class Tuple, class Pos = void>
		struct mutli_ostream_format
		{
			using tuple_type = Tuple;
			tuple_type d_tuple;

			mutli_ostream_format()
				:d_tuple()
			{}
			template<class ...Args>
			explicit mutli_ostream_format(Args&&... args)
				// Construct from multiple values.
				// Mark as explicit otherwise it replaces the default copy constructor
				: d_tuple(std::forward<Args>(args)...)
			{}
			

			template<class ...Args>
			mutli_ostream_format& operator()(Args&&... args)
			{
				// Update internal tuple with new values.
				AffectValues<Tuple>::convert(d_tuple, std::forward<Args>(args)...);
				return *this;
			}
			template<size_t ...T, class ...Args>
			mutli_ostream_format& operator()(Positional<T...>, Args&&... args)
			{
				// update internal tuple with new values for given indexes
				AffectValuesWithPos<Positional<T...>, Tuple>::convert(d_tuple, std::forward<Args>(args)...);
				return *this;
			}

			template<size_t N, class T>
			void set(const T& value) {
				std::get<N>(d_tuple)(value);
			}
			template<size_t N>
			typename std::tuple_element<N, Tuple>::type& get() noexcept {
				return std::get<N>(d_tuple);
			}
			template<size_t N>
			const typename std::tuple_element<N, Tuple>::type& get() const noexcept {
				return std::get<N>(d_tuple);
			}

			template<class String = std::string>
			String& append(String& out) const
			{
				// append the content of this formatting object to a string-like object
				Converter<Tuple>::convert(out, d_tuple);
				return out;
			}

			char* to_chars(char* dst)
			{
				std::string& tmp = detail::to_chars_buffer();
				tmp.clear();
				this->append(tmp);
				memcpy(dst, tmp.data(), tmp.size());
				return dst + tmp.size();
			}

			std::pair<char*, size_t> to_chars(char* dst, size_t max)
			{
				std::string& tmp = detail::to_chars_buffer();
				tmp.clear();
				this->append(tmp);
				if (tmp.size() > max) {
					memcpy(dst, tmp.data(), max);
					return { dst + max,tmp.size() };
				}
				memcpy(dst, tmp.data(), tmp.size());
				return { dst + tmp.size(),tmp.size() };
			}

			template<class String = std::string>
			String str() const
			{
				// convert this formatting object to string-like object
				String res;
				return append(res);
			}

			operator std::string() const
			{
				// convertion operator to std::string
				return str();
			}
			template<size_t S, class Al>
			operator tiny_string<S, Al>() const
			{
				// convertion operator to tiny_string
				return str< tiny_string<S, Al> >();
			}
		};

		template<class Tuple, size_t ...Ts>
		struct mutli_ostream_format<Tuple, Positional<Ts...>>
		{
			// Partial specialization of mutli_ostream_format that supports Positional type as template argument

			using tuple_type = Tuple;
			using pos_type = Positional<Ts...>;
			tuple_type d_tuple;

			mutli_ostream_format()
				:d_tuple()
			{}
			template<class ...Args>
			explicit mutli_ostream_format(Args&&... args)
				// Construct from multiple values.
				// Mark as explicit otherwise it replaces the default copy constructor
				: d_tuple(std::forward<Args>(args)...)
			{}

			template<class ...Args>
			mutli_ostream_format& operator()(Args&&... args)
			{
				AffectValuesWithPos<pos_type, Tuple>::convert(d_tuple, std::forward<Args>(args)...);
				return *this;
			}
			template<size_t ...T, class ...Args>
			mutli_ostream_format& operator()(Positional<T...>, Args&&... args)
			{
				AffectValuesWithPos<Positional<T...>, Tuple>::convert(d_tuple, std::forward<Args>(args)...);
				return *this;
			}

			template<size_t N, class T>
			void set(const T& value){
				std::get<N>(d_tuple)(value);
			}
			template<size_t N>
			typename std::tuple_element<N, Tuple>::type& get() noexcept {
				return std::get<N>(d_tuple);
			}
			template<size_t N>
			const typename std::tuple_element<N, Tuple>::type& get() const noexcept {
				return std::get<N>(d_tuple);
			}

			template<class String = std::string>
			String& append(String& out) const
			{
				Converter<Tuple>::convert(out, d_tuple);
				return out;
			}
			template<class String = std::string>
			String str() const
			{
				String res;
				Converter<Tuple>::convert(res, d_tuple);
				return res;
			}

			operator std::string() const
			{
				return str();
			}
			template<size_t S, class Al>
			operator tiny_string<S, Al>() const
			{
				return str< tiny_string<S, Al> >();
			}
		};

	}// end detail

	

	/// @brief Returns a positional object used either by seq::fmt() or operator() of formatting object 
	template <size_t... Ts>
	detail::Positional<Ts...> pos()
	{
		return detail::Positional<Ts...>();
	}


	namespace detail
	{
		// Helper class to build a mutli_ostream_format when multiple arguments are given to seq::fmt(),
		// or a ostream_format when only one argument is given to seq::fmt().

		template<size_t NArgs, class Pos, class ...Args>
		struct BuildFormat
		{
			using wrapped_tuple = metafunction::result_of<metafunction::transform_elements<std::tuple<Args...>, apply_wrapper>>;
			using return_type = mutli_ostream_format< wrapped_tuple, Pos>;

			static return_type build(Args&&... args)
			{
				return return_type(std::forward<Args>(args)...);
			}
		};

		
		template<class ...Args>
		struct BuildFormat<1, void, Args...>
		{
			using type = typename std::remove_reference< Args...>::type;
			using return_type = ostream_format<type>;

			static return_type build(Args&&... args)
			{
				return return_type(std::forward<Args>(args)...);
			}
		};

	}

	
	template<class ...Args>
	auto fmt(Args&&... args) -> typename detail::BuildFormat< sizeof...(Args), void, Args...>::return_type
	{
		// Returns a formatting object (multi_ostream_format or ostream_format) for given input values
		using return_type = typename detail::BuildFormat< sizeof...(Args) , void, Args...>::return_type;
		return return_type(std::forward<Args>(args)...);
	}

	template<size_t ...Ts, class ...Args>
	auto fmt(detail::Positional<Ts...>, Args&&... args) -> typename detail::BuildFormat< sizeof...(Args), detail::Positional<Ts...>, Args...>::return_type
	{
		// Returns a formatting object (multi_ostream_format or ostream_format) for given input values
		// The Positional type is embedded within the multi_ostream_format type.
		using return_type = typename detail::BuildFormat< sizeof...(Args), detail::Positional<Ts...>, Args...>::return_type;
		return return_type(std::forward<Args>(args)...);
	}


	template<class ...Args>
	auto fmt() ->  typename detail::BuildFormat< sizeof...(Args), void, Args...>::return_type
	{
		// Returns a default-initialized formatting object (multi_ostream_format or ostream_format) for given types
		using return_type = typename detail::BuildFormat< sizeof...(Args), void, Args...>::return_type;
		return return_type();
	}


	static inline ostream_format<float> fmt(float value, char format = 'g')
	{
		// Floating point formatting
		return ostream_format<float>(value, format);
	}
	
	static inline ostream_format<double> fmt(double value, char format = 'g')
	{
		// Floating point formatting
		return ostream_format<double>(value, format);
	}
	
	static inline ostream_format<long double> fmt(long double value, char format = 'g')
	{
		// Floating point formatting
		return ostream_format<long double>(value, format);
	}

	
	static inline ostream_format<tstring_view> fmt(const char* str)
	{
		// String formatting
		return ostream_format<tstring_view>(tstring_view(str));
	}
	
	static inline ostream_format<tstring_view> fmt(const char* str, size_t size)
	{
		return ostream_format<tstring_view>(tstring_view(str, size));
	}
	
	static inline ostream_format<tstring_view> fmt(const std::string& str)
	{
		// String formatting
		return ostream_format<tstring_view>(tstring_view(str.data(), str.size()));
	}
	
	template<size_t Ss, class Al>
	static inline ostream_format<tstring_view> fmt(const tiny_string<Ss, Al>& str)
	{
		// String formatting
		return ostream_format<tstring_view>(tstring_view(str.data(), str.size()));
	}

	static inline ostream_format<tstring_view> fmt(const tstring_view& str)
	{
		// String formatting
		return ostream_format<tstring_view>(str);
	}
#ifdef SEQ_HAS_CPP_17
	static inline ostream_format<tstring_view> fmt(const std::string_view& str)
	{
		// String formatting
		return ostream_format<tstring_view>(tstring_view(str.data(), str.size()));
	}
#endif

	template<class T>
	static inline ostream_format<T> e(T val = T())
	{
		// Helper function for floating point formatting
		return fmt(val,'e');
	}
	template<class T>
	static inline ostream_format<T> E(T val = T())
	{
		// Helper function for floating point formatting
		return fmt(val, 'E');
	}
	template<class T>
	static inline ostream_format<T> g(T val = T())
	{
		// Helper function for floating point formatting
		return fmt(val, 'g');
	}
	template<class T>
	static inline ostream_format<T> G(T val = T())
	{
		// Helper function for floating point formatting
		return fmt(val, 'G');
	}
	template<class T>
	static inline ostream_format<T> f(T val = T())
	{
		// Helper function for floating point formatting
		return fmt(val, 'f');
	}
	template<class T>
	static inline ostream_format<T> F(T val = T())
	{
		// Helper function for floating point formatting
		return fmt(val, 'F');
	}

	template<class T>
	static inline ostream_format<T> hex(T val = T())
	{
		// Helper function for integral formatting
		return fmt(val).base(16);
	}
	template<class T>
	static inline ostream_format<T> oct(T val = T())
	{
		// Helper function for integral formatting
		return fmt(val).base(8);
	}
	template<class T>
	static inline ostream_format<T> bin(T val = T())
	{
		// Helper function for integral formatting
		return fmt(val).base(2);
	}

	template<class T>
	static inline ostream_format<T> ch(T val = T())
	{
		// Format an integral value as a character
		return fmt(val).c();
	}

	static inline ostream_format<tstring_view> str()
	{
		// Null string formatting, used with seq::fmt().
		// Ex.: fmt(pos<1, 3>(),"|", seq::str().c(20), "|", seq::str().c(20), "|");
		return fmt<tstring_view>();
	}



	template<class T>
	struct is_formattable
	{
		static constexpr bool value = ostream_format<T>::is_formattable;//decltype(test<T>(0))::value;
	};

} // end namespace seq




namespace std
{
	/// @brief Write a ostream_format object to a std::ostream object
	template<class Elem, class Traits, class T, class P>
	static SEQ_ALWAYS_INLINE basic_ostream<Elem, Traits>& operator<<(basic_ostream<Elem, Traits>& oss, const seq::detail::mutli_ostream_format<T, P>& val)
	{
		seq::detail::ToOstream<T>::convert(oss, val.d_tuple);
		return oss;
	}
}



/** @}*/
//end charconv
