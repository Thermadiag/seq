# Charconv: arithmetic value convertion from/to string

The charconv module provides low-level fast routines to convert numerical values from/to string.
This module was initially developped for very fast containers dump in files or strings, where C++17 is not available.

## Low level functions


The main functions of charconv module are `seq::to_chars` and `seq::from_chars` which provide a similar interface to C++17 functions `std::from_chars` and `std::to_chars`. 
They aim to provide a faster alternative to C++ streams for converting floating point and integer types from/to string. Note that they were developped to accomodate my needs, and might not be used in all circumstances.

`seq::from_chars()` is similar to `std::from_chars` with the following differences:
-	Leading spaces are consumed.
-	For integral types, a leading '0x' prefix is considered valid.
-	For floating point values: if the pattern is a valid floating point text representation too large or too small to be stored in given output value, value will be set to (+-)inf or (+-)0, 
	and the full pattern will be consumed. Therefore, std::errc::result_out_of_range is never returned.
-	Leading '+' sign is considered valid.
-	Custom 'dot' character can be passed as argument.
-	For floating point values: this function IS NOT AN EXACT PARSER. In some cases it relies on unprecise floating point arithmetic wich might produce different roundings than strtod() function.
	Note that the result is almost always equal to the result of strtod(), and potential differences are located in the last digits. Use this function when the speed factor is more important than 100% perfect exactitude.

`seq::to_chars` is similar to `std::to_chars` with the following differences
-	For integral types, additional options on the form of a `seq::integral_chars_format` object can be passed. They add the possibility to output a leading '0x' for hexadecimal
	formatting, a minimum width (with zeros padding), upper case outputs for hexadecimal formatting.
-	For floating point values, the 'dot' character can be specified.
-	For floating point values, this function is NOT AN EXACT FORMATTER.
	There are currently a lot of different algorithms to provide fast convertion of floating point values to strings with round-trip guarantees: ryu, grisu-exact, dragonbox... 
	This function tries to provide a faster and lighter alternative when perfect round-trip is not a requirement (which is my case).
	When converting double values, obtained strings are similar to the result of printf in 100% of the cases when the required precision is below 12. 
	After that, the ratio decreases to 86% of exactitude for a precision of 17. Converting a very high (or very small) value with the 'f' specifier will usually produce slightly different output, especially in the "garbage" digits.


## Working with C++ streams


To write numerical values to C++ `std::ostream` objects, see the [format](format.md) module.

To read numerical values from `std::istream` object, the charconv module provides the stream adapter `seq::std_input_stream`.
It was developped to read huge tables or matrices from ascii files or strings.

Basic usage:

```cpp

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

```

Internal benchmarks show that using seq::from_stream() is around 10 times faster (or more) than using *std::istream::operator>>()* when reading floating point values from a huge string.

In additional to `seq::std_input_stream`, *charconv* module provides the similar `seq::buffer_input_stream` and `seq::file_input_stream`.