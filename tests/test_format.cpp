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


#include <utility>

#include "seq/format.hpp"
#include "seq/testing.hpp"



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


inline void test_format()
{

	using namespace seq;

	const double PI = 3.14159265358979323846;

	{
		// Test formatting single values

		SEQ_TEST_TO_OSTREAM("3.14159", fmt(PI)); //default double formatting
		SEQ_TEST_TO_OSTREAM("3.141593E+00", fmt(PI, 'E')); //scientific notation, equivalent to fmt(PI).format('E') or fmt(PI).t('E')
		SEQ_TEST_TO_OSTREAM("3.14159265359E+00", fmt(PI, 'E').precision(12)); //scientific notation with maximum precision, equivalent to fmt(PI).t('E').precision(12) or fmt(PI).t('E').p(12)
		SEQ_TEST_TO_OSTREAM("3,14159", fmt(PI).dot(',')); //change dot, equivalent to fmt(PI).d(',')
		SEQ_TEST_TO_OSTREAM("---3.14159", fmt(PI).right(10).fill('-')); //align to the right and pad with '-', equivalent to fmt(PI).r(10).f('-')
		SEQ_TEST_TO_OSTREAM("3.14159---", fmt(PI).left(10).fill('-')); //align to the left and pad with '-', equivalent to fmt(PI).l(10).f('-')
		SEQ_TEST_TO_OSTREAM("-3.14159--", fmt(PI).center(10).fill('-')); //align to the center and pad with '-', equivalent to fmt(PI).c(10).f('-')
		SEQ_TEST_TO_OSTREAM("0x1E240", fmt(123456).base(16).hex_prefix().upper()); //hexadecimal upper case with '0x' prefix. equivalent to fmt(123456).b(16).h().u() or hex(123456).h().u()
		SEQ_TEST_TO_OSTREAM("**hello***", fmt("hello").c(10).f('*')); //center string and pad with '*', equivalent to fmt("hello").center(10).fill('*')
		SEQ_TEST_TO_OSTREAM("ell", fmt("hello").c(3).f('*'));  //center and truncate string
			

		// Direct string conversion
		std::string str = fmt(PI);
		// Direct string conversion using .str()
		std::string str2 = "PI value is " + fmt(PI).str();
		SEQ_TEST_TO_OSTREAM("PI value is 3.14159", str2);
	}

	{
		// Test formatting single values with shortcuts

		SEQ_TEST_TO_OSTREAM("u", ch('u'));	//equivalent to fmt('u').as_char() or fmt('u').c()
		SEQ_TEST_TO_OSTREAM("1.2e+00", e(1.2));	//equivalent to fmt(1.2,'e') or fmt(1.2).format('e') or fmt(1.2).t('e')
		SEQ_TEST_TO_OSTREAM("1.2E+00", E(1.2));	//equivalent to fmt(1.2,'E') or fmt(1.2).format('E') or fmt(1.2).t('E')
		SEQ_TEST_TO_OSTREAM("1.2", f(1.2));	//equivalent to fmt(1.2,'f') or fmt(1.2).format('f') or fmt(1.2).t('f')
		SEQ_TEST_TO_OSTREAM("1.2", F(1.2) );	//equivalent to fmt(1.2,'F') or fmt(1.2).format('F') or fmt(1.2).t('F')
		SEQ_TEST_TO_OSTREAM("1.2", g(1.2) );	//equivalent to fmt(1.2,'g') or fmt(1.2).format('g') or fmt(1.2).t('g')
		SEQ_TEST_TO_OSTREAM("1.2", G(1.2) );	//equivalent to fmt(1.2,'G') or fmt(1.2).format('G') or fmt(1.2).t('G')
		SEQ_TEST_TO_OSTREAM("100", fmt(100));	//
		SEQ_TEST_TO_OSTREAM("64", hex(100));	//equivalent to fmt(100).base(16) or fmt(100).b(16)
		SEQ_TEST_TO_OSTREAM("144", oct(100));	//equivalent to fmt(100).base(8) or fmt(100).b(8)
		SEQ_TEST_TO_OSTREAM("1100100", bin(100));	//equivalent to fmt(100).base(2) or fmt(100).b(2)
	}

	{
		// Nested formatting
		SEQ_TEST_TO_OSTREAM("-----#####**surrounded text***#####-----", fmt(fmt(fmt(fmt("surrounded text")).c(20).f('*')).c(30).f('#')).c(40).f('-'));
	}

	{
		// Test formatting multiple values

		// Direct stream
		SEQ_TEST_TO_OSTREAM("The answer is 43 ...", fmt("The answer is ", 43, " ..."));
		// Direct stream with nested formatting
		SEQ_TEST_TO_OSTREAM("...Or it could be 4.33e+01 ", fmt("...Or it could be", fmt(43.3, 'e').c(10)));


		// Reuse a formatting object built without arguments
		auto f = fmt<int, tstring_view, double, tstring_view, double>();
		SEQ_TEST_TO_OSTREAM("1 + 2.2 = 3.2", f(1, " + ", 2.2, " = ", 3.2));


		// Reuse a formatting object and use seq::null to only update some arguments
		auto f2 = fmt(int(), " + ", fmt<double>().format('g'), " = ", fmt<double>().format('e'));
		SEQ_TEST_TO_OSTREAM("1 + 2.2 = 3.2e+00", f2(1, null, 2.2, null, 3.2));

		// Convert to string or tstring
		std::string s1 = f2(1, null, 2.2, null, 3.2);	//equivalent to s1 = f2(1, null, 2.2, null, 3.2).str();
		tstring s2 = f2(1, null, 2.2, null, 3.2);		//equivalent to s2 = f2(1, null, 2.2, null, 3.2).str<tstring>();
		SEQ_TEST(s1 == "1 + 2.2 = 3.2e+00");
		SEQ_TEST(s2 == "1 + 2.2 = 3.2e+00");

		// Append to string
		s2 += ", repeat-> ";
		f2(1, null, 2.2, null, 3.2).append(s2);			// append formatted result to s2
		SEQ_TEST_TO_OSTREAM("1 + 2.2 = 3.2e+00, repeat-> 1 + 2.2 = 3.2e+00", s2);


		// Modify formatting object using get() and/or set()
		f2.set<0>(fmt<int>().base(16).h().u()); // reset the formatting object at position 0
		f2.get<2>().format('e');				// modifiy the formatting object at position 2
		SEQ_TEST_TO_OSTREAM( "0x1 + 2.2e+00 = 3.2e+00", f2(1, null, 2.2, null, 3.2));


		// Use positional argument
		SEQ_TEST_TO_OSTREAM("0x1 + 2.2e+00 = 3.2e+00", f2(pos<0, 2, 4>(), 1, 2.2, 3.2)); // provided arguments are used for positions 0, 2 and 4

		// Positional directly in the fmt call
		auto f3 = fmt(pos<0, 2, 4>(), int(), " + ", seq::g<double>(), " = ", seq::e<double>());
		SEQ_TEST_TO_OSTREAM( "1 + 2.2 = 3.2e+00", f3(1, 2.2, 3.2));


		// Building tables

		// header/trailer format, 2 columns of width 20 centered, separated by a '|'
		auto header = fmt(pos<1, 3>(), "|", seq::str().c(20), "|", seq::str().c(20), "|");
		//line format, 2 columns of width 20 centered, separated by a '|'
		auto line = fmt(pos<1, 3>(), "|", seq::fmt<double>().c(20), "|", seq::fmt<double>().c(20), "|");
		// write table
		SEQ_TEST_TO_OSTREAM("|      Header 1      |      Header 2      |", header("Header 1", "Header 2"));
		SEQ_TEST_TO_OSTREAM("|        1.1         |        2.2         |", line(1.1, 2.2));
		SEQ_TEST_TO_OSTREAM("|        3.3         |        4.4         |", line(3.3, 4.4));
		SEQ_TEST_TO_OSTREAM("|     Trailer 1      |     Trailer 2      |", header("Trailer 1", "Trailer 2"));

	}


	{
		// Print to std::ostream
		SEQ_TEST_TO_OSTREAM("1.12346", seq::fmt(1.123456789, 'g'));

		// Convert to string
		std::string str = seq::fmt(1.123456789, 'g').str<std::string>();
		SEQ_TEST(str == "1.12346");

		// Append to an existing string
		std::string str2;
		seq::fmt(1.123456789, 'g').append(str2);
		SEQ_TEST(str2 == "1.12346");

		// write to buffer (to_chars(char*) returns past-the-end pointer)
		char dst[100];
		*seq::fmt(1.123456789, 'g').to_chars(dst) = 0;
		SEQ_TEST(dst == std::string("1.12346"));

		// write to buffer with maximum size (to_chars(char*,size_t) returns a pair of past-the-end pointer and size without truncation)
		char dst2[100];
		*seq::fmt(1.123456789, 'g').to_chars(dst2, sizeof(dst2)).first = 0;
		SEQ_TEST(dst2 == std::string("1.12346"));

	}

	{
		// Formatting custom types
		SEQ_TEST_TO_OSTREAM("Print a pair of float: (1.2, 3.4)", fmt("Print a pair of float: ", std::make_pair(1.2f, 3.4f)));

		// Formatting custom types with custom format
		SEQ_TEST_TO_OSTREAM("Print a pair of double: (1.2e+00, 3.4e+00)", fmt("Print a pair of double: ", fmt(std::make_pair(1.2, 3.4)).format('e')));

		// Formatting custom types with custom format and alignment
		SEQ_TEST_TO_OSTREAM("Print a pair of double centered: ******(1.2e+00, 3.4e+00)******", fmt("Print a pair of double centered: ", fmt(std::make_pair(1.2, 3.4)).t('e').c(30).f('*')));
	}
}



int test_format(int , char*[])
{
	SEQ_TEST_MODULE_RETURN(format, 1, test_format());
	return 0;
}
