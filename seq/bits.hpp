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

#ifndef SEQ_BITS_HPP
#define SEQ_BITS_HPP



/** @file */

/**\defgroup bits Bits: collection of functions for low level bits manipulation.

The bits module provides several portable low-level functions for bits manipulation:
	-	seq::popcnt64: population count on a 64 bits word
	-	seq::popcnt32: population count on a 32 bits word
	-	seq::popcnt16: population count on a 16 bits word
	-	seq::popcnt8: population count on a 8 bits word
	-	seq::bit_scan_forward_32: index of the lowest set bit in a 32 bits word
	-	seq::bit_scan_forward_64: index of the lowest set bit in a 64 bits word
	-	seq::bit_scan_reverse_32: index of the highest set bit in a 32 bits word
	-	seq::bit_scan_reverse_64: index of the highest set bit in a 32 bits word
	-	seq::bit_scan_forward: index of the lowest set bit in a size_t word
	-	seq::bit_scan_reverse: index of the highest set bit in a size_t word
	-	seq::static_bit_scan_reverse: index of the highest set bit at compile time
	-	seq::count_digits_base_10: number of digits to represent an integer in base 10
	-	seq::nth_bit_set: index of the nth set bit in a 64 bits word
	-	seq::byte_swap_16: byte swap for 16 bits word
	-	seq::byte_swap_32: byte swap for 32 bits word
	-	seq::byte_swap_64: byte swap for 64 bits word

See functions documentation for more details.
*/


/** \addtogroup bits
 *  @{
 */

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cassert>


#if defined(__APPLE__)
// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#endif

#if defined(__sun) || defined(sun)
#include <sys/byteorder.h>
#endif

#if defined(__FreeBSD__)
#include <sys/endian.h>
#endif

#if defined(__OpenBSD__)
#include <sys/types.h>
#endif

#if defined(__NetBSD__)
#include <sys/types.h>
#include <machine/bswap.h>
#endif

// Disable old style cast warning for gcc
#ifdef __GNUC__
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif


// Global grow factor for most containers
#ifndef SEQ_GROW_FACTOR
#define SEQ_GROW_FACTOR 1.6
#endif


// From rapsody library
//https://stackoverflow.com/questions/4239993/determining-endianness-at-compile-time

#define SEQ_BYTEORDER_LITTLE_ENDIAN 0 // Little endian machine.
#define SEQ_BYTEORDER_BIG_ENDIAN 1 // Big endian machine.

// Find byte order
#ifndef SEQ_BYTEORDER_ENDIAN
	// Detect with GCC 4.6's macro.
#   if defined(__BYTE_ORDER__)
#       if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#           define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_LITTLE_ENDIAN
#       elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#           define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_BIG_ENDIAN
#       else
#           error "Unknown machine byteorder endianness detected. User needs to define SEQ_BYTEORDER_ENDIAN."
#       endif
	// Detect with GLIBC's endian.h.
#   elif defined(__GLIBC__)
#       include <endian.h>
#       if (__BYTE_ORDER == __LITTLE_ENDIAN)
#           define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_LITTLE_ENDIAN
#       elif (__BYTE_ORDER == __BIG_ENDIAN)
#           define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_BIG_ENDIAN
#       else
#           error "Unknown machine byteorder endianness detected. User needs to define SEQ_BYTEORDER_ENDIAN."
#       endif
	// Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro.
#   elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#       define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_LITTLE_ENDIAN
#   elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#       define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_BIG_ENDIAN
	// Detect with architecture macros.
#   elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) || defined(__ppc__) || defined(__hpux) || defined(__hppa) || defined(_MIPSEB) || defined(_POWER) || defined(__s390__)
#       define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_BIG_ENDIAN
#   elif defined(__i386__) || defined(__alpha__) || defined(__ia64) || defined(__ia64__) || defined(_M_IX86) || defined(_M_IA64) || defined(_M_ALPHA) || defined(__amd64) || defined(__amd64__) || defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__bfin__)
#       define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_LITTLE_ENDIAN
#   elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
#       define SEQ_BYTEORDER_ENDIAN SEQ_BYTEORDER_LITTLE_ENDIAN
#   else
#       error "Unknown machine byteorder endianness detected. User needs to define SEQ_BYTEORDER_ENDIAN."
#   endif
#endif


// Find 32/64 bits
#if defined(__x86_64__) || defined(__ppc64__) || defined(_WIN64)
#define SEQ_ARCH_64
#else
#define SEQ_ARCH_32
#endif

// BIM2 instruction set is not properly defined on msvc
#if defined(_MSC_VER) && defined(__AVX2__)
#define __BMI2__
#endif

// __MINGW32__ doesn't seem to be properly defined, so define it.
#ifndef __MINGW32__
#if	(defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && defined(__GNUC__) && !defined(__CYGWIN__)
#define __MINGW32__
#endif
#endif

