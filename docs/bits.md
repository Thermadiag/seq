# Bits: collection of functions for low level bits manipulation.

The *bits* module provides several portable low-level functions for bits manipulation:
-	*seq::popcnt64*: population count on a 64 bits word
-	*seq::popcnt32*: population count on a 32 bits word
-	*seq::popcnt16*: population count on a 16 bits word
-	*seq::popcnt8*: population count on a 8 bits word
-	*seq::bit_scan_forward_32*: index of the lowest set bit in a 32 bits word
-	*seq::bit_scan_forward_64*: index of the lowest set bit in a 64 bits word
-	*seq::bit_scan_reverse_32*: index of the highest set bit in a 32 bits word
-	*seq::bit_scan_reverse_64*: index of the highest set bit in a 32 bits word
-	*seq::bit_scan_forward*: index of the lowest set bit in a size_t word
-	*seq::bit_scan_reverse*: index of the highest set bit in a size_t word
-	*seq::static_bit_scan_reverse*: index of the highest set bit at compile time
-	*seq::count_digits_base_10*: number of digits to represent an integer in base 10
-	*seq::nth_bit_set*: index of the nth set bit in a 64 bits word
-	*seq::bswap_16*: byte swap for 16 bits word
-	*seq::bswap_32*: byte swap for 32 bits word
-	*seq::bswap_64*: byte swap for 64 bits word

See functions documentation for more details.