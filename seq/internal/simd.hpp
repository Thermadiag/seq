/**
 * MIT License
 *
 * Copyright (c) 2026 Victor Moncada <vtr.moncada@gmail.com>
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

#ifndef SEQ_SIMD_HPP
#define SEQ_SIMD_HPP

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

#include "../bits.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
// Silence msvc warning message about alignment
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif

#if defined(__MINGW64_VERSION_MAJOR) || defined(_MSC_VER)
#include <intrin.h>
#endif

// With msvc, try to auto detect AVX and thus sse4.2
#if (defined(_M_IX86) || defined(_M_X64)) && !defined(_CHPE_ONLY_) && (!defined(_M_ARM64EC) || !defined(_DISABLE_SOFTINTRIN_))
#ifndef __AVX__
#define __AVX__
#endif
#endif

// For msvc, define __SSE__ and __SSE2__ manually
#if defined(_MSC_VER) && (defined(_M_X64) || _M_IX86_FP >= 2)
#ifndef __SSE__
#define __SSE__ 1
#endif
#ifndef __SSE2__
#define __SSE2__ 1
#endif
#endif

// SSE intrinsics
#if defined(__SSE2__)
#if defined(__unix) || defined(__linux) || defined(__posix)
#include <emmintrin.h>
#include <xmmintrin.h>
#else
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

#endif

#if defined(_MSC_VER) && (defined(_M_AVX) || defined(__AVX__))
// Visual Studio defines __AVX__ when /arch:AVX is passed, but not the earlier macros
// See: https://msdn.microsoft.com/en-us/library/b0084kay.aspx
// SSE2 is handled by _M_IX86_FP below
#define __SSE3__ 1
#define __SSSE3__ 1
// no Intel CPU supports SSE4a, so don't define it
#define __SSE4_1__ 1
#define __SSE4_2__ 1
#ifndef __AVX__
#define __AVX__ 1
#endif
#endif

// With msvc, __AVX__ could be defined without __SSE4_1__ or __SSE4_2__
#if defined(__AVX__)
#ifndef __SSE4_1__
#define __SSE4_1__ 1
#endif
#ifndef __SSE4_2__
#define __SSE4_2__ 1
#endif
#endif

// SSE3 intrinsics
#if defined(__SSE3__)
#include <pmmintrin.h>
#endif

// SSSE3 intrinsics
#if defined(__SSSE3__)
#include <tmmintrin.h>
#endif

// SSE4.1 intrinsics
#if defined(__SSE4_1__)
#include <smmintrin.h>
#endif

// SSE4.2 intrinsics
#if defined(__SSE4_2__)
#include <nmmintrin.h>

#if defined(__SSE4_2__) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
// POPCNT instructions:
// All processors that support SSE4.2 support POPCNT
// (but neither MSVC nor the Intel compiler define this macro)
#define __POPCNT__ 1
#endif
#endif

// AVX intrinsics
#if defined(__AVX__)
// immintrin.h is the ultimate header, we don't need anything else after this
#include <immintrin.h>

#if defined(__AVX__) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
// AES, PCLMULQDQ instructions:
// All processors that support AVX support AES, PCLMULQDQ
// (but neither MSVC nor the Intel compiler define these macros)
#define __AES__ 1
#define __PCLMUL__ 1
#endif

#if defined(__AVX2__) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
// F16C & RDRAND instructions:
// All processors that support AVX2 support F16C & RDRAND:
// (but neither MSVC nor the Intel compiler define these macros)
#define __F16C__ 1
#define __RDRND__ 1
#endif
#endif

#if defined(__AES__) || defined(__PCLMUL__)
#include <wmmintrin.h>
#endif

// other x86 intrinsics
#if defined(_M_IX86) && ((defined(__GNUC__) && (__GNUC__ >= 4)) || (defined(__clang_major__) && (__clang_major__ >= 2)) || defined(__INTEL_COMPILER))
#ifdef __INTEL_COMPILER
// The Intel compiler has no <x86intrin.h> -- all intrinsics are in <immintrin.h>;
#include <immintrin.h>
#else
// GCC 4.4 and Clang 2.8 added a few more intrinsics there
#include <x86intrin.h>
#endif
#endif

// NEON intrinsics
// note: as of GCC 4.9, does not support function targets for ARM
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#ifndef __ARM_NEON__
// __ARM_NEON__ is not defined on AArch64, but we need it in our NEON detection.
#define __ARM_NEON__
#endif
#endif
// AArch64/ARM64
#if (defined(_M_ARM64) || defined(__arm__)) && defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#endif



#if defined(__ARM_NEON) || defined(__ARM_NEON__)

// All of this is taken from sse2neon (https://github.com/DLTcollab/sse2neon)

/* Architecture detection */
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define SSE2NEON_ARCH_AARCH64 1
#else
#define SSE2NEON_ARCH_AARCH64 0
#endif

