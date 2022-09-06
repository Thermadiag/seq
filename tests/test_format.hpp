

#include <utility>

#include "format.hpp"


namespace seq
{
	// Specialization of ostream_format for std::pair<T,T>

	template<class T>
	class ostream_format<std::pair<T, T> > : public base_ostream_format<std::pair<T, T>, ostream_format<std::pair<T, T> > >
	{
		using base_type = base_ostream_format<std::pair<T, T>, ostream_format<std::pair<T, T> > >;

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


void test_format()
{

	using namespace seq;

	const double PI = 3.14159265358979323846;

	{
		std::cout << fmt(PI) << std::endl;										//default double formatting
		std::cout << fmt(PI, 'E') << std::endl;									//scientific notation, equivalent to fmt(PI).format('E') or fmt(PI).t('E')
		std::cout << fmt(PI, 'E').precision(12) << std::endl;					//scientific notation with maximum precision, equivalent to fmt(PI).t('E').precision(12) or fmt(PI).t('E').p(12)
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
	}

	{
		std::cout << ch('u') << std::endl;	//equivalent to fmt('u').as_char() or fmt('u').c()
		std::cout << e(1.2) << std::endl;	//equivalent to fmt(1.2,'e') or fmt(1.2).format('e') or fmt(1.2).t('e')
		std::cout << E(1.2) << std::endl;	//equivalent to fmt(1.2,'E') or fmt(1.2).format('E') or fmt(1.2).t('E')
		std::cout << f(1.2) << std::endl;	//equivalent to fmt(1.2,'f') or fmt(1.2).format('f') or fmt(1.2).t('f')
		std::cout << F(1.2) << std::endl;	//equivalent to fmt(1.2,'F') or fmt(1.2).format('F') or fmt(1.2).t('F')
		std::cout << g(1.2) << std::endl;	//equivalent to fmt(1.2,'g') or fmt(1.2).format('g') or fmt(1.2).t('g')
		std::cout << G(1.2) << std::endl;	//equivalent to fmt(1.2,'G') or fmt(1.2).format('G') or fmt(1.2).t('G')
		std::cout << fmt(100) << std::endl;	//
		std::cout << hex(100) << std::endl;	//equivalent to fmt(100).base(16) or fmt(100).b(16)
		std::cout << oct(100) << std::endl;	//equivalent to fmt(100).base(8) or fmt(100).b(8)
		std::cout << bin(100) << std::endl;	//equivalent to fmt(100).base(2) or fmt(100).b(2)
	}

	{
		// Nested formatting
		std::cout << fmt(fmt(fmt(fmt("surrounded text")).c(20).f('*')).c(30).f('#')).c(40).f('-') << std::endl;
	}

	{
		// Formatting multiple values

// Direct stream
		std::cout << fmt("The answer is ", 43, " ...") << std::endl;
		// Direct stream with nested formatting
		std::cout << fmt("...Or it could be", fmt(43.3, 'e').c(10)) << std::endl;

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
		std::cout << s2 << std::endl;

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
		auto header = fmt(pos<1, 3>(), "|", seq::str().c(20), "|", seq::str().c(20), "|");
		//line format, 2 columns of width 20 centered, separated by a '|'
		auto line = fmt(pos<1, 3>(), "|", seq::fmt<double>().c(20), "|", seq::fmt<double>().c(20), "|");
		// write table
		std::cout << header("Header 1", "Header 2") << std::endl;
		std::cout << line(1.1, 2.2) << std::endl;
		std::cout << line(3.3, 4.4) << std::endl;
		std::cout << header("Trailer 1", "Trailer 2") << std::endl;

		std::cout << std::endl;

	}


	{
		// Print to std::cout
		std::cout << seq::fmt(1.123456789, 'g') << std::endl;

		// Convert to string
		std::string str = seq::fmt(1.123456789, 'g').str<std::string>();
		std::cout << str << std::endl;

		// Append to an existing string
		std::string str2;
		seq::fmt(1.123456789, 'g').append(str2);
		std::cout << str2 << std::endl;

		// write to buffer (to_chars(char*) returns past-the-end pointer)
		char dst[100];
		*seq::fmt(1.123456789, 'g').to_chars(dst) = 0;
		std::cout << dst << std::endl;

		// write to buffer with maximum size (to_chars(char*,size_t) returns a pair of past-the-end pointer and size without truncation)
		char dst2[100];
		*seq::fmt(1.123456789, 'g').to_chars(dst2, sizeof(dst2)).first = 0;
		std::cout << dst2 << std::endl;

	}

	{
		// Formatting custom types
		std::cout << fmt("Print a pair of float: ", std::make_pair(1.2f, 3.4f)) << std::endl;

		// Formatting custom types with custom format
		std::cout << fmt("Print a pair of double: ", fmt(std::make_pair(1.2, 3.4)).format('e')) << std::endl;

		// Formatting custom types with custom format and alignment
		std::cout << fmt("Print a pair of double centered: ", fmt(std::make_pair(1.2, 3.4)).t('e').c(30).f('*')) << std::endl;
	}
}