// With some version of mingw, alignof(std::max_align_t) (16 or 32) is inconsistent with the real alignment obtained through malloc (8)
#ifdef __MINGW32__
#ifdef SEQ_ARCH_64
#define SEQ_DEFAULT_ALIGNMENT alignof(double)
namespace seq {
	using max_align_t = double;
}
#else 
#define SEQ_DEFAULT_ALIGNMENT alignof(double)
namespace seq {
	using max_align_t = double;
}
#endif
#else
#define SEQ_DEFAULT_ALIGNMENT alignof(std::max_align_t)
namespace seq {
	using max_align_t = std::max_align_t;
} // namespace seq
#endif

// Abort program with a last message
#define SEQ_ABORT( ...)\
	printf( __VA_ARGS__ ); fflush(stdout);\
	abort();

// going through a variable to avoid cppcheck error with SEQ_OFFSETOF
static const void* __dummy_ptr_with_long_name = nullptr;
// Redefine offsetof to get rid of warning "'offsetof' within non-standard-layout type ...."
#define SEQ_OFFSETOF(s,m) (reinterpret_cast<::size_t>(&reinterpret_cast<char const volatile&>(((static_cast<const s*>(__dummy_ptr_with_long_name))->m))))



// Check for C++17
#ifdef _MSC_VER
	#if _MSVC_LANG >= 201703L
		#define SEQ_HAS_CPP_17
	#endif
#else
	#if __cplusplus >= 201703L
		#define SEQ_HAS_CPP_17
	#endif
#endif



//pragma directive might be different between compilers, so define a generic SEQ_PRAGMA macro.
//Use SEQ_PRAGMA with no quotes around argument (ex: SEQ_PRAGMA(omp parallel) and not SEQ_PRAGMA("omp parallel") ).
#ifdef _MSC_VER
#define _SEQ_PRAGMA(text) __pragma(text)
#else
#define _SEQ_PRAGMA(text) _Pragma(#text)
#endif
#define SEQ_PRAGMA(text) _SEQ_PRAGMA(text)

// no inline
#ifdef _MSC_VER
#define SEQ_NOINLINE(...) __declspec(noinline) __VA_ARGS__
#else
#define SEQ_NOINLINE(...) __VA_ARGS__ __attribute__((noinline))
#endif

// fallthrough
#ifndef __has_cpp_attribute 
#    define __has_cpp_attribute(x) 0
#endif
#if __has_cpp_attribute(clang::fallthrough)
#    define SEQ_FALLTHROUGH() [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
#    define SEQ_FALLTHROUGH() [[gnu::fallthrough]]
#else
#    define SEQ_FALLTHROUGH()
#endif

// likely/unlikely definition
#ifndef _MSC_VER
#define SEQ_LIKELY(x)    __builtin_expect (!!(x), 1)
#define SEQ_UNLIKELY(x)  __builtin_expect (!!(x), 0)
#else
#define SEQ_LIKELY(x) x
#define SEQ_UNLIKELY(x) x
#endif

// Simple function inlining
#define SEQ_INLINE inline

// Strongest available function inlining
#if defined(__GNUC__) && (__GNUC__>=4)
#define SEQ_ALWAYS_INLINE __attribute__((always_inline)) inline
#define SEQ_EXTENSION __extension__
#elif defined(__GNUC__)
#define SEQ_ALWAYS_INLINE  inline
#define SEQ_EXTENSION __extension__
#elif (defined _MSC_VER) || (defined __INTEL_COMPILER)
#define SEQ_ALWAYS_INLINE __forceinline
#else
#define SEQ_ALWAYS_INLINE inline
#endif

#ifndef SEQ_EXTENSION 
#define SEQ_EXTENSION 
#endif


// Debug assertion
#ifdef NDEBUG
#define SEQ_ASSERT_DEBUG(condition, msg) 
#else
#define SEQ_ASSERT_DEBUG(condition, ... )  assert((condition) && (__VA_ARGS__))
#endif


// Support for __has_builtin
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

// Support for __has_attribute
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

//Check for aligned memory allocation functions
#if ((defined __QNXNTO__) || (defined _GNU_SOURCE) || ((defined _XOPEN_SOURCE) && (_XOPEN_SOURCE >= 600))) \
 && (defined _POSIX_ADVISORY_INFO) && (_POSIX_ADVISORY_INFO > 0)
#define SEQ_HAS_POSIX_MEMALIGN 1
#else
#define SEQ_HAS_POSIX_MEMALIGN 0
#endif

#if defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)
#define SEQ_HAS_MM_MALLOC 1
#else
#define SEQ_HAS_MM_MALLOC 0
#endif

