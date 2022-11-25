#pragma once

#if defined( _MSC_VER) && !defined(__clang__)
 // Silence msvc warning message about alignment
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif


#if defined(__MINGW64_VERSION_MAJOR) || defined(_MSC_VER)
#include <intrin.h>
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
#if defined(VIP_PROCESSOR_X86) && ((defined(VIP_CC_GNU) && (VIP_CC_GNU >= 404)) \
    || (defined(VIP_CC_CLANG) && (VIP_CC_CLANG >= 208)) \
    || defined(VIP_CC_INTEL))
#  ifdef VIP_CC_INTEL
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
#if defined(VIP_PROCESSOR_ARM_V8) && defined(__ARM_FEATURE_CRC32)
#  include <arm_acle.h>
#endif

#include <cstdint>
#include <type_traits>

union alignas(16) hse_vector {
	char i8[16];
	unsigned char u8[16];
	unsigned short u16[8];
	std::uint32_t u32[4];
	//__m128i simd;
} ; //assume always aligned to 16 bytes

static inline const __m128i& __get(const hse_vector& v) {
	return *reinterpret_cast<const __m128i*>( & v); }
static inline void __set(hse_vector& v, const __m128i& sse) {
	_mm_store_si128(reinterpret_cast<__m128i*>( &v), sse); }

typedef hse_vector hse_array_type[16];
