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

#ifndef SEQ_CHARCONV_HPP
#define SEQ_CHARCONV_HPP



/** @file */

/**\defgroup charconv Charconv: arithmetic value convertion from/to string

The charconv module provides fast routines to convert numerical values from/to string.
This module was initially developped for very fast containers dump in files or strings.

Low level functions
-------------------

The main functions of charconv module are seq::to_chars and seq::from_chars which provide a similar interface to C++17 functions std::from_chars and std::to_chars. 
They aim to provide a faster alternative to C++ streams for converting floating point and integer types from/to string. Note that they were developped to accomodate
my needs, and might not be used in all circumstances.

seq::from_chars() is similar to std::from_chars with the following differences:
	-	Leading spaces are consumed.
	-	For integral types, a leading '0x' prefix is considered valid.
	-	For floating point values: if the pattern is a valid floating point text representation too large or too small to be stored in given output value, value will be set to (+-)inf or (+-)0, 
		and the full pattern will be consumed. Therefore, std::errc::result_out_of_range is never returned.
	-	Leading '+' sign is considered valid.
	-	Custom 'dot' character can be passed as argument.
	-	For floating point values: this function IS NOT AN EXACT PARSER. In some cases it relies on unprecise floating point arithmetic wich might produce different roundings than strtod() function.
		Note that the result is almost always equal to the result of strtod(), and potential differences are located in the last digits. Use this function when the speed factor is more important than 100% perfect exactitude.

seq::to_chars is similar to std::to_chars with the following differences
	-	For integral types, additional options on the form of a seq::integral_chars_format object can be passed. They add the possibility to output a leading '0x' for hexadecimal
		formatting, a minimum width (with zeros padding), upper case outputs for hexadecimal formatting.
	-	For floating point values, the 'dot' character can be specified.
	-	For floating point values, this function is NOT AN EXACT FORMATTER. There are currently a lot of different algorithms to provide fast convertion of floating point 
		values to strings with round-trip guarantees: ryu, grisu-exact, dragonbox... This function tries to provide a faster and lighter alternative when perfect round-trip is not a requirement (which is my case).
		When converting double values, obtained strings are similar to the result of printf in 100% of the cases when the required precision 
		is below 12. After that, the ratio decreases to 86% of exactitude for a precision of 17. Converting a very high (or very small) value with the 'f' specifier will usually produce 
		slightly different output, especially in the "garbage" digits.


Working with C++ streams
------------------------

To write numerical values to C++ std::ostream objects, see the \ref format "format" module.

To read numerical values from std::istream object, the charconv module provides the stream adapter seq::std_input_stream.
It was developped to read huge tables or matrices from ascii files or strings.

Basic usage:

\code{.cpp}

std::ifstream fin("my_huge_file.txt");
seq::std_input_stream<> istream(fin);

// Read trailing lines
std::string trailer;
seq::read_line_from_stream(istream, trailer);
//...

// Read words
std::string word;
seq::from_stream(istream, word);
//...

// Read all numeric values into a vector
std::vector<double> vec;
while (true) {
	double v;
	seq::from_stream(istream, v);
	if (istream)
		vec.push_back(v);
	else
		break;
}

\endcode

Internal benchmarks show that using from_stream() is around 20 faster (or more) than using std::istream::operator>>() when reading floating point values from a huge string.

In additional to seq::std_input_stream, charconv module provides the similar seq::buffer_input_stream and seq::file_input_stream.
*/



/** \addtogroup charconv
 *  @{
 */

#ifdef _MSC_VER
#define CRT_DISABLE_PERFCRIT_LOCKS
#endif

#include <cmath>
#include <cfloat>
#include <system_error>

#include "bits.hpp"
#include "tiny_string.hpp"