#if defined(_MSC_VER) && (!defined(_WIN32_WCE))
#define SEQ_HAS_ALIGNED_MALLOC 1
#else
#define SEQ_HAS_ALIGNED_MALLOC 0
#endif

#if defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
#define SEQ_HAS_MINGW_ALIGNED_MALLOC 1
#else
#define SEQ_HAS_MINGW_ALIGNED_MALLOC 0
#endif

namespace seq
{
	namespace detail
	{

		/// \internal Like malloc, but the returned pointer is guaranteed to be alignment-byte aligned.
		/// Fast, but wastes alignment additional bytes of memory. Does not throw any exception.
		inline auto handmade_aligned_malloc(size_t size, size_t alignment) -> void*
		{
			void* ptr = nullptr;
			alignment--;

			size_t offset = 0;
			std::uint8_t* mem = nullptr;

			// Room for padding and extra pointer stored in front of allocated area 
			size_t overhead = alignment + sizeof(void*);

			// Avoid integer overflow 
			if (size > (SIZE_MAX - overhead)) {
				return nullptr;
			}

			mem = static_cast<std::uint8_t*>(malloc(size + overhead));
			if (mem == nullptr) {
				return mem;
}

			// Use the fact that alignment + 1U is a power of 2
			offset = ((alignment ^ (reinterpret_cast<std::uintptr_t>(mem + sizeof(void*)) & alignment)) + 1U) & alignment;
			ptr = static_cast<void*>(mem + sizeof(void*) + offset);
			(static_cast<void**>(ptr))[-1] = mem;
			return ptr;
		}

		/// \internal Frees memory allocated with handmade_aligned_malloc
		inline void handmade_aligned_free(void* ptr)
		{
			// Generic implementation has malloced pointer stored in front of used area 
			if (ptr != nullptr) {
				free((static_cast<void**>(ptr))[-1]);
			}
		}

	}


	/// @brief  Allocates \a size bytes. The returned pointer is guaranteed to have \a align bytes alignment.
	/// @param size size in bytes to allocate
	/// @param align alignment of result pointer 
	/// @return algned pointer or NULL on error
	inline auto aligned_malloc(size_t size, size_t align) -> void*
	{
		void* result = nullptr;

	#if SEQ_HAS_POSIX_MEMALIGN
		if (posix_memalign(&result, align, size)) result = 0;
	#elif SEQ_HAS_MM_MALLOC
		result = _mm_malloc(size, align);
	#elif SEQ_HAS_ALIGNED_MALLOC
		result = _aligned_malloc(size, align);
	#elif SEQ_HAS_MINGW_ALIGNED_MALLOC
		result = __mingw_aligned_malloc(size, align);
	#else
		result = detail::handmade_aligned_malloc(size, align);
	#endif
		return result;
	}


	/// @brief Frees memory allocated with aligned_malloc.
	inline void aligned_free(void* ptr)
	{
	#if SEQ_HAS_POSIX_MEMALIGN
		free(ptr);
	#elif SEQ_HAS_MM_MALLOC
		_mm_free(ptr);
	#elif SEQ_HAS_ALIGNED_MALLOC
		_aligned_free(ptr);
	#elif SEQ_HAS_MINGW_ALIGNED_MALLOC
		__mingw_aligned_free(ptr);
	#else
		detail::handmade_aligned_free(ptr);
	#endif
	}

}//end namespace seq


#ifdef __GNUC__
#define GNUC_PREREQ(x, y) \
      (__GNUC__ > x || (__GNUC__ == x && __GNUC_MINOR__ >= y))
#else
#define GNUC_PREREQ(x, y) 0
#endif

#ifdef __clang__
#define CLANG_PREREQ(x, y) \
      (__clang_major__ > (x) || (__clang_major__ == (x) && __clang_minor__ >= (y)))
#else
#define CLANG_PREREQ(x, y) 0
#endif

#if (_MSC_VER < 1900) && \
    !defined(__cplusplus)
#define inline __inline
#endif

#if (defined(__i386__) || \
     defined(__x86_64__) || \
     defined(_M_IX86) || \
     defined(_M_X64))
#define X86_OR_X64
#endif

#if GNUC_PREREQ(4, 2) || \
    __has_builtin(__builtin_popcount)
#define HAVE_BUILTIN_POPCOUNT
#endif

#if GNUC_PREREQ(4, 2) || \
    CLANG_PREREQ(3, 0)
#define HAVE_ASM_POPCNT
#endif

#if defined(X86_OR_X64) && \
   (defined(HAVE_ASM_POPCNT) || \
    defined(_MSC_VER))
#define HAVE_POPCNT
#endif


#if defined( _MSC_VER) || defined(__MINGW32__)
#include <immintrin.h>
#include <intrin.h>
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <nmmintrin.h>
#endif


namespace seq
{
	namespace detail
	{
		// Define default popcount functions

