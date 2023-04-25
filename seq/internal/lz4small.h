/*
 *  LZ4 - Fast LZ compression algorithm
 *  Header File
 *  Copyright (C) 2011-2017, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/lz4/lz4
*/

#ifndef SEQ_LZ4_SMALL_H
#define SEQ_LZ4_SMALL_H

#include "../bits.hpp"



#ifndef SEQ_HEADER_ONLY
namespace seq
{
	SEQ_EXPORT unsigned lz4_requiredMemorySize();
	SEQ_EXPORT int lz4_compressBound(int isize);
	SEQ_EXPORT int lz4_compress_fast(const char* source, char* dest, int inputSize, int maxOutputSize, int acceleration, void* state);
	SEQ_EXPORT int lz4_compress_default(const char* source, char* dest, int inputSize, int maxOutputSize, void* state);
	SEQ_EXPORT int lz4_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize);
	SEQ_EXPORT int lz4_decompress_fast(const char* source, char* dest, int originalSize);
}
#else
#include "lz4small.cpp"
#endif

#include "block_codec.h"
#include "transpose.h"

#ifdef __SSE4_1__

namespace seq
{
	/// @brief Encoder struct to be used with cvector.
	/// Directly use lz4 on the flat input. Almost always slower/less efficient than the default block encoder, except for string inputs (seq::tiny_string).
	struct Lz4FlatEncoder
	{
		// inplace compression
		static unsigned compress(void* in_out, unsigned BPP, unsigned block_size, unsigned dst_size, unsigned acceleration)
		{
			int r = lz4_compress_fast((char*)in_out, (char*)get_comp_buffer(dst_size), block_size * BPP , dst_size, acceleration + 1, NULL);
			if (r <= 0)
				return SEQ_ERROR_DST_OVERFLOW;
			memcpy(in_out, get_comp_buffer(0), r);
			return r;
		}
		// restore values in case of failed compression
		static void restore(void* in_out, void* dst, unsigned BPP, unsigned block_size)
		{
			memcpy(dst, in_out, BPP * block_size );
		}
		// decompress
		static unsigned decompress(const void* src, unsigned src_size, unsigned BPP, unsigned block_size, void* dst)
		{
			int r = lz4_decompress_fast((const char*)src, (char*)dst, BPP * block_size );
			if (r <= 0)
				return SEQ_ERROR_CORRUPTED_DATA;
			return r;
		}
	};

	/// @brief Encoder struct to be used with cvector.
	/// Use lz4 on the transposed input. Almost always slower/less efficient than the default block encoder. Provided as an example of transposed encoder.
	struct Lz4TransposeEncoder
	{
		// inplace compression
		static unsigned compress(void* in_out, unsigned BPP, unsigned block_size, unsigned dst_size, unsigned acceleration)
		{
			transpose_generic((char*)in_out, (char*)get_comp_buffer(BPP * block_size ), block_size, BPP);
			int r = lz4_compress_fast((char*)get_comp_buffer(0), (char*)in_out, block_size * BPP , dst_size, acceleration + 1, NULL);
			if (r <= 0)
				return SEQ_ERROR_DST_OVERFLOW;
			return r;
		}
		// restore values in case of failed compression
		static void restore(void* in_out, void* dst, unsigned BPP, unsigned block_size)
		{
			transpose_inv_generic((char*)get_comp_buffer(0), (char*)dst, block_size, BPP);
		}
		// decompress
		static unsigned decompress(const void* src, unsigned src_size, unsigned BPP, unsigned block_size, void* dst)
		{
			int r = lz4_decompress_fast((const char*)src, (char*)get_comp_buffer(BPP * block_size ), BPP * block_size );
			if (r <= 0)
				return SEQ_ERROR_CORRUPTED_DATA;
			transpose_inv_generic((char*)get_comp_buffer(0), (char*)dst, block_size, BPP);
			return r;
		}
	};
}

#endif //__SSE4_1__

#endif