#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace seq
{
	/// @brief A BitmaskType used to specify floating-point formatting for seq::to_chars.
	/// This is similar to std::chars_format, but without the hex value.
	enum chars_format
	{
		scientific = 0x8000000,
		fixed = 0x10000000,
		//hex = 0x4, // unsupported for now
		general = scientific|fixed
	};
	

	/// @brief Stream object status for classes inheriting basic_input_stream
	enum StreamState
	{
		Ok,			//! No error
		EndOfFile,	//! End of file reached
		BadInputFormat	//! Unable to read numerical value
	};


	/// @brief Return type for seq::to_chars functions, similar to std::to_chars_result
	struct to_chars_result {
		char* ptr;
		std::errc ec;
	} ;

	/// @brief Return type for seq::from_chars functions, similar to std::from_chars_result
	struct from_chars_result {
		const char* ptr;
		std::errc ec;
	};

	/// @brief Parameters for integer to string conversion using seq::to_chars
	struct integral_chars_format
	{
		/// @brief Minimum digits to represent integral types (use zero padding).
		unsigned char integral_min_width;	

		/// @brief Add '0x' prefix to hexadecimal numbers
		bool hex_prefix;

		/// @brief Output upper characters for hexadecimal numbers
		bool upper_case; 

		integral_chars_format()
			:integral_min_width(0), hex_prefix(false), upper_case(false) {}
		integral_chars_format(unsigned char min_width , bool hp , bool uc )
			:integral_min_width(min_width), hex_prefix(hp), upper_case(uc) {}
	};


	/**
	* @brief Base class for input text streams.
	* 
	* Input text streams behave mostly like a (very) lightweight version of std::istream.
	* They are used for reading numerical values, strings and lines from either:
	*	- a FILE descriptor (seq::file_input_stream)
	*	- a std::istream object (seq::std_input_stream)
	*	- a sequence of characters (seq::buffer_input_stream)
	* 
	* The main advantage of using basic_input_stream instead of standard std::istream classes is its speed.
	* std::istream is a very heavy machinery, full of virtual function dispatching and providing all possible features,
	* including very exotic ones.
	* 
	* basic_input_stream does not rely on virtual function call, does not used locales nor std::streambuf. 
	* Optimized integer and floating point read functions provided in the seq library rely on basic_input_stream.
	* 
	* Note that it is still possible to use the optimized read function on a std::istream object using the 
	* std_input_stream class. This is still several times faster than using the raw std::istream.
	*/
	template<class Derived>
	class basic_input_stream
	{
		SEQ_ALWAYS_INLINE auto derived() noexcept -> Derived& { return static_cast<Derived&>(*this); }
		SEQ_ALWAYS_INLINE auto derived() const noexcept -> const Derived& { return static_cast<const Derived&>(*this); }

	public:

		/*
		Mandatory members of derived classes:

		void close() noexcept;
		bool is_open() const noexcept;
		size_t tell() const noexcept;
		void back() noexcept;
		size_t seek(size_t pos) noexcept;
		int getc() noexcept;
		size_t read(char* dst, size_t count) noexcept;
		void set_state(State st) noexcept;
		State state() const noexcept;

		*/

		SEQ_ALWAYS_INLINE auto is_ok() const noexcept -> bool { return derived().state() == Ok; }
		SEQ_ALWAYS_INLINE void reset() noexcept { derived().set_state(Ok); }
		SEQ_ALWAYS_INLINE explicit operator bool() const noexcept { return is_ok(); }
	};


	


	namespace detail
	{
		
		struct float_chars_format
		{
			// Parameters for float to string conversion

			chars_format fmt;	//! Format used for float to string conversion.
			char dot;			//! Dot character for float to string conversion, default to '.'.
			char exp;			//! Exponent character for float to string conversion, default to 'e'.
			char upper;			//! Upper/lower case for nan and inf values

			float_chars_format(chars_format fm = general, char d = '.', char e = 'e', char u = 0)
				:fmt(fm), dot(d), exp(e), upper(u){}
		} ;

		struct char_range
		{
			// Write value in a bounded sequence of character
			static constexpr bool extendible = false;
			char* pos;
			char* end;
			char_range(char* start , char* en )
				:pos(start), end(en) {}
			SEQ_ALWAYS_INLINE auto add_size(size_t count) const noexcept -> char* { 
				const char* tmp = pos;
				const_cast<char_range*>(this)->pos += count;
				return (pos > end) ? nullptr : const_cast<char*>(tmp); 
			}
			SEQ_ALWAYS_INLINE auto current() const noexcept -> char* { return const_cast<char*>(pos); } //current position
			SEQ_ALWAYS_INLINE auto end_ptr() const noexcept -> char* { return const_cast<char*>(end); } //end ptr for error
			SEQ_ALWAYS_INLINE auto back() const noexcept -> char* { return --const_cast<char_range*>(this)->pos; }
			SEQ_ALWAYS_INLINE auto append(char v) const noexcept -> bool {
				if (SEQ_UNLIKELY(pos == end)) { return false;
}
				*const_cast<char_range*>(this)->pos++ = v;
				return true;
			}
		} ;

		template<class String>
		struct string_range
		{
			// Write value into a string-like object
			static constexpr bool extendible = true;
			String* str;
			explicit string_range(String* str ) : str(str) { }
			SEQ_ALWAYS_INLINE auto add_size(size_t count) const -> char* {
				size_t size = str->size();
				const_cast<String*>(str)->resize(size + count);
				return const_cast<char*>(str->data()) + size;
			}
			SEQ_ALWAYS_INLINE auto current() const noexcept -> char* { return const_cast<char*>(&const_cast<String*>(str)->back()); } //current position
			SEQ_ALWAYS_INLINE auto end_ptr() const noexcept -> char* { return NULL; } //end ptr for error
			SEQ_ALWAYS_INLINE auto back() const noexcept -> char* { const_cast<String*>(str)->pop_back(); return current(); }
			SEQ_ALWAYS_INLINE constexpr auto append(char v) const -> bool {
				const_cast<String*>(str)->push_back( v);
				return true;
			}
		};

		


		using u_double = union{  
			unsigned short bytes[sizeof(double)];
			double val;
		};
		using u_float = union{  
			unsigned short bytes[sizeof(float)];
			float val;
		};
		
		SEQ_ALWAYS_INLINE auto signbit(float X) noexcept -> bool
		{
			// faster signbit for little endian machines
#if SEQ_BYTEORDER_ENDIAN == SEQ_BYTEORDER_LITTLE_ENDIAN
			return ((reinterpret_cast<u_float*>(reinterpret_cast<char*>(&(X))))->bytes[1] & (static_cast<unsigned short>(0x8000))) != 0;
#else
			return std::signbit(X);
#endif
		}
		SEQ_ALWAYS_INLINE auto signbit(double X) noexcept -> bool
		{
			// faster signbit for little endian machines
#if SEQ_BYTEORDER_ENDIAN == SEQ_BYTEORDER_LITTLE_ENDIAN
			return ((reinterpret_cast<u_double*>(reinterpret_cast<char*>(&(X))))->bytes[3] & (static_cast<unsigned short>(0x8000))) != 0;
#else
			return std::signbit(X);
#endif
		}
		SEQ_ALWAYS_INLINE auto signbit(long double X) noexcept -> bool
		{
			// faster signbit for little endian machines
#if SEQ_BYTEORDER_ENDIAN == SEQ_BYTEORDER_LITTLE_ENDIAN
			return sizeof(long double) == sizeof(double) ?
				signbit(static_cast<double>(X)) :
				std::signbit(X);
#else
			return std::signbit(X);
#endif
		}


		
		//////////////////////////////////////////////////////////////////////////////
		/// Some convinient function for char manipulation
		//////////////////////////////////////////////////////////////////////////////

		template<class T>
		static SEQ_ALWAYS_INLINE auto sign_value(T val, int sign) noexcept -> T
		{
			return sign == -1 ? -val : val;
		}
		static SEQ_ALWAYS_INLINE auto sign_value(unsigned char val, int sign) noexcept -> unsigned char
		{
			return sign == -1 ? (0U - val) : val;
		}
		static SEQ_ALWAYS_INLINE auto sign_value(unsigned short val, int sign) noexcept -> unsigned short
		{
			return sign == -1 ? (0U - val) : val;
		}
		static SEQ_ALWAYS_INLINE auto sign_value(unsigned int val, int sign) noexcept -> unsigned int
		{
			return sign == -1 ? (0U - val) : val;
		}
		static SEQ_ALWAYS_INLINE auto sign_value(unsigned long val, int sign) noexcept -> unsigned long
		{
			return sign == -1 ? (0U - val) : val;
		}
		static SEQ_ALWAYS_INLINE auto sign_value(unsigned long long val, int sign) noexcept -> unsigned long long
		{
			return sign == -1 ? (0ULL - val) : val;
		}


		static SEQ_ALWAYS_INLINE auto digit_value(int c)  noexcept -> unsigned
		{
			return static_cast<unsigned>(c - '0');
		}
		static SEQ_ALWAYS_INLINE auto is_digit(int c) noexcept -> bool
		{
			return digit_value(c) <= 9;
		}
		
		static SEQ_ALWAYS_INLINE auto is_space(int c)  noexcept -> bool
		{
			return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
		}
		static SEQ_ALWAYS_INLINE auto is_eol(int c)  noexcept -> bool
		{
			return c == '\n' || c == '\r';
		}
		static SEQ_ALWAYS_INLINE auto to_upper(int c) -> char
		{
			static constexpr char offset = 'a' - 'A';
			return static_cast<char>(c >= 'a' ? c - offset : c);
		}
		static SEQ_ALWAYS_INLINE auto to_digit_hex(int c) -> unsigned
		{
			c = to_upper(c);
			return (c < ':' && c > '/') ?
				static_cast<unsigned>(c - '0')
				: ((c >= 'A' && c <= 'Z') ? static_cast<unsigned>(c - 'A' + 10) : static_cast<unsigned>(-1));
		}



		//////////////////////////////////////////////////////////////////////////////
		/// Read functions
		//////////////////////////////////////////////////////////////////////////////

		static inline auto get_pow_double(int exponent) -> double
		{
			static const double  pow10[] = { 1E-323, 1E-322, 1E-321, 1E-320, 1E-319, 1E-318, 1E-317, 1E-316, 1E-315, 1E-314, 1E-313, 1E-312, 1E-311, 1E-310, 1E-309, 1E-308, 1E-307, 1E-306, 1E-305, 1E-304, 1E-303, 1E-302, 1E-301, 1E-300, 1E-299, 1E-298, 1E-297, 1E-296, 1E-295, 1E-294, 1E-293, 1E-292, 1E-291, 1E-290, 1E-289, 1E-288, 1E-287, 1E-286, 1E-285, 1E-284, 1E-283, 1E-282, 1E-281, 1E-280, 1E-279, 1E-278, 1E-277, 1E-276, 1E-275, 1E-274, 1E-273, 1E-272, 1E-271, 1E-270, 1E-269, 1E-268, 1E-267, 1E-266, 1E-265, 1E-264, 1E-263, 1E-262, 1E-261, 1E-260, 1E-259, 1E-258, 1E-257, 1E-256, 1E-255, 1E-254, 1E-253, 1E-252, 1E-251, 1E-250, 1E-249, 1E-248, 1E-247, 1E-246, 1E-245, 1E-244, 1E-243, 1E-242, 1E-241, 1E-240, 1E-239, 1E-238, 1E-237, 1E-236, 1E-235, 1E-234, 1E-233, 1E-232, 1E-231, 1E-230, 1E-229, 1E-228, 1E-227, 1E-226, 1E-225, 1E-224, 1E-223, 1E-222, 1E-221, 1E-220, 1E-219, 1E-218, 1E-217, 1E-216, 1E-215, 1E-214, 1E-213, 1E-212, 1E-211, 1E-210, 1E-209, 1E-208, 1E-207, 1E-206, 1E-205, 1E-204, 1E-203, 1E-202, 1E-201, 1E-200, 1E-199, 1E-198, 1E-197, 1E-196, 1E-195, 1E-194, 1E-193, 1E-192, 1E-191, 1E-190, 1E-189, 1E-188, 1E-187, 1E-186, 1E-185, 1E-184, 1E-183, 1E-182, 1E-181, 1E-180, 1E-179, 1E-178, 1E-177, 1E-176, 1E-175, 1E-174, 1E-173, 1E-172, 1E-171, 1E-170, 1E-169, 1E-168, 1E-167, 1E-166, 1E-165, 1E-164, 1E-163, 1E-162, 1E-161, 1E-160, 1E-159, 1E-158, 1E-157, 1E-156, 1E-155, 1E-154, 1E-153, 1E-152, 1E-151, 1E-150, 1E-149, 1E-148, 1E-147, 1E-146, 1E-145, 1E-144, 1E-143, 1E-142, 1E-141, 1E-140, 1E-139, 1E-138, 1E-137, 1E-136, 1E-135, 1E-134, 1E-133, 1E-132, 1E-131, 1E-130, 1E-129, 1E-128, 1E-127, 1E-126, 1E-125, 1E-124, 1E-123, 1E-122, 1E-121, 1E-120, 1E-119, 1E-118, 1E-117, 1E-116, 1E-115, 1E-114, 1E-113, 1E-112, 1E-111, 1E-110, 1E-109, 1E-108, 1E-107, 1E-106, 1E-105, 1E-104, 1E-103, 1E-102, 1E-101, 1E-100, 1E-099, 1E-098, 1E-097, 1E-096, 1E-095, 1E-094, 1E-093, 1E-092, 1E-091, 1E-090, 1E-089, 1E-088, 1E-087, 1E-086, 1E-085, 1E-084, 1E-083, 1E-082, 1E-081, 1E-080, 1E-079, 1E-078, 1E-077, 1E-076, 1E-075, 1E-074, 1E-073, 1E-072, 1E-071, 1E-070, 1E-069, 1E-068, 1E-067, 1E-066, 1E-065, 1E-064, 1E-063, 1E-062, 1E-061, 1E-060, 1E-059, 1E-058, 1E-057, 1E-056, 1E-055, 1E-054, 1E-053, 1E-052, 1E-051, 1E-050, 1E-049, 1E-048, 1E-047, 1E-046, 1E-045, 1E-044, 1E-043, 1E-042, 1E-041, 1E-040, 1E-039, 1E-038, 1E-037, 1E-036, 1E-035, 1E-034, 1E-033, 1E-032, 1E-031, 1E-030, 1E-029, 1E-028, 1E-027, 1E-026, 1E-025, 1E-024, 1E-023, 1E-022, 1E-021, 1E-020, 1E-019, 1E-018, 1E-017, 1E-016, 1E-015, 1E-014, 1E-013, 1E-012, 1E-011, 1E-010, 1E-009, 1E-008, 1E-007, 1E-006, 1E-005, 1E-004, 1E-003, 1E-002, 1E-001, 1E+000, 1E+001, 1E+002, 1E+003, 1E+004, 1E+005, 1E+006, 1E+007, 1E+008, 1E+009, 1E+010, 1E+011, 1E+012, 1E+013, 1E+014, 1E+015, 1E+016, 1E+017, 1E+018, 1E+019, 1E+020, 1E+021, 1E+022, 1E+023, 1E+024, 1E+025, 1E+026, 1E+027, 1E+028, 1E+029, 1E+030, 1E+031, 1E+032, 1E+033, 1E+034, 1E+035, 1E+036, 1E+037, 1E+038, 1E+039, 1E+040, 1E+041, 1E+042, 1E+043, 1E+044, 1E+045, 1E+046, 1E+047, 1E+048, 1E+049, 1E+050, 1E+051, 1E+052, 1E+053, 1E+054, 1E+055, 1E+056, 1E+057, 1E+058, 1E+059, 1E+060, 1E+061, 1E+062, 1E+063, 1E+064, 1E+065, 1E+066, 1E+067, 1E+068, 1E+069, 1E+070, 1E+071, 1E+072, 1E+073, 1E+074, 1E+075, 1E+076, 1E+077, 1E+078, 1E+079, 1E+080, 1E+081, 1E+082, 1E+083, 1E+084, 1E+085, 1E+086, 1E+087, 1E+088, 1E+089, 1E+090, 1E+091, 1E+092, 1E+093, 1E+094, 1E+095, 1E+096, 1E+097, 1E+098, 1E+099, 1E+100, 1E+101, 1E+102, 1E+103, 1E+104, 1E+105, 1E+106, 1E+107, 1E+108, 1E+109, 1E+110, 1E+111, 1E+112, 1E+113, 1E+114, 1E+115, 1E+116, 1E+117, 1E+118, 1E+119, 1E+120, 1E+121, 1E+122, 1E+123, 1E+124, 1E+125, 1E+126, 1E+127, 1E+128, 1E+129, 1E+130, 1E+131, 1E+132, 1E+133, 1E+134, 1E+135, 1E+136, 1E+137, 1E+138, 1E+139, 1E+140, 1E+141, 1E+142, 1E+143, 1E+144, 1E+145, 1E+146, 1E+147, 1E+148, 1E+149, 1E+150, 1E+151, 1E+152, 1E+153, 1E+154, 1E+155, 1E+156, 1E+157, 1E+158, 1E+159, 1E+160, 1E+161, 1E+162, 1E+163, 1E+164, 1E+165, 1E+166, 1E+167, 1E+168, 1E+169, 1E+170, 1E+171, 1E+172, 1E+173, 1E+174, 1E+175, 1E+176, 1E+177, 1E+178, 1E+179, 1E+180, 1E+181, 1E+182, 1E+183, 1E+184, 1E+185, 1E+186, 1E+187, 1E+188, 1E+189, 1E+190, 1E+191, 1E+192, 1E+193, 1E+194, 1E+195, 1E+196, 1E+197, 1E+198, 1E+199, 1E+200, 1E+201, 1E+202, 1E+203, 1E+204, 1E+205, 1E+206, 1E+207, 1E+208, 1E+209, 1E+210, 1E+211, 1E+212, 1E+213, 1E+214, 1E+215, 1E+216, 1E+217, 1E+218, 1E+219, 1E+220, 1E+221, 1E+222, 1E+223, 1E+224, 1E+225, 1E+226, 1E+227, 1E+228, 1E+229, 1E+230, 1E+231, 1E+232, 1E+233, 1E+234, 1E+235, 1E+236, 1E+237, 1E+238, 1E+239, 1E+240, 1E+241, 1E+242, 1E+243, 1E+244, 1E+245, 1E+246, 1E+247, 1E+248, 1E+249, 1E+250, 1E+251, 1E+252, 1E+253, 1E+254, 1E+255, 1E+256, 1E+257, 1E+258, 1E+259, 1E+260, 1E+261, 1E+262, 1E+263, 1E+264, 1E+265, 1E+266, 1E+267, 1E+268, 1E+269, 1E+270, 1E+271, 1E+272, 1E+273, 1E+274, 1E+275, 1E+276, 1E+277, 1E+278, 1E+279, 1E+280, 1E+281, 1E+282, 1E+283, 1E+284, 1E+285, 1E+286, 1E+287, 1E+288, 1E+289, 1E+290, 1E+291, 1E+292, 1E+293, 1E+294, 1E+295, 1E+296, 1E+297, 1E+298, 1E+299, 1E+300, 1E+301, 1E+302, 1E+303, 1E+304, 1E+305, 1E+306, 1E+307, 1E+308 };
			return pow10[exponent + 323];
		}
		template<class T>
		inline auto get_pow(int exp) -> T
		{
			return T();
		}
		template<>
		inline auto get_pow(int exp) -> float
		{
			return static_cast<float>(get_pow_double(exp));
		}
		template<>
		inline auto get_pow(int exp) -> double
		{
			return (get_pow_double(exp));
		}
		template<>
		inline auto get_pow(int exp) -> long double
		{
#if LDBL_MANT_DIG > DBL_MANT_DIG
			return std::pow(10.L, static_cast<long double>(exp));
#endif
			return static_cast<long double>(get_pow_double(exp));
		}
		
		

		
		

		template< class T, class Stream>
		auto read_integral_base_10(Stream& str) -> T
		{
			// Fast decimal integral reading

			str.reset();

			typename Stream::streamsize saved = str.tell();
			T x = 0;
			int sign = 1;

			// get frist character
			int first = str.getc();

			// skip spaces
			while (is_space(first))
				first = str.getc();

			// check EOF
			if (SEQ_UNLIKELY(first == EOF))
				return 0;

			// check sign
			if (first == '+') {
				sign = 1;
				first = str.getc();
			}
			else if (first == '-') {
				if (!std::is_signed<T>::value)
					goto error;
				sign = -1;
				first = str.getc();
			}
			else if (SEQ_UNLIKELY(!is_digit(first)))
				goto error;
			else {
				x = static_cast<T>(first - '0');
				first = str.getc();
			}

			while (is_digit(first)) {
				x = (x * static_cast<T>(10)) + static_cast<T>(first - '0');
				first = str.getc();
			}
			if (first != EOF)
				str.back();

			str.reset();
			
			return sign_value(x, sign);

		error:
			if (str) str.set_state(BadInputFormat);
			str.seek(saved);
			return 0;
		}


		template< class T, class Stream>
		auto read_integral(Stream& str, unsigned base) -> T
		{
			// Generic integral reading

			if (base == 10)
				// faster version for base 10
				return read_integral_base_10<T>(str);

			SEQ_ASSERT_DEBUG(base <= 36, "invalid 'base' value");

			str.reset();

			typename Stream::streamsize saved = str.tell();
			T x = 0;
			int sign = 1;
			unsigned val;
			// get frist character
			int first = str.getc();

			// skip spaces
			while (is_space(first))
				first = str.getc();

			// check EOF
			if (SEQ_UNLIKELY(first == EOF))
				return 0;

			// check sign
			if (first == '+') {
				sign = 1;
				first = str.getc();
			}
			else if (first == '-') {
				if (SEQ_UNLIKELY(!std::is_signed<T>::value))
					goto error;
				sign = -1;
				first = str.getc();
			}

			{
				//read first digit
				val = to_digit_hex(first);
				if (SEQ_UNLIKELY(val >= base))
					goto error;
				x = static_cast<T>(val);

				//read second one
				first = str.getc();
				if (base == 16 && to_upper(first) == 'X') {
					//hexadecimal starting with '0x' or '0X'
					if (x != 0) {
						//X after non 0 digit, return
						str.back();
						return sign_value(x, sign);//sign == -1 ? -x : x;
					}
					first = str.getc();
					val = to_digit_hex(first);
					if (SEQ_UNLIKELY(val >= base))
						// next char after '0x' is not a valid hex digit
						goto error;
				}
				else
					val = to_digit_hex(first);

				if (SEQ_UNLIKELY(val >= base))
				{
					//invalid char
					str.back();
					return sign_value(x, sign); //sign == -1 ? -x : x;
				}
				x = (x * static_cast<T>(base)) + static_cast<T>(val);
				first = str.getc();
				val = to_digit_hex(first);
			}

			while (val < base) {
				x = (x * static_cast<T>(base)) + static_cast<T>(val);
				first = str.getc();
				val = to_digit_hex(first);
			}
			if (first != EOF)
				str.back();

			str.reset();
			return sign_value(x, sign); //sign == -1 ? -x : x;

		error:
			if (str) str.set_state(BadInputFormat);
			str.seek(saved);
			return 0;
		}




		static constexpr unsigned _EOF = static_cast<unsigned>(4294967247U);//  EOF - '0';

		template<class Stream>
		SEQ_ALWAYS_INLINE auto read_int64(Stream& str) -> std::int64_t
		{
			// Read an int64 without the sign and leading spaces
			// Read digits by group of 2 to reduce integer multiplications
			// Returns -1 if the integer contains more than 18 digits

			static const char integral_read_table[10][10] =
			{ {0,1,2,3,4,5,06,7,8,9},
			{10,11,12,13,14,15,16,17,18,19},
			{20,21,22,23,24,25,26,27,28,29},
			{30,31,32,33,34,35,36,37,38,39},
			{40,41,42,43,44,45,46,47,48,49},
			{50,51,52,53,54,55,56,57,58,59},
			{60,61,62,63,64,65,66,67,68,69},
			{70,71,72,73,74,75,76,77,78,79},
			{80,81,82,83,84,85,86,87,88,89},
			{90,91,92,93,94,95,96,97,98,99} };


			std::int64_t x = 0;
			int count = 0;
			// get frist character
			unsigned first = static_cast<unsigned>(str.getc() - '0');
			unsigned second;

			// check EOF
			if (SEQ_UNLIKELY(first == _EOF || !(first <= 9))) {
				goto error;
			}
			
			second = static_cast<unsigned>(str.getc() - '0');
			count = 0;
			while ((first <= 9) && (second <= 9))
			{
				x = x * static_cast<int64_t>(100) + static_cast<int64_t>(integral_read_table[first][second] );
				first = static_cast<unsigned>(str.getc() - '0');
				second = static_cast<unsigned>(str.getc() - '0');
				count += 2;
				if (SEQ_UNLIKELY(count > 18)) {
					goto too_long_int;
				}
			}
			if (second != _EOF) {
				str.back();
			}
			if ((first <= 9)) {
				x = x * 10 + static_cast<int64_t>(first );
				if (++count > 18) {
					goto too_long_int;
				}
				
			}
			else if (first != _EOF) {
				str.back();
			}
			
			str.reset();
			return x;

		too_long_int:
			str.set_state(BadInputFormat);
			return -1;
		error:
			str.set_state(BadInputFormat);
			return 0;
		}


		template<class T, class Integral, class Stream>
		SEQ_NOINLINE(auto) read_nan(Integral saved_pos, Stream& str) -> T
		{
			// read 'nan'
			char a = to_upper(str.getc());
			char n = to_upper(str.getc());
			if (a == 'A' && n == 'N') {
				return std::numeric_limits<T>::quiet_NaN();
			} 
			str.set_state(BadInputFormat);
			str.seek(saved_pos);
			return 0;
		
		}
		template<class T, class Integral, class Stream>
		SEQ_NOINLINE(auto) read_inf(Integral saved_pos, int sign, Stream& str) -> T
		{
			// read 'inf'
			char n = to_upper(str.getc());
			char f = to_upper(str.getc());
			if (n == 'N' && f == 'F') {
				return sign_value(std::numeric_limits<T>::infinity(), sign);
			} 
			str.set_state(BadInputFormat);
			str.seek(saved_pos);
			return 0;
		
		}
		template<class T, class Integral, class Stream>
		SEQ_NOINLINE(auto) return_bad_format(Integral saved_pos, Stream& str) -> T
		{
			str.set_state(BadInputFormat);
			str.seek(saved_pos);
			return 0;
		}


		template<class Integral, bool Pointer = std::is_pointer<Integral>::value>
		struct NullValue{
			static constexpr Integral value = 0;
		};
		template<class Integral>
		struct NullValue<Integral,true>{
			static constexpr Integral value = nullptr;
		};

		template<class T, class Integral, class Stream>
		SEQ_NOINLINE(auto) read_long_double(T res, Integral saved, int sign, char dot, Stream& str, const chars_format& fmt) -> T
		{
			// Read double too long to be read as separate integrals

			if (std::isnan(res)) {
				res = 0;
			}

			Integral save_point = NullValue<Integral>::value;
			int first = str.getc();

			//read decimal
			while (is_digit(first)) {
				res = (res * static_cast<T>(10)) + static_cast<T>(first - '0');
				first = str.getc();
			}
			if (first == EOF) {
				if (SEQ_UNLIKELY((fmt & scientific) && !(fmt & fixed))) {
					str.set_state(BadInputFormat);
					goto error;
				}
				str.reset();
				return sign_value(res, sign);
			}

			if (first == dot) {

				//read fractional part
				first = str.getc();
				T factor = static_cast<T>(0.1);
				while (is_digit(first)) {
					res = res + static_cast<T>(first - '0') * factor;
					factor *= static_cast<T>(0.1);
					first = str.getc();
				}
				if (first == EOF) {
					if (SEQ_UNLIKELY((fmt & scientific) && !(fmt & fixed))) {
						str.set_state(BadInputFormat);
						goto error;
					}

					str.reset();
					return sign_value(res, sign);
				}
			}

			if (first == 'e' || first == 'E') {
				if (SEQ_UNLIKELY((fmt & scientific) == 0))
				{
					str.back();
					return sign_value(res, sign);
				}

				save_point = str.tell() - 1;
				// read exponent part
				first = str.getc();
				int exp_sign = 1;
				int exp = 0;
				if (first == '-') {
					exp_sign = -1;
					first = str.getc();
					if (!is_digit(first)) {
						goto partial;
					}
				}
				else if (first == '+') {
					first = str.getc();
					if (!is_digit(first)) {
						goto partial;
					}
				}
				else if (!is_digit(first)) {
					goto partial;
				}

				do {
					exp = (exp * 10) + static_cast<int>(first - '0');
					first = str.getc();
				} while (is_digit(first));
				exp = exp_sign == -1 ? -exp : exp;
				if (exp < -323 || exp > 308)
				{
#if LDBL_MANT_DIG > DBL_MANT_DIG
					// long double are supported
					res *= std::pow(10.L, exp);
#else
					res = exp > 0 ? std::numeric_limits<T>::infinity() : 0;
#endif
				}
				else {
					res *= static_cast<T>(get_pow_double(exp));//pow10[exp + 323];
				}

				if (first != EOF) {
					str.back();
				}
			}
			else {
				if (SEQ_UNLIKELY((fmt & scientific) && !(fmt & fixed))) {
					str.set_state(BadInputFormat);
					goto error;
				}
				if (first != EOF) {
					str.back();
				}
			}


			str.reset();
			return sign_value(res, sign);

		error:
			if (str) {
				str.set_state(BadInputFormat);
			}
			str.seek(saved);
			return 0;
		partial:
			str.reset();
			str.seek(save_point);
			return sign_value(res, sign);
		}

		template<class T, class Stream>
		SEQ_ALWAYS_INLINE auto read_double(Stream& str, chars_format fmt, char dot) -> T
		{
			using Integral = typename Stream::streamsize;

			str.reset();
			Integral saved = str.tell();
			int first = str.getc();
			int sign = 1;
			int c;
			T res = 0;
			

			// skip spaces
			while (is_space(first)) {
				first = str.getc();
			}
			// check EOF
			if (SEQ_UNLIKELY(first == EOF)) {
				return 0;
			}
				
			if (first == '-') {
				// first character is a sign
				sign = -1;
				first = str.getc();
				if (SEQ_UNLIKELY(first == EOF)) {
					return 0;
				}
			}
			else if (first == '+') {
				// first character is a sign
				first = str.getc();
				if (SEQ_UNLIKELY(first == EOF)) {
					return 0;
				}
			}
			// potential nan
			else if (SEQ_UNLIKELY(to_upper(first) == 'N')) {
				// read 'nan'
				return read_nan<T>(saved, str);
			}
			// read potential infinity
			if (SEQ_UNLIKELY(first == 'i' || first == 'I')) {
				// read 'inf'
				return read_inf<T>(saved,sign, str);
			}
			if (SEQ_UNLIKELY(!is_digit(first) && first != dot))
				// first character must be either the dot or a digit
			{
				return return_bad_format<T>(saved, str);
			}

			
			// fast path: read integral part and decimal part as integers
			str.back();
			Integral check_point = str.tell();
			// read intergal part
			int64_t integral = read_int64(str);
			if (SEQ_UNLIKELY(!str)) {
				if (integral == -1) { 
					//too big: switch to the default slower version
					goto slow_path;
				}
				
				// invalid intergal part: invalid format
				return return_bad_format<T>(saved, str);
			}

			// keep going with dot, decimal part, exponent.
			// mark this state as a chack point, as the intergal part might be reused by the slow path.
			res = static_cast<T>(integral);
			check_point = str.tell();

			c = str.getc();
			if (c == dot) {
				//read decimal part
				Integral new_check_point = str.tell();
				int64_t decimal = read_int64(str);
				if (SEQ_UNLIKELY(!str)) {
					if (decimal == -1) {
						//too big: switch to the default slower version
						goto slow_path;
					}
					if (fmt == scientific) { 
						// exponent is mandatory
						return return_bad_format<T>(saved, str);
					}
					//we accept a dot with nothing after
					return sign_value(res, sign);
				}
				int dist = static_cast<int>(str.tell() - new_check_point);
				res += static_cast<T>(decimal) * get_pow<T>(-dist);
				c = str.getc();
			}
			if (c == 'e' || c == 'E') {
				if (SEQ_UNLIKELY(fmt == fixed)) {
					// exponent forbiden
					return return_bad_format<T>(saved, str);
				}
				//read exponent
				int exp = 0;
				int esign = 1;
				c = str.getc();
				if (!is_digit(c)) {
					//read exponent sign
					if (c == '-') { esign = -1;
					} else if (SEQ_UNLIKELY(c != '+')) {
						return return_bad_format<T>(saved, str);
					}
					c = str.getc();
					if (!is_digit(c)) {
						return return_bad_format<T>(saved, str);
					}
				}
				// read exponent value
				unsigned cc = static_cast<unsigned>(c- '0');
				do {
					exp = (exp * 10) + static_cast<int>(cc);
					cc = static_cast<unsigned>(str.getc() - '0');
				} while (cc <= 9U);
				if (cc != _EOF) {
					str.back();
				} else {
					str.reset();
				}

				exp = sign_value(exp, esign);
				// check exponent validity
				if (SEQ_UNLIKELY(exp > std::numeric_limits<T>::max_exponent10)) {
					return sign_value(std::numeric_limits<T>::infinity(), sign);
				} if (SEQ_UNLIKELY(exp < std::numeric_limits<T>::min_exponent10))
					return sign_value(static_cast<T>(0), sign);

				// success
				T p = get_pow<T>(exp);
				res = sign_value(res * p, sign);
				return res;
			}
			if (SEQ_UNLIKELY(fmt == scientific)) {
				// exponent is mandatory
				return return_bad_format<T>(saved, str);
			}
			else {
				// success
				str.reset();
				if (c != EOF)
					str.back();
				return sign_value(res, sign);
			}
			
		slow_path:
			// slower way of reading a floating point value, relies on floating point arithmetic.
			// reuse the integral part if possible.
			str.seek(check_point);
			return read_long_double<T>(integral != -1 ? res : std::numeric_limits<T>::quiet_NaN(), saved, sign, dot, str,fmt);
		}




		template<class T, class Stream>
		auto read_string(Stream& str) -> T
		{
			// Read a string object (std::string or seq::tiny_string) from input stream
			str.reset();

			int first = str.getc();

			// skip spaces
			while (is_space(first))
				first = str.getc();
			// check EOF
			if (SEQ_UNLIKELY(first == EOF))
				return T();

			T res;
			res.push_back(static_cast<char>(first));
			for (;;) {
				first = str.getc();
				if (!is_space(first) && first != EOF)
					res.push_back(first);
				else
					break;
			}

			if (first != EOF) {
				str.back();
			}
			str.reset();
			return res;
		}


		template<class T, class Stream>
		auto read_line(Stream& str) -> T
		{
			// Read a line (std::string or seq::tiny_string) from input stream
			str.reset();

			int first = str.getc();

			// skip spaces
			while (is_space(first)) {
				first = str.getc();
			}
			// check EOF
			if (SEQ_UNLIKELY(first == EOF))
				return T();

			T res;
			res.push_back(static_cast<char>(first));
			for (;;) {
				first = str.getc();
				if (!is_eol(first) && first != EOF)
					res.push_back(first);
				else
					break;
			}

			if (first != EOF) {
				str.back();
			}
			str.reset();
			return res;
		}





		//////////////////////////////////////////////////////////////////////////////
		/// Write functions
		//////////////////////////////////////////////////////////////////////////////



		template <class Range, class T>
		SEQ_ALWAYS_INLINE auto write_integer_generic(const Range & range, T _val, int base, const integral_chars_format& fmt) -> to_chars_result
		{
			// Generic version of integer to string.
			// Write in reverse order in a temporay buffer, and copy/reverse in dst.
			// Outputs '0x' prefix for base 16 if specified.

			SEQ_ASSERT_DEBUG(base <= 36, "invalid 'base' value");

			static const char* upper_chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
			static const char* lower_chars = "0123456789abcdefghijklmnopqrstuvwxyz";
			
			if (!_val) {
				int size = fmt.integral_min_width ? static_cast<int>(fmt.integral_min_width) : 1;
				char* dst = range.add_size(static_cast<size_t>(size));
				if (SEQ_UNLIKELY(!dst)) {
					return { range.end_ptr(), std::errc::value_too_large };
}
				while(size-- > 0) {
					*dst++ = ( '0');
}
				return { dst , std::errc()};
			}
			const char* chars = fmt.upper_case ? upper_chars : lower_chars;
			int min_width = static_cast<int>(fmt.integral_min_width);
			char tmp[66];
			int index = 65;
			tmp[index] = 0;
			unsigned neg = 0;

			using uint_type = typename integer_abs_return<T>::type;
			uint_type val;

			if (std::is_signed<T>::value && _val < 0) {
				neg = 1;
				val = negate_if_signed(_val);
			}
			else {
				val = static_cast<uint_type>(_val);
			}

			while (val)
			{
				uint_type _new = val / static_cast<uint_type>(base);
				uint_type rem = val % static_cast<uint_type>(base);
				tmp[--index] = chars[rem];
				val = _new;
			}
			if (min_width >= 0) {
				int count = 65 - index;
				while (min_width-- > count && index > 0) {
					tmp[--index] = '0';
				}
			}
			if (base == 16 && fmt.hex_prefix) {
				tmp[--index] = 'x';
				tmp[--index] = '0';
			}
			if (std::is_signed<T>::value && neg) {
				tmp[--index] = '-';
			}

			size_t size = sizeof(tmp) - static_cast<size_t>(index) - 1;
			char* dst = range.add_size(size);
			if (SEQ_UNLIKELY(!dst) ) {
				return { range.end_ptr(), std::errc::value_too_large };
			}
			memcpy(dst, tmp + index, size);
			return { dst + size, std::errc() };
		}





		static SEQ_ALWAYS_INLINE auto decimal_table() -> const char*
		{
			// Static table used to write integers by chunk of 2 digits (in base 10 only)
			static const char* table = "00102030405060708090011121314151617181910212223242526272829203132333435363738393041424344454647484940515253545556575859506162636465666768696071727374757677787970818283848586878889809192939495969798999";
			return table;
		}


		template<class Range, class T>
		SEQ_ALWAYS_INLINE auto write_integral(const Range & range, T _value, int base, const integral_chars_format& fmt) -> to_chars_result
		{
			// Write integral in any base
			// Use well known trick to write 2 digits at once based on a lookup table (for base 10 only).

			if (base != 10) {
				return detail::write_integer_generic(range, _value, base, fmt);
			}

			const char* table = decimal_table();
			unsigned neg = 0;
			
			using uint_type = typename integer_abs_return<T>::type;
			uint_type value;

			if (std::is_signed<T>::value && _value < 0) {
				neg = 1;
				value = negate_if_signed(_value);
			}
			else {
				value = static_cast<uint_type>(_value);
			}

			unsigned digit = count_digits_base_10(value);
			if (SEQ_UNLIKELY(fmt.integral_min_width > digit)) {
				digit = fmt.integral_min_width;
			}
			if (std::is_signed<T>::value) { 
				digit += neg;
			}
			char* buffer = range.add_size(digit );
			if (SEQ_UNLIKELY(!buffer)) {
				return { range.end_ptr(), std::errc::value_too_large };
			}

			char* start = buffer + neg;
			char* res = buffer + digit;
			buffer = res;
			

			while (value >= 100) {
				const auto i = static_cast<unsigned>( (value % 100U) * 2U);
				value /= 100U;
				
				*--buffer = table[i];
				*--buffer = table[i + 1];
			}

			if (value < 10U) {
				*--buffer = static_cast<char>(value) + '0';
			}
			else {
				const auto i = static_cast<unsigned>(value * 2U);
				*--buffer = table[i];
				*--buffer = table[i + 1];
			}
			while (buffer > start) {
				*--buffer = '0';
			}
			if (std::is_signed<T>::value && neg) {
				*--buffer = '-';
			}
			return { res, std::errc() };
		}

		template<class Range, class T>
		SEQ_ALWAYS_INLINE auto write_integer_decimal_part(const Range & range, T value, int min_width, bool null_first) -> to_chars_result
		{
			static_assert(std::is_unsigned<T>::value, "");

			// Write decimal part of floating point number.
			// Remove the trailling zeros.

			const char* table = decimal_table();

			// skip trailing zero
			while (value && value % 10U == 0 && min_width > 0) {
				value /= 10U;
				--min_width;
			}

			// check if no need to output
			if(null_first && value == 1 && min_width <= 1) {
				return { range.current(),std::errc() };
			}


			unsigned digit = count_digits_base_10(value);
			
			char* buffer = range.add_size(digit);
			if (SEQ_UNLIKELY(!buffer)) {
				return { range.end_ptr(), std::errc::value_too_large };
			}

			char* res = buffer + digit;
			char* start = buffer;
			buffer = res;

			while (value >= 100U) {
				const auto i = static_cast<unsigned>((value % 100U) * 2U);
				value /= 100U;
				*--buffer = table[i];
				*--buffer = table[i + 1];
			}

			if (value < 10U) {
				*--buffer = static_cast<char>(value) + '0';
			}
			else {
				const auto i = static_cast<unsigned>( value * 2U);
				*--buffer = table[i];
				*--buffer = table[i + 1];
			}
			while (buffer > start) {
				*--buffer = '0';
			}
			if (null_first) {
				--(*buffer);
			}
			return { res,std::errc() };
		}



		
		
		
		template< class T>
		struct float_tables
		{
			// Table type used to extract floating point exponent
			const T* mul_table;
			const T* comp_table;
			int size;
		};

		// 
		template<class T> 
		SEQ_ALWAYS_INLINE  auto get_pow_table() -> const std::int16_t* {
			// Returns table of power of 10 used in conjunction with float_tables
			return NULL; 
		}
		template<>
		SEQ_ALWAYS_INLINE  auto get_pow_table<float>() -> const std::int16_t* {
			static const std::int16_t pow_table[] = { 16,8,4,2,1 };
			return pow_table;
		}
		template<>
		SEQ_ALWAYS_INLINE  auto get_pow_table<double>() -> const std::int16_t* {
			static const std::int16_t pow_table[] = { 256,128,64,32,16,8,4,2,1 };
			return pow_table;
		}
#if LDBL_MANT_DIG > DBL_MANT_DIG
		template<>
		SEQ_ALWAYS_INLINE  const std::int16_t* get_pow_table<long double>() {
			static const std::int16_t pow_table[] = { 4096, 2048, 1024, 512, 256,128,64,32,16,8,4,2,1 };
			return pow_table;
		}
#else
		template<>
		SEQ_ALWAYS_INLINE  auto get_pow_table<long double>() -> const std::int16_t* {
			static const std::int16_t pow_table[] = { 256,128,64,32,16,8,4,2,1 };
			return pow_table;
		}
#endif


		
		template<class T>
		SEQ_ALWAYS_INLINE auto get_float_high_div_tables() -> const float_tables<T>* {
			// returns table used to extract positive exponent from floating point number
			return NULL;
		}
		template<>
		SEQ_ALWAYS_INLINE auto get_float_high_div_tables<float>() -> const float_tables<float>* {
			static const float mul_table[] = { 1e-16F,1e-8F,1e-4F,1e-2F,1e-1F };
			static const float comp_table[] = { 1e16F,1e8F,1e4F,1e2F,1e1F };
			static const float_tables<float> tables = { mul_table ,comp_table , 5 };
			return &tables;
		}
		template<>
		SEQ_ALWAYS_INLINE auto get_float_high_div_tables<double>() -> const float_tables<double>* {
			static const double mul_table[] = { 1e-256,1e-128,1e-64,1e-32,1e-16,1e-8,1e-4,1e-2,1e-1 };
			static const double comp_table[] = { 1e256,1e128,1e64,1e32,1e16,1e8,1e4,1e2,1e1 };
			static const float_tables<double> tables = { mul_table ,comp_table , 9 };
			return &tables;
		}
#if LDBL_MANT_DIG > DBL_MANT_DIG
		template<>
		SEQ_ALWAYS_INLINE const float_tables<long double>* get_float_high_div_tables<long double>() {
			static const long double mul_table[] = { 1e-4096L, 1e-2048L, 1e-1024L, 1e-512L, 1e-256L,1e-128L,1e-64L,1e-32L,1e-16L,1e-8L,1e-4L,1e-2L,1e-1L };
			static const long double comp_table[] = { 1e4096L, 1e2048L, 1e1024L, 1e512L, 1e256L,1e128L,1e64L,1e32L,1e16L,1e8L,1e4L,1e2L,1e1L };
			static const float_tables<long double> tables = { mul_table ,comp_table , 13 };
			return &tables;
		}
#else
		template<>
		SEQ_ALWAYS_INLINE auto get_float_high_div_tables<long double>() -> const float_tables<long double>* {
			static const long double mul_table[] = { 1e-256,1e-128,1e-64,1e-32,1e-16,1e-8,1e-4,1e-2,1e-1 };
			static const long double comp_table[] = { 1e256,1e128,1e64,1e32,1e16,1e8,1e4,1e2,1e1 };
			static const float_tables<long double> tables = { mul_table ,comp_table , 9 };
			return &tables;
		}
#endif


		template<class T>
		SEQ_ALWAYS_INLINE auto get_float_low_div_tables() -> const float_tables<T>* {
			// returns table used to extract negative exponent from floating point number
			return NULL;
		}
		template<>
		SEQ_ALWAYS_INLINE auto get_float_low_div_tables<float>() -> const float_tables<float>* {
			static const float mul_table[] = { 1e16F,1e8F,1e4F,1e2F,1e1F };
			static const float comp_table[] = { 1e-15F,1e-7F,1e-3F,1e-1F,1e0F };
			static const float_tables<float> tables = { mul_table ,comp_table , 5 };
			return &tables;
		}
		template<>
		SEQ_ALWAYS_INLINE auto get_float_low_div_tables<double>() -> const float_tables<double>* {
			static const double mul_table[] = { 1e256,1e128,1e64,1e32,1e16,1e8,1e4,1e2,1e1 };
			static const double comp_table[] = { 1e-255,1e-127,1e-63,1e-31,1e-15,1e-7,1e-3,1e-1,1e0 };
			//static const double comp_table[] = { 1e-255,1e-127,1e-63,1e-31,1e-15,1e-7,1e-3,1e-1,1e0 };
			static const float_tables<double> tables = { mul_table ,comp_table , 9 };
			return &tables;
		}
#if LDBL_MANT_DIG > DBL_MANT_DIG
		template<>
		SEQ_ALWAYS_INLINE const float_tables<long double>* get_float_low_div_tables<long double>() {
			static const long double mul_table[] = { 1e4096L, 1e2048L, 1e1024L, 1e512L, 1e256L,1e128L,1e64L,1e32L,1e16L,1e8L,1e4L,1e2L,1e1L };
			static const long double comp_table[] = { 1e-4095L, 1e-2047L, 1e-1023L, 1e-511L, 1e-255L,1e-127L,1e-63L,1e-31L,1e-15L,1e-7L,1e-3L,1e-1L,1e0L };
			static const float_tables<long double> tables = { mul_table ,comp_table , 13 };
			return &tables;
		}
#else
		template<>
		SEQ_ALWAYS_INLINE auto get_float_low_div_tables<long double>() -> const float_tables<long double>* {
			static const long double mul_table[] = { 1e256,1e128,1e64,1e32,1e16,1e8,1e4,1e2,1e1 };
			static const long double comp_table[] = { 1e-255,1e-127,1e-63,1e-31,1e-15,1e-7,1e-3,1e-1,1e0 };
			static const float_tables<long double> tables = { mul_table ,comp_table , 9 };
			return &tables;
		}
#endif
		

		template<class T>
		SEQ_ALWAYS_INLINE auto normalize_double(T& value,  const float_chars_format& fmt) -> int {

			// Compute exponent and adjust value accordingly.
			// If scientific_notation is true, compute the maximum exponent value.
			
			(void)fmt;
			int exponent = 0;
			//const T saved = value;
			const int16_t* pow_table = get_pow_table<T>();

			if (value >= 1) 
			{
				const float_tables<T>* t = get_float_high_div_tables<T>();
				for (int start = 0; start < t->size; ++start) {
					if (value >= t->comp_table[start]) {
						value *= t->mul_table[start];
						exponent += pow_table[start];
					}
					
				}
			}
			else if (value > 0 && value < 1)
			{
				const float_tables<T>* t = get_float_low_div_tables<T>();
				for (int start = 0; start < t->size; ++start) {
					if (value < t->comp_table[start]) {
						value *= t->mul_table[start];
						exponent -= pow_table[start];
					}
				}
			}

			//for higher precision, this method can be used (note: reduce precision for long double as it might be the result of std::pow)
			//if (sizeof(T) <= 8)
			//	value = saved * get_pow<T>(-exponent);

			return exponent;
		}

		template<class T, class UInt>
		SEQ_ALWAYS_INLINE auto split_double(T value, UInt& integral, UInt& decimals, int16_t& exponent, bool & null_first, int width,  float_chars_format& fmt) -> int
		{
			// Split floating point value into its integral, decimal and exponent part.
			// Uses width to compute the proper exponent and decimal value.

			static constexpr int max_exp_for_fixed = sizeof(T) <= 4 ? 8 : 17;

			T saved = value;
			exponent = static_cast<int16_t>(normalize_double(value, fmt));

			if (fmt.fmt == seq::general) {
				//see https://stackoverflow.com/questions/30658919/the-precision-of-printf-with-specifier-g
				width = width == 0 ? 1 : width;
				// adjust format and precision
				if (width > exponent && exponent >= -4 && exponent <= max_exp_for_fixed){
					fmt.fmt = seq::fixed;
					width = width - 1 - exponent;
				}
				else{
					fmt.fmt = seq::scientific;
					width = width - 1;
				}
			}
			if (fmt.fmt == seq::fixed) {
				value = saved;
				exponent = 0;
			}

			integral = static_cast<UInt>(value);
			T remainder = value - static_cast<T>(integral);

			null_first = false;
			if (remainder != 0 && remainder < 0.1) {
				// remainder is inf to 0.1, we must add 0.1 to avoid truncating the leading zeros
				remainder += 0.1;
				null_first = true;
			}

			
			int p = width > 17 ? 17 : width;
			remainder *= get_pow<T>(p);
			decimals = static_cast<UInt>(remainder);

			// rounding
			remainder -= static_cast<T>(decimals);
			if (remainder >= 0.5) {
				decimals++;
				const UInt max_val = static_cast<UInt>(get_pow<T>(p) + static_cast<T>(0.5));
				if (SEQ_UNLIKELY(decimals >= max_val)) {
					decimals = 0;
					integral++;
					if (exponent != 0 && integral >= 10) {
						exponent++;
						integral = 1;
					}
				}
			}
			
			return width;
		}

		template<class Range, class T>
		SEQ_ALWAYS_INLINE auto write_double_fixed(const Range & range, T value, int width,  float_chars_format fmt) -> to_chars_result
		{
			// Write floating point value with fixed method (usually slower than scientific or general)

			// extract full exponent
			fmt.fmt = scientific;
			std::int16_t exponent = static_cast<std::int16_t>(normalize_double(value, fmt));

			// output sign
			
			char* start = range.current();
			const char* dec_table = decimal_table();

			// append leading 0, just use for rounding purpose
			if (SEQ_UNLIKELY(!range.append('0'))) {
				return { range.end_ptr(), std::errc::value_too_large };
			}

			
			std::int16_t exp = exponent;
			if (exponent >= 0) {
				// output integral part for positive exponent
				
				// reduce the number of floating point operations by outputting 2 digits at once
				if (exp) {
					value *= 10;
					do {
						char v = static_cast<char>(value);
						char* dst = range.add_size(2);
						if (SEQ_UNLIKELY(!dst)) {
							return { range.end_ptr(), std::errc::value_too_large };
						}
						const char* d = dec_table + static_cast<int>(v) * 2;
						dst[0] = d[1];
						dst[1] = d[0];
						exp -= 2;
						// for big exponents, we end up with a value of 0, therefore avoid the useless floating point math
						if (v != 0 || value != 0) {
							value -= v;
							value *= 100;
						}
					} while (exp >= 1);
					if (exp <= 0) { value *= static_cast<T>(0.1);}
				}
				while (exp >= 0) {
					char v = static_cast<char>(value);
					if (SEQ_UNLIKELY(!range.append(v + '0'))) {
						return { range.end_ptr(), std::errc::value_too_large };
					}
					--exp;
					value -= v;
					value *= 10.;
				}
			}
			else{
				// output 0 for negative exponent
				if (SEQ_UNLIKELY(!range.append('0'))) {
					return { range.end_ptr(), std::errc::value_too_large };
				}
			}
			// output dot
			if (SEQ_UNLIKELY(!range.append(fmt.dot))) {
				return { range.end_ptr(), std::errc::value_too_large };
			}

			// for negative exponent, output possibly LOTS of 0 before the remaining of decimal part
			while (++exponent < 0) {
				if (SEQ_UNLIKELY(!range.append('0'))) {
					return { range.end_ptr(), std::errc::value_too_large };
				}
				--width;
				if (width == -1) {
					width = 0;
					break; // we went outside the required precision, stop
				}
			}

			// output decimal part with a precision of width +1 for rounding purpose

			// We still have a precision issue, as normalize_double() might produce a remainder like 0.49999... while it should be 0.5
			// Therefore, the value might be rounded to the lower decimal instead of the upper one, only for values ending with '5'
			// Current fix is to add a very small value that can compensate the computation rounding errors
			// value += 1e-9; // Disabled for now
			
			// reduce the number of floating point operations by outputting 2 digits at once
			if (exponent >= 0) {
				if (width) {
					value *= 10;
					do {
						char v = static_cast<char>(value);
						char* dst = range.add_size(2);
						if (SEQ_UNLIKELY(!dst)) {
							return { range.end_ptr(), std::errc::value_too_large };
						}
						const char* d = dec_table  + static_cast<int>(v) * 2;
						dst[0] = d[1];
						dst[1] = d[0];
						width -= 2;
						if (v != 0 || value != 0) {
							value -= v;
							value *= 100;
						}
					} while (width >= 1);
					if (width <= 0) { value *= static_cast<T>(0.1); }
				}
				while (width >= 0) {
					char v = static_cast<char>(value);
					if (SEQ_UNLIKELY(!range.append(v + '0'))) {
						return { range.end_ptr(), std::errc::value_too_large };
					}
					--width;
					value -= v;
					value *= 10.;
				}
			}

			// round
			char* last = range.current() - 1;
			if (*last >= '5') {
				char* saved_last = last - 1;
				do {
					--last;
					//skip dot to round the integral part
					if (*last == fmt.dot) {
						--last;
					}
					(*last)++;
					if (*last == ':') { *last = '0';
					} else { break;}
				} while (last > start);

				if (last != start) {
					//remove leading 0
					memmove(start, start + 1, static_cast<size_t>(saved_last - start ));
					--saved_last;
				}
				last = saved_last;
			}
			else {
				//remove leading 0
				memmove(start, start + 1, static_cast<size_t>(last - start - 1));
				last -= 2;
			}

			//remove trailing 0(s)
			while (*last == '0') {
				--last;
			}

			// do not keep a dot without trailing values
			if (*last == fmt.dot) {
				--last;
			}
				

			return { last +1 ,std::errc() };
			
		}


		
		template<class Range>
		SEQ_NOINLINE(auto) write_nan(const Range& range, const float_chars_format fmt ) -> to_chars_result
		{
			// outputs 'nan'
			char* dst = range.add_size(3);
			if (SEQ_UNLIKELY(!dst)) {
				return { range.end_ptr(), std::errc::value_too_large };
			}
			if (fmt.upper) { memcpy(dst, "NAN", 3);
			} else {
				memcpy(dst, "nan",3);
			}
			return { dst + 3, std::errc() };
		}
		template<class Range>
		SEQ_NOINLINE(auto) write_inf(const Range& range, const float_chars_format fmt) -> to_chars_result
		{
			// outputs 'inf'
			char* dst = range.add_size(3);
			if (SEQ_UNLIKELY(!dst)) {
				return { range.end_ptr(), std::errc::value_too_large };
			}
			if (fmt.upper) {
				memcpy(dst, "INF",3);
			} else {
				memcpy(dst, "inf",3);
			}
			return { dst + 3, std::errc() };
		}

		template<class Range, class T>
		SEQ_ALWAYS_INLINE auto write_double_abs(const Range& range, T value, int width, float_chars_format& fmt) -> to_chars_result
		{
			using UInt = std::uint64_t;

			// Compute integral, decimal and exponent parts
			UInt integral;
			UInt decimals;
			int16_t exponent = 0;
			bool decrement_first = 0;
			width = split_double(value, integral, decimals, exponent, decrement_first, width, fmt);

			// Write integral part, at least one digit
			integral_chars_format tmp;
			tmp.integral_min_width = 1;
			to_chars_result r = write_integral(range, integral, 10, tmp);
			if (SEQ_UNLIKELY(r.ec == std::errc::value_too_large)) {
				return r;
			}

			if (SEQ_LIKELY(decimals)) {
				// Write dot first

				if (SEQ_UNLIKELY(!range.append(fmt.dot))) {
					return { range.end_ptr(), std::errc::value_too_large };
				}

				char* t = range.current();
				r = write_integer_decimal_part(range, (decimals), width, decrement_first);
				if (SEQ_UNLIKELY(r.ec == std::errc::value_too_large)) {
					return r;
				}

				// if nothing was outputed, remove the dot
				if (r.ptr == t) {
					r.ptr = range.back();
				}
			}
			//write exponent
			if (exponent | (fmt.fmt == scientific)) {
				if (SEQ_UNLIKELY(!range.append(fmt.exp))) {
					return { range.end_ptr(), std::errc::value_too_large };
				}
				if (exponent >= 0) {
					// For scientific notation (equivalent to 'e' or 'E'), force the '+' sign
					if (SEQ_UNLIKELY(!range.append('+'))) {
						return { range.end_ptr(), std::errc::value_too_large };
					}
				}

				tmp.integral_min_width = 2;
				r = write_integral(range, exponent, 10, tmp);
			}
			return r;
		}



		

		template<class Range, class T>
		SEQ_ALWAYS_INLINE auto write_double(const Range & range, T value, int width = 6, float_chars_format fmt = float_chars_format()) -> to_chars_result
		{
			// Write floating point value based on given format.
			// Main idea from https://blog.benoitblanchon.fr/lightweight-float-to-string/

			
			//if (SEQ_UNLIKELY(std::isnan(value)))
			if(SEQ_UNLIKELY(value != value)) {
				return write_nan(range, fmt);
			}
			if (detail::signbit(value)) {
				//extract sign, outputs '-' if negative, take abolute value
				if (SEQ_UNLIKELY(!range.append('-'))) {
					return { range.end_ptr(), std::errc::value_too_large };
				}
				value = -value;
			}
			if (SEQ_UNLIKELY(/*std::isinf(value))*/value == std::numeric_limits<double>::infinity())) {
				return write_inf(range, fmt);
			}
			
			if (width < 0) { 
				width = 6;
			}

			if ((fmt.fmt == fixed && (value < static_cast<T>(1e-15) || value >= static_cast<T>(1e16) || width > 17))) {
				// 'fixed' format is processed differently
				return write_double_fixed(range, value, width, fmt);
			}
				
			return write_double_abs(range, value, width, fmt);
		}






		/// @brief Input stream working on a sequence of characters. Lighter version of buffer_input_stream.
		///
		/// buffer_input_stream is a #basic_input_stream used to extract numbers, words and lines from a sequence of characters.
		/// Use seq::from_stream to extract numerical values and words, seq::read_line_from_stream to extract full lines.
		/// 
		class from_chars_stream 
		{
		public:
			using State = StreamState;
			using streamsize = const char*;

		private:
			const char* d_start;
			const char* d_end;
			std::errc d_state;

			auto set_eof() noexcept -> char {
				d_state = std::errc::invalid_argument;
				return EOF;
			}

		public:
			/// @brief Default ctor
			from_chars_stream()
				:d_start(nullptr), d_end(nullptr), d_state() {}
			/// @brief Construct from sequence of characters and size
			from_chars_stream(const char* data, const char * end)
				:d_start(data), d_end(end), d_state() {}
			
			/// @brief Set the stream state
			/// @param st stream state
			SEQ_ALWAYS_INLINE void set_state(State st) noexcept
			{
				d_state = st != Ok ? std::errc::invalid_argument : std::errc() ;
			}
			/// @brief Returns the current stream state
			SEQ_ALWAYS_INLINE auto state() const noexcept -> State
			{
				return d_state == std::errc() ? Ok : BadInputFormat;
			}
			SEQ_ALWAYS_INLINE auto error() const noexcept -> std::errc { return d_state; }
			SEQ_ALWAYS_INLINE void reset() {
				d_state = std::errc();
			}
			SEQ_ALWAYS_INLINE explicit operator bool() const noexcept {
				return d_state == std::errc();
			}
			/// @brief Close the stream
			void close()
			{
				d_start = d_end =nullptr;
				d_state = std::errc();
			}
			/// @brief Returns true if the stream is open
			SEQ_ALWAYS_INLINE auto is_open() const noexcept -> bool
			{
				return d_start != nullptr;
			}
			/// @brief Returns true if the stream read its last character
			SEQ_ALWAYS_INLINE auto at_end() const noexcept -> bool
			{
				return d_start >= d_end;
			}
			/// @brief Returns the internal character sequence size
			static SEQ_ALWAYS_INLINE auto size() noexcept -> size_t
			{
				return 0;
			}
			/// @brief Returns the current read position in the stream 
			SEQ_ALWAYS_INLINE auto tell() const noexcept -> const char*
			{
				return d_start;
			}
			/// @brief Go back one character (if possible)
			SEQ_ALWAYS_INLINE void back() noexcept
			{
				--d_start;
			}
			/// @brief Seek at given position
			/// @return The new read position
			SEQ_ALWAYS_INLINE void seek(const char* pos) noexcept
			{
				d_start = pos;
			}
			/// @brief Extract a single character from the stream and put the read position to the next character
			/// @return extracted character or EOF
			SEQ_ALWAYS_INLINE auto getc() noexcept -> int
			{
				if (SEQ_LIKELY(d_start != d_end)) {
					return *d_start++;
}
				d_state = std::errc::invalid_argument;
				return EOF;
			}
			
		};



	}// end namespace detail
	



	/// @brief Input stream working on a sequence of characters.
	///
	/// buffer_input_stream is a #basic_input_stream used to extract numbers, words and lines from a sequence of characters.
	/// Use seq::from_stream to extract numerical values and words, seq::read_line_from_stream to extract full lines.
	/// 
	class buffer_input_stream : public basic_input_stream< buffer_input_stream >
	{
	public:
		using base_type = basic_input_stream< buffer_input_stream >;
		using State = StreamState;
		using streamsize = size_t;

	private:
		const char* d_buff;
		streamsize d_len;
		streamsize d_pos;
		StreamState d_state;

		auto set_eof() noexcept -> char {
			d_state = EndOfFile;
			return EOF;
		}

	public:
		/// @brief Default ctor
		buffer_input_stream()
			:d_buff(nullptr), d_len(0), d_pos(0), d_state(Ok) {}
		/// @brief Construct from sequence of characters and size
		buffer_input_stream(const char* data, streamsize len)
			:d_buff(data), d_len(len), d_pos(0), d_state(Ok) {}
		/// @brief Construct from a null terminated string
		explicit buffer_input_stream(const char* data)
			:buffer_input_stream(data, strlen(data)) {}
		/// @brief Construct from a string-like object (usually std::string or tiny_string)
		template<class StringLike>
		explicit buffer_input_stream(const StringLike& str)
			: buffer_input_stream(str.data(), static_cast<streamsize>(str.size())) {}

		/// @brief Set the stream state
		/// @param st stream state
		SEQ_ALWAYS_INLINE void set_state(State st) noexcept
		{
			d_state = st;
		}
		/// @brief Returns the current stream state
		SEQ_ALWAYS_INLINE auto state() const noexcept -> State
		{
			return d_state;
		}
		/// @brief Close the stream
		void close()
		{
			d_buff = nullptr;
			d_len = d_pos = 0;
			d_state = Ok;
		}
		/// @brief Returns true if the stream is open
		SEQ_ALWAYS_INLINE auto is_open() const noexcept -> bool
		{
			return d_buff != nullptr;
		}
		/// @brief Returns true if the stream read its last character
		SEQ_ALWAYS_INLINE auto at_end() const noexcept -> bool
		{
			return d_pos >= d_len;
		}
		/// @brief Returns the internal character sequence size
		SEQ_ALWAYS_INLINE auto size() const noexcept -> streamsize
		{
			return d_len;
		}
		/// @brief Returns the current read position in the stream 
		SEQ_ALWAYS_INLINE auto tell() const noexcept -> streamsize
		{
			return d_pos;
		}
		/// @brief Go back one character (if possible)
		SEQ_ALWAYS_INLINE void back() noexcept
		{
			if (d_pos > 0) {
				--d_pos;
}
		}
		/// @brief Seek at given position
		/// @return The new read position
		SEQ_ALWAYS_INLINE auto seek(streamsize pos) noexcept -> streamsize
		{
			if (pos >= d_len) {
				pos = d_len;
			}
			return d_pos = pos;
		}
		/// @brief Extract a single character from the stream and put the read position to the next character
		/// @return extracted character or EOF
		SEQ_ALWAYS_INLINE auto getc() noexcept -> int
		{
			if (SEQ_LIKELY(d_pos != d_len)) { return d_buff[d_pos++];
			}
			return set_eof();
			//return d_pos != d_len ? d_buff[d_pos++] : set_eof();
		}
		/// @brief Read several characters at once
		/// @return The number of read characters
		auto read(char* dst, streamsize size) noexcept -> size_t
		{
			streamsize rem = d_len - d_pos;
			streamsize to_read = std::min(size, rem);
			memcpy(dst, d_buff + d_pos, to_read);
			if (size > rem) { set_eof();
			}
			d_pos += to_read;
			return to_read;
		}
	};


	/// @brief Input stream working on a std::istream
	///
	/// std_input_stream is a #basic_input_stream used to extract numbers, words and lines from a std::istream object.
	/// Its goal is to be able to use the optimized reading function (seq::from_stream to extract numerical values and words,
	/// seq::read_line_from_stream to extract full lines) on a std::istream object.
	/// 
	/// std_input_stream uses internally a buffer of size 'buff_size' to read from the underlying std::istream object by chunks.
	/// Even if the std::istream is internally bufferized, it makes the reading of single characters several times faster.
	/// 
	template<unsigned buff_size = 32>
	class std_input_stream : public basic_input_stream< std_input_stream<buff_size> >
	{
	public:
		using base_type = basic_input_stream< std_input_stream<buff_size> >;
		using State = StreamState;
		using streamsize = std::uint64_t;
	private:
		std::istream* d_file;
		size_t d_pos;
		char d_buff[buff_size];
		char* d_buff_pos;
		char* d_buff_end;
		State d_state;

		auto set_eof() noexcept -> char {
			set_state(EndOfFile);
			return EOF;
		}

		auto fillbuff() -> int
		{
			d_file->read(d_buff, buff_size);
			size_t read = d_file->gcount();
			d_file->clear();

			d_buff_pos = d_buff;
			d_buff_end = d_buff + read;
			if (SEQ_LIKELY(read > 0)) {
				++d_pos;
				return *d_buff_pos++;
			}
			return set_eof();
		}

	public:
		/// @brief Default ctor 
		std_input_stream()
			:d_file(NULL), d_pos(0), d_buff_pos(NULL), d_buff_end(NULL), d_state(Ok) {}
		/// @brief Construct from a std::istream object
		explicit std_input_stream(std::istream& iss)
			:d_file(NULL), d_pos(0), d_buff_pos(NULL), d_buff_end(NULL), d_state(Ok) {
			open(iss);
		}
		/// @brief Destructor, close the stream and update the underlying std::istream get position
		~std_input_stream() {
			close();
		}
		/// @brief Set the stream state
		void set_state(State st) noexcept
		{
			d_state = st;
		}
		/// @brief Returns the stream state
		auto state() const noexcept -> State
		{
			return d_state;
		}
		/// @brief Close the stream and update the underlying std::istream get position
		void close() noexcept {
			if (d_file)
				d_file->seekg(d_pos);
			d_file = NULL;
			d_pos = 0;
			d_buff_pos = d_buff_end = NULL;
			d_state = Ok;
		}
		/// @brief Open based on a std::istream object
		auto open(std::istream& iss) -> bool
		{
			close();
			if (iss) {
				d_file = &iss;
				d_pos = iss.tellg();
				return true;
			}
			return false;
		}
		/// @brief Returns true if the stream is open (the underlying std::istream object is valid)
		auto is_open() const noexcept -> bool
		{
			return d_file != NULL;
		}
		/// @brief Returns the stream get position
		auto tell() const noexcept -> size_t
		{
			return d_pos;
		}
		/// @brief Go back to previous character if possible
		void back() noexcept
		{
			if (SEQ_LIKELY(d_buff_pos && d_buff_pos != d_buff)) {
				--d_buff_pos;
				--d_pos;
			}
			else
				seek(d_pos - 1);
		}
		/// @brief Seek to given position and returns the new get position
		SEQ_NOINLINE(auto) seek(size_t pos) noexcept -> size_t
		{
			if (pos < d_pos) {
				int start = d_buff_pos ? static_cast<int>(d_buff_pos - d_buff) : 0;
				if (pos + start >= d_pos) {
					d_buff_pos -= d_pos - pos;
					d_pos = pos;
				}
				else {
					d_buff_pos = d_buff_end = NULL;
					if (!d_file->seekg(pos, std::ios::beg))
						return d_pos = d_file->tellg();
					d_pos = pos;
				}
			}
			else {
				int end = d_buff_end ? static_cast<int>(d_buff_end - d_buff_pos) : 0;
				if (d_pos + end > pos) {
					d_buff_pos += pos - d_pos;
					d_pos = pos;
				}
				else {
					d_buff_pos = d_buff_end = NULL;
					if (!d_file->seekg(pos, std::ios::beg))
						return d_pos = d_file->tellg();
					d_pos = pos;
				}
			}
			return d_pos;
		}
		/// @brief Returns the next character from the stream and update the get position 
		SEQ_ALWAYS_INLINE auto getc() noexcept -> int
		{
			// We need this one to be very fast
			if (SEQ_UNLIKELY(d_buff_pos == d_buff_end))
				return fillbuff();
			++d_pos;
			return *d_buff_pos++;
		}
		/// @brief Read several bytes from the stream, and return the number of read bytes.
		auto read(char* dst, size_t size) noexcept -> size_t
		{
			size_t rem = d_buff_end - d_buff_pos;
			size_t from_buffer = std::min(size, rem);
			size_t read_vals = from_buffer;
			if (from_buffer) {
				memcpy(dst, d_buff_pos, from_buffer);
				d_buff_pos += from_buffer;
				dst += from_buffer;
			}
			if (size > from_buffer) {
				size -= from_buffer;
				d_file->read(dst, size);
				read_vals += d_file->gcount();
				if (d_file->eof()) set_eof();
				d_file->clear();
			}
			d_pos += read_vals;
			return read_vals;
		}
	};


	/// @brief Input stream working on a FILE pointer
	///
	/// file_input_stream is a seq::basic_input_stream used to extract numbers, words and lines from a FILE pointer or directly a filename.
	/// Its goal is to be able to use the optimized reading function (seq::from_stream to extract numerical values and words,
	/// seq::read_line_from_stream to extract full lines) on a text file.
	/// 
	/// file_input_stream uses internally a buffer of size 'buff_size' to read from the underlying FILE object by chunks.
	/// 
	template<unsigned buff_size = 32>
	class file_input_stream : public basic_input_stream< file_input_stream<buff_size> >
	{
	public:
		using base_type = basic_input_stream< file_input_stream<buff_size> >;
		using State = StreamState;
		using streamsize = std::uint64_t;

	private:
		FILE* d_file;
		streamsize d_pos;
		char d_buff[buff_size];
		char* d_buff_pos;
		char* d_buff_end;
		State d_state;
		int d_flag;

		auto set_eof() noexcept -> char {
			set_state(EndOfFile);
			return EOF;
		}

		auto fillbuff() -> int
		{
			streamsize read = fread(d_buff, 1, buff_size, d_file);
			d_buff_pos = d_buff;
			d_buff_end = d_buff + read;
			if (SEQ_LIKELY(read)) {
				++d_pos;
				return *d_buff_pos++;
			}
			return set_eof();
		}

	public:
		/// @brief Default ctor
		file_input_stream()
			:d_file(NULL), d_pos(0), d_buff_pos(NULL), d_buff_end(NULL), d_state(Ok), d_flag(0) {}
		/// @brief Construct from a filename
		explicit file_input_stream(const char* filename, const char* format)
			:d_file(NULL), d_pos(0), d_buff_pos(NULL), d_buff_end(NULL), d_state(Ok), d_flag(0) {
			open(filename, format);
		}
		/// @brief Construct from a FILE object. 
		/// If 'own' is true, closing the stream will also close the FILE object.
		explicit file_input_stream(FILE* file, bool own)
			:d_file(file), d_pos(0), d_buff_pos(NULL), d_buff_end(NULL), d_state(Ok), d_flag(0) {
			open(file, own);
		}
		/// @brief Destructor. Close the stream, and close/update the FILE object based on the own strategy.
		~file_input_stream() {
			close();
		}
		/// @brief Set the stream state
		void set_state(State st) noexcept
		{
			d_state = st;
		}
		/// @brief Returns the stream state
		auto state() const noexcept -> State
		{
			return d_state;
		}
		/// @brief Close the stream. If the stream owns the FILE object, this function close it with fclose(). Otherwise, the FILE object
		/// get position is updated.
		void close() noexcept {
			if (d_file) {
				if (d_flag)
					fclose(d_file);
				else
					fseek(d_file, d_pos, SEEK_SET);
			}
			d_file = NULL;
			d_pos = d_flag = 0;
			d_buff_pos = d_buff_end = NULL;
			d_state = Ok;
		}
		/// @brief Open given file
		/// @return true on success, false otherwise.
		auto open(const char* filename, const char* format) -> bool
		{
			close();
			d_file = fopen(filename, format);
			if (d_file) {
				d_flag = 1;
				return true;
			}
			return false;
		}
		/// @brief Open the stream based on a FILE object
		/// If 'own' is true, the FILE object will be closed in the stream destructor or with the close() member. 
		auto open(FILE* file, bool own) -> bool
		{
			close();
			d_file = file;
			if (d_file) {
				d_flag = own;
				d_pos = ftell(d_file);
				return true;
			}
			return false;
		}
		/// @brief Returns true if the stream is open (underlying FILE object is valid)
		auto is_open() const noexcept -> bool
		{
			return d_file != NULL;
		}
		/// @brief Returns the current get position
		auto tell() const noexcept -> streamsize
		{
			return d_pos;
		}
		/// @brief Go back to previous character (if possible)
		void back() noexcept
		{
			if (SEQ_LIKELY(d_buff_pos && d_buff_pos != d_buff)) {
				--d_buff_pos;
				--d_pos;
			}
			else
				seek(d_pos - 1);
		}
		/// @brief Seek to given location, and return the new get position
		SEQ_NOINLINE(auto) seek(streamsize pos) noexcept -> streamsize
		{
			if (pos < d_pos) {
				int start = d_buff_pos ? static_cast<int>(d_buff_pos - d_buff) : 0;
				if (pos + start >= d_pos) {
					d_buff_pos -= d_pos - pos;
					d_pos = pos;
				}
				else {

					d_buff_pos = d_buff_end = NULL;
					if (fseek(d_file, pos, SEEK_SET) != 0)
						return d_pos = ftell(d_file);
					d_pos = pos;
				}
			}
			else {
				int end = d_buff_end ? static_cast<int>(d_buff_end - d_buff_pos) : 0;
				if (d_pos + end > pos) {
					d_buff_pos += pos - d_pos;
					d_pos = pos;
				}
				else {
					d_buff_pos = d_buff_end = NULL;
					if (fseek(d_file, pos, SEEK_SET) != 0)
						return d_pos = ftell(d_file);
					d_pos = pos;
				}
			}
			return d_pos;
		}
		/// @brief Returns the next character from the stream and update the get position 
		SEQ_ALWAYS_INLINE auto getc() noexcept -> int
		{
			// We need this one to be very fast
			if (SEQ_UNLIKELY(d_buff_pos == d_buff_end))
				return fillbuff();
			++d_pos;
			return *d_buff_pos++;
		}
		/// @brief Read several bytes from the stream, and return the number of read bytes.
		auto read(char* dst, streamsize size) noexcept -> streamsize
		{
			streamsize rem = d_buff_end - d_buff_pos;
			streamsize from_buffer = std::min(size, rem);
			streamsize read_vals = from_buffer;
			if (from_buffer) {
				memcpy(dst, d_buff_pos, from_buffer);
				d_buff_pos += from_buffer;
				dst += from_buffer;
			}
			if (size > from_buffer) {
				size -= from_buffer;
				streamsize _fread = fread(dst, 1, size, d_file);
				read_vals += _fread;
				if (_fread != size) set_eof();
			}
			d_pos += read_vals;
			return read_vals;
		}
	};





	


	/// @brief Read an integral value from a seq::basic_input_stream object.
	/// @param str stream object to read from
	/// @param value output read value 
	/// @param base integer base (default to 10) 
	/// @return reference to the stream object
	/// 
	/// Analyzes the stream for an integral pattern. If no characters match the pattern or if the value obtained 
	/// by parsing the matched characters is not representable in the type of value, value is set to 0, otherwise 
	/// the characters matching the pattern are interpreted as a text representation of an arithmetic value, which is stored in value.
	/// 
	/// For hexadecimal numbers, the (potential) '0x' prefix will be automatically detected and handled. Leading spaces are consumed.
	/// If the pattern is a valid integral text representation too large to be stored in a value, the full pattern will still be consumed.
	/// Reading a negative text representation in an unsigned variable is not valid.
	/// 
	/// If the parsing fails (no integral pattern was detected), the stream status will be set to BadInputFormat or EndOfFile
	/// depending on the situation.
	/// 
	/// seq::from_chars internally uses seq::from_stream with a seq::buffer_input_stream object.
	/// 
	template<class Stream, class T>
	inline auto from_stream(Stream& str, T& value, int base = 10) -> Stream&
	{
		value = detail::read_integral<T>(str, base);
		return str;
	}
	
	/// @brief Read a floating point value from a #basic_input_stream object.
	/// @param str stream object to read from
	/// @param value output read value 
	/// @param dot dot character, usually '.' for 'C' locale.
	/// @return reference to the stream object
	/// 
	/// Analyzes the stream for a floating point pattern. If no characters match the pattern, value is set to 0, otherwise 
	/// the characters matching the pattern are interpreted as a text representation of an floating point value, which is stored in value.
	/// 
	/// Nan and infinit values (upper or lower case) are handled by this function. 
	/// 
	/// If the parsing fails (no floating point pattern was detected), the stream status will be set to #BadInputFormat or #EndOfFile
	/// depending on the situation, and stream get position will be reverted to its original state.
	/// 
	/// This function is simillar to C++17 std::from_chars with the following differences:
	///		- Leading spaces are consumed.
	///		- If the pattern is a valid floating point text representation too large or
	///		too small to be stored in given output value, value will be set to (+-)inf or (+-)0, 
	///		and the full pattern will be consumed. Therefore, std::errc::result_out_of_range is
	///		never returned.
	///		- Leading '+' sign is considered valid.
	///		- This function is not an exact parser. In some cases it relies on unprecise floating point arithmetic wich might produce different roundings than strtod() function.
	///		Note that the result is almost always equal to the result of strtod(), and potential differences are located in the last digits. Use this function when the speed factor 
	///		is more important than 100% perfect exactitude.
	/// 
	/// This function is usefull when you need to read a huge amount of floating point values of a std::istream object. Internal benchmarks show that using from_stream()
	/// is around 20 faster (or more) than using std::istream::operator>>().
	/// 
	/// seq::from_chars internally uses seq::from_stream with a seq::buffer_input_stream object.
	/// 
	template<class Stream>
	inline auto from_stream(Stream& str, float& value, chars_format fmt = seq::general, char dot = '.') -> Stream&
	{
		value = detail::read_double<float>(str, fmt, dot);
		return str;
	}

	/// @brief Read a floating point value from a #basic_input_stream object.
	/// @param str stream object to read from
	/// @param value output read value 
	/// @param dot dot character, usually '.' for 'C' locale.
	/// @return reference to the stream object
	/// 
	/// Analyzes the stream for a floating point pattern. If no characters match the pattern, value is set to 0, otherwise 
	/// the characters matching the pattern are interpreted as a text representation of an floating point value, which is stored in value.
	/// 
	/// Nan and infinit values (upper or lower case) are handled by this function. 
	/// 
	/// If the parsing fails (no floating point pattern was detected), the stream status will be set to #BadInputFormat or #EndOfFile
	/// depending on the situation, and stream get position will be reverted to its original state.
	/// 
	/// This function is simillar to C++17 std::from_chars with the following differences:
	///		- Leading spaces are consumed.
	///		- If the pattern is a valid floating point text representation too large or
	///		too small to be stored in given output value, value will be set to (+-)inf or (+-)0, 
	///		and the full pattern will be consumed. Therefore, std::errc::result_out_of_range is
	///		never returned.
	///		- Leading '+' sign is considered valid.
	///		- This function is not an exact parser. In some cases it relies on unprecise floating point arithmetic wich might produce different roundings than strtod() function.
	///		Note that the result is almost always equal to the result of strtod(), and potential differences are located in the last digits. Use this function when the speed factor 
	///		is more important than 100% perfect exactitude.
	/// 
	/// This function is usefull when you need to read a huge amount of floating point values of a std::istream object. Internal benchmarks show that using from_stream()
	/// is around 20 faster (or more) than using std::istream::operator>>().
	/// 
	/// seq::from_chars internally uses seq::from_stream with a seq::buffer_input_stream object.
	/// 
	template<class Stream>
	inline auto from_stream(Stream& str, double& value, chars_format fmt = seq::general, char dot = '.') -> Stream&
	{
		value = detail::read_double<double>(str, fmt, dot);
		return str;
	}

	/// @brief Read a floating point value from a #basic_input_stream object.
	/// @param str stream object to read from
	/// @param value output read value 
	/// @param dot dot character, usually '.' for 'C' locale.
	/// @return reference to the stream object
	/// 
	/// Analyzes the stream for a floating point pattern. If no characters match the pattern, value is set to 0, otherwise 
	/// the characters matching the pattern are interpreted as a text representation of an floating point value, which is stored in value.
	/// 
	/// Nan and infinit values (upper or lower case) are handled by this function. 
	/// 
	/// If the parsing fails (no floating point pattern was detected), the stream status will be set to #BadInputFormat or #EndOfFile
	/// depending on the situation, and stream get position will be reverted to its original state.
	/// 
	/// This function is simillar to C++17 std::from_chars with the following differences:
	///		- Leading spaces are consumed.
	///		- If the pattern is a valid floating point text representation too large or
	///		too small to be stored in given output value, value will be set to (+-)inf or (+-)0, 
	///		and the full pattern will be consumed. Therefore, std::errc::result_out_of_range is
	///		never returned.
	///		- Leading '+' sign is considered valid.
	///		- This function is not an exact parser. In some cases it relies on unprecise floating point arithmetic wich might produce different roundings than strtod() function.
	///		Note that the result is almost always equal to the result of strtod(), and potential differences are located in the last digits. Use this function when the speed factor 
	///		is more important than 100% perfect exactitude.
	/// 
	/// This function is usefull when you need to read a huge amount of floating point values of a std::istream object. Internal benchmarks show that using from_stream()
	/// is around 20 faster (or more) than using std::istream::operator>>().
	/// 
	/// seq::from_chars internally uses seq::from_stream with a seq::buffer_input_stream object.
	/// 
	template<class Stream>
	inline auto from_stream(Stream& str, long double& value, chars_format fmt = seq::general, char dot = '.') -> Stream&
	{
		value = detail::read_double<long double>(str, fmt, dot);
		return str;
	}

	/// @brief Read a word from a #basic_input_stream object.
	/// @param str stream object
	/// @param value output string object
	/// @return reference to the stream object
	/// 
	/// Read a full word from a stream object. In a text files, words are delimited by white-space characters (' ', '\t', '\n', '\v', '\f', '\r', EOF).
	/// If no string was read because the get position was after the last stream character, the value string is cleared and stream state is set
	/// to #EndOfFile.
	/// 
	template<class Stream, class Traits, class Al>
	inline auto from_stream(Stream& str, std::basic_string<char,Traits,Al>& value) -> Stream&
	{
		value = detail::read_string<std::basic_string<char, Traits, Al>>(str);
		return str;
	}

	/// @brief Read a word from a #basic_input_stream object.
	/// @param str stream object
	/// @param value output string object
	/// @return reference to the stream object
	/// 
	/// Read a full word from a stream object. In a text files, words are delimited by white-space characters (' ', '\t', '\n', '\v', '\f', '\r', EOF).
	/// If no string was read because the get position was after the last stream character, the value string is cleared and stream state is set
	/// to #EndOfFile.
	/// 
	template<class Stream, class Traits, size_t Ss, class Al>
	inline auto from_stream(Stream& str, tiny_string<char,Traits,Al,Ss>& value) -> Stream&
	{
		value = detail::read_string<tiny_string<char, Traits, Al, Ss>>(str);
		return str;
	}

	/// @brief Read a line from a #basic_input_stream object.
	/// @param str stream object
	/// @param value output string object
	/// @return reference to the stream object
	/// 
	/// Read a full line from a stream object. In a text files, lines are delimited by characters '\n', '\r' or EOF.
	/// If no string was read because the get position was after the last stream character, the value string is cleared and stream state is set
	/// to #EndOfFile.
	/// 
	template<class Stream, class Traits, class Al>
	inline auto read_line_from_stream(Stream& str, std::basic_string<char,Traits,Al>& value) -> Stream&
	{
		value = detail::read_line<std::basic_string<char, Traits, Al>>(str);
		return str;
	}

	/// @brief Read a line from a #basic_input_stream object.
	/// @param str stream object
	/// @param value output string object
	/// @return reference to the stream object
	/// 
	/// Read a full line from a stream object. In a text files, lines are delimited by characters '\n', '\r' or EOF.
	/// If no string was read because the get position was after the last stream character, the value string is cleared and stream state is set
	/// to #EndOfFile.
	/// 
	template<class Stream, class Traits, size_t Ss, class Al>
	inline auto read_line_from_stream(Stream& str, tiny_string<char,Traits, Al,Ss>& value) -> Stream&
	{
		value = detail::read_line<tiny_string<char, Traits, Al, Ss> >(str);
		return str;
	}

} //end namespace seq