		// This uses fewer arithmetic operations than any other known
		// implementation on machines with fast multiplication.
		// It uses 12 arithmetic operations, one of which is a multiply.
		// http://en.wikipedia.org/wiki/Hamming_weight#Efficient_implementation
		inline auto popcount64(std::uint64_t x) -> unsigned
		{
			std::uint64_t m1 = 0x5555555555555555LL;
			std::uint64_t m2 = 0x3333333333333333LL;
			std::uint64_t m4 = 0x0F0F0F0F0F0F0F0FLL;
			std::uint64_t h01 = 0x0101010101010101LL;

			x -= (x >> 1) & m1;
			x = (x & m2) + ((x >> 2) & m2);
			x = (x + (x >> 4)) & m4;

			return (x * h01) >> 56;
		}
		inline auto popcount32(uint32_t i) -> unsigned
		{
			i = i - ((i >> 1) & 0x55555555);        // add pairs of bits
			i = (i & 0x33333333) + ((i >> 2) & 0x33333333);  // quads
			i = (i + (i >> 4)) & 0x0F0F0F0F;        // groups of 8
			return (i * 0x01010101) >> 24;          // horizontal sum of bytes
		}
	}


#if defined(HAVE_ASM_POPCNT) && \
		defined(__x86_64__)

	inline auto popcnt64(std::uint64_t x) -> unsigned
	{
		__asm__("popcnt %1, %0" : "=r" (x) : "0" (x));
		return x;
	}

	inline auto popcnt32(uint32_t x) -> unsigned
	{
		return detail::popcount32(x);
	}

#elif defined(HAVE_ASM_POPCNT) && \
		  defined(__i386__)

	inline unsigned popcnt32(uint32_t x)
	{
		__asm__("popcnt %1, %0" : "=r" (x) : "0" (x));
		return x;
	}

	inline unsigned popcnt64(std::uint64_t x)
	{
		return popcnt32((uint32_t)x) +
			popcnt32((uint32_t)(x >> 32));
	}

#elif defined(_MSC_VER) && \
		  defined(_M_X64)

	inline unsigned popcnt64(std::uint64_t x)
	{
		return (unsigned)_mm_popcnt_u64(x);
	}

	inline unsigned popcnt32(uint32_t x)
	{
		return (unsigned)_mm_popcnt_u32(x);
	}

#elif defined(_MSC_VER) && \
		  defined(_M_IX86)


	inline unsigned popcnt64(std::uint64_t x)
	{
		return _mm_popcnt_u32((uint32_t)x) +
			_mm_popcnt_u32((uint32_t)(x >> 32));
	}
	inline unsigned popcnt32(uint32_t x)
	{
		return _mm_popcnt_u32(x);
	}

	/* non x86 CPUs */
#elif defined(HAVE_BUILTIN_POPCOUNT)

	inline std::uint64_t popcnt64(std::uint64_t x)
	{
		return __builtin_popcountll(x);
	}
	inline uint32_t popcnt32(uint32_t x)
	{
		return __builtin_popcount(x);
	}

	/* no hardware POPCNT,
	 * use pure integer algorithm */
#else

	inline std::uint64_t popcnt64(std::uint64_t x)
	{
		return detail::popcount64(x);
	}
	inline uint32_t popcnt32(uint32_t x)
	{
		return detail::popcount32(x);
	}

#endif

	inline auto popcnt8(unsigned char value) -> unsigned
	{
		static const unsigned char ones[256] =
		{ 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
			1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,1,2,2,
			3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,
			4,5,3,4,4,5,4,5,5,6,1,2,2,3,2,3,3,4,2,
			3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,
			4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,
			6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,1,2,
			2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,
			4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,
			3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,
			6,5,6,6,7,2,3,3,4,3,4,4,5,3,4,4,5,4,5,
			5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,3,
			4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,
			5,6,6,7,5,6,6,7,6,7,7,8 };
		return ones[value];
	}
	inline auto popcnt16(unsigned short value) -> unsigned
	{
#ifdef _MSC_VER
		return __popcnt16(value);
#else
		return popcnt8(value & 0xFF) + popcnt8(value >> 8);
#endif
	}



	///
	/// @function unsigned popcnt16(unsigned short value)
	/// @brief Returns the number of set bits in \a value.
	///

	///
	/// @function unsigned popcnt32(unsigned int value)
	/// @brief Returns the number of set bits in \a value.
	///

	///
	/// @function unsigned popcnt64(unsigned long long value)
	/// @brief Returns the number of set bits in \a value.
	///


	namespace detail
	{
		const unsigned forward_index64[64] = {
			0, 47,  1, 56, 48, 27,  2, 60,
		   57, 49, 41, 37, 28, 16,  3, 61,
		   54, 58, 35, 52, 50, 42, 21, 44,
		   38, 32, 29, 23, 17, 11,  4, 62,
		   46, 55, 26, 59, 40, 36, 15, 53,
		   34, 51, 20, 43, 31, 22, 10, 45,
		   25, 39, 14, 33, 19, 30,  9, 24,
		   13, 18,  8, 12,  7,  6,  5, 63
		};