#if defined(__clang__)
/* Clang compiler detected (including Apple Clang) */
#define SSE2NEON_COMPILER_GCC_COMPAT 1 /* Clang supports GCC extensions */
#if defined(_MSC_VER)
#define SSE2NEON_COMPILER_MSVC 1 /* Clang-CL: Clang with MSVC on Windows */
#else
#define SSE2NEON_COMPILER_MSVC 0
#endif

#elif defined(__GNUC__)
/* GCC compiler (only reached if not Clang, since Clang also defines __GNUC__)
 */
#define SSE2NEON_COMPILER_CLANG 0
#define SSE2NEON_COMPILER_GCC_COMPAT 1
#define SSE2NEON_COMPILER_MSVC 0

#elif defined(_MSC_VER)
/* Microsoft Visual C++ (native, not Clang-CL) */
#define SSE2NEON_COMPILER_CLANG 0
#define SSE2NEON_COMPILER_GCC_COMPAT 0 /* No GCC extensions available */
#define SSE2NEON_COMPILER_MSVC 1

#else
#endif


typedef int64x2_t __m128i; /* 128-bit vector containing integers */

// Some intrinsics operate on unaligned data types.
typedef int16_t SEQ_ALIGN_TO_BOUNDARY(1) unaligned_int16_t;
typedef int32_t SEQ_ALIGN_TO_BOUNDARY(1) unaligned_int32_t;
typedef std::int64_t SEQ_ALIGN_TO_BOUNDARY(1) unaligned_int64_t;

#define vreinterpretq_s8_m128i(x) vreinterpretq_s8_s64(x)
#define vreinterpretq_m128i_s32(x) vreinterpretq_s64_s32(x)
#define vreinterpretq_m128i_u8(x) vreinterpretq_s64_u8(x)
#define vreinterpretq_m128i_s8(x) vreinterpretq_s64_s8(x)
#define vreinterpretq_u8_m128i(x) vreinterpretq_u8_s64(x)
#define _sse2neon_reinterpret_cast(t, e) reinterpret_cast<t>(e)