#ifndef SEQ_HEADER_ONLY

namespace seq
{
	/// @brief Read an integral value from the sequence of characters [first,last).
	/// @param first first character of the sequence
	/// @param last past-the-end character
	/// @param value output read value 
	/// @param base integer base (default to 10) 
	/// @return from_chars_result object storing a past-the-end pointer (on success) and an error code.
	/// 
	/// Analyzes the character sequence [first,last) for an integral pattern. If no characters match the pattern, value is set to 0, otherwise 
	/// the characters matching the pattern are interpreted as a text representation of an arithmetic value, which is stored in value.
	/// 
	/// This function is similar to std::from_chars except for the following situations:
	/// 
	///		- For hexadecimal numbers, the (potential) '0x' prefix will be automatically detected and handled.
	///		- Leading spaces are consumed.
	///		- Leading '+' sign is valid.
	///		- If the pattern is a valid integral text representation too large to be stored in a value, the full pattern will still be consumed,
	///		the function will return a 'success' from_chars_result, and value will silently overflow.
	///		- Reading a negative text representation in an unsigned variable is NOT valid.
	/// 
	/// On success, returns a value of type seq::from_chars_result such that ptr points at the first character not matching the pattern, 
	/// or has the value equal to last if all characters match and ec is value-initialized.
	///
	/// If there is no pattern match, returns a value of type from_chars_result such that ptr equals first and ec equals std::errc::invalid_argument.
	/// value is set to 0. 
	/// 
	/// 
	SEQ_EXPORT auto from_chars(const char* first, const char* last, char& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, signed char& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, unsigned char& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, short& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, unsigned short& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, int& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, unsigned int& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, long& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, unsigned long& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, long long& value, int base = 10)->from_chars_result;
	SEQ_EXPORT auto from_chars(const char* first, const char* last, unsigned long long& value, int base = 10)->from_chars_result;