		const unsigned backward_index64[64] = {
		0, 47,  1, 56, 48, 27,  2, 60,
		57, 49, 41, 37, 28, 16,  3, 61,
		54, 58, 35, 52, 50, 42, 21, 44,
		38, 32, 29, 23, 17, 11,  4, 62,
		46, 55, 26, 59, 40, 36, 15, 53,
		34, 51, 20, 43, 31, 22, 10, 45,
		25, 39, 14, 33, 19, 30,  9, 24,
		13, 18,  8, 12,  7,  6,  5, 63
		};


		

		static const wint_t scan_reverse_8[] =
		{ 8, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4,
			4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5,
			5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
			5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7,
			7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
			7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
			7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
			7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
			7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
			7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
			7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
			7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };

	}

	inline auto bit_scan_forward_8(wint_t  val) -> unsigned int
	{
		static const std::uint8_t scan_forward_8[] =
		{ 8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
			0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		};
		return scan_forward_8[val];
	}
	inline auto bit_scan_reverse_8(std::uint8_t  val) -> unsigned int
	{
		return detail::scan_reverse_8[val];
	}
	

	/// @brief Returns the lowest set bit index in \a val
	/// Undefined if val==0.
	inline auto bit_scan_forward_32(std::uint32_t  val) -> unsigned int
	{
#   if defined(_MSC_VER)   /* Visual */
		unsigned long r = 0;
		_BitScanForward(&r, val);
		return static_cast<unsigned>(r);
#   elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3)))   /* Use GCC Intrinsic */
		return __builtin_ctz(val);
#   else   /* Software version */
		static const int MultiplyDeBruijnBitPosition[32] =
		{
			0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
			31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
		};
		return MultiplyDeBruijnBitPosition[((uint32_t)((val & -val) * 0x077CB531U)) >> 27];
#   endif
	}

