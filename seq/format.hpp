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

#ifndef SEQ_FORMAT_HPP
#define SEQ_FORMAT_HPP



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


Nested formatting
-----------------

Nested formatting occurs when using *fmt* calls within other *fmt* calls. The complexity comes from the argument replacement when using formatting objects as functors.
The following example shows how to use nested *fmt* calls with multiple arguments and argument replacement:

\code{.cpp}

// Build a formatting functor used to display 2 couples animal/species
auto f = fmt(
		pos<1,3>(), //we can modifies positions 1 and 3 (the 2 couples animal/species)
		"We have 2 couples:\nAnimal/Species: ",
		fmt(pos<0,2>(),"","/","").c(20),	//A couple Animal/Species centered on a 20 characters width string
		"\nAnimal/Species: ",
		fmt(pos<0,2>(),"","/","").c(20)		//Another couple Animal/Species centered on a 20 characters width string
	);

// Use this functor with custom values.
// fmt calls are used to replace arguments in a multi-formatting object
	std::cout << f(
		fmt("Tiger", "P. tigris"),
		fmt("Panda", "A. melanoleuca")
	) << std::endl;

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



#ifdef SEQ_HAS_CPP_17
#include <string_view>
#include <charconv>
#endif

#include <iostream>
#include <iomanip>
#include <tuple>
#include <sstream>

#include "charconv.hpp"
#include "range.hpp"

#undef min
#undef max

namespace seq
{

	/// @brief Placehoder when reusing a formatting object
	struct null_format {} ;
	static const null_format null;

	//forward declaration
	template<class T, bool Slot>
	class ostream_format;

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

		explicit width_format(unsigned short w = 0, char align = 0, char f = ' ') noexcept : width(w), alignment(align), pad(f) {}

		/// @brief Align to the left with given width
		width_format& left(unsigned short w) noexcept {
			width = w;
			alignment = align_left;
			return *this;
		}
		width_format& l(unsigned short w) noexcept {return left(w);}
		/// @brief Align to the right with given width
		width_format& right(unsigned short w) noexcept {
			width = w;
			alignment = align_right;
			return *this;
		}
		width_format& r(unsigned short w) noexcept { return right(w); }
		/// @brief Align to the center with given width
		width_format& center(unsigned short w) noexcept {
			width = w;
			alignment = align_center;
			return *this;
		}
		width_format& c(unsigned short w) noexcept { return center(w); }
		/// @brief Set the filling character
		width_format& fill(char c) noexcept {
			pad = c;
			return *this;
		}
		width_format& f(char c) noexcept { return fill(c); }
		/// @brief Reset alignment anf fill character
		width_format& reset()noexcept {
			width = 0;
			alignment = 0;
			pad = ' ';
			return *this;
		}

		bool has_format() const noexcept {
			return alignment != 0;
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
					memmove(const_cast<char*>(str.data()) + from, str.data() + to - w.width, str.size() - to + w.width);
				}
				else if (w.alignment == width_format::align_left) {
					memmove(const_cast<char*>(str.data()) + from + w.width, str.data() + to, str.size() - to);
				}
				else {
					memmove(const_cast<char*>(str.data()) + from, str.data() + from + diff / 2, w.width);
					memmove(const_cast<char*>(str.data()) + from + w.width, str.data() + to, str.size() - to);
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
					memmove(const_cast<char*>(str.data()) + from + w.width, str.data() + to, old_size - to);
				}