	/// @brief Read a floating point value from the sequence of characters [first,last).
	/// @param first first character of the sequence
	/// @param last past-the-end character
	/// @param value output read value 
	/// @param dot decimal point character
	/// @return from_chars_result object storing a past-the-end pointer (on success) and an error code.
	/// 
	/// Analyzes the character sequence [first,last) for a floating point pattern. If no characters match the pattern, value is set to 0, otherwise 
	/// the characters matching the pattern are interpreted as a text representation of an arithmetic value, which is stored in value.
	/// 
	/// Nan and infinit values (upper or lower case) are handled by this function.
	/// 
	/// This function is simillar to C++17 std::from_chars with the following differences:
	///		- Leading spaces are consumed.
	///		- If the pattern is a valid floating point text representation too large or too small to be stored in given output value, value will be set to (+-)inf or (+-)0, 
	///		and the full pattern will be consumed. Therefore, std::errc::result_out_of_range is never returned.
	///		- Leading '+' sign is considered valid.
	///		- This function is not an exact parser. In some cases it relies on unprecise floating point arithmetic wich might produce different roundings than strtod() function.
	///		Note that the result is almost always equal to the result of strtod, and potential differences are located in the last digits. Use this function when the speed factor is more
	///		important than 100% perfect exactitude.
	///  
	/// On success, returns a value of type seq::from_chars_result such that ptr points at the first character not matching the pattern, 
	/// or has the value equal to last if all characters match and ec is value-initialized.
	///
	/// If there is no pattern match, returns a value of type from_chars_result such that ptr equals first and ec equals std::errc::invalid_argument.
	/// value is set to 0. 
	/// 
	/// 
	SEQ_EXPORT  auto from_chars(const char* first, const char* last, float& value, chars_format fmt = seq::general, char dot = '.')->from_chars_result;