// Load 128-bits of integer data from memory into dst. mem_addr must be aligned
// on a 16-byte boundary or a general-protection exception may be generated.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_load_si128
SEQ_ALWAYS_INLINE __m128i _mm_load_si128(const __m128i* p)
{
	return vreinterpretq_m128i_s32(vld1q_s32(_sse2neon_reinterpret_cast(const int32_t*, p)));
}
// Load 128-bits of integer data from memory into dst. mem_addr does not need to
// be aligned on any particular boundary.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadu_si128
SEQ_ALWAYS_INLINE __m128i _mm_loadu_si128(const __m128i* p)
{
	return vreinterpretq_m128i_s32(vld1q_s32(_sse2neon_reinterpret_cast(const unaligned_int32_t*, p)));
}
// Compare packed 8-bit integers in a and b for equality, and store the results
// in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpeq_epi8
SEQ_ALWAYS_INLINE __m128i _mm_cmpeq_epi8(__m128i a, __m128i b)
{
	return vreinterpretq_m128i_u8(vceqq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Return vector of type __m128i with all elements set to zero.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_setzero_si128
SEQ_ALWAYS_INLINE __m128i _mm_setzero_si128(void)
{
	return vreinterpretq_m128i_s32(vdupq_n_s32(0));
}

// Broadcast 8-bit integer a to all elements of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_set1_epi8
SEQ_ALWAYS_INLINE __m128i _mm_set1_epi8(signed char w)
{
	return vreinterpretq_m128i_s8(vdupq_n_s8(w));
}

// Create mask from the most significant bit of each 8-bit element in a, and
// store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_movemask_epi8
//
//   Input (__m128i): 16 bytes, extract bit 7 (MSB) of each
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|1|2|3|4|5|6|7|8|9|A|B|C|D|E|F|  byte index
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |   ...                     |
//   MSB                         MSB
//    v   v v v v v v v v v v v v v v
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|1|2|3|4|5|6|7|8|9|A|B|C|D|E|F|  bit position in result
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |<-- low byte ->|<-- high byte->|
//
//   Output (int): 16-bit mask where bit[i] = MSB of input byte[i]
SEQ_ALWAYS_INLINE int _mm_movemask_epi8(__m128i a)
{
	uint8x16_t input = vreinterpretq_u8_m128i(a);

#if SSE2NEON_ARCH_AARCH64
	// AArch64: Variable shift + horizontal add (vaddv).
	//
	// Step 1: Extract MSB of each byte (vshr #7: 0x80->1, 0x7F->0)
	uint8x16_t msbs = vshrq_n_u8(input, 7);

	// Step 2: Shift each byte left by its bit position (0-7 per half)
	//
	//   msbs:     [ 1  ][ 0  ][ 1  ][ 1  ][ 0  ][ 1  ][ 0  ][ 1  ] (example)
	//   shifts:   [ 0  ][ 1  ][ 2  ][ 3  ][ 4  ][ 5  ][ 6  ][ 7  ]
	//               |     |     |     |     |     |     |     |
	//              <<0   <<1   <<2   <<3   <<4   <<5   <<6   <<7
	//               v     v     v     v     v     v     v     v
	//   result:  [0x01][0x00][0x04][0x08][0x00][0x20][0x00][0x80]
	//
	//   Horizontal sum: 0x01+0x04+0x08+0x20+0x80 = 0xAD = 0b10101101
	//   Each bit in sum corresponds to one input byte's MSB.
	static const int8_t shift_table[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7 };
	int8x16_t shifts = vld1q_s8(shift_table);
	uint8x16_t positioned = vshlq_u8(msbs, shifts);

	// Step 3: Sum each half -> bits [7:0] and [15:8]
	return vaddv_u8(vget_low_u8(positioned)) | (vaddv_u8(vget_high_u8(positioned)) << 8);
#else
	// ARMv7: Shift-right-accumulate (no vaddv).
	//
	// Step 1: Extract MSB of each byte
	uint8x16_t msbs = vshrq_n_u8(input, 7);
	uint64x2_t bits = vreinterpretq_u64_u8(msbs);

	// Step 2: Parallel bit collection via shift-right-accumulate
	//
	//   Initial (8 bytes shown):
	//   byte:     [  0 ][  1 ][  2 ][  3 ][  4 ][  5 ][  6 ][  7 ]
	//   value:    [ 01 ][ 00 ][ 01 ][ 01 ][ 00 ][ 01 ][ 00 ][ 01 ]
	//
	//   vsra(..., 7):  add original + (original >> 7)
	//   byte 1 gets: orig[1] + orig[0] = b1|b0 in bits [1:0]
	//   byte 3 gets: orig[3] + orig[2] = b3|b2 in bits [1:0]
	//   ...
	//   Result: pairs combined into odd bytes
	//
	//   vsra(..., 14): combine pairs -> 4 bits in bytes 3,7
	//   vsra(..., 28): combine all   -> 8 bits in byte 7 (actually byte 0)
	bits = vsraq_n_u64(bits, bits, 7);
	bits = vsraq_n_u64(bits, bits, 14);
	bits = vsraq_n_u64(bits, bits, 28);

	// Step 3: Extract packed result from byte 0 of each half
	uint8x16_t output = vreinterpretq_u8_u64(bits);
	return vgetq_lane_u8(output, 0) | (vgetq_lane_u8(output, 8) << 8);
#endif
}

#endif




#ifdef _MSC_VER

#include "Windows.h"
//  Windows
namespace seq
{
	namespace detail
	{

		static inline void seq_cpuid_impl(int* info, int InfoType)
		{
			__cpuidex(info, InfoType, 0);
		}
	}
}
#else


#if defined(_M_ARM64) || defined(__arm__) || defined(__ARM_NEON__)
// ARM
namespace seq
{
	namespace detail
	{
		static inline void seq_cpuid_impl(int info[4], int InfoType)
		{
			info[0] = info[1] = info[2] = info[3] = 0;
			(void)InfoType;
		}
	}
}
#else
//  GCC Intrinsics
#include <cpuid.h>
namespace seq
{
	namespace detail
	{
		static inline void seq_cpuid_impl(int info[4], int InfoType)
		{
			__cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
		}
	}
}
#endif
	
#endif


// Undef min and max defined in Windows.h
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <cstdint>
#include <type_traits>

namespace seq
{

	struct CPUFeatures
	{
		//  Misc.
		bool HAS_MMX;
		bool HAS_x64;
		bool HAS_ABM; // Advanced Bit Manipulation
		bool HAS_RDRAND;
		bool HAS_BMI1;
		bool HAS_BMI2;
		bool HAS_ADX;
		bool HAS_PREFETCHWT1;

		//  SIMD: 128-bit
		bool HAS_SSE;
		bool HAS_SSE2;
		bool HAS_SSE3;
		bool HAS_SSSE3;
		bool HAS_SSE41;
		bool HAS_SSE42;
		bool HAS_SSE4a;
		bool HAS_AES;
		bool HAS_SHA;

		//  SIMD: 256-bit
		bool HAS_AVX;
		bool HAS_XOP;
		bool HAS_FMA3;
		bool HAS_FMA4;
		bool HAS_AVX2;

		//  SIMD: 512-bit
		bool HAS_AVX512F;    //  AVX512 Foundation
		bool HAS_AVX512CD;   //  AVX512 Conflict Detection
		bool HAS_AVX512PF;   //  AVX512 Prefetch
		bool HAS_AVX512ER;   //  AVX512 Exponential + Reciprocal
		bool HAS_AVX512VL;   //  AVX512 Vector Length Extensions
		bool HAS_AVX512BW;   //  AVX512 Byte + Word
		bool HAS_AVX512DQ;   //  AVX512 Doubleword + Quadword
		bool HAS_AVX512IFMA; //  AVX512 Integer 52-bit Fused Multiply-Add
		bool HAS_AVX512VBMI; //  AVX512 Vector Byte Manipulation Instructions
	};

	
	namespace detail
	{
		inline CPUFeatures compute_cpu_feature()
		{
			CPUFeatures features;
			memset(&features, 0, sizeof(features));

			int info[4];
			seq_cpuid_impl(info, 0);
			int nIds = info[0];

			seq_cpuid_impl(info, static_cast<int>(0x80000000));
			unsigned nExIds = static_cast<unsigned>(info[0]);

			//  Detect Features
			if (nIds >= 0x00000001) {
				seq_cpuid_impl(info, 0x00000001);
				features.HAS_MMX = (info[3] & (1 << 23)) != 0;
				features.HAS_SSE = (info[3] & (1 << 25)) != 0;
				features.HAS_SSE2 = (info[3] & (1 << 26)) != 0;
				features.HAS_SSE3 = (info[2] & (1 << 0)) != 0;

				features.HAS_SSSE3 = (info[2] & (1 << 9)) != 0;
				features.HAS_SSE41 = (info[2] & (1 << 19)) != 0;
				features.HAS_SSE42 = (info[2] & (1 << 20)) != 0;
				features.HAS_AES = (info[2] & (1 << 25)) != 0;

				features.HAS_AVX = (info[2] & (1 << 28)) != 0;
				features.HAS_FMA3 = (info[2] & (1 << 12)) != 0;

				features.HAS_RDRAND = (info[2] & (1 << 30)) != 0;
			}
			if (nIds >= 0x00000007) {
				seq_cpuid_impl(info, 0x00000007);
				features.HAS_AVX2 = (info[1] & (1 << 5)) != 0;

				features.HAS_BMI1 = (info[1] & (1 << 3)) != 0;
				features.HAS_BMI2 = (info[1] & (1 << 8)) != 0;
				features.HAS_ADX = (info[1] & (1 << 19)) != 0;
				features.HAS_SHA = (info[1] & (1 << 29)) != 0;
				features.HAS_PREFETCHWT1 = (info[2] & (1 << 0)) != 0;

				features.HAS_AVX512F = (info[1] & (1 << 16)) != 0;
				features.HAS_AVX512CD = (info[1] & (1 << 28)) != 0;
				features.HAS_AVX512PF = (info[1] & (1 << 26)) != 0;
				features.HAS_AVX512ER = (info[1] & (1 << 27)) != 0;
				features.HAS_AVX512VL = (info[1] & static_cast<int>(1U << 31U)) != 0;
				features.HAS_AVX512BW = (info[1] & (1 << 30)) != 0;
				features.HAS_AVX512DQ = (info[1] & (1 << 17)) != 0;
				features.HAS_AVX512IFMA = (info[1] & (1 << 21)) != 0;
				features.HAS_AVX512VBMI = (info[2] & (1 << 1)) != 0;
			}
			if (nExIds >= 0x80000001) {
				seq_cpuid_impl(info, static_cast<int>(0x80000001));
				features.HAS_x64 = (info[3] & (1 << 29)) != 0;
				features.HAS_ABM = (info[2] & (1 << 5)) != 0;
				features.HAS_SSE4a = (info[2] & (1 << 6)) != 0;
				features.HAS_FMA4 = (info[2] & (1 << 16)) != 0;
				features.HAS_XOP = (info[2] & (1 << 11)) != 0;
			}

			return features;
		}
	}

	/// @brief Returns the plateform CPU features
	SEQ_ALWAYS_INLINE const CPUFeatures& cpu_features()
	{
		static CPUFeatures features = detail::compute_cpu_feature();
		return features;
	}
}





#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif // SEQ_SIMD_HPP
