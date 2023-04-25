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

#ifndef SEQ_BLOCK_CODEC_H
#define SEQ_BLOCK_CODEC_H

#include "../bits.hpp"
#include "simd.hpp"

#ifdef __SSE4_1__

namespace seq
{
	/// @brief Compile time maximum size taken by a compressed block of 256 element.
	template<class T>
	struct BlockBound
	{
		static constexpr unsigned value = 256 * sizeof(T) + sizeof(T) + sizeof(T) / 2 + sizeof(T) % 2 ;
	};

	template<class T>
	struct MinimalBlockBound
	{
		static constexpr unsigned sizeof_header = sizeof(T) / 2 + sizeof(T) % 2;
		static constexpr unsigned value = sizeof_header + sizeof(T);
		static void compress(const T& v, void * dst)
		{
			memset(dst, 0, sizeof_header);
			memcpy(static_cast<char*>(dst) + sizeof_header, &v, sizeof(T));
		}
	};

};


#ifndef SEQ_HEADER_ONLY

namespace seq
{
	/// @brief Returns a thread local buffer of given size and aligned on 16 bytes
	SEQ_EXPORT void* get_comp_buffer(size_t size);


	/// @brief Returns the maximum size taken by a compressed block of 256 element of size BPP.
	SEQ_EXPORT unsigned block_bound(unsigned BPP);

	/// @brief Compress a block of 256 elements of size BPP into dst.
	/// @param src input data
	/// @param BPP size of one element in bytes
	/// @param dst compressed output
	/// @param dst_size size of compressed output buffer
	/// @param buffer buffer allocated with alloc_block_buffer
	/// Returns the size of compressed block.
	SEQ_EXPORT unsigned block_encode_256(const void* src, unsigned BPP, unsigned block_count, void* dst, unsigned dst_size, unsigned acceleration = 0);
	SEQ_EXPORT unsigned block_decode_256(const void* src, unsigned size, unsigned BPP, unsigned block_count, void* dst);

}

#else
#include "block_codec.cpp"
#endif

#endif //__SSE4_1__

#endif