	/// @brief Returns the highest set bit index in \a val
	/// Undefined if val==0.
	inline auto bit_scan_reverse_32(std::uint32_t  val) -> unsigned int
	{
#   if defined(_MSC_VER)   /* Visual */
		unsigned long r = 0;
		_BitScanReverse(&r, val);
		return static_cast<unsigned>(r);
#   elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3)))   /* Use GCC Intrinsic */
		return 31 - __builtin_clz(val);
#   else   /* Software version */
		static const unsigned int pos[32] = { 0, 1, 28, 2, 29, 14, 24, 3,
			30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19,
			16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
		//does not work for 0
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v = (v >> 1) + 1;
		return pos[(v * 0x077CB531UL) >> 27];
#   endif
	}


	
	/// @brief Returns the lowest set bit index in \a bb.
	/// Developed by Kim Walisch (2012).
	/// Undefined if bb==0.
	inline auto bit_scan_forward_64(std::uint64_t bb)noexcept -> unsigned {
#       if defined(_MSC_VER) && defined(_WIN64) 
		unsigned long r = 0;
		_BitScanForward64(&r, bb);
		return static_cast<unsigned>(r);
#       elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3)))
		return __builtin_ctzll(bb);
#       else
		const std::uint64_t debruijn64 = std::int64_t(0x03f79d71b4cb0a89);
		return detail::forward_index64[((bb ^ (bb - 1)) * debruijn64) >> 58];
#endif
	}


	/// @brief Returns the highest set bit index in \a bb.
	/// Developed by Kim Walisch, Mark Dickinson.
	/// Undefined if bb==0.
	inline auto bit_scan_reverse_64(std::uint64_t  bb)noexcept -> unsigned {
#       if (defined(_MSC_VER) && defined(_WIN64) ) //|| defined(__MINGW64_VERSION_MAJOR)
		unsigned long r = 0;
		_BitScanReverse64(&r, bb);
		return static_cast<unsigned>(r);
#       elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3)))
		return  63 - __builtin_clzll(bb);
#       else
		const std::uint64_t  debruijn64 = std::int64_t(0x03f79d71b4cb0a89);
		//assert(bb != 0);
		bb |= bb >> 1;
		bb |= bb >> 2;
		bb |= bb >> 4;
		bb |= bb >> 8;
		bb |= bb >> 16;
		bb |= bb >> 32;
		return detail::backward_index64[(bb * debruijn64) >> 58];
#endif
	}


	/// @brief Returns the lowest set bit index in \a bb.
	/// Undefined if bb==0.
	inline auto bit_scan_forward(size_t bb)noexcept -> unsigned {
#ifdef SEQ_ARCH_64
		return bit_scan_forward_64(bb);
#else
		return bit_scan_forward_32(bb);
#endif
	}

	/// @brief Returns the highest set bit index in \a bb.
	/// Undefined if bb==0.
	inline auto bit_scan_reverse(size_t bb)noexcept -> unsigned {
#ifdef SEQ_ARCH_64
		return bit_scan_reverse_64(bb);
#else
		return bit_scan_reverse_32(bb);
#endif
	}


	/**
	* Returns the number of digits used to represent an integer in base 10.
	* This function only works for unsigned integral types
	*/
	template <class T>
	SEQ_ALWAYS_INLINE auto count_digits_base_10(T x) -> unsigned {

		static_assert(std::is_unsigned<T>::value, "");

		if (sizeof(T) > 4) {
			if (x >= 10000000000ULL) {
				if (x >= 100000000000000ULL) {
					if (x >= 10000000000000000ULL) {
						if (x >= 100000000000000000ULL) {
							if (x >= 1000000000000000000ULL) {
								if (x >= 10000000000000000000ULL)
									return 20;
								return 19;
							}
							return 18;
						}
						return 17;
					}
					if (x >= 1000000000000000ULL)
						return 16;
					return 15;
				}
				if (x >= 1000000000000ULL) {
					if (x >= 10000000000000ULL)
						return 14;
					return 13;
				}
				if (x >= 100000000000ULL)
					return 12;
				return 11;
			}
		}

		if (sizeof(T) > 2) {
			if (x >= 100000U) {
				if (x >= 10000000U) {
					if (x >= 100000000U) {
						if (x >= 1000000000U)
							return 10;
						return 9;
					}
					return 8;
				}
				if (x >= 1000000U)
					return 7;
				return 6;
			}
		}

		if (x >= 100U) {
			if (x >= 1000U) {
				if (x >= 10000U)
					return 5;
				return 4;
			}
			return 3;
		}
		if (x >= 10U)
			return 2;
		return 1;
	}

	
	/*
	// Slightly slower than version above
	template<typename T>
	SEQ_ALWAYS_INLINE unsigned count_digits_base_10(T x)
	{
		static_assert(std::is_unsigned<T>::value, "");

		//described in https://stackoverflow.com/questions/25892665/performance-of-log10-function-returning-an-int
		if (x == 0) return 1;
		static constexpr T tenToThe[] =
		{
			T(1ULL),
			T(10ULL),
			T(100ULL),
			T(1000ULL),
			T(10000ULL),
			T(100000ULL),
			T(1000000ULL),
			T(10000000ULL),
			T(100000000ULL),
			T(1000000000ULL),
			T(10000000000ULL),
			T(100000000000ULL),
			T(1000000000000ULL),
			T(10000000000000ULL),
			T(100000000000000ULL),
			T(1000000000000000ULL),
			T(10000000000000000ULL),
			T(100000000000000000ULL),
			T(1000000000000000000ULL),
			T(10000000000000000000ULL),
		};
		static constexpr unsigned guess[65] =
		{
			0 ,0 ,0 ,0 , 1 ,1 ,1 , 2 ,2 ,2 ,
			3 ,3 ,3 ,3 , 4 ,4 ,4 , 5 ,5 ,5 ,
			6 ,6 ,6 ,6 , 7 ,7 ,7 , 8 ,8 ,8 ,
			9 ,9 ,9 ,9 , 10,10,10, 11,11,11,
			12,12,12,12, 13,13,13, 14,14,14,
			15,15,15,15, 16,16,16, 17,17,17,
			18,18,18,18, 19
		};
		const unsigned base_2 = (sizeof(x) > 4) ? bit_scan_reverse_64((std::uint64_t)x) + 1 : bit_scan_reverse_32((std::uint32_t)x) + 1;
		const auto digits = guess[base_2];
		return digits + (x >= tenToThe[digits]);
	}*/

	namespace detail
	{
		inline auto generic_nth_bit_set(std::uint64_t value, unsigned int n) noexcept -> unsigned int
		{
			if (value == 0) {
				return 64;
			}

			unsigned pos = bit_scan_forward_64(value);
			for (unsigned i = 0; i < n; ++i) {
				value &= ~(1ULL << pos);
				if (value == 0) {
					return 64;
				}
				pos = bit_scan_forward_64(value);
			}
			return pos;
		}
	}


#ifdef SEQ_ARCH_64
#if defined(_MSC_VER) && defined(__BMI2__ )

	inline unsigned nth_bit_set(std::uint64_t x, unsigned n) noexcept {
		return (unsigned)_tzcnt_u64(_pdep_u64(1ULL << n, x));
	}

#elif ((defined(__clang__) || (defined(__GNUC__) && (__GNUC__>=3))) && defined(__BMI2__ ))
	inline unsigned nth_bit_set(std::uint64_t x, unsigned n) noexcept {
		return (unsigned)_tzcnt_u64(_pdep_u64(1ULL << n, x));
	}

#else
	inline auto nth_bit_set(std::uint64_t x, unsigned n)noexcept -> unsigned {
		return detail::generic_nth_bit_set(x, n);
	}
#endif
#else

	inline unsigned nth_bit_set(std::uint64_t x, unsigned n)noexcept {
		return detail::generic_nth_bit_set(x, n);
	}
#endif


	/// @function unsigned nth_bit_set(std::uint64_t x, unsigned n)
	/// @brief Returns the index of the nth set bit in \a x, or 64 if such bit does not exist.

	namespace detail
	{
		template<size_t ConsecutiveNBits>
		auto find_consecutive_bits(size_t num) -> size_t {
			return (num >> (ConsecutiveNBits - 1)) & find_consecutive_bits< ConsecutiveNBits - 1>(num);
		}
		template<>
		inline auto find_consecutive_bits<1>(size_t num) -> size_t {
			return (num);
		}
	}

	/// @brief Returns the position of the first consecutive N bits within \a num
	/// @param num number of consecutive bits to look for
	/// @return position of the first consecutive N bits within \a num
	template<size_t ConsecutiveNBits>
	auto consecutive_N_bits(size_t num) -> unsigned {
		static_assert(ConsecutiveNBits > 0, "invalid 0 consecutive bits requested");
		num = detail::find_consecutive_bits< ConsecutiveNBits>(num);
		return num ? bit_scan_forward(num) : static_cast<unsigned>(-1);
	}


	/// @brief Returns a byte-swapped representation of the 16-bit argument.
	inline auto byte_swap_16(std::uint16_t value) -> std::uint16_t {
#if defined(_MSC_VER) && !defined(_DEBUG)
		return _byteswap_ushort(value);
#else
		return (value << 8) | (value >> 8);
#endif
	}

	/// @brief Returns a byte-swapped representation of the 32-bit argument.
	inline auto byte_swap_32(std::uint32_t value) -> std::uint32_t {
#if  defined(__GNUC__) && (__GNUC__>=4 && __GNUC_MINOR__>=3) && !defined(__ICC)
		return __builtin_bswap32(value);
#elif defined(__APPLE__)
		return OSSwapInt32(value);
#elif defined(__sun) || defined(sun)
		return BSWAP_32(value);
#elif defined(__FreeBSD__)
		return bswap32(value);
#elif defined(__OpenBSD__)
		return swap32(value);
#elif defined(__NetBSD__)
		return bswap32(value);
#elif defined(_MSC_VER) && !defined(_DEBUG)
		return _byteswap_ulong(value);
#else
		return  ((((value) & 0xff000000) >> 24) | (((value) & 0x00ff0000) >> 8) |
			(((value) & 0x0000ff00) << 8) | (((value) & 0x000000ff) << 24));
#endif
	}

	/// @brief Returns a byte-swapped representation of the 64-bit argument.
	inline auto byte_swap_64(std::uint64_t value) -> std::uint64_t {
#if  defined(__GNUC__) && (__GNUC__>=4 && __GNUC_MINOR__>=3) && !defined(__ICC)
		return __builtin_bswap64(value);
#elif defined(__APPLE__)
		return OSSwapInt64(value);
#elif defined(__sun) || defined(sun)
		return BSWAP_64(value);
#elif defined(__FreeBSD__)
		return bswap64(value);
#elif defined(__OpenBSD__)
		return swap64(value);
#elif defined(__NetBSD__)
		return bswap64(value);
#elif defined(_MSC_VER) && !defined(_DEBUG)
		return _byteswap_uint64(value);
#else
		return (SEQ_EXTENSION((((value) & 0xff00000000000000ull) >> 56)
			| (((value) & 0x00ff000000000000ull) >> 40)
			| (((value) & 0x0000ff0000000000ull) >> 24)
			| (((value) & 0x000000ff00000000ull) >> 8)
			| (((value) & 0x00000000ff000000ull) << 8)
			| (((value) & 0x0000000000ff0000ull) << 24)
			| (((value) & 0x000000000000ff00ull) << 40)
			| (((value) & 0x00000000000000ffull) << 56)));
#endif
	}


	/// @brief Write 16 bits integer value to dst in little endian order
	inline void write_LE_16(void* dst, std::uint16_t value)
	{
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_LITTLE_ENDIAN
		value = byte_swap_16(value);
#endif
		memcpy(dst, &value, sizeof(std::uint16_t));
	}
	/// @brief Write 32 bits integer value to dst in little endian order
	inline void write_LE_32(void* dst, std::uint32_t value)
	{
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_LITTLE_ENDIAN
		value = byte_swap_32(value);
#endif
		memcpy(dst, &value, sizeof(std::uint32_t));
	}
	/// @brief Write 64 bits integer value to dst in little endian order
	inline void write_LE_64(void* dst, std::uint64_t value)
	{
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_LITTLE_ENDIAN
		value = byte_swap_64(value);
#endif
		memcpy(dst, &value, sizeof(std::uint64_t));
	}


	/// @brief Read 16 bits integer from src in little endian order
	inline auto read_LE_16(const void* src) -> std::uint16_t
	{
		std::uint16_t value = 0;
		memcpy(&value, src, sizeof(std::uint16_t));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_LITTLE_ENDIAN
		value = byte_swap_16(value);
#endif
		return value;
	}
	/// @brief Read 32 bits integer from src in little endian order
	inline auto read_LE_32(const void* src) -> std::uint32_t
	{
		std::uint32_t value = 0;
		memcpy(&value, src, sizeof(std::uint32_t));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_LITTLE_ENDIAN
		value = byte_swap_32(value);
#endif
		return value;
	}
	/// @brief Read 64 bits integer from src in little endian order
	inline auto read_LE_64(const void* src) -> std::uint64_t
	{
		std::uint64_t value = 0;
		memcpy(&value, src, sizeof(std::uint64_t));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_LITTLE_ENDIAN
		value = byte_swap_64(value);
#endif
		return value;
	}


	/// @brief Reads 16 bits integer from src
	inline auto read_16(const void* src) -> std::uint16_t
	{
		std::uint16_t value = 0;
		memcpy(&value, src, sizeof(std::uint16_t));
		return value;
	}
	/// @brief Reads 32 bits integer from src
	inline auto read_32(const void* src) -> std::uint32_t
	{
		std::uint32_t value = 0;
		memcpy(&value, src, sizeof(std::uint32_t));
		return value;
	}
	/// @brief Reads 64 bits integer from src
	inline auto read_64(const void* src) -> std::uint64_t
	{
		std::uint64_t value = 0;
		memcpy(&value, src, sizeof(std::uint64_t));
		return value;
	}

	/// @brief Reads uintptr_t integer from src
	inline auto read_ptr_t(const void* src) -> std::uintptr_t
	{
		std::uintptr_t value = 0;
		memcpy(&value, src, sizeof(std::uintptr_t));
		return value;
	}


	/// @brief Reads 16 bits integer from src in big endian order
	inline auto read_BE_16(const void* src) -> std::uint16_t
	{
		std::uint16_t value = 0;
		memcpy(&value, src, sizeof(std::uint16_t));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
		value = byte_swap_16(value);
#endif
		return value;
	}
	/// @brief Reads 32 bits integer from src in big endian order
	inline auto read_BE_32(const void* src) -> std::uint32_t
	{
		std::uint32_t value = 0;
		memcpy(&value, src, sizeof(std::uint32_t));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
		value = byte_swap_32(value);
#endif
		return value;
	}
	/// @brief Reads 64 bits integer from src in big endian order
	inline auto read_BE_64(const void* src) -> std::uint64_t
	{
		std::uint64_t value = 0;
		memcpy(&value, src, sizeof(std::uint64_t));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
		value = byte_swap_64(value);
#endif
		return value;
	}


	/// @brief Reads size_t object from src 
	inline auto read_size_t(const void* src) -> size_t
	{
		size_t res = 0;
		memcpy(&res, src, sizeof(size_t));
		return res;
	}
	/// @brief Reads size_t object from src in little endian order
	inline auto read_LE_size_t(const void* src) -> size_t
	{
		size_t res = 0;
		memcpy(&res, src, sizeof(size_t));
#if SEQ_BYTEORDER_ENDIAN != BYTEORDER_LITTEL_ENDIAN
		if (sizeof(size_t) == 8) res = byte_swap_64(res);
		else res = byte_swap_32(res);
#endif
		return res;
	}
	/// @brief Reads size_t object from src in big endian order
	inline auto read_BE_size_t(const void* src) -> size_t
	{
		size_t res = 0;
		memcpy(&res, src, sizeof(size_t));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
		if (sizeof(size_t) == 8) { res = byte_swap_64(res);
		} else { res = static_cast<size_t>(byte_swap_32(static_cast<std::uint32_t>(res)));
}
#endif
		return res;
	}

	/// @brief Static version of bit_scan_reverse
	template<size_t Size>
	struct static_bit_scan_reverse
	{
		static const size_t value = Size > 1ULL ? 1ULL + static_bit_scan_reverse<Size / 2ULL>::value : 1ULL;
	};
	template<>
	struct static_bit_scan_reverse<1>
	{
		static const size_t value = 0ULL;
	} ;
	template<>
	struct static_bit_scan_reverse<0ULL>
	{
	} ;

}//end namespace seq


#ifdef __GNUC__
//#pragma GCC diagnostic pop
#endif

/** @}*/
//end bits

#endif