	/// @brief Read a floating point value from the sequence of characters [first,last).
	/// @param first first character of the sequence
	/// @param last past-the-end character
	/// @param value output read value 
	/// @param dot decimal point character
	/// @return from_chars_result object storing a past-the-end pointer (on success) and an error code.
	/// 
	/// Analyzes the character sequence [first,last) for a floating point pattern. If no characters match the pattern, value is set to 0, otherwise 
	/// the characters matching the pattern are interpreted as a text representation of an arithmetic value, which is stored in value.
	/// 
	/// Nan and infinit values (upper or lower case) are handled by this function.
	/// 
	/// This function is simillar to C++17 std::from_chars with the following differences:
	///		- Leading spaces are consumed.
	///		- If the pattern is a valid floating point text representation too large or too small to be stored in given output value, value will be set to (+-)inf or (+-)0, 
	///		and the full pattern will be consumed. Therefore, std::errc::result_out_of_range is never returned.
	///		- Leading '+' sign is considered valid.
	///		- This function is not an exact parser. In some cases it relies on unprecise floating point arithmetic wich might produce different roundings than strtod() function.
	///		Note that the result is almost always equal to the result of strtod, and potential differences are located in the last digits. Use this function when the speed factor is more
	///		important than 100% perfect exactitude.
	///  
	/// On success, returns a value of type seq::from_chars_result such that ptr points at the first character not matching the pattern, 
	/// or has the value equal to last if all characters match and ec is value-initialized.
	///
	/// If there is no pattern match, returns a value of type from_chars_result such that ptr equals first and ec equals std::errc::invalid_argument.
	/// value is set to 0. 
	/// 
	/// 
	SEQ_EXPORT auto from_chars(const char* first, const char* last, double& value, chars_format fmt = seq::general, char dot = '.')->from_chars_result;

