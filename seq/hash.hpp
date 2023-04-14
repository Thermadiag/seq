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

#ifndef SEQ_HASH_HPP
#define SEQ_HASH_HPP



/** @file */


/**\defgroup hash Hash: small collection of hash utilities

The hash module provides several hash-related functions:
	-	seq::hash_combine: combine 2 hash values
	-	seq::hash_bytes_murmur64: murmurhash2 algorithm
	-	seq::hash_bytes_fnv1a: fnv1a hash algorithm working on chunks of 4 to 8 bytes
	-	seq::hash_bytes_fnv1a_slow: standard fnv1a hash algorithm

Note that the specialization of std::hash for seq::tiny_string uses murmurhash2 algorithm.

*/


/** \addtogroup hash
 *  @{
 */


#include "bits.hpp"

namespace seq
{
	/// @brief Combine 2 hash values. Uses either murmurhash2 for 64 bits platform or the boost version for 32 bits platform.
	/// @param h1 first hash value
	/// @param h2 second hash value
	/// @return combination of both hash value
	inline auto hash_combine(size_t h1, size_t h2)noexcept -> size_t
	{
#ifdef SEQ_ARCH_64
		// murmurhash2 like
		h2 *= 0xff51afd7ed558ccd;
		h2 ^= h2 >> 33ULL;
		h2 ^= h1;
		h2 ^= h2 >> 33ULL;
		h2 *= 0xc4ceb9fe1a85ec53;
		return h2;
#else
		// boost way
		return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
#endif

	}

}


#ifndef SEQ_HEADER_ONLY
namespace seq
{
	SEQ_EXPORT auto hash_bytes_murmur64(const std::uint8_t* ptr, size_t len) noexcept -> size_t;
	SEQ_EXPORT auto hash_bytes_fnv1a(const unsigned char* ptr, size_t size)noexcept -> size_t;
	SEQ_EXPORT auto hash_bytes_fnv1a(const unsigned char* ptr, size_t size)noexcept -> size_t;
}
#else
#include "internal/hash.cpp"
#endif




/** @}*/
//end hash

#endif
