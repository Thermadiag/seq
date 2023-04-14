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


#include "../charconv.hpp"

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
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, char& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<char>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, signed char& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<signed char>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, unsigned char& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<unsigned char>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, short& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<short>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, unsigned short& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<unsigned short>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, int& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<int>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, unsigned int& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<unsigned int>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, long& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<long>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, unsigned long& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<unsigned long>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, long long& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<long long>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, unsigned long long& value, int base SEQ_HEADER_ONLY_ARG(= 10)) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_integral<unsigned long long>(str, static_cast<unsigned>(base));
		return { str ? str.tell() : first, str.error() };
	}


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
	SEQ_HEADER_ONLY_EXPORT_FUNCTION  auto from_chars(const char* first, const char* last, float& value, chars_format fmt SEQ_HEADER_ONLY_ARG (= seq::general), char dot SEQ_HEADER_ONLY_ARG(='.')) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_double<float>(str, fmt, dot);
		return { str ? str.tell() : first, str.error() };
	}

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
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto from_chars(const char* first, const char* last, double& value, chars_format fmt SEQ_HEADER_ONLY_ARG (= seq::general), char dot SEQ_HEADER_ONLY_ARG (= '.')) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_double<double>(str, fmt, dot);
		return { str ? str.tell() : first, str.error() };
	}

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
	SEQ_HEADER_ONLY_EXPORT_FUNCTION  auto from_chars(const char* first, const char* last, long double& value, chars_format fmt SEQ_HEADER_ONLY_ARG (= seq::general), char dot SEQ_HEADER_ONLY_ARG (= '.')) -> from_chars_result
	{
		detail::from_chars_stream str(first, last);
		value = detail::read_double<long double>(str, fmt, dot);
		return { str ? str.tell() : first, str.error() };
	}








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
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, char value, int base SEQ_HEADER_ONLY_ARG (= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, signed char value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, unsigned char value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, short value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, unsigned short value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, int value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, unsigned int value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, long value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, unsigned long value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, long long value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, unsigned long long value, int base SEQ_HEADER_ONLY_ARG(= 10), const integral_chars_format& fmt SEQ_HEADER_ONLY_ARG(= integral_chars_format())) -> to_chars_result
	{
		return detail::write_integral(detail::char_range(first, last), value, base, fmt);
	}
	


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
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, float value) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), static_cast<double>(value));
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, double value) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), value);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, long double value) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), value);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, float value, chars_format fmt) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), static_cast<double>(value), 6, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, double value, chars_format fmt) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), value, 6, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, long double value, chars_format fmt) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), value, 6, fmt);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, float value, chars_format fmt, int precision, char dot SEQ_HEADER_ONLY_ARG(= '.'), char exp  SEQ_HEADER_ONLY_ARG(= 'e'), bool upper SEQ_HEADER_ONLY_ARG(= false)) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), static_cast<double>(value), precision, detail::float_chars_format(fmt, dot, exp, static_cast<char>(upper)));
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, double value, chars_format fmt, int precision, char dot SEQ_HEADER_ONLY_ARG(= '.'), char exp SEQ_HEADER_ONLY_ARG(= 'e'), bool upper SEQ_HEADER_ONLY_ARG(= false)) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), value, precision, detail::float_chars_format(fmt, dot, exp, static_cast<char>(upper)));
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto to_chars(char* first, char* last, long double value, chars_format fmt, int precision, char dot SEQ_HEADER_ONLY_ARG(= '.'), char exp SEQ_HEADER_ONLY_ARG(= 'e'), bool upper SEQ_HEADER_ONLY_ARG(= false)) -> to_chars_result
	{
		return detail::write_double(detail::char_range(first, last), value, precision, detail::float_chars_format(fmt, dot, exp, static_cast<char>(upper)));
	}


}// end namespace seq