	/// @brief Read a floating point value from the sequence of characters [first,last).
	/// @param first first character of the sequence
	/// @param last past-the-end character
	/// @param value output read value 
	/// @param dot decimal point character
	/// @return from_chars_result object storing a past-the-end pointer (on success) and an error code.
	/// 
	/// Analyzes the character sequence [first,last) for a floating point pattern. If no characters match the pattern, value is set to 0, otherwise 
	/// the characters matching the pattern are interpreted as a text representation of an arithmetic value, which is stored in value.
	/// 
	/// Nan and infinit values (upper or lower case) are handled by this function.
	/// 
	/// This function is simillar to C++17 std::from_chars with the following differences:
	///		- Leading spaces are consumed.
	///		- If the pattern is a valid floating point text representation too large or too small to be stored in given output value, value will be set to (+-)inf or (+-)0, 
	///		and the full pattern will be consumed. Therefore, std::errc::result_out_of_range is never returned.
	///		- Leading '+' sign is considered valid.
	///		- This function is not an exact parser. In some cases it relies on unprecise floating point arithmetic wich might produce different roundings than strtod() function.
	///		Note that the result is almost always equal to the result of strtod, and potential differences are located in the last digits. Use this function when the speed factor is more
	///		important than 100% perfect exactitude.
	///  
	/// On success, returns a value of type seq::from_chars_result such that ptr points at the first character not matching the pattern, 
	/// or has the value equal to last if all characters match and ec is value-initialized.
	///
	/// If there is no pattern match, returns a value of type from_chars_result such that ptr equals first and ec equals std::errc::invalid_argument.
	/// value is set to 0. 
	/// 
	/// 
	SEQ_EXPORT  auto from_chars(const char* first, const char* last, long double& value, chars_format fmt = seq::general, char dot = '.')->from_chars_result;








