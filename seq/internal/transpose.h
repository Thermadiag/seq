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

#ifndef SEQ_TRANSPOSE_H
#define SEQ_TRANSPOSE_H

#include "simd.hpp"
#include "../bits.hpp"

#ifdef __SSE4_1__

namespace seq
{
	namespace detail
	{

		union alignas(16) hse_vector {
			char i8[16];
			unsigned char u8[16];
			unsigned short u16[8];
			std::uint32_t u32[4];
		};

		static inline const __m128i& __get(const hse_vector& v) {
			return *reinterpret_cast<const __m128i*>(&v);
		}
		static inline void __set(hse_vector& v, const __m128i& sse) {
			_mm_store_si128(reinterpret_cast<__m128i*>(&v), sse);
		}

		typedef hse_vector hse_array_type[16];

	}
}

#ifndef SEQ_HEADER_ONLY
namespace seq
{
	SEQ_EXPORT void transpose_16x16(const __m128i* in, __m128i* out);

	SEQ_EXPORT void transpose_256_rows(const char* src, char* aligned_dst, unsigned BPP);
	SEQ_EXPORT void transpose_inv_256_rows(const char* src, char* dst, unsigned BPP);

	SEQ_EXPORT void transpose_generic(const char* src, char* dst, unsigned block_size, unsigned BPP);
	SEQ_EXPORT void transpose_inv_generic(const char* src, char* dst, unsigned block_size, unsigned BPP);
}
#else
#include "transpose.cpp"
#endif

#endif //__SSE4_1__

#endif
