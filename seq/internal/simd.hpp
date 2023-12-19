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


#pragma once

#include "../bits.hpp"

#if defined( _MSC_VER) && !defined(__clang__)
 // Silence msvc warning message about alignment
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif


#if defined(__MINGW64_VERSION_MAJOR) || defined(_MSC_VER)
#include <intrin.h>
#endif


// With msvc, try to auto detect AVX and thus sse4.2
#if (defined (_M_IX86) || defined (_M_X64)) && !defined(_CHPE_ONLY_) && (!defined(_M_ARM64EC) || !defined(_DISABLE_SOFTINTRIN_))
#ifndef __AVX__
#define __AVX__
#endif
#endif


//For msvc, define __SSE__ and __SSE2__ manually
#if defined(_MSC_VER) && (defined(_M_X64) || _M_IX86_FP >= 2)
#  define __SSE__ 1
#  define __SSE2__ 1
#endif

// SSE intrinsics
#if defined(__SSE2__) 
#if defined(__unix ) || defined(__linux ) || defined(__posix )
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
#  define __SSE3__ 1
#  define __SSSE3__ 1
	// no Intel CPU supports SSE4a, so don't define it
#  define __SSE4_1__ 1
#  define __SSE4_2__ 1
#  ifndef __AVX__
#    define __AVX__ 1
#  endif
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

#  if defined(__SSE4_2__) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
	// POPCNT instructions:
	// All processors that support SSE4.2 support POPCNT
	// (but neither MSVC nor the Intel compiler define this macro)
#    define __POPCNT__                      1
#  endif
#endif

	// AVX intrinsics
#if defined(__AVX__)
	// immintrin.h is the ultimate header, we don't need anything else after this
#include <immintrin.h>

#  if defined(__AVX__) &&  (defined(__INTEL_COMPILER) || defined(_MSC_VER))
	// AES, PCLMULQDQ instructions:
	// All processors that support AVX support AES, PCLMULQDQ
	// (but neither MSVC nor the Intel compiler define these macros)
#    define __AES__                         1
#    define __PCLMUL__                      1
#  endif

#  if defined(__AVX2__) &&  (defined(__INTEL_COMPILER) || defined(_MSC_VER))
	// F16C & RDRAND instructions:
	// All processors that support AVX2 support F16C & RDRAND:
	// (but neither MSVC nor the Intel compiler define these macros)
#    define __F16C__                        1
#    define __RDRND__                       1
#  endif
#endif

#if defined(__AES__) || defined(__PCLMUL__)
#  include <wmmintrin.h>
#endif




	// other x86 intrinsics
#if defined(_M_IX86) && ((defined(__GNUC__) && (__GNUC__ >= 4)) \
    || (defined(__clang_major__) && (__clang_major__ >= 2)) \
    || defined(__INTEL_COMPILER))
#  ifdef __INTEL_COMPILER
	// The Intel compiler has no <x86intrin.h> -- all intrinsics are in <immintrin.h>;
#    include <immintrin.h>
#  else
	// GCC 4.4 and Clang 2.8 added a few more intrinsics there
#    include <x86intrin.h>
#  endif
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
#  include <arm_acle.h>
#endif

#include <cstdint>
#include <type_traits>
#include <iostream>




namespace seq
{
	struct CPUFeatures
	{
		//  Misc.
		bool HAS_MMX;
		bool HAS_x64;
		bool HAS_ABM;      // Advanced Bit Manipulation
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


}



#ifndef SEQ_HEADER_ONLY
namespace seq
{
	namespace detail
	{
		SEQ_EXPORT void compute_cpu_feature(CPUFeatures & ,bool &);
	}
}
#else
#include "simd.cpp"
#endif

namespace seq 
{

	SEQ_ALWAYS_INLINE const CPUFeatures& cpu_features()
	{
		static CPUFeatures features;
		static bool initialized = false;
		if (!initialized)
			detail::compute_cpu_feature(features, initialized);
		return features;
	}
	
	inline void print_simd_features()
	{
		if(cpu_features().HAS_SSE2)
			std::printf("Has SSE2\n");
		if (cpu_features().HAS_SSE3)
			std::printf("Has SSE3\n");
		if (cpu_features().HAS_SSE41)
			std::printf("Has SSE4.1\n");
		if (cpu_features().HAS_AVX)
			std::printf("Has AVX\n");
		if (cpu_features().HAS_AVX2)
			std::printf("Has AVX2\n");
	}

}