	// integer to chars

	/// @brief Converts value into a character string by successively filling the range [first, last), where [first, last) is required to be a valid range.
	/// @param first first output character
	/// @param last past-the-end character
	/// @param value input value to convert to string
	/// @param base integer base (default to 10)
	/// @param fmt optional format
	/// @return to_chars_result object
	/// 
	/// value is converted to a string of digits in the given base (with no redundant leading zeroes by default). 
	/// Digits in the range 10..35 (inclusive) are represented as lowercase characters a..z by default.
	/// If value is less than zero, the representation starts with a minus sign. 
	/// The library provides overloads for all signed and unsigned integer types and for the type char as the type of the parameter value.
	/// This function produces a similar output as std::to_chars.
	/// 
	/// Output formatting can be controlled through a #integral_chars_format object:
	///		- Digits in the range 10..35 (inclusive) can be represented as uppercase characters A..Z with integral_chars_format::upper_case
	///		- For base 16, a trailing '0x' can be added with integral_chars_format::hex_prefix
	///		- A minimum width (in number of digits) can be specified to add leading zeros with integral_chars_format::integral_min_width
	/// 
	/// On success, returns a value of type seq::to_chars_result such that ec equals value-initialized std::errc and ptr is the one-past-the-end pointer 
	/// of the characters written. Note that the string is not NULL-terminated.
	/// 
	/// On error, returns a value of type seq::to_chars_result holding std::errc::value_too_large in ec, 
	/// a copy of the value last in ptr, and leaves the contents of the range[first, last) in unspecified state.
	/// 
	SEQ_EXPORT auto to_chars(char* first, char* last, char value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, signed char value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, unsigned char value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, short value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, unsigned short value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, int value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, unsigned int value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, long value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, unsigned long value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, long long value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, unsigned long long value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result;




	// floating-point to chars

	/// @brief Converts value into a character string by successively filling the range [first, last), where [first, last) is required to be a valid range.
	/// @param first first output character
	/// @param last past-the-end character
	/// @param value input value to convert to string
	/// @param fmt floating-point formatting to use, a bitmask of type seq::chars_format
	/// @param precision maximum digits after the radix point
	/// @param fmt optional format
	/// @param dot optional decimal point sequence, default to "."
	/// @param exp optional exponential character, default to 'e'
	/// @return to_chars_result object
	/// 
	/// value is converted to a string as if by std::printf in the default ("C") locale (with some differences, see below). 
	/// This function DOES NOT provide exact formatting of the input unlike printf familly of functions.
	/// 
	/// The conversion specifier is 'f' or 'e' (resolving in favor of f in case of a tie), chosen according to the requirement 
	/// for a shortest representation: the string representation consists of the smallest number of characters such that there 
	/// is at least one digit before the radix point (if present). 
	/// 
	/// The conversion specified for the as-if printf is 'f' if fmt is seq::chars_format::fixed, 
	/// 'e' if fmt is seq::chars_format::scientific, and 'g' if fmt is chars_format::general (default).
	/// 
	/// If the precision is specified by the parameter 'precision', this function will output up-to (but to necessarily exactly) 'precision'
	/// digits after the radix point. The default precision is 6. This function will always try to output the smallest possible character sequence.

	/// If 'dot' is specified, it will replace the '.' decimal point used by the "C" locale.
	/// 
	/// If 'exp' is specified, it will replace the 'e' exponent character used by the "C" locale.
	/// 
	/// NaN and infinit values are handled by this function.
	/// 
	/// On success, returns a value of type seq::to_chars_result such that ec equals value-initialized std::errc and ptr is the one-past-the-end pointer 
	/// of the characters written. Note that the string is not NULL-terminated.
	/// 
	/// On error, returns a value of type seq::to_chars_result holding std::errc::value_too_large in ec, 
	/// a copy of the value last in ptr, and leaves the contents of the range[first, last) in unspecified state.
	/// 
	/// 
	/// There are currently a lot of different algorithms to provide fast and exact convertion of floating point values to strings: ryu, grisu-exact, dragonbox...
	/// This function tries to provide a faster and lighter alternative when exact precision is not a requirement (which is my case). Internal benchmarks show
	/// that convertiing double values to string using this function is around 2 times faster than with ryu library for scientific or general formatting, and 3 times faster with
	/// fixed formatting.
	/// 
	/// When converting double values, obtained strings are similar to the result of printf in 100% of the cases when the required precision is below 12. After that, the ratio decreases
	/// to 87% of exactitude for a precision of 17. Converting a very high (or very small) value with the 'f' specifier will usually produce slightly different output, especially in the
	/// "garbade" digits.
	/// 
	/// Use this function when you need very fast formatting of a huge amount of floating point values without exact formatting requirement.
	/// 
	/// 
	SEQ_EXPORT auto to_chars(char* first, char* last, float value)->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, double value)->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, long double value)->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, float value, chars_format fmt)->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, double value, chars_format fmt)->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, long double value, chars_format fmt)->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, float value, chars_format fmt, int precision, char dot = '.', char exp = 'e', bool upper = false)->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, double value, chars_format fmt, int precision, char dot = '.', char exp = 'e', bool upper = false)->to_chars_result;
	SEQ_EXPORT auto to_chars(char* first, char* last, long double value, chars_format fmt, int precision, char dot = '.', char exp = 'e', bool upper = false)->to_chars_result;

}// end namespace seq

#else
#include "internal/charconv.cpp"
#endif


namespace seq
{
	auto to_chars(char* first, char* last, bool value, int base = 10, const integral_chars_format& fmt = integral_chars_format())->to_chars_result = delete;
}// end namespace seq


/** @}*/
//end charconv

#endif