				if (w.alignment == width_format::align_right) {
					memmove(const_cast<char*>(str.data()) + from + w.width - f_size, str.data() + from, f_size);
					memset(const_cast<char*>(str.data()) + from, w.pad, w.width - f_size);

				}
				else if (w.alignment == width_format::align_center) {
					size_t s2 = (w.width - f_size) / 2;
					memmove(const_cast<char*>(str.data()) + from + s2, str.data() + from, f_size);
					memset(const_cast<char*>(str.data()) + from, w.pad, s2);
					memset(const_cast<char*>(str.data()) + from + s2 + f_size, w.pad, w.width - (s2 + f_size));
				}
				else { //left
					memset(const_cast<char*>(str.data()) + from + f_size, w.pad, w.width - f_size);
				}
			}
		}
	};


	namespace detail
	{
		enum FormatFlags
		{
			// Internal flags used by the numeric_format struct
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
				memmove(const_cast<char*>(str.data()) + w.width - size, str.data(), size);
				memset(const_cast<char*>(str.data()), w.pad, w.width - size);

			}
			else if (w.alignment == width_format::align_center) {
				size_t s2 = (w.width - size) / 2;
				memmove(const_cast<char*>(str.data()) + s2, str.data(), size);
				memset(const_cast<char*>(str.data()), w.pad, s2);
				memset(const_cast<char*>(str.data()) + s2 + size, w.pad, w.width - (s2 + size));
			}
			else {
				memset(const_cast<char*>(str.data()) + size, w.pad, w.width - size);
			}
		}



		static inline auto ostream_buffer() -> std::string&
		{
			// Returns buffer suitable to write values into a std::ostream
			static thread_local std::string tmp;
			return tmp;
		}
		static inline auto numeric_buffer() -> std::string&
		{
			// Returns buffer suitable to write numerical values to string
			static thread_local std::string tmp;
			return tmp;
		}
		static inline auto to_chars_buffer() -> std::string&
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
			static auto append(String& out, const Ostream& d) -> String&
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
			static auto append(String& out, const Ostream& d) -> String&
			{
				// Append arithmetic value: use a temporary buffer

				std::string& tmp = numeric_buffer();
				tmp.clear();
				size_t s = d.to_string(tmp);
				out.append(tmp.data(), s);
				return out;
			}

		};


		/// @brief Can be specialized to force inline storage of value inside ValueHolder
		/// @tparam T 
		template<class T>
		struct inline_value_storage : std::false_type
		{
		};


		/// @brief Tells if T is one of format library basic type (arithmetic or tstring_view types)
		template<class T, bool IsBasicType = (std::is_arithmetic<T>::value || is_tiny_string<T>::value || inline_value_storage<T>::value) >
		struct is_fmt_basic_type
		{
			static constexpr bool value = IsBasicType;
		};


		template<class Out, class In>
		typename std::enable_if<std::is_convertible<In,Out>::value, Out>::type HolderConvert(const In& v)
		{
			return static_cast<Out>(v);
		}
		template<class Out, class In>
		typename std::enable_if<!std::is_convertible<In, Out>::value, const In&>::type HolderConvert(const In& v)
		{
			return v;
		}

		/// @brief Hold a value (basic type) or a const pointer (complex types)
		template<class T, bool IsBasicType = is_fmt_basic_type<T>::value >
		struct ValueHolder
		{
			T _value;
			ValueHolder() : _value() {}
			explicit ValueHolder(const T& value) : _value(value) {}
			auto value() noexcept -> T& { return _value; }
			auto value() const noexcept -> const T& { return _value; }
			template<class U>
			void set_value(const U& value) { _value = HolderConvert<T>(value); }
			void reset() {_value = T();}
		};
		template<class T >
		struct ValueHolder<T, false>
		{
			const T * _value;
			ValueHolder() 
				: _value(nullptr) {}
			explicit ValueHolder(const T& value) 
				: _value(&value) {}
			auto value() noexcept -> T& { return *_value; }
			auto value() const noexcept -> const T& { return *_value; }
			void set_value(const T& value) { 
				_value = &value; 
			}
			void reset() {}
		};

		template<class T , bool Slot>
		struct ValueHolder<ostream_format<T, Slot>, false>
		{
			// Handle properly nested formats
			ostream_format<T, Slot> _value;
			ValueHolder() : _value() {}
			explicit ValueHolder(const ostream_format<T, Slot>& value) : _value(value) {}
			auto value() noexcept -> ostream_format<T, Slot>& { return _value; }
			auto value() const noexcept -> const ostream_format<T, Slot>& { return _value; }
			void set_value(const ostream_format<T, Slot>& value) { _value( value); }
			template<class U>
			void set_value(const U& value) { _value( value); }
			void reset() {_value.reset();}
		};


	}// end namespace detail



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
		explicit numeric_format(char base_or_format)
			:_base_or_format(base_or_format), _dot('.'), _precision_or_formatting(6) {}
		
		/// @brief Returns the base for integral types
		auto base() const noexcept -> char { return _base_or_format; }
		/// @brief Returns the format for integral types
		auto format() const noexcept -> char { return _base_or_format; }
		/// @brief Returns the format dot character for floating point types
		auto dot() const noexcept -> char { return static_cast<char>(_dot); }
		/// @brief Returns the precision for floating point types
		auto precision() const noexcept -> unsigned char { return _precision_or_formatting; }
		/// @brief Returns the formatting options for integral types
		auto formatting() const noexcept -> unsigned char { return _precision_or_formatting; }

		/// @brief Set the base for integral types
		auto base(char b) noexcept -> numeric_format& {
			//static_assert(std::is_integral<T>::value, "'base' property only supported for integral types");
			_base_or_format = (b);
			return *this;
		}
		/// @brief Set the base for integral types
		auto b(char _b) noexcept -> numeric_format& { return base(_b); }

		/// @brief Set the format for floating point types
		auto format(char b) noexcept -> numeric_format& {
			//static_assert(std::is_floating_point<T>::value, "'format' property only supported for floating point types");
			_base_or_format = (b);
			return *this;
		}
		/// @brief Set the format for floating point types
		auto t(char f) noexcept -> numeric_format& { return format(f); }


		/// @brief Set the precision for floating point types, default to 6
		auto precision(int p) noexcept -> numeric_format& {
			//static_assert(std::is_floating_point<T>::value, "'precision' property only supported for floating point types");
			_precision_or_formatting = static_cast<uchar>(p);
			return *this;
		}
		/// @brief Set the precision for floating point types, default to 6
		auto p(int _p) noexcept -> numeric_format& { return precision(_p); }

		/// @brief For integral types only and base > 10, output upper case characters
		auto upper() noexcept -> numeric_format& {
			//static_assert(std::is_integral<T>::value, "'upper' property only supported for integral types");
			_precision_or_formatting |= static_cast<uchar>(detail::f_upper);
			return *this;
		}
		/// @brief For integral types only and base > 10, output upper case characters
		auto u() noexcept -> numeric_format& { return upper(); }

		/// @brief For integral types only and base == 16, output '0x' prefix
		auto hex_prefix() noexcept -> numeric_format& {
			//static_assert(std::is_integral<T>::value, "'hex_prefix' property only supported for integral types");
			_precision_or_formatting |= static_cast<uchar>(detail::f_prefix);
			return *this;
		}
		/// @brief For integral types only and base == 16, output '0x' prefix
		auto h() noexcept -> numeric_format& { return hex_prefix(); }

		/// @brief For floating point types only, set the dot character (default to '.')
		auto dot(char d) noexcept -> numeric_format& {
			//static_assert(std::is_floating_point<T>::value, "'dot' property only supported for floating point types");
			_dot = static_cast<uchar>(d);
			return *this;
		}
		/// @brief For floating point types only, set the dot character (default to '.')
		auto d(char _d) noexcept -> numeric_format& { return dot(_d); }

		/// @brief Print integral value as a character
		auto as_char() noexcept -> numeric_format&{return dot('c');}
		/// @brief Print integral value as a character
		auto c() noexcept -> numeric_format& { return dot('c'); }
	};


	/// @brief Filler character
	struct filler
	{
		char c;
	};
	/// @brief Returns a filler object used to fill a string slot within a formatting expression
	/// @param c filler character
	/// @return filler object
	inline filler fill(char c) {
		return filler{ c };
	}

	
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

		base_ostream_format() : _value(), _width(), _format(std::is_floating_point<T>::value ? 'g' : static_cast<char>(10)) {}
		explicit base_ostream_format(const T& val) : _value(val), _width(), _format(std::is_floating_point<T>::value ? 'g' : static_cast<char>(10)) {}
		base_ostream_format(const T& val, char base_or_format) : _value(val), _width(), _format(base_or_format) {}
		base_ostream_format(const T& val, const numeric_format& fmt) : _value(val), _width(), _format(fmt) {}

		

		//getters
		auto base() const noexcept -> char { return _format.base(); }
		auto format() const noexcept -> char { return _format.format(); }
		auto dot() const noexcept -> char { return  static_cast<char>(_format.dot()); }
		auto precision() const noexcept -> unsigned char { return _format.precision(); }
		auto formatting() const noexcept -> unsigned char { return _format.formatting(); }

		/// @brief Returns the value held by this base_ostream_format
		auto value() noexcept -> T& { return _value.value(); }
		/// @brief Returns the value held by this base_ostream_format
		auto value() const noexcept -> const T& { return _value.value(); }

		/// @brief Returns derived object
		auto derived() noexcept -> Derived& { return static_cast<Derived&>(*this); }
		/// @brief Returns derived object
		auto derived() const noexcept -> const Derived& { return static_cast<const Derived&>(*this); }

		
		/// @brief reset content while setting the fill character
		auto reset(char c = 0) -> Derived& {
			_value.reset();
			if(c) fill(c);
			return derived();
		}

		/// @brief Returns the arithmetic format options
		auto numeric_fmt() const -> numeric_format { return _format; }
		/// @brief Returns the width format options
		auto width_fmt() const noexcept -> width_format { return _width; }

		void set_width_format(const width_format& f) { _width = f; }
		void set_numeric_format(const numeric_format& f) { _format = f; }

		/// @brief Returns width value of the width formatting options
		auto width() const noexcept -> unsigned short { return _width.width; }
		/// @brief Returns fill character of the width formatting options
		auto fill_character() const noexcept -> char { return _width.pad; }
		/// @brief Returns alignment value of the width formatting options
		auto alignment() const noexcept -> char { return _width.alignment; }

		/// @brief Set the exact width and the alignment
		auto left(int w)noexcept -> Derived& {
			_width.left(static_cast<unsigned short>(w));
			return derived();
		}
		/// @brief Set the exact width and the alignment
		auto l(int w) -> Derived& { return left(w); }

		/// @brief Set the exact width and the alignment
		auto right(int w)noexcept -> Derived& {
			_width.right(static_cast<unsigned short>(w));
			return derived();
		}
		/// @brief Set the exact width and the alignment
		auto r(int w) noexcept -> Derived& { return right(w); }

		/// @brief Set the exact width and the alignment
		auto center(int w)noexcept -> Derived& {
			_width.center(static_cast<unsigned short>(w));
			return derived();
		}
		/// @brief Set the exact width and the alignment
		auto c(int w) noexcept -> Derived& { return center(w); }

		/// @brief Reset alignment options
		auto no_align() noexcept -> Derived& {
			_width.reset();
			return derived();
		}
		/// @brief Set the fill character, used with left(), right() and center()
		auto fill(char f) noexcept -> Derived& {
			_width.fill(f);
			return derived();
		}
		/// @brief Set the fill character, used with left(), right() and center()
		auto f(char _f) noexcept -> Derived& { return fill(_f); }

		/// @brief Set the base for integral types
		auto base(char b) noexcept -> Derived& {
			_format.base(b);
			return derived();
		}
		/// @brief Set the base for integral types
		auto b(char _b) noexcept -> Derived& { return base(_b); }

		/// @brief Set the format for floating point types
		auto format(char f) noexcept -> Derived& {
			_format.format(f);
			return derived();
		}
		/// @brief Set the format for floating point types
		auto format(seq::chars_format f, bool upper) noexcept -> Derived& {
			if(!upper) _format.format(f == fixed ? 'f' : (f == general ? 'g' : 'e'));
			else  _format.format(f == fixed ? 'F' : (f == general ? 'G' : 'E'));
			return derived();
		}
		/// @brief Set the format for floating point types
		auto t(char f) noexcept -> Derived& { return format(f); }
		auto t(seq::chars_format f, bool upper) noexcept -> Derived& {return format(f, upper);}

		/// @brief Set the precision for floating point types, default to 6
		auto precision(int p) noexcept -> Derived& {
			_format.precision(p);
			return derived();
		}
		/// @brief Set the precision for floating point types, default to 6
		auto p(int _p) noexcept -> Derived& { return precision(_p); }

		/// @brief For integral types only and base > 10, output upper case characters
		auto upper() noexcept -> Derived& {
			_format.upper();
			return derived();
		}
		/// @brief For integral types only and base > 10, output upper case characters
		auto u() noexcept -> Derived& { return upper(); }

		/// @brief For integral types only and base == 16, output '0x' prefix
		auto hex_prefix() noexcept -> Derived& {
			_format.hex_prefix();
			return derived();
		}
		/// @brief For integral types only and base == 16, output '0x' prefix
		auto h() noexcept -> Derived& { return hex_prefix(); }

		/// @brief For floating point types only, set the dot character (default to '.')
		auto dot(char d) noexcept -> Derived& {
			_format.dot(d);
			return derived();
		}
		/// @brief For floating point types only, set the dot character (default to '.')
		auto d(char _d) noexcept -> Derived& { return dot(_d); }

		/// @brief For integral types, print the value as a character
		auto as_char() noexcept -> Derived& {
			_format.as_char();
			return derived();
		}
		/// @brief For integral types, print the value as a character
		auto c() noexcept -> Derived& { 
			return as_char();
		}


		/// @brief Copy v to this ostream_format
		/// @param v input value
		/// @return reference to *this
		auto operator()(const T& v) -> Derived& {
			_value.set_value(v);
			return derived();
		}
		template<class U>
		auto operator()(const U& v) -> Derived& {
			_value.set_value(v);
			return derived();
		}

		/// @brief operator() using placeholder object, no-op
		auto operator()(const null_format& /*unused*/) -> Derived& {
			return derived();
		}
		/// @brief Equivalent to dervied() = other
		auto operator()(const Derived& other) -> Derived& {
			return derived() = other;
		}
		template<class U, bool S>
		auto operator()(const ostream_format<U,S>& other) -> Derived& {
			_value.set_value(static_cast<T>(other.value()));
			set_numeric_format(other.numeric_fmt());
			set_width_format(other.width_fmt());
			return derived();
		}

		/// @brief Set the fill character with the parenthesis operator 
		auto operator()(filler v) -> Derived& {
			this->fill(v.c);
			_value.reset();
			return derived();
		}


		/// @brief Conversion operator to std::string
		template<class Traits, class Al>
		operator std::basic_string<char,Traits,Al>() const {
			return str<std::basic_string<char, Traits, Al> >();
		}
		/// @brief Conversion operator to tiny_string
		template<class Traits, size_t Ss, class Al>
		operator tiny_string<char,Traits,Al, Ss>() const {
			return str<tiny_string<char, Traits, Al, Ss> >();
		}

		/// @brief Convert the formatting object to String
		template<class String = std::string>
		auto str() const -> String {
			String res;
			return append(res);
		}

		/// @brief Append the formatting object to a string-like object
		template<class String>
		auto append(String& out) const -> String&
		{
			return detail::AppendHelper<Derived>::append(out, derived());
		}

		auto to_chars(char* dst) -> char*
		{
			std::string& tmp = detail::to_chars_buffer();
			tmp.clear();
			this->append(tmp);
			memcpy(dst, tmp.data(), tmp.size());
			return dst + tmp.size();
		}

		auto to_chars(char* dst, size_t max) -> std::pair<char*, size_t>
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

		template<class Iter>
		auto to_iter(Iter dst) -> Iter
		{
			std::string& tmp = detail::to_chars_buffer();
			tmp.clear();
			this->append(tmp);
			return std::copy(tmp.begin(), tmp.end(), dst);
		}

		// Unused, but needed by join()
		Derived& set_separator(...) noexcept { return derived(); }

		// copy
		Derived operator*() const {
			return derived();
		}

	};


	namespace detail
	{

		// Helper class to write a ostream_format to string
		template<class T, bool IsIntegral = std::is_integral<T>::value, bool IsFloat = std::is_floating_point<T>::value, bool IsString = is_tiny_string<T>::value, bool IsStreamable = is_ostreamable<T>::value>
		struct OstreamToString
		{
			template<class String, class Ostream>
			static auto write(String& out, const Ostream& val) -> size_t
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
		{
		};
		template<class T>
		struct OstreamToString<T, false, false, true, true>
		{
			template<class String, class Ostream>
			static auto write(String& out, const Ostream& val) -> size_t
			{
				// Specialization for tstring_view
				return val.write_string_to_string(out, val);
			}
		};
		template<class T>
		struct OstreamToString<T, true, false, false, true>
		{
			template<class String, class Ostream>
			static auto write(String& out, const Ostream& val) -> size_t
			{
				// Specialization for integral types
				return val.write_integral_to_string(out, val);
			}
		};
		template<class T>
		struct OstreamToString<T, false, true, false, true>
		{
			template<class String, class Ostream>
			static auto write(String& out, const Ostream& val) -> size_t
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
			static constexpr bool value = decltype(test<T>(0))::value;
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
	template<class T, bool Slot = false>
	class ostream_format : public base_ostream_format<T, ostream_format<T, Slot> >
	{
		using base_type = base_ostream_format<T, ostream_format<T, Slot> >;

		template<class U, bool S>
		friend class ostream_format;

		template<class U, bool IsIntegral, bool IsFloat, bool IsString, bool IsStreamable>
		friend struct detail::OstreamToString;

	public:

		static constexpr bool is_slot = Slot;
		static constexpr bool auto_width_format = true;

		// set flag telling if type T can be formatted with ostream_format
		static constexpr bool is_formattable = detail::is_default_formattable<T, ostream_format<T,Slot> >::value;

		/// @brief Default constructor. 
		///
		/// If the type is integral, initialize to base 10.
		/// If the type ia floating point, initialize the format with 'g'.
		ostream_format()
			: base_type() {}

		explicit ostream_format(const T& value)
			:base_type(value) {}

		/// @brief Construct from a value and a base or format value
		/// @param value input value
		/// @param base_or_format the base for integral type (default to 10), the format for floating point types (like 'g')
		ostream_format(const T& value, char base_or_format)
			:base_type(value, base_or_format) {}

		ostream_format(const T& value, const numeric_format& fmt)
			:base_type(value, fmt) {}


		template<class String>
		auto to_string(String& str) const -> size_t
		{
			return detail::OstreamToString<T>::write(str, *this);
		}

	private:

		template<class String, class U, bool S>
		auto write_integral_to_string(String& tmp, const ostream_format<U,S>& val) const -> size_t
		{
			to_chars_result f;
			size_t size;
			size_t min_cap = std::max(static_cast<size_t>(14), static_cast<size_t>(val.width()));
			if (SEQ_UNLIKELY(tmp.capacity() < min_cap)) {
				tmp.reserve(min_cap);
			}

			if (val.dot() == 'c') {
				//print as char
				const_cast<char&>(*tmp.data()) = static_cast<char>(val.value());
				size = 1;
			}
			else {
				for (;;) {
					f = detail::write_integral(detail::char_range(const_cast<char*>(tmp.data()), const_cast<char*>(tmp.data()) + tmp.capacity()), val.value(),
						val.base(),
						integral_chars_format(0, (val.formatting() & detail::f_prefix) != 0, (val.formatting() & detail::f_upper) != 0)
					);
					if (SEQ_LIKELY(f.ec == std::errc()))
						break;
					tmp.reserve(tmp.capacity() * 2);
				}
				size = static_cast<size_t>(f.ptr - tmp.data());
			}

			
			if (val.width() > size) {
				detail::format_width(tmp, size, val.width_fmt());
				size = val.width();
			}
			return size;
		}

		template<class String, class U, bool S>
		auto write_float_to_string(String& tmp, const ostream_format<U,S>& val) const -> size_t
		{
			size_t min_cap = std::max(static_cast<size_t>(14), static_cast<size_t>(val.width()));
			if (SEQ_UNLIKELY(tmp.capacity() < min_cap)) {
				tmp.reserve(min_cap);
			}

			char fmt = detail::to_upper(val.format());
			bool upper = val.format() <= 'Z';

#if defined( SEQ_FORMAT_USE_STD_TO_CHARS) && defined(SEQ_HAS_CPP_17)
			std::to_chars_result f;
			std::chars_format format = fmt == 'E' ? std::chars_format::scientific : (fmt == 'F' ? std::chars_format::fixed : std::chars_format::general);
			for (;;) {
				f = to_chars(const_cast<char*>(tmp.data()), const_cast<char*>(tmp.data()) + tmp.capacity(), val.value(), format, val.precision());
				if (SEQ_LIKELY(f.ec == std::errc()))
					break;
				tmp.reserve(tmp.capacity() * 2);
			}
#else
			char exp = upper ? 'E' : 'e';
			chars_format format = fmt == 'E' ? scientific : (fmt == 'F' ? fixed : general);
			to_chars_result f;

			for (;;) {
				f = seq::to_chars(const_cast<char*>(tmp.data()), const_cast<char*>(tmp.data()) + tmp.capacity(), val.value() , format, val.precision(), val.dot(), exp, upper);
				if (SEQ_LIKELY(f.ec == std::errc()))
					break;
				tmp.reserve(tmp.capacity() * 2);
			}
#endif
			size_t size = static_cast<size_t>(f.ptr - tmp.data());
			if (val.width() > size) {
				detail::format_width(tmp, size, val.width_fmt());
				size = val.width();
			}
			return size;
		}

		template<class String, class U,bool S>
		auto write_string_to_string(String& tmp, const ostream_format<U,S>& val) const -> size_t
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
	template<class T, bool S1, bool S2>
	class ostream_format<ostream_format<T,S1>, S2 > : public base_ostream_format<ostream_format<T,S1>, ostream_format<ostream_format<T,S1>,S2 > >
	{
		using base_type = base_ostream_format<ostream_format<T, S1>, ostream_format<ostream_format<T, S1>, S2 > >;

	public:

		ostream_format()
			: base_type() {}

		explicit ostream_format(const ostream_format<T,S1>& value)
			:base_type(value) {}

		template<class String>
		auto to_string(String& str) const -> size_t
		{
			size_t prev = str.size();
			this->value().append(str);
			return str.size() - prev;
		}
	};



	/// @brief Write a ostream_format object to a std::ostream 
	template<class Elem, class Traits, class T, bool S>
	inline auto operator<<(std::basic_ostream<Elem, Traits>&  oss, const ostream_format<T,S>&  val) -> std::basic_ostream<Elem, Traits>&
	{
		std::string& tmp = detail::ostream_buffer();
		tmp.clear();
		size_t s = val.to_string(tmp);
		if (!ostream_format<T,S>::auto_width_format) {
			// non default ostream_format, the width formatting must be applied
			if (val.alignment()) {
				width_format::format(tmp, 0, tmp.size(), val.width_fmt());
				s = tmp.size();
			}
		}
		oss.rdbuf()->sputn(tmp.data(), static_cast<std::streamsize>(s));
		return oss;
	}


	namespace detail
	{
		//
		// Helper structure used to build a ostream_format based on a type T
		//

		template<typename T, bool S>
		struct FormatWrapper
		{
			// Default behavior: use ostream_format<T>
			using type = ostream_format<T,S>;
		};
		template<typename T, bool S>
		struct FormatWrapper<const T&,S>
		{
			using type = ostream_format<T, S>;
		};
		template<typename T, bool S>
		struct FormatWrapper<T&,S>
		{
			using type = ostream_format<T, S>;
		};

		template<bool S>
		struct FormatWrapper<char*,S>
		{
			// Literal strings should produce ostream_format<tstring_view>
			using type = ostream_format<tstring_view, S>;
		} ;
		template<bool S>
		struct FormatWrapper<const char*,S>
		{
			using type = ostream_format<tstring_view, S>;
		} ;
		template<class Traits, class Al, bool S>
		struct FormatWrapper<std::basic_string<char,Traits,Al>,S>
		{
			// ALL string classes should produce ostream_format<tstring_view>
			using type = ostream_format<tstring_view, S>;
		} ;

#ifdef SEQ_HAS_CPP_17
		template<class Traits, bool S>
		struct FormatWrapper<std::basic_string_view<char,Traits>,S>
		{
			// ALL string classes should produce ostream_format<tstring_view>
			using type = ostream_format<tstring_view, S>;
		};
#endif

		template<class Traits, size_t S, class Alloc, bool Slot>
		struct FormatWrapper<tiny_string<char,Traits,Alloc,S> , Slot>
		{
			using type = ostream_format<tstring_view, Slot>;
		};
		template<size_t N, bool S>
		struct FormatWrapper<char[N] ,S>
		{
			using type = ostream_format<tstring_view, S>;
		};
		template<size_t N, bool S>
		struct FormatWrapper<const char[N] ,S>
		{
			using type = ostream_format<tstring_view, S>;
		};
		template<size_t N, bool S>
		struct FormatWrapper<char const (&)[N] ,S>
		{
			using type = ostream_format<tstring_view, S>;
		};

		template<class T, bool S>
		struct FormatWrapper<ostream_format<T,S>, S >
		{
			using type = ostream_format<T, S>;
		};

		// forward declaration
		template<class Tuple, bool HS, class Pos, bool Slot >
		struct mutli_ostream_format;

		// Specialize FormatWrapper for mutli_ostream_format (for nesting purpose)
		template<class Tuple, bool HS, class Pos, bool Slot>
		struct FormatWrapper<mutli_ostream_format<Tuple, HS, Pos, Slot> , Slot>
		{
			using type = mutli_ostream_format<Tuple, HS, Pos, Slot>;
		};
				



		/// @brief Tells if given type is a slot
		template<class T>
		struct is_slot : std::false_type {};
		template<class T, bool S>
		struct is_slot<ostream_format<T,S> > : std::integral_constant<bool,S> {};
		template<class T, bool S>
		struct is_slot<ostream_format<T, S> &> : std::integral_constant<bool, S> {};
		template<class T, bool S>
		struct is_slot<const ostream_format<T, S> &> : std::integral_constant<bool, S> {};
		template<class Tuple, bool HS, class Pos, bool S >
		struct is_slot<mutli_ostream_format<Tuple, HS, Pos, S> > : std::integral_constant<bool, S> {};
		template<class Tuple, bool HS, class Pos, bool S >
		struct is_slot<mutli_ostream_format<Tuple, HS, Pos, S>& > : std::integral_constant<bool, S> {};
		template<class Tuple, bool HS, class Pos, bool S >
		struct is_slot<const mutli_ostream_format<Tuple, HS, Pos, S>& > : std::integral_constant<bool, S> {};



		// meta-function which yields FormatWrapper<Element>::type from Element
		template<class Element>
		struct apply_wrapper
		{
			using tmp_type = typename std::remove_const< typename std::remove_reference<Element>::type >::type;
			using result = typename FormatWrapper<tmp_type, is_slot<tmp_type>::value>::type;
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


		static inline auto multi_ostream_buffer() -> std::string&
		{
			// Returns buffer suitable to write multi_ostream_format values into a std::ostream
			static thread_local std::string tmp;
			return tmp;
		}



		/// @brief Get the full string size if the tuple only contains tstring_view objects. Otherwise return -1.
		template<class Tuple, int N = std::tuple_size<Tuple>::value >
		struct GetStringSize
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;
			static constexpr int pos = tuple_size - N;
			static constexpr size_t invalid = static_cast<size_t>(-1);

			template<class T>
			static inline size_t get_string_size(const T&) noexcept {
				return static_cast<size_t>(-1);
			}
			template<class OtherTuple, bool HS, class Pos, bool S>
			static inline size_t get_string_size(const mutli_ostream_format<OtherTuple, HS,Pos,S>& m) noexcept {
				return GetStringSize< OtherTuple>::get(0, m.d_tuple);
			}
			template<bool S>
			static inline size_t get_string_size(const ostream_format<tstring_view,S>& s) noexcept {
				return s.value().size();
			}
			static size_t get(size_t prev, const Tuple& t) noexcept
			{
				size_t s = get_string_size(std::get<pos>(t));
				if (s == invalid)
					return  invalid;
				return GetStringSize<Tuple, N - 1>::get(s + prev, t);
			}
		};
		template<class Tuple>
		struct GetStringSize<Tuple, 0>
		{
			static size_t get(size_t s, const Tuple& /*unused*/) noexcept {
				return s;
			}
		};


		template<class Tuple, class Derived, bool HasSeparator>
		class base_mutli_ostream_format;

		template<class Tuple, class Derived, bool HasSeparator>
		size_t get_multi_ostream_size(const base_mutli_ostream_format<Tuple, Derived, HasSeparator>& m) noexcept
		{
			static constexpr size_t invalid = static_cast<size_t>(-1);
			size_t res = GetStringSize<Tuple>::get(0, m.d_tuple);
			if SEQ_CONSTEXPR(HasSeparator) {
				if (res != invalid) {
					res += (std::tuple_size<Tuple>::value - 1) * m.separator().size();
				}
			}
			return res;
		}


		/// @brief Convert a tuple of ostream_format to string 
		template<bool HasSeparator, class Tuple, int N = std::tuple_size<Tuple>::value >
		struct Converter
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;

			template<class String>
			static void convert(String& out, const Tuple& t, tstring_view sep)
			{
				static constexpr int pos = tuple_size - N;
				std::get<pos>(t).append(out);
				if SEQ_CONSTEXPR(HasSeparator && pos != (tuple_size - 1)) {
					out.append(sep);
				}
				Converter<HasSeparator,Tuple, N - 1>::convert(out, t,sep);
			}
		};
		template<bool HasSeparator, class Tuple>
		struct Converter<HasSeparator,Tuple, 0>
		{
			template<class String>
			static void convert(String& /*unused*/, const Tuple& /*unused*/, tstring_view)
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
			static void convert(Out& /*unused*/, Args&&... args)
			{}
		};


		/// @brief Affect new values (taken from another tuple) to all members of a tuple of ostream_format objects
		template<class Tuple, class SrcTuple, int N = std::tuple_size<Tuple>::value >
		struct AffectValuesFromTuple
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;

			template<class Out>
			static void convert(Out& out, const SrcTuple & src)
			{
				std::get<tuple_size - N>(out)(std::get<tuple_size - N>(src));
				AffectValuesFromTuple<Tuple, SrcTuple, N - 1>::convert(out, src);
			}
		};
		template<class Tuple, class SrcTuple>
		struct AffectValuesFromTuple<Tuple, SrcTuple, 0>
		{
			template<class Out, class ...Args>
			static void convert(Out& /*unused*/, const SrcTuple& /*unused*/)
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





		/// @brief Prepend value to a Positional argument
		/// @tparam Type 
		template<size_t Val, class Type>
		struct PrependPos
		{
			using type = Positional<Val>;
		};
		template<size_t Val, size_t... T>
		struct PrependPos<Val, Positional< T...> >
		{
			using type = Positional<Val, T...>;
		};

		/// @brief Returns type at given position for given variadic template, avoid using std::tuple
		template<size_t N, typename T, typename... types>
		struct get_type
		{
			using type = typename get_type<N - 1, types...>::type;
		};
		template<typename T, typename... types>
		struct get_type<0, T, types...>
		{
			using type = T;
		};

		template<size_t Index, class Start, class ...Args>
		struct FindSlotPos
		{
			using type_at_index = typename get_type<Index, Args...>::type;
			using type =

				typename FindSlotPos<Index - 1,
				typename std::conditional< is_slot<type_at_index>::value, typename PrependPos<Index, Start>::type, Start>::type,
				Args...>::type;
		};
		template<class Start, class ...Args>
		struct FindSlotPos<0, Start, Args...>
		{
			using type_at_index = typename get_type<0, Args...>::type;
			using type = typename std::conditional< is_slot<type_at_index>::value, typename PrependPos<0, Start>::type, Start>::type;
		};

		/// @brief Find all indexes of slot_format types and return them as a Positional type
		/// @tparam ...Args 
		template<class ...Args>
		struct find_slots
		{
			using type = typename FindSlotPos<(sizeof...(Args)) - 1, void, Args...>::type;
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
			static void convert(Out& /*unused*/, Args&&... args)
			{}
		};


		/// @brief Same as AffectValuesWithPos, but takes input values from a tuple
		template<class Pos, class Tuple, class SrcTuple, int N = Pos::size >
		struct AffectValuesWithPosFromTuple
		{
			static_assert(std::tuple_size<SrcTuple>::value == Pos::size, "invalid input tuple size");
			static constexpr size_t pos_size = Pos::size;

			template<class Out>
			static void convert(Out& out, const SrcTuple & t)
			{
				static constexpr size_t pos = pos_size - N;
				using get_type = typename Pos::template get<pos>;
				static constexpr size_t tuple_pos = get_type::value;
				std::get<tuple_pos>(out)(std::get<pos>(t));
				AffectValuesWithPosFromTuple<Pos, Tuple,SrcTuple, N - 1>::convert(out, t);
			}
		};
		template<class Pos, class Tuple, class SrcTuple>
		struct AffectValuesWithPosFromTuple<Pos, Tuple, SrcTuple, 0>
		{
			template<class Out>
			static void convert(Out& /*unused*/, const SrcTuple&/*unused*/ )
			{}
		};




		/// @brief Affect new values (taken from another tuple) to all members of a tuple of ostream_format objects
		template<class Tuple, int N = std::tuple_size<Tuple>::value >
		struct ResetTuple
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;

			static void reset(Tuple& out, char c)
			{
				std::get<tuple_size - N>(out).reset(c);
				ResetTuple<Tuple,  N - 1>::reset(out, c);
			}
		};
		template<class Tuple>
		struct ResetTuple<Tuple, 0>
		{
			static void reset(Tuple& , char )
			{}
		};




		/// @brief Store separator string (only if HasSeparator is true) used by join() function
		template<class Derived, bool HasSeparator>
		class format_separator
		{
			tstring_view sep;
		public:
			static constexpr bool has_separator = true;
			format_separator() {}
			tstring_view separator() const noexcept {return sep;}
			Derived& set_separator(tstring_view s) noexcept { sep = s; return static_cast<Derived&>(*this); }
		};
		template<class Derived>
		class format_separator<Derived,false>
		{
		public:
			static constexpr bool has_separator = false;
			format_separator() {}
			tstring_view separator() const noexcept { return tstring_view(); }
			Derived& set_separator(tstring_view ) noexcept { return static_cast<Derived&>(*this); }
		};

		/// @brief Base class for mutli_ostream_format
		template<class Tuple, class Derived, bool HasSeparator = false>
		class base_mutli_ostream_format : public format_separator<Derived, HasSeparator>
		{
		public:
			Tuple d_tuple;
			width_format d_width;
			Derived& derived() noexcept { return static_cast<Derived&>(*this); }
			const Derived& derived() const noexcept { return static_cast<const Derived&>(*this); }

		public:
			//static constexpr bool auto_width_format = true;

			base_mutli_ostream_format() : d_tuple(), d_width() {}

			template<class ...Args, class = typename std::enable_if<((sizeof...(Args)) > 1), void>::type>
			explicit base_mutli_ostream_format(Args&&... args)
				// Construct from multiple values.
				// Mark as explicit otherwise it replaces the default copy constructor.
				// Note: explicit is not enough, SFINAE is required
#if defined(__GNUG__) && __GNUC__ < 5
				: d_tuple(std::allocator_arg_t{}, std::allocator<char>{}, std::forward<Args>(args)...), d_width()
#else
				: d_tuple(std::forward<Args>(args)...), d_width()
#endif
			{}

			// copy
			Derived operator*() const {
				return derived();
			}


			auto reset(char c = 0) -> Derived {
				ResetTuple<Tuple>::reset(d_tuple, c);
				return derived();
			}

			/// @brief Returns the width format options
			auto width_fmt() const noexcept -> width_format { return d_width; }

			void set_width_format(const width_format& f) { d_width = f; }

			// Just provided for compatibility in order to store a mutli_ostream_format object in seq::any
			void set_numeric_format(const numeric_format&) {}

			/// @brief Returns width value of the width formatting options
			auto width() const noexcept -> unsigned short { return d_width.width; }
			/// @brief Returns fill character of the width formatting options
			auto fill_character() const noexcept -> char { return d_width.pad; }
			/// @brief Returns alignment value of the width formatting options
			auto alignment() const noexcept -> char { return d_width.alignment; }

			/// @brief Set the exact width and the alignment
			auto left(int w)noexcept -> Derived& {
				d_width.left(static_cast<unsigned short>(w));
				return derived();
			}
			/// @brief Set the exact width and the alignment
			auto l(int w) noexcept -> Derived& { return left(w); }
			auto ll(int w) noexcept -> Derived& { return left(w); }

			/// @brief Set the exact width and the alignment
			auto right(int w)noexcept -> Derived& {
				d_width.right(static_cast<unsigned short>(w));
				return derived();
			}
			/// @brief Set the exact width and the alignment
			auto r(int w) noexcept -> Derived& { return right(w); }

			/// @brief Set the exact width and the alignment
			auto center(int w)noexcept -> Derived& {
				d_width.center(static_cast<unsigned short>(w));
				return derived();
			}
			/// @brief Set the exact width and the alignment
			auto c(int w) noexcept -> Derived& { return center(w); }

			/// @brief Reset alignment options
			auto no_align() noexcept -> Derived& {
				d_width.reset();
				return derived();
			}
			/// @brief Set the fill character, used with left(), right() and center()
			auto fill(char f) noexcept -> Derived& {
				d_width.fill(f);
				return derived();
			}
			/// @brief Set the fill character, used with left(), right() and center()
			auto f(char _f) noexcept -> Derived& { return fill(_f); }


			template<size_t N, class T>
			void set(const T& value) {
				std::get<N>(this->d_tuple)(value);
			}
			template<size_t N>
			auto get() noexcept -> typename std::tuple_element<N, Tuple>::type& {
				return std::get<N>(this->d_tuple);
			}
			template<size_t N>
			auto get() const noexcept -> const typename std::tuple_element<N, Tuple>::type& {
				return std::get<N>(this->d_tuple);
			}

			template<class String>
			void reserve_string(String& str) const
			{
				size_t s = get_multi_ostream_size(*this);
				if (s != static_cast<size_t>(-1))
					str.reserve(s);
			}

			template<class String = std::string>
			auto append(String& out) const -> String&
			{
				// append the content of this formatting object to a string-like object
				size_t prev = out.size();
				Converter<HasSeparator, Tuple>::convert(out, d_tuple, this->separator());
				if (this->d_width.has_format()) {
					width_format::format(out, prev, out.size(), this->d_width);
				}
				return out;
			}

			auto to_chars(char* dst) -> char*
			{
				return to_iter(dst);
			}

			auto to_chars(char* dst, size_t max) -> std::pair<char*, size_t>
			{
				std::string& tmp = detail::to_chars_buffer();
				tmp.clear();
				reserve_string(tmp);
				this->append(tmp);
				if (tmp.size() > max) {
					memcpy(dst, tmp.data(), max);
					return { dst + max,tmp.size() };
				}
				memcpy(dst, tmp.data(), tmp.size());
				return { dst + tmp.size(),tmp.size() };
			}

			template<class Iter>
			auto to_iter(Iter dst) -> Iter {
				std::string& tmp = detail::to_chars_buffer();
				tmp.clear();
				reserve_string(tmp);
				this->append(tmp);
				return std::copy(tmp.begin(), tmp.end(), dst);
			}

			template<class String = std::string>
			auto str() const -> String
			{
				// convert this formatting object to string-like object
				String res;
				reserve_string(res);
				return append(res);
			}

			template<class Traits, class Al>
			operator std::basic_string<char,Traits,Al>() const
			{
				// convertion operator to std::string
				return str<std::basic_string<char, Traits, Al> >();
			}
			template<class Traits, size_t S, class Al>
			operator tiny_string<char,Traits, Al,S>() const
			{
				// convertion operator to tiny_string
				return str< tiny_string<char, Traits, Al, S> >();
			}
		};

		/// @brief Formatting class for multiple values, returned by seq::fmt(... , ...).
		/// @tparam Tuple tuple of ostream_format
		/// @tparam Pos possible Positional type
		template<class Tuple, bool HasSeparator = false, class Pos = void, bool Slot = false>
		struct mutli_ostream_format : public base_mutli_ostream_format<Tuple,mutli_ostream_format<Tuple, HasSeparator,Pos, Slot> , HasSeparator>
		{
			static constexpr bool is_slot = Slot;

			using base_type = base_mutli_ostream_format<Tuple, mutli_ostream_format<Tuple, HasSeparator, Pos, Slot>, HasSeparator>;
			using this_type = mutli_ostream_format<Tuple, HasSeparator, Pos, Slot>;
			using tuple_type = Tuple;
			

			mutli_ostream_format()
				:base_type()
			{}

			template<class ...Args, class = typename std::enable_if<((sizeof...(Args)) > 1),void>::type>
			explicit mutli_ostream_format(Args&&... args)
				// Construct from multiple values.
				// Mark as explicit otherwise it replaces the default copy constructor.
				// Note: explicit is not enough, SFINAE is required
			: base_type(std::forward<Args>(args)...)
			{}

			
			template<class ...Args>
			auto operator()(Args&&... args) -> mutli_ostream_format&
			{
				static_assert((sizeof...(Args)) == std::tuple_size<Tuple>::value, "format: invalid number of arguments");
				// Update internal tuple with new values.
				AffectValues<Tuple>::convert(this->d_tuple, std::forward<Args>(args)...);
				return *this;
			}
			template<size_t ...T, class ...Args>
			auto operator()(Positional<T...> /*unused*/, Args&&... args) -> mutli_ostream_format&
			{
				static_assert((sizeof...(Args)) == (sizeof...(T)), "format with positional argument: invalid number of arguments");
				// update internal tuple with new values for given indexes
				AffectValuesWithPos<Positional<T...>, Tuple>::convert(this->d_tuple, std::forward<Args>(args)...);
				return *this;
			}
			template<size_t ...T, class TT, bool HS, class PP, bool S>
			auto operator()(Positional<T...> /*unused*/, const mutli_ostream_format<TT, HS, PP,S>& o) -> mutli_ostream_format&
			{
				// update internal tuple with new values for given indexes
				AffectValuesWithPosFromTuple<Positional<T...>, Tuple,TT>::convert(this->d_tuple, o.d_tuple);
				return *this;
			}
			template<class T,bool HS, class P, bool S>
			auto operator()(const mutli_ostream_format<T,HS,P,S> & o) -> mutli_ostream_format&
			{
				// Affect values based on another mutli_ostream_format, for nested formatting
				AffectValuesFromTuple<Tuple,T>::convert(this->d_tuple, o.d_tuple);
				return *this;
			}

		};

		template<class Tuple, bool HasSeparator, size_t ...Ts, bool Slot>
		struct mutli_ostream_format<Tuple, HasSeparator, Positional<Ts...>,Slot> : public base_mutli_ostream_format<Tuple, mutli_ostream_format<Tuple, HasSeparator, Positional<Ts...>, Slot>, HasSeparator>
		{
			// Partial specialization of mutli_ostream_format that supports Positional type as template argument

			static constexpr bool is_slot = Slot;

			using base_type = base_mutli_ostream_format<Tuple, mutli_ostream_format<Tuple, HasSeparator, Positional<Ts...>, Slot>, HasSeparator>;
			using tuple_type = Tuple;
			using pos_type = Positional<Ts...>;

			mutli_ostream_format()
				:base_type()
			{}
			template<class ...Args, class = typename std::enable_if<((sizeof...(Args)) > 1), void>::type>
			explicit mutli_ostream_format(Args&&... args)
				// Construct from multiple values.
				// Mark as explicit otherwise it replaces the default copy constructor
				: base_type(std::forward<Args>(args)...)
			{}

			template<class ...Args>
			auto operator()(Args&&... args) -> mutli_ostream_format&
			{
				static_assert((sizeof...(Args)) == (sizeof...(Ts)), "format with positional argument: invalid number of arguments");
				AffectValuesWithPos<pos_type, Tuple>::convert(this->d_tuple, std::forward<Args>(args)...);
				return *this;
			}
			template<size_t ...T, class ...Args>
			auto operator()(Positional<T...> /*unused*/, Args&&... args) -> mutli_ostream_format&
			{
				static_assert((sizeof...(Args)) == (sizeof...(T)), "format with positional argument: invalid number of arguments");
				AffectValuesWithPos<Positional<T...>, Tuple>::convert(this->d_tuple, std::forward<Args>(args)...);
				return *this;
			}
			template<size_t ...T, class TT, bool HS, class PP, bool S>
			auto operator()(Positional<T...> /*unused*/, const mutli_ostream_format<TT,HS, PP,S>& o) -> mutli_ostream_format&
			{
				// update internal tuple with new values for given indexes
				AffectValuesWithPosFromTuple<Positional<T...>, Tuple, TT>::convert(this->d_tuple, o.d_tuple);
				return *this;
			}
			template<class T, bool HS, class P, bool S>
			auto operator()(const mutli_ostream_format<T,HS, P,S>& o) -> mutli_ostream_format&
			{
				// Affect values based on another mutli_ostream_format, for nested formatting
				AffectValuesWithPosFromTuple<pos_type, Tuple, T>::convert(this->d_tuple, o.d_tuple);
				return *this;
			}

		};

		/// @brief Used to specialize ostream_format for iterable types
		/// @tparam T 
		template<class T>
		struct Iterable {};

	}// end namespace detail

	

	/// @brief Returns a positional object used either by seq::fmt() or operator() of formatting object 
	template <size_t... Ts>
	auto pos() -> detail::Positional<Ts...>
	{
		return detail::Positional<Ts...>();
	}


	namespace detail
	{
		// Helper class to build a mutli_ostream_format when multiple arguments are given to seq::fmt(),
		// or a ostream_format when only one argument is given to seq::fmt().

		template<size_t NArgs, bool HS, class Pos, bool Slot, class ...Args>
		struct BuildFormat
		{
			using wrapped_tuple = metafunction::result_of<metafunction::transform_elements<std::tuple<Args...>, apply_wrapper>>;
			using return_type = mutli_ostream_format< wrapped_tuple, HS, Pos, Slot>;

			static return_type build(Args&&... args)
			{
				return return_type(std::forward<Args>(args)...);
			}
		};

		
		template<bool HS, bool Slot, class ...Args>
		struct BuildFormat<1, HS, void, Slot, Args...>
		{
			using type =  typename std::remove_const< typename std::remove_reference<Args...>::type >::type;
			using return_type = typename FormatWrapper<type,Slot>::type;

			static auto build(Args&&... args) -> return_type
			{
				return return_type(std::forward<Args>(args)...);
			}
		};

		// Check that the first type of variadict template is iterable
		template<class T, class ...Args>
		struct IsFirstIterable : is_iterable<typename std::decay<T>::type>{
			using type = typename std::decay<T>::type;
		};


	}// end namespace detail

	
	template<class ...Args>
	auto fmt(Args&&... args) -> typename detail::BuildFormat< sizeof...(Args),false, typename detail::find_slots<Args...>::type, false, Args...>::return_type
	{
		// Returns a formatting object (multi_ostream_format or ostream_format) for given input values
		using return_type = typename detail::BuildFormat< sizeof...(Args) ,false, typename detail::find_slots<Args...>::type, false, Args...>::return_type;
		return return_type(std::forward<Args>(args)...);
	}

	template<size_t ...Ts, class ...Args>
	auto fmt(detail::Positional<Ts...> /*unused*/, Args&&... args) -> typename detail::BuildFormat< sizeof...(Args),false, detail::Positional<Ts...>, false, Args...>::return_type
	{
		// Returns a formatting object (multi_ostream_format or ostream_format) for given input values
		// The Positional type is embedded within the multi_ostream_format type.
		using return_type = typename detail::BuildFormat< sizeof...(Args),false, detail::Positional<Ts...>, false, Args...>::return_type;
		return return_type(std::forward<Args>(args)...);
	}


	template<class ...Args>
	auto fmt() ->  typename detail::BuildFormat< sizeof...(Args),false, typename detail::find_slots<Args...>::type, false, Args...>::return_type
	{
		// Returns a default-initialized formatting object (multi_ostream_format or ostream_format) for given types
		using return_type = typename detail::BuildFormat< sizeof...(Args),false, typename detail::find_slots<Args...>::type, false, Args...>::return_type;
		return return_type();
	}

	template<class T, bool S>
	auto fmt( ostream_format<T, S> && o) -> ostream_format< ostream_format<T, S>, false >
	{
		return ostream_format< ostream_format<T, S>, false >(o);
	}
	template<class T, bool S>
	auto fmt( ostream_format<T, S>& o) -> ostream_format< ostream_format<T, S>, false >
	{
		return ostream_format< ostream_format<T, S>, false >(o);
	}



	template<class ...Args>
	auto _fmt(Args&&... args) -> typename detail::BuildFormat< sizeof...(Args), false, typename detail::find_slots<Args...>::type, true, Args...>::return_type
	{
		// Returns a formatting object (multi_ostream_format or ostream_format) for given input values
		using return_type = typename detail::BuildFormat< sizeof...(Args), false, typename detail::find_slots<Args...>::type, true, Args...>::return_type;
		return return_type(std::forward<Args>(args)...);
	}

	template<class ...Args>
	auto _fmt() ->  typename detail::BuildFormat< sizeof...(Args), false, typename detail::find_slots<Args...>::type, true, Args...>::return_type
	{
		// Returns a default-initialized formatting object (multi_ostream_format or ostream_format) for given types
		using return_type = typename detail::BuildFormat< sizeof...(Args), false, typename detail::find_slots<Args...>::type, true, Args...>::return_type;
		return return_type();
	}







	inline auto fmt(float value, char format) -> ostream_format<float>
	{
		// Floating point formatting
		return ostream_format<float>(value, format);
	}
	inline auto _fmt(float value, char format) -> ostream_format<float,true>
	{
		// Floating point formatting
		return ostream_format<float,true>(value, format);
	}
	
	inline auto fmt(double value, char format) -> ostream_format<double>
	{
		// Floating point formatting
		return ostream_format<double>(value, format);
	}
	inline auto _fmt(double value, char format) -> ostream_format<double,true>
	{
		// Floating point formatting
		return ostream_format<double,true>(value, format);
	}
	
	inline auto fmt(long double value, char format) -> ostream_format<long double>
	{
		// Floating point formatting
		return ostream_format<long double>(value, format);
	}
	inline auto _fmt(long double value, char format) -> ostream_format<long double,true>
	{
		// Floating point formatting
		return ostream_format<long double,true>(value, format);
	}

	
	inline auto fmt(const char* str) -> ostream_format<tstring_view>
	{
		// String formatting
		return ostream_format<tstring_view>(tstring_view(str));
	}
	inline auto _fmt(const char* str) -> ostream_format<tstring_view,true>
	{
		// String formatting
		return ostream_format<tstring_view,true>(tstring_view(str));
	}
	
	inline auto fmt(const char* str, size_t size) -> ostream_format<tstring_view>
	{
		return ostream_format<tstring_view>(tstring_view(str, size));
	}
	inline auto _fmt(const char* str, size_t size) -> ostream_format<tstring_view,true>
	{
		return ostream_format<tstring_view,true>(tstring_view(str, size));
	}
	
	inline auto fmt(const std::string& str) -> ostream_format<tstring_view>
	{
		// String formatting
		return ostream_format<tstring_view>(tstring_view(str.data(), str.size()));
	}
	inline auto _fmt(const std::string& str) -> ostream_format<tstring_view,true>
	{
		// String formatting
		return ostream_format<tstring_view,true>(tstring_view(str.data(), str.size()));
	}
	
	template<class Traits, size_t Ss, class Al>
	inline auto fmt(const tiny_string<char,Traits, Al,Ss>& str) -> ostream_format<tstring_view>
	{
		// String formatting
		return ostream_format<tstring_view>(tstring_view(str.data(), str.size()));
	}
	template<class Traits, size_t Ss, class Al>
	inline auto _fmt(const tiny_string<char, Traits, Al, Ss>& str) -> ostream_format<tstring_view,true>
	{
		// String formatting
		return ostream_format<tstring_view,true>(tstring_view(str.data(), str.size()));
	}

	inline auto fmt(const tstring_view& str) -> ostream_format<tstring_view>
	{
		// String formatting
		return ostream_format<tstring_view>(str);
	}
	inline auto _fmt(const tstring_view& str) -> ostream_format<tstring_view,true>
	{
		// String formatting
		return ostream_format<tstring_view,true>(str);
	}

#ifdef SEQ_HAS_CPP_17
	inline ostream_format<tstring_view> fmt(const std::string_view& str)
	{
		// String formatting
		return ostream_format<tstring_view>(tstring_view(str.data(), str.size()));
	}
	inline ostream_format<tstring_view,true> _fmt(const std::string_view& str)
	{
		// String formatting
		return ostream_format<tstring_view,true>(tstring_view(str.data(), str.size()));
	}
#endif

	template<class T = double>
	inline auto e(T val = T()) -> ostream_format<T>
	{
		// Helper function for floating point formatting
		return fmt(val,'e');
	}
	template<class T = double>
	inline auto _e(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for floating point formatting
		return _fmt(val, 'e');
	}

	template<class T = double>
	inline auto E(T val = T()) -> ostream_format<T>
	{
		// Helper function for floating point formatting
		return fmt(val, 'E');
	}
	template<class T = double>
	inline auto _E(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for floating point formatting
		return _fmt(val, 'E');
	}

	template<class T = double>
	inline auto g(T val = T()) -> ostream_format<T>
	{
		// Helper function for floating point formatting
		return fmt(val, 'g');
	}
	template<class T = double>
	inline auto _g(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for floating point formatting
		return _fmt(val, 'g');
	}

	template<class T = double>
	inline auto G(T val = T()) -> ostream_format<T>
	{
		// Helper function for floating point formatting
		return fmt(val, 'G');
	}
	template<class T = double>
	inline auto _G(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for floating point formatting
		return _fmt(val, 'G');
	}

	template<class T = double>
	inline auto f(T val = T()) -> ostream_format<T>
	{
		// Helper function for floating point formatting
		return fmt(val, 'f');
	}
	template<class T = double>
	inline auto _f(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for floating point formatting
		return _fmt(val, 'f');
	}

	template<class T = double>
	inline auto F(T val = T()) -> ostream_format<T>
	{
		// Helper function for floating point formatting
		return fmt(val, 'F');
	}
	template<class T = double>
	inline auto _F(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for floating point formatting
		return _fmt(val, 'F');
	}

	template<class T = typename std::make_signed<size_t>::type >
	inline auto dec(T val = T()) -> ostream_format<T>
	{
		// Helper function for integral formatting
		return fmt(val);
	}
	template<class T = typename std::make_signed<size_t>::type >
	inline auto _dec(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for integral formatting
		return _fmt(val);
	}

	template<class T = typename std::make_signed<size_t>::type>
	inline auto d(T val = T()) -> ostream_format<T>
	{
		// Helper function for integral formatting
		return fmt(val);
	}
	template<class T = typename std::make_signed<size_t>::type>
	inline auto _d(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for integral formatting
		return _fmt(val);
	}

	template<class T = size_t>
	inline auto u(T val = T()) -> ostream_format<T>
	{
		return fmt(val);
	}
	template<class T = size_t>
	inline auto _u(T val = T()) -> ostream_format<T, true>
	{
		return _fmt(val);
	}


	template<class T = size_t>
	inline auto hex(T val = T()) -> ostream_format<T>
	{
		// Helper function for integral formatting
		return fmt(val).base(16);
	}
	template<class T = size_t>
	inline auto _hex(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for integral formatting
		return _fmt(val).base(16);
	}

	template<class T = size_t>
	inline auto x(T val = T()) -> ostream_format<T>
	{
		return fmt(val).base(16);
	}
	template<class T = size_t>
	inline auto _x(T val = T()) -> ostream_format<T,true>
	{
		return _fmt(val).base(16);
	}

	template<class T = size_t>
	inline auto X(T val = T()) -> ostream_format<T>
	{
		return fmt(val).base(16).upper();
	}
	template<class T = size_t>
	inline auto _X(T val = T()) -> ostream_format<T,true>
	{
		return _fmt(val).base(16).upper();
	}


	template<class T = size_t>
	inline auto oct(T val = T()) -> ostream_format<T>
	{
		// Helper function for integral formatting
		return fmt(val).base(8);
	}
	template<class T = size_t>
	inline auto _oct(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for integral formatting
		return _fmt(val).base(8);
	}

	template<class T = size_t>
	inline auto o(T val = T()) -> ostream_format<T>
	{
		return fmt(val).base(8);
	}
	template<class T = size_t>
	inline auto _o(T val = T()) -> ostream_format<T,true>
	{
		return _fmt(val).base(8);
	}
	

	template<class T = typename std::make_signed<size_t>::type >
	inline auto bin(T val = T()) -> ostream_format<T>
	{
		// Helper function for integral formatting
		return fmt(val).base(2);
	}
	template<class T = typename std::make_signed<size_t>::type >
	inline auto _bin(T val = T()) -> ostream_format<T,true>
	{
		// Helper function for integral formatting
		return _fmt(val).base(2);
	}

	template<class T = char>
	inline auto ch(T val = T()) -> ostream_format<T>
	{
		// Format an integral value as a character
		return fmt(val).c();
	}
	template<class T = char>
	inline auto _ch(T val = T()) -> ostream_format<T,true>
	{
		// Format an integral value as a character
		return _fmt(val).c();
	}

	inline auto str() -> ostream_format<tstring_view>
	{
		// Null string formatting, used with seq::fmt().
		// Ex.: fmt(pos<1, 3>(),"|", seq::str().c(20), "|", seq::str().c(20), "|");
		return fmt<tstring_view>();
	}
	inline auto s() -> ostream_format<tstring_view>
	{
		// Null string formatting, used with seq::fmt().
		// Ex.: fmt(pos<1, 3>(),"|", seq::str().c(20), "|", seq::str().c(20), "|");
		return fmt<tstring_view>();
	}
	inline auto _str() -> ostream_format<tstring_view,true>
	{
		// Null string formatting, used with seq::fmt().
		// Ex.: fmt(pos<1, 3>(),"|", seq::str().c(20), "|", seq::str().c(20), "|");
		return _fmt<tstring_view>();
	}
	inline auto _s() -> ostream_format<tstring_view, true>
	{
		// Null string formatting, used with seq::fmt().
		// Ex.: fmt(pos<1, 3>(),"|", seq::str().c(20), "|", seq::str().c(20), "|");
		return _fmt<tstring_view>();
	}

	/// @brief Repeat count times character c
	inline auto rep(char c, int count) -> ostream_format<tstring_view>
	{
		// Repeat count times character c
		return str().l(count).f(c);
	}


	/// @brief Check if given type is formattable using seq::fmt()
	template<class T>
	struct is_formattable
	{
		static constexpr bool value = ostream_format<T,false>::is_formattable;
	};



	

	/// @brief Specialize ostream_format for detail::Iterable to provide a join() function working on iterables
	template<class T, bool Slot>
	class ostream_format<detail::Iterable<T>, Slot > : public base_ostream_format<T, ostream_format<detail::Iterable<T>, Slot > >
	{
		using base_type = base_ostream_format<T, ostream_format<detail::Iterable<T>, Slot > >;
		using this_type = ostream_format<detail::Iterable<T>, Slot >;

	public:
		tstring_view separator;
		width_format wfmt;

		ostream_format(tstring_view s, const T& v) : base_type(v), separator(s) {}
		template<class U, bool S>
		ostream_format(tstring_view s, const T& v, const ostream_format<U,S> & wf) : base_type(v), separator(s), wfmt(wf.width_fmt()) {
			this->set_numeric_format(wf.numeric_fmt());
		}

		size_t to_string(std::string& out) const
		{
			using val_type = typename T::value_type;
			 
			size_t prev = out.size();
			auto first = this->value().begin();
			auto last = this->value().end();

			if (first == last)
				return prev;

			auto f = seq::fmt<val_type>();
			f.set_numeric_format(this->numeric_fmt());
			f.set_width_format(wfmt);

			f(*first).append(out);
			++first;
			while (first != last) {
				out.append(separator.c_str(), separator.size());
				f(*first).append(out);
				++first;
			}

			return out.size() - prev;
		}
	};



	template<class ...Args, class K = typename std::enable_if<!detail::IsFirstIterable<Args...>::value, void>::type >
	auto join(tstring_view sep, Args&&... args) -> typename detail::BuildFormat< sizeof...(Args), true, typename detail::find_slots<Args...>::type,false, Args...>::return_type
	{
		// Returns a formatting object (multi_ostream_format or ostream_format) for given input values
		using return_type = typename detail::BuildFormat< sizeof...(Args), true, typename detail::find_slots<Args...>::type, false, Args...>::return_type;
		return return_type(std::forward<Args>(args)...).set_separator(sep);
	}

	template<class IterRange>
	auto join(tstring_view sep, const IterRange& c) noexcept -> ostream_format<detail::Iterable<IterRange> >
	{
		return ostream_format<detail::Iterable<IterRange> >(sep, c);
	}

	template<class IterRange, class T, bool S>
	auto join(tstring_view sep, const IterRange& c, const ostream_format<T,S>& wf) noexcept -> ostream_format<detail::Iterable<IterRange> >
	{
		return ostream_format<detail::Iterable<IterRange> >(sep, c, wf);
	}



	template<class ...Args, class K = typename std::enable_if<!detail::IsFirstIterable<Args...>::value, void>::type >
	auto _join(tstring_view sep, Args&&... args) -> typename detail::BuildFormat< sizeof...(Args), true, typename detail::find_slots<Args...>::type, true, Args...>::return_type
	{
		// Returns a formatting object (multi_ostream_format or ostream_format) for given input values
		using return_type = typename detail::BuildFormat< sizeof...(Args), true, typename detail::find_slots<Args...>::type, true, Args...>::return_type;
		return return_type(std::forward<Args>(args)...).set_separator(sep);
	}

	template<class IterRange>
	auto _join(tstring_view sep, const IterRange& c) noexcept -> ostream_format<detail::Iterable<IterRange>,true >
	{
		return ostream_format<detail::Iterable<IterRange> ,true>(sep, c);
	}

	template<class IterRange, class T, bool S>
	auto _join(tstring_view sep, const IterRange& c, const ostream_format<T,S>& wf) noexcept -> ostream_format<detail::Iterable<IterRange>,true >
	{
		return ostream_format<detail::Iterable<IterRange> ,true>(sep, c, wf);
	}




	/// @brief Write a ostream_format object to a std::ostream object
	template<class Elem, class Traits, class T, bool HS, class P, bool S>
	inline auto operator<<(std::basic_ostream<Elem, Traits>& oss, const detail::mutli_ostream_format<T,HS, P, S>& val) -> std::basic_ostream<Elem, Traits>&
	{
		std::string& tmp = detail::multi_ostream_buffer();
		tmp.clear();
		val.append(tmp);
		oss.rdbuf()->sputn(tmp.data(), static_cast<std::streamsize>(tmp.size()));
		
		return oss;
	}



	/// @brief Base class for match finders used by split() function
	/// @tparam Derived derived class
	template<class Derived>
	struct match_base {};

	/// @brief String matcher used by split() function
	struct by_string : match_base<by_string>
	{
		tstring_view match;
		by_string(tstring_view m) : match(m) {}
		by_string() {}
		size_t find(tstring_view v, size_t start) const noexcept{return v.find(match, start);}
		size_t next(size_t pos) const noexcept {return pos + match.size();}
		bool full_split() const noexcept { return match.size() == 0; }
	};

	/// @brief String matcher used by split() function, match a single character
	struct by_char : match_base<by_char>
	{
		char match;
		by_char(char m) : match(m) {}
		by_char(): match(0) {}
		size_t find(tstring_view v, size_t start) const noexcept {return v.find(match, start);}
		size_t next(size_t pos) const noexcept {return pos + 1;}
		bool full_split() const noexcept { return false; }
	};

	/// @brief String matcher used by split() function, match any of given set of characters
	struct by_any_char : match_base<by_any_char>
	{
		tstring_view match;
		by_any_char(tstring_view m) : match(m) {}
		by_any_char() {}
		size_t find(tstring_view v, size_t start) const noexcept {return v.find_first_of(match, start);}
		size_t next(size_t pos) const noexcept {return pos + 1;}
		bool full_split() const noexcept { return match.size() == 0; }
	};

	/// @brief String matcher used by split() function, match any character not present in given set of characters
	struct by_not_any_char : match_base<by_not_any_char>
	{
		tstring_view match;
		by_not_any_char(tstring_view m) : match(m) {}
		by_not_any_char() {}
		size_t find(tstring_view v, size_t start) const noexcept {return v.find_first_not_of(match, start);}
		size_t next(size_t pos) const noexcept {return pos + 1;}
		bool full_split() const noexcept { return false; }
	};

	/// @brief String matcher used by split() function, match word break. 
	/// Should be used in conjunction with skip_empty to extract words from text content.
	struct by_word : match_base<by_word>
	{
		by_word() {}
		size_t find(tstring_view v, size_t start) const noexcept {
			for (; start != v.size(); ++start)
				if (detail::is_space(v[start]))
					return start;
			return tstring::npos;
		}
		size_t next(size_t pos) const noexcept { return pos + 1; }
		bool full_split() const noexcept { return false; }
	};

	/// @brief Skip function used by split() function
	struct no_skip
	{
		static bool skip(const char* , size_t ) noexcept {
			return false;
		}
	};

	/// @brief Skip empty strings
	struct skip_empty
	{
		static bool skip(const char* , size_t len) noexcept {
			return len == 0;
		}
	};

	/// @brief Skip empty strings and whitespaces
	struct skip_whitespace
	{
		static bool skip(const char* src, size_t len) noexcept {
			for (size_t i = 0; i != len; ++i)
				if (src[i] != ' ')
					return false;
			return true;
		}
	};

	/// @brief Skip empty strings and spaces (' ', '\t', '\n', '\v', '\f', '\r');
	struct skip_space
	{
		static bool skip(const char* src, size_t len) noexcept {
			for (size_t i = 0; i != len; ++i)
				if (!detail::is_space(src[i]))
					return false;
			return true;
		}
	};


	namespace detail
	{
		// Iterator for split function
		template<class Match, class Skip>
		struct SplitIter
		{
			tstring_view source;
			tstring_view current; // current match
			Match match;
			size_t start;

			using value_type = tstring_view;
			using reference = const tstring_view&;
			using pointer = const tstring_view*;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;

			void find_next() noexcept
			{
				// Special case: split all characters
				if (match.full_split()) {
					if(start == source.size())
						start = tstring::npos;
					else {
						current = tstring_view(source.data() + start, 1);
						++start;
					}
					return;
				}

				// end condition
				if (SEQ_UNLIKELY(start == tstring::npos - 1)) {
					start = tstring::npos;
					return;
				}

				do {
					size_t found = match.find(source, start);
					if (SEQ_UNLIKELY(found == tstring::npos)) {
						//last occurence
						current = source.substr(start, source.size() - start);
						start = tstring::npos - 1;
						return;
					}
					else {
						current = source.substr(start,found - start);
						start = match.next(found);
					}
				} while (Skip::skip(current.data(), current.size()));
			}

			SplitIter(tstring_view src, Match m)
				:source(src), match(m), start(0){
				// Find first match
				find_next();
			}
			SplitIter(size_t st)
				:start(st) {}
			SplitIter() : start(tstring::npos) {}

			auto operator++() noexcept -> SplitIter& {
				find_next();
				return *this;
			}
			auto operator++(int) noexcept -> SplitIter {
				SplitIter _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { return current; }
			auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			bool operator==(const SplitIter& it) const noexcept { return start == it.start; }
			bool operator!=(const SplitIter& it) const noexcept { return start != it.start; }
		};


		template<class Match, class Skip = no_skip>
		auto internal_split(tstring_view str, Match match, Skip ) -> iterator_range< SplitIter<Match, Skip> >
		{
			return iterator_range< SplitIter<Match, Skip> >(SplitIter<Match, Skip>(str,match),SplitIter<Match, Skip>(tstring::npos));
		}
	}
	

	template<class Skip = no_skip>
	auto split(tstring_view str, tstring_view match, Skip skip = Skip()) -> iterator_range< detail::SplitIter<by_string, Skip> >
	{
		return detail::internal_split(str, by_string(match), skip);
	}
	template<class Skip = no_skip>
	auto split(tstring_view str, char match, Skip skip = Skip()) -> iterator_range< detail::SplitIter<by_char, Skip> >
	{
		return detail::internal_split(str, by_char(match), skip);
	}
	template<class Match, class Skip = no_skip>
	auto split(tstring_view str, const match_base<Match>& match, Skip skip = Skip()) -> iterator_range< detail::SplitIter<Match, Skip> >
	{
		return detail::internal_split(str, static_cast<const Match&> (match), skip);
	}

	inline auto replace(tstring_view src, tstring_view match, tstring_view _new) -> std::string
	{
		auto r = split(src, match);
		return join(_new,r ).str();
	}

} // end namespace seq

/** @}*/
//end charconv

#endif
