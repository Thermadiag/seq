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

#include <utility>
#include <cstdio>

#include "simd.hpp"

#ifdef __SSE4_1__



#include "block_codec.h"
#include "transpose.h"
#include "shuffle_table.h"
#include "unshuffle_table.h"



#define __SEQ_BLOCK_ALL_SAME	0
#define __SEQ_BLOCK_ALL_RAW		1
#define __SEQ_BLOCK_NORMAL		2


namespace seq
{

	namespace detail
	{

		static const unsigned char header_0[2][9] = {
			{ 0,1,2,3,4,5,6,15,15 },
			{ 8,9,10,11,12,13,14,15,15 }
			//15 is for 8 bits
		};
		static const unsigned bit_count_0[16] = { 0,1,2,3,4,5,6,8,0,1,2,3,4,5,6,8 };


		struct PackBits
		{
			hse_vector _mins, types, _bits, use_rle;
			unsigned char all_same;
			unsigned char all_raw;
			unsigned full_size;
			unsigned char count_8;
			bool has_rle;
			std::uint16_t rle_masks[16];
			std::uint8_t rle_pop_cnt[16];
		};

		struct BlockEncoder
		{
			hse_array_type* arrays;
			hse_array_type* tr;
			PackBits *packs;			
			char* firsts; //first value for each BPP
		};

		/// @brief Initialize block encoder for given BPP
		static inline void initBlockEncoder(void* buffer, unsigned BPP, BlockEncoder* e)
		{
			e->arrays = reinterpret_cast<hse_array_type*>(buffer);
			e->tr = reinterpret_cast<hse_array_type*>(static_cast<char*>(buffer) + 256 * BPP);
			e->packs = reinterpret_cast<PackBits*>(static_cast<char*>(buffer) + 256 * BPP + 256);
			e->firsts = (static_cast<char*>(buffer) + 256 * BPP + 256 + sizeof(PackBits) * BPP);
		}

		/// @brief Size of internal compression buffer for given BPP
		static inline unsigned compressionBufferSize(unsigned BPP)
		{
			return 256 * BPP + 256 + sizeof(PackBits) * BPP + BPP;
		}

		/// @brief Bit scan reverse on 16 unsigned bytes
		static inline __m128i bitScanReverse8(__m128i v) {
			const __m128i lut_lo = _mm_set_epi8(4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8);
			const __m128i lut_hi = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 8);
			//const __m128i lut_hi = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 3, 8); //custom version that convert output 7 to 8
			__m128i t = _mm_and_si128(_mm_srli_epi16(v, 4), _mm_set1_epi8(0x0F));
			t = _mm_shuffle_epi8(lut_hi, t);
			v = _mm_shuffle_epi8(lut_lo, v);
			v = _mm_min_epu8(v, t);
			v = _mm_sub_epi8(_mm_set1_epi8(8), v);
			return v;
		}

		/// @brief Compute rle mask and size for given row
		static SEQ_ALWAYS_INLINE void compute_rle_row(PackBits* p, unsigned index, const __m128i row, const __m128i prev_row)
		{
			__m128i shift = _mm_slli_si128(row, 1);
			shift = _mm_or_si128(shift, _mm_srli_si128(prev_row, 15));
			__m128i diff = _mm_sub_epi8(row, shift);
			__m128i compare = _mm_cmpeq_epi8(diff, _mm_setzero_si128());
			p->rle_masks[index] = static_cast<std::uint16_t>(_mm_movemask_epi8(compare));
			p->rle_pop_cnt[index] = static_cast<std::uint8_t>(16 - popcnt16(p->rle_masks[index]));
		}

		/// @brief Write rle encoded row (compute_rle_row musty be called before)
		static inline unsigned char* write_rle(PackBits* p, unsigned char* dst, unsigned i, __m128i row)
		{
			__m128i values = _mm_shuffle_epi8(row, _mm_load_si128(reinterpret_cast<const __m128i*>(get_shuffle_table()) + p->rle_masks[i]));
			write_LE_16(dst, p->rle_masks[i]); dst += 2;

			_mm_storeu_si128(reinterpret_cast<__m128i*>(dst), values);
			return dst + p->rle_pop_cnt[i];
		}

		/// @brief Horizontal sum of 16 unsigned bytes
		static inline uint32_t hsum_epu8(__m128i v)
		{
			__m128i vsum = _mm_sad_epu8(v, _mm_setzero_si128());
			return static_cast<uint32_t>( _mm_extract_epi16(vsum, 0) + _mm_extract_epi16(vsum, 4));
		}

		/// @brief Multiplication of 16 bytes
		static inline __m128i mullo_epi8(__m128i a, __m128i b)
		{
			// https://stackoverflow.com/questions/8193601/sse-multiplication-16-x-uint8-t
			
			// unpack and multiply
			__m128i dst_even = _mm_mullo_epi16(a, b);
			__m128i dst_odd = _mm_mullo_epi16(_mm_srli_epi16(a, 8), _mm_srli_epi16(b, 8));
			// repack
#ifdef __AVX2__
	// only faster if have access to VPBROADCASTW
			return _mm_or_si128(_mm_slli_epi16(dst_odd, 8), _mm_and_si128(dst_even, _mm_set1_epi16(0xFF)));
#else
			return _mm_or_si128(_mm_slli_epi16(dst_odd, 8), _mm_srli_epi16(_mm_slli_epi16(dst_even, 8), 8));
#endif
		}



		static inline __m128i min_epi8(__m128i a, __m128i b)
		{
//#ifdef __SSE4_1__
			return _mm_min_epi8(a, b);
/*#else
			// simulate _mm_min_epi8 with SSE3 at most
			__m128i _a = _mm_add_epi8(a, _mm_set1_epi8(static_cast<char>(-128)));
			__m128i _b = _mm_add_epi8(b, _mm_set1_epi8(static_cast<char>(-128)));
			__m128i r = _mm_min_epu8(_a, _b);
			return _mm_sub_epi8(r, _mm_set1_epi8(static_cast<char>(-128)));
#endif*/
		}

		static inline __m128i max_epi8(__m128i a, __m128i b)
		{
//#ifdef __SSE4_1__
			return _mm_max_epi8(a, b);
/*#else
			// simulate _mm_max_epi8 with SSE3 at most
			__m128i _a = _mm_add_epi8(a, _mm_set1_epi8(static_cast<char>(-128)));
			__m128i _b = _mm_add_epi8(b, _mm_set1_epi8(static_cast<char>(-128)));
			__m128i r = _mm_max_epu8(_a, _b);
			return _mm_sub_epi8(r, _mm_set1_epi8(static_cast<char>(-128)));
#endif*/
		}



		/// @brief Process block of 256 elements and find the best compression method for each row of 16 elements: bit packing, delta coding or rle.
		static inline unsigned findPackBitsParams(hse_vector* src, hse_vector* trs, unsigned char first, PackBits* pack, unsigned level, unsigned acceleration)
		{
			//static const __m128i mask_not_t0 = _mm_setr_epi8(static_cast<char>(0xFF), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

			__m128i sub, min_sub, max_sub, t0, bits0, bits1;
			__m128i tr_row = __get(trs[0]);
			__m128i max = tr_row, min = max;
			__m128i first_val = _mm_set1_epi8(static_cast<char>(first));
			__m128i tr_prev = tr_row;

			pack->all_same = _mm_movemask_epi8(_mm_cmpeq_epi32(tr_row, first_val)) == 0xFFFF;
			pack->all_raw = 0;

			__m128i start = _mm_slli_si128(__get(trs[15]), 1);// initialize to last row shifted right by one
			min_sub = _mm_sub_epi8(tr_row, start);
			max_sub = min_sub;

			for (int i = 1; i < 16; ++i)
			{
				tr_row = __get(trs[i]);
				if (pack->all_same)
					pack->all_same &= (_mm_movemask_epi8(_mm_cmpeq_epi32(tr_row, first_val)) == 0xFFFF);
				min = min_epi8(min, tr_row);
				max = max_epi8(max, tr_row);
				sub = _mm_sub_epi8(tr_row, tr_prev);
				min_sub = min_epi8(min_sub, sub);
				max_sub = max_epi8(max_sub, sub);
				tr_prev = tr_row;
			}

			if (pack->all_same)
				return 1;

			bits0 = bitScanReverse8(_mm_sub_epi8(max, min));
			bits1 = bitScanReverse8(_mm_sub_epi8(max_sub, min_sub));
			__m128i bits = _mm_min_epu8(bits0, bits1);
			__set(pack->_bits, bits);

			t0 = _mm_cmpeq_epi8(bits0, bits);
			__set(pack->types, _mm_andnot_si128(t0, _mm_set1_epi8(1)));
			__set(pack->_mins, _mm_or_si128(_mm_and_si128(t0, min), _mm_andnot_si128(t0, min_sub)));

			

			if (level != 0 && acceleration != 7)
			{
				// Apply rle depending on acceleration factor
				// Try to favor rle instead of type 1 which is slower to encode

				__m128i prev, row;

				hse_vector check_for_rle;
				_mm_store_si128(reinterpret_cast<__m128i*>( check_for_rle.i8), _mm_cmpgt_epi8(bits, _mm_sub_epi8(_mm_set1_epi8(static_cast<char>(acceleration) + 1), t0)));

				prev = _mm_setzero_si128();
				memset(pack->rle_pop_cnt, 16, sizeof(pack->rle_pop_cnt));

				for (unsigned i = 0; i < 16; ++i) {
					row = __get(src[i]);
					if (SEQ_LIKELY(check_for_rle.i8[i])) {
						compute_rle_row(pack, i, row, prev);
					}
					prev = row;
				}
			}
			else
			{
				// No rle case

				pack->has_rle = false;
				memset(&pack->use_rle, 0, sizeof(pack->use_rle));
				//count number of 8
				unsigned count_8 = seq::popcnt16(static_cast<unsigned short>(_mm_movemask_epi8(_mm_cmpeq_epi8(bits, _mm_set1_epi8(8)))));
				unsigned full_size = (hsum_epu8(bits)) * 2 + 16 + 8 - count_8;
				return full_size;
			}

			// compute size
			__m128i sizes = mullo_epi8(bits, _mm_set1_epi8(2));
			__m128i add = _mm_andnot_si128(_mm_cmpeq_epi8(bits, _mm_set1_epi8(8)), _mm_set1_epi8(1));
			sizes = _mm_add_epi8(sizes, add);

			//rle size
			__m128i rle_size = _mm_add_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pack->rle_pop_cnt)), _mm_set1_epi8(2));
			
			__m128i use_rle = _mm_cmplt_epi8(rle_size, sizes);
			__set(pack->use_rle, use_rle);
			unsigned mmask = static_cast<unsigned>(_mm_movemask_epi8(use_rle));
			pack->has_rle = mmask != 0;

			sizes = min_epi8(sizes, rle_size);
			unsigned sum = hsum_epu8(sizes) + 8;
			return sum;
		}




		static inline const unsigned char* read_16_bits(const unsigned char* src, const unsigned char* end, unsigned char* out, unsigned bits);

		static inline unsigned char* write_16(const unsigned char* v, unsigned char* dst, unsigned char bits)
		{
#ifdef SEQ_DEBUG
			unsigned char* saved = dst;
#endif


#if defined(  __BMI2__) && defined (SEQ_ARCH_64)
			static const std::uint64_t mask[9] = {
				0,
				0x0101010101010101ULL,
				0x0303030303030303ULL,
				0x0707070707070707ULL,
				0x0F0F0F0F0F0F0F0FULL,
				0x1F1F1F1F1F1F1F1FULL,
				0x3F3F3F3F3F3F3F3FULL,
				0x7F7F7F7F7F7F7F7FULL,
				0xFFFFFFFFFFFFFFFFULL,
			};
			uint64_t v1 = _pext_u64(seq::read_LE_64(v), mask[bits]);
			uint64_t v2 = _pext_u64(seq::read_LE_64(v + 8), mask[bits]);
			seq::write_LE_64(dst, v1);
			seq::write_LE_64(dst + bits, v2);
#else

#define _U64(val) static_cast<std::uint64_t>(val)

			switch (bits) {
			case 1:
				dst[0] = v[0] | (v[1] << 1U) | (v[2] << 2U) | (v[3] << 3U) | (v[4] << 4U) | (v[5] << 5U) | (v[6] << 6U) | (v[7] << 7U);
				dst[1] = v[8] | (v[9] << 1U) | (v[10] << 2U) | (v[11] << 3U) | (v[12] << 4U) | (v[13] << 5U) | (v[14] << 6U) | (v[15] << 7U);
				break;
			case 2:
				dst[0] = v[0] | (v[1] << 2U) | (v[2] << 4U) | (v[3] << 6U);
				dst[1] = v[4] | (v[5] << 2U) | (v[6] << 4U) | (v[7] << 6U);
				dst[2] = v[8] | (v[9] << 2U) | (v[10] << 4U) | (v[11] << 6U);
				dst[3] = v[12] | (v[13] << 2U) | (v[14] << 4U) | (v[15] << 6U);
				break;
			case 3:
				seq::write_LE_32(dst, (v[0] | (v[1] << 3U) | (v[2] << 6U) | (v[3] << 9U) | (v[4] << 12U) | (v[5] << 15U) | (v[6] << 18U) | (v[7] << 21U)));
				seq::write_LE_32(dst + 3, (v[8] | (v[9] << 3U) | (v[10] << 6U) | (v[11] << 9U) | (v[12] << 12U) | (v[13] << 15U) | (v[14] << 18U) | (v[15] << 21U)));
				break;
			case 4:
				seq::write_LE_32(dst, (v[0] | (v[1] << 4U) | (v[2] << 8U) | (v[3] << 12U) | (v[4] << 16U) | (v[5] << 20U) | (v[6] << 24U) | (v[7] << 28U)));
				seq::write_LE_32(dst + 4, (v[8] | (v[9] << 4U) | (v[10] << 8U) | (v[11] << 12U) | (v[12] << 16U) | (v[13] << 20U) | (v[14] << 24U) | (v[15] << 28U)));
				break;
			default:
				seq::write_LE_64(dst, v[0] | (v[1] << bits) | (v[2] << bits * 2) | (v[3] << bits * 3)
					| (_U64(v[4]) << bits * 4) | (_U64(v[5]) << bits * 5) | (_U64(v[6]) << bits * 6) | (_U64(v[7]) << bits * 7));
				seq::write_LE_64(dst + bits, v[8] | (_U64(v[9]) << bits) | (_U64(v[10]) << bits * 2) | (_U64(v[11]) << bits * 3)
					| (_U64(v[12]) << bits * 4) | (_U64(v[13]) << bits * 5) | (_U64(v[14]) << bits * 6) | (_U64(v[15]) << bits * 7));

				break;
			}

#undef _U64

#endif

#ifdef SEQ_DEBUG
			unsigned char read_b[16];
			unsigned char vv[16]; memcpy(vv, v, 16);
			read_16_bits(reinterpret_cast<const unsigned char*>(saved), reinterpret_cast<const unsigned char*>(saved) + bits * 2, read_b, bits);
			if (!(read_b[0] == v[0] && read_b[1] == v[1] && read_b[2] == v[2] && read_b[3] == v[3] && read_b[4] == v[4] && read_b[5] == v[5] && read_b[6] == v[6] && read_b[7] == v[7] &&
				read_b[8] == v[8] && read_b[9] == v[9] && read_b[10] == v[10] && read_b[11] == v[11] && read_b[12] == v[12] && read_b[13] == v[13] && read_b[14] == v[14] && read_b[15] == v[15]))
				throw std::runtime_error("");

#endif

			return dst + bits * 2;
		}





		static inline unsigned char* write_raw(const hse_vector* src, unsigned char* dst, unsigned char* end)
		{
			// check dst overflow
			if (SEQ_UNLIKELY((end - dst) < 256))
				return nullptr;
			// direct copy to destination
			for (int i = 0; i < 16; ++i) {
				memcpy(dst, &src[i], 16);
				dst += 16;
			}
			return dst;
		}

		static inline unsigned char* encode16x16_2(const hse_vector* src, unsigned char first, PackBits* pack, unsigned char* dst, unsigned char* end)
		{
			// Note: this function works if (end -dst) > compress_size-16

			//unsigned char* saved_dst = dst;

			if (pack->all_same) {
				// check dst overflow
				if (SEQ_UNLIKELY(end == dst))
					return nullptr;
				// copy first byte to destination
				*dst++ = first;
				return dst;
			}
			
			// max headers + mins size: 8 + 16 bytes
			if (end && ((end - dst) < 24))
				return nullptr;

			//const unsigned char* end_for_raw = end - 16;
			const __m128i mask = _mm_setr_epi8(-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

			// copy headers (by group of 2) and mins to destination
			for (unsigned i = 0; i < 16; i += 2)
			{
				unsigned char h0 =  pack->use_rle.u8[i] ? 7 : header_0[pack->types.u8[i]][pack->_bits.u8[i]];
				unsigned char h1 = pack->use_rle.u8[i+1] ? 7 : header_0[pack->types.u8[i+1]][pack->_bits.u8[i+1]];
				*dst++ = h0 | static_cast<unsigned char>(h1 << 4U);

				//if (SEQ_UNLIKELY(end && dst > end_for_raw))
				//	return write_raw(src, saved_dst, end);

				__m128i row = __get(src[i]);
				if (h0 == 15)
				{
					_mm_storeu_si128(reinterpret_cast<__m128i*>(dst), row);
					dst += 16;
				}
				else if(h0 != 7){
					*dst++ = pack->_mins.u8[i];
					unsigned char bit_cnt = pack->_bits.u8[i];
					if (bit_cnt)
					{
						// type 0 and 1
						hse_vector t;
						__set(t, _mm_sub_epi8(
							pack->types.i8[i] == 0 ?
							row :
							_mm_sub_epi8(row, _mm_or_si128(_mm_slli_si128(row, 1), _mm_and_si128(_mm_set1_epi8(i == 0 ? 0 : src[i - 1].i8[15]), mask)))
							, _mm_set1_epi8(pack->_mins.i8[i])));

						dst = write_16(&t.u8[0], dst, bit_cnt);
					}
				}
				else
					dst = write_rle(pack, dst, i, row);


				//if (SEQ_UNLIKELY(end && dst > end_for_raw))
				//	return write_raw(src, dst, end);

				row = __get(src[i + 1]);
				if (h1 == 15)
				{
					_mm_storeu_si128(reinterpret_cast<__m128i*>(dst), row);
					dst += 16;
				}
				else if(h1 != 7){
					*dst++ = pack->_mins.u8[i + 1];
					unsigned char bit_cnt = pack->_bits.u8[i + 1];
					if (bit_cnt )
					{
						// type 0 and 1
						hse_vector t;
						__set(t, _mm_sub_epi8(
							pack->types.i8[i + 1] == 0 ?
							row :
							_mm_sub_epi8(row, _mm_or_si128(_mm_slli_si128(row, 1), _mm_and_si128(_mm_set1_epi8(src[i].i8[15]), mask)))
							, _mm_set1_epi8(pack->_mins.i8[i + 1])));

						dst = write_16(&t.u8[0], dst, bit_cnt);
					}
				}
				else
					dst = write_rle(pack, dst, i + 1, row);
					
			}

			return dst;
		}




		static inline unsigned computeBlock_generic(BlockEncoder* encoder, char first, unsigned index, unsigned level, unsigned acceleration)
		{
			transpose_16x16(reinterpret_cast<const __m128i*>( & encoder->arrays[index][0]), reinterpret_cast<__m128i*>( & (*encoder->tr[0])));
			return findPackBitsParams(encoder->arrays[index], encoder->tr[0], static_cast<unsigned char>(first), encoder->packs + index, level, acceleration);
		}



		struct CompressedBuffer
		{
			void* buffer;
			size_t size;
			CompressedBuffer() : buffer(nullptr), size(0) {}
			~CompressedBuffer() {
				if (buffer)
					seq::aligned_free(buffer);
			}
		};

	}// end namespace detail




	SEQ_HEADER_ONLY_EXPORT_FUNCTION void* get_comp_buffer(size_t size)
	{
		static thread_local detail::CompressedBuffer buf;
		if (buf.size < size) {
			if (buf.buffer)
				seq::aligned_free(buf.buffer);
			buf.buffer = seq::aligned_malloc(size, 16);
		}
		return buf.buffer;
	}


	/*SEQ_HEADER_ONLY_EXPORT_FUNCTION unsigned block_memory(unsigned BPP, unsigned block_count, unsigned level)
	{
		(void)level;
		return (256 * (BPP)*block_count + 256);
	}*/

	


	static SEQ_ALWAYS_INLINE unsigned block_encode_256_full(const void* __src, unsigned BPP, unsigned block_count, void* __dst, unsigned dst_size, unsigned level, unsigned acceleration)
	{

		void* buff_src = nullptr;
		const char* _src = static_cast<const char*>(__src);
		char* dst = static_cast<char*>(__dst);
		char* dst_end = dst + dst_size;
		//compute minimum size: headers + cst value in case of all same
		unsigned minimum_size = ((BPP >> 1) + (BPP & 1) + BPP) * (block_count) + 1;
		if (acceleration > 7) acceleration = 7;
		static const unsigned diff[8] = { 16,25,45,70,90,110,130,150 };
		unsigned target = 256 - diff[acceleration];
		char* saved = dst;// dst++;//save the compression method on 1 byte

		if (static_cast<size_t>(dst_end - saved) < minimum_size)
			return SEQ_ERROR_DST_OVERFLOW;

		buff_src = get_comp_buffer(detail::compressionBufferSize(BPP));

		detail::BlockEncoder e;
		detail::BlockEncoder* encoder = &e;
		detail::initBlockEncoder(buff_src, BPP, &e);

		for (unsigned bcount = 0; bcount < (block_count); ++bcount)
		{
			const char* src = _src + bcount * BPP * 256;
			unsigned char* anchor = reinterpret_cast<unsigned char*>(dst);
			unsigned offset = 0;
			unsigned i = 0;
			dst += (BPP >> 1) + (BPP & 1);


			//read source
			transpose_256_rows(src, reinterpret_cast<char*>(e.arrays), BPP);

			//copy first value for each BPP
			memcpy(e.firsts, src, BPP);

			for (i = 0; i < BPP; ++i)
			{
				unsigned size = detail::computeBlock_generic(&e, /*src[i]*/e.firsts[i], i, level, acceleration);
				
				if (SEQ_UNLIKELY(dst + size > dst_end)) {
					return SEQ_ERROR_DST_OVERFLOW;
				}
				if (size > target || static_cast<int>(size) > static_cast<int>(dst_end - dst) - 16) {
					encoder->packs[i].all_raw = 1;
					dst = reinterpret_cast<char*>(detail::write_raw(encoder->arrays[i], reinterpret_cast<unsigned char*>(dst), reinterpret_cast<unsigned char*>(dst_end)));
				}
				else {
					dst = reinterpret_cast<char*>(detail::encode16x16_2(encoder->arrays[i], static_cast<unsigned char>(e.firsts[i]), encoder->packs + i, reinterpret_cast<unsigned char*>(dst), reinterpret_cast<unsigned char*>(dst_end)));
				}
				if (SEQ_UNLIKELY(!dst)) {
					return SEQ_ERROR_DST_OVERFLOW;
				}

				unsigned char h;
				if (encoder->packs[i].all_same)		h = __SEQ_BLOCK_ALL_SAME;
				else if (encoder->packs[i].all_raw)	h = __SEQ_BLOCK_ALL_RAW;
				else								h = __SEQ_BLOCK_NORMAL;

				//store header value
				if (offset == 0) *anchor = 0;
				*anchor |= (h << offset);
				offset += 4;
				if (offset == 8) {
					++anchor;
					offset = 0;
				}

			}
		}

		unsigned count = static_cast<unsigned>(dst - saved);
		return count;
	}

	SEQ_HEADER_ONLY_EXPORT_FUNCTION unsigned block_encode_256(const void* src, unsigned BPP, unsigned block_count, void* dst, unsigned dst_size, unsigned acceleration)
	{
		return block_encode_256_full(src, BPP, block_count, dst, dst_size, 1, acceleration);
	}




	/*SEQ_HEADER_ONLY_EXPORT_FUNCTION unsigned block_compute_1_256(const void* __src, unsigned BPP, unsigned dst_size, unsigned acceleration)
	{
		const char* src = (const char*)__src;
		if (acceleration > 7) acceleration = 7;
		static const unsigned diff[8] = { 10,25,45,70,90,110,130,150 };
		unsigned target = 256 - diff[acceleration];
		void* buff_src = buff_src = get_comp_buffer(detail::compressionBufferSize(BPP));

		detail::BlockEncoder e;
		detail::BlockEncoder* encoder = &e;
		detail::initBlockEncoder(buff_src, BPP, &e);
			
		unsigned i = 0;
		unsigned res = 0;

		//read source
		transpose_256_rows(src, (char*)e.arrays, BPP);

		//copy first value for each BPP
		memcpy(e.firsts, src, BPP);

		for (i = 0; i < BPP; ++i)
		{
			unsigned size = detail::computeBlock_generic(&e, e.firsts[i], i, 1, acceleration);
			if (size > target) {
				encoder->packs[i].all_raw = 1;
				size = 256;
			}
			res += size;
			if (res > dst_size)
				return SEQ_ERROR_DST_OVERFLOW;
		}
		return res + (BPP >> 1) + (BPP & 1);
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION unsigned block_encode_1_256(const void* __src, unsigned BPP, void* __dst, unsigned dst_size)
	{
		void* buff_src = NULL;
		const char* src = (const char*)__src;
		char* dst = (char*)__dst;
		char* dst_end = dst + dst_size;
		char* saved = dst;// dst++;//save the compression method on 1 byte

		buff_src = get_comp_buffer(0);

		detail::BlockEncoder e;
		detail::BlockEncoder* encoder = &e;
		detail::initBlockEncoder(buff_src, BPP, &e);
		
		unsigned char* anchor = (unsigned char*)dst;
		unsigned offset = 0;
		unsigned i = 0;
		dst += (BPP >> 1) + (BPP & 1);

		for (i = 0; i < BPP; ++i)
		{
			if (encoder->packs[i].all_raw) {
				dst = (char*)detail::write_raw(encoder->arrays[i], (unsigned char*)dst, (unsigned char*)dst_end);
			}
			else {
				dst = (char*)detail::encode16x16_2(encoder->arrays[i], e.firsts[i], encoder->packs + i, (unsigned char*)dst, NULL);
			}
			if (SEQ_UNLIKELY(!dst))
				return SEQ_ERROR_DST_OVERFLOW;

			unsigned char h;
			if (encoder->packs[i].all_same)		h = __SEQ_BLOCK_ALL_SAME;
			else if (encoder->packs[i].all_raw)	h = __SEQ_BLOCK_ALL_RAW;
			else								h = __SEQ_BLOCK_NORMAL;

			//store header value
			if (offset == 0) *anchor = 0;
			*anchor |= (h << offset);
			offset += 4;
			if (offset == 8) {
				++anchor;
				offset = 0;
			}
		}
		unsigned count = static_cast<unsigned>(dst - saved);
		return count;
	}
	*/

	namespace detail
	{

#define _UCH static_cast<unsigned char>

		static inline const unsigned char* read_16_bits_slow(const unsigned char* src, const unsigned char*, unsigned char* out, unsigned bits)
		{
			if (bits == 1)
			{
				unsigned tmp = *src;
				out[0] = _UCH(tmp & 0x1U); out[1] = _UCH((tmp >> 1U) & 0x1U); out[2] = _UCH((tmp >> 2U) & 0x1U); out[3] = _UCH((tmp >> 3U) & 0x1U);
				out[4] = _UCH((tmp >> 4U) & 0x1U); out[5] = _UCH((tmp >> 5U) & 0x1U); out[6] = _UCH((tmp >> 6U) & 0x1U); out[7] = _UCH(tmp >> 7U);
				tmp = *++src;
				out += 8;
				out[0] = _UCH(tmp & 0x1U); out[1] = _UCH((tmp >> 1U) & 0x1U); out[2] = _UCH((tmp >> 2U) & 0x1U); out[3] = _UCH((tmp >> 3U) & 0x1U);
				out[4] = _UCH((tmp >> 4U) & 0x1U); out[5] = _UCH((tmp >> 5U) & 0x1U); out[6] = _UCH((tmp >> 6U) & 0x1U); out[7] = _UCH(tmp >> 7U);
			}
			else if (bits == 2)
			{
				out[0] = _UCH(*src & 0x3); out[1] = _UCH((*src >> 2) & 0x3); out[2] = _UCH((*src >> 4) & 0x3); out[3] = _UCH((*src >> 6));
				out[4] = _UCH(src[1] & 0x3); out[5] = _UCH((src[1] >> 2) & 0x3); out[6] = _UCH((src[1] >> 4) & 0x3); out[7] = _UCH((src[1] >> 6));
				src += 2;// bits;
				out += 8;
				out[0] = _UCH(*src & 0x3); out[1] = _UCH((*src >> 2) & 0x3); out[2] = _UCH((*src >> 4) & 0x3); out[3] = _UCH((*src >> 6));
				out[4] = _UCH(src[1] & 0x3); out[5] = _UCH((src[1] >> 2) & 0x3); out[6] = _UCH((src[1] >> 4) & 0x3); out[7] = _UCH((src[1] >> 6));
			}
			else if (bits == 3)
			{
				unsigned r = seq::read_LE_32(src);
				out[0] = _UCH(r & 0x7); out[1] = _UCH((r >> 3) & 0x7); out[2] = _UCH((r >> 6) & 0x7); out[3] = _UCH((r >> 9) & 0x7);
				out[4] = _UCH((r >> 12) & 0x7); out[5] = _UCH((r >> 15) & 0x7); out[6] = _UCH((r >> 18) & 0x7); out[7] = _UCH((r >> 21) & 0x7);
				src += 3;// bits;
				out += 8;
				r = seq::read_LE_32(src);
				out[0] = _UCH(r & 0x7); out[1] = _UCH((r >> 3) & 0x7); out[2] = _UCH((r >> 6) & 0x7); out[3] = _UCH((r >> 9) & 0x7);
				out[4] = _UCH((r >> 12) & 0x7); out[5] = _UCH((r >> 15) & 0x7); out[6] = _UCH((r >> 18) & 0x7); out[7] = _UCH((r >> 21) & 0x7);
			}
			else if (bits == 4)
			{

				unsigned r1 = seq::read_LE_32(src);
				src += 4;// bits;
				unsigned r2 = seq::read_LE_32(src);

				out[0] = _UCH(r1 & 0xFU); out[1] = _UCH((r1 >> 4U) & 0xFU); out[2] = _UCH((r1 >> 8U) & 0xFU); out[3] = _UCH((r1 >> 12U) & 0xFU);
				out[4] = _UCH((r1 >> 16U) & 0xFU); out[5] = _UCH((r1 >> 20U) & 0xFU); out[6] = _UCH((r1 >> 24U) & 0xFU); out[7] = _UCH(r1 >> 28U);
				out[8] = _UCH(r2 & 0xFU); out[9] = _UCH((r2 >> 4U) & 0xFU); out[10] = _UCH((r2 >> 8U) & 0xFU); out[11] = _UCH((r2 >> 12U) & 0xFU);
				out[12] = _UCH((r2 >> 16U) & 0xFU); out[13] = _UCH((r2 >> 20U) & 0xFU); out[14] = _UCH((r2 >> 24U) & 0xFU); out[15] = _UCH(r2 >> 28U);
			}
			else
			{

				std::uint64_t r1 = seq::read_LE_64(src);
				src += bits;
				std::uint64_t r2 = seq::read_LE_64(src);
				std::uint64_t mask = (1ull << static_cast<std::uint64_t>(bits)) - 1ull;
				std::uint64_t _bits = bits;
				out[0] = _UCH(r1 & mask); out[1] = _UCH((r1 >> _bits) & mask); out[2] = _UCH((r1 >> _bits * 2ULL) & mask); out[3] = _UCH((r1 >> _bits * 3ULL) & mask);
				out[4] = _UCH((r1 >> _bits * 4ULL) & mask); out[5] = _UCH((r1 >> _bits * 5ULL) & mask); out[6] = _UCH((r1 >> _bits * 6ULL) & mask); out[7] = _UCH((r1 >> _bits * 7ULL) & mask);
				out[8] = _UCH(r2 & mask); out[9] = _UCH((r2 >> _bits) & mask); out[10] = _UCH((r2 >> _bits * 2ULL) & mask); out[11] = _UCH((r2 >> _bits * 3ULL) & mask);
				out[12] = _UCH((r2 >> _bits * 4ULL) & mask); out[13] = _UCH((r2 >> _bits * 5ULL) & mask); out[14] = _UCH((r2 >> _bits * 6ULL) & mask); out[15] = _UCH((r2 >> _bits * 7ULL) & mask);

			}

			return src + bits;
		}



		static inline const unsigned char* read_16_bits(const unsigned char* src, const unsigned char* end, unsigned char* out, unsigned bits)
		{

#if defined(  __BMI2__) && defined(SEQ_ARCH_64)
			(void)end;
			static const std::uint64_t mask[9] = {
				0,
				0x0101010101010101ULL,
				0x0303030303030303ULL,
				0x0707070707070707ULL,
				0x0F0F0F0F0F0F0F0FULL,
				0x1F1F1F1F1F1F1F1FULL,
				0x3F3F3F3F3F3F3F3FULL,
				0x7F7F7F7F7F7F7F7FULL,
				0xFFFFFFFFFFFFFFFFULL,
			};
			uint64_t v1 = _pdep_u64(seq::read_LE_64(src), mask[bits]);
			uint64_t v2 = _pdep_u64(seq::read_LE_64(src + bits), mask[bits]);
			seq::write_LE_64(out, v1);
			seq::write_LE_64(out + 8, v2);
			return src + bits * 2;

#else
			return read_16_bits_slow(src, end, out, bits);
#endif
		}

		static inline void fast_copy_stridded_0_16(unsigned char* SEQ_RESTRICT dst, const unsigned char* SEQ_RESTRICT src, unsigned  stride)
		{
			*dst = src[0]; dst += stride;
			*dst = src[1]; dst += stride;
			*dst = src[2]; dst += stride;
			*dst = src[3]; dst += stride;
			*dst = src[4]; dst += stride;
			*dst = src[5]; dst += stride;
			*dst = src[6]; dst += stride;
			*dst = src[7]; dst += stride;
			*dst = src[8]; dst += stride;
			*dst = src[9]; dst += stride;
			*dst = src[10]; dst += stride;
			*dst = src[11]; dst += stride;
			*dst = src[12]; dst += stride;
			*dst = src[13]; dst += stride;
			*dst = src[14]; dst += stride;
			*dst = src[15];
		}
		static inline void fast_copy_stridded_16(unsigned char* SEQ_RESTRICT dst,  unsigned char* SEQ_RESTRICT src, unsigned char offset, unsigned  stride)
		{
			_mm_store_si128(reinterpret_cast<__m128i*>(src), _mm_add_epi8(_mm_load_si128(reinterpret_cast<const __m128i*>(src)), _mm_set1_epi8(static_cast<char>(offset))));
			fast_copy_stridded_0_16(dst, src, stride);

		}
		static inline void fast_memset_stridded_16(unsigned char* dst, unsigned char val, unsigned  stride)
		{
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val; dst += stride;
			*dst = val;

		}
		static inline void fast_copyleft_stridded_16(unsigned char* SEQ_RESTRICT dst, const unsigned char* SEQ_RESTRICT src, unsigned char first, unsigned char offset, unsigned  inner)
		{
			unsigned pos = 0;
			dst[0] = src[0] + first + offset;

			dst[pos + inner] = src[1] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[2] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[3] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[4] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[5] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[6] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[7] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[8] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[9] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[10] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[11] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[12] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[13] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[14] + dst[pos] + offset; pos += inner;
			dst[pos + inner] = src[15] + dst[pos] + offset; pos += inner;

		}


		static inline void fast_copyleft_inner_stridded_16(unsigned char* dst, unsigned char first, unsigned char min, unsigned  inner)
		{
			dst[0] = first + min;
			unsigned pos = 0;

			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
			dst[pos + inner] = dst[pos] + min; pos += inner;
		}

		static inline const unsigned char* decode_RAW(const unsigned char* src, unsigned char* dst, unsigned  inner_stride, unsigned  outer_stride, const unsigned char* end)
		{
			// check for src overflow
			if (SEQ_UNLIKELY((end - src) < 256))
				return nullptr;

			for (unsigned i = 0; i < 16; ++i)
			{
				fast_copy_stridded_0_16((dst + i * outer_stride), src, inner_stride);
				src += 16;
			}
			return src;
		}

		static inline const unsigned char* decode_SAME(const unsigned char* SEQ_RESTRICT src, unsigned char* SEQ_RESTRICT dst, unsigned inner_stride, unsigned outer_stride, const unsigned char* end)
		{
			unsigned char same;
			unsigned y;

			// check overflow
			if (SEQ_UNLIKELY(src >= end))
				return nullptr;

			// set the block to the same unique value
			same = *src++;

			for (y = 0; y < 16; ++y) {
				unsigned char* out = dst + y * outer_stride;
				fast_memset_stridded_16(out, same, inner_stride);
			}
			return src;
		}

		static inline const unsigned char* decode_rle(const unsigned char* src, const unsigned char* end, unsigned char* dst, unsigned char prev, unsigned  inner)
		{
			//++_decode_rle;

			unsigned remaining = static_cast<unsigned>(end - src);
			if (SEQ_UNLIKELY(remaining < 2))
				return nullptr;

			std::uint16_t mask = read_LE_16(src);
			src += 2;
			remaining -= 2;

			unsigned size = 16 - popcnt16(mask);
			if (SEQ_UNLIKELY(size > remaining))
				return nullptr;

			hse_vector buff;
			__m128i __src;
			if (SEQ_UNLIKELY(remaining < 16)) {
				memcpy(&buff, src, remaining);
				__src = _mm_load_si128(reinterpret_cast<const __m128i*>( & buff));
			}
			else
				__src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));

			if (mask & 1)
				// the row starts with a repetition from previous row
				__src = _mm_or_si128(_mm_slli_si128(__src, 1), _mm_srli_si128(_mm_set1_epi8(static_cast<char>(prev)), 15));

			_mm_store_si128(reinterpret_cast<__m128i*>( & buff), _mm_shuffle_epi8(__src, *(reinterpret_cast<const __m128i*>(get_unshuffle_table()) + mask)));

			//copy to dst
			fast_copy_stridded_0_16(dst, buff.u8, inner);
			
			return src + size;
		}


		static inline const unsigned char* decodeBlock(const unsigned char* src,
			unsigned char* dst, unsigned inner_stride, unsigned outer_stride, const unsigned char* end)
		{
			unsigned  cnt;
			unsigned char min;
			alignas(16) unsigned char column[16];
			unsigned char headers[16];

#ifdef __BMI2__
			const unsigned char* end_fast = end - 13;
#endif

			// decode rows
			for (unsigned i = 0; i < 16; i += 2)
			{
				// check overflow
				if (SEQ_UNLIKELY(src >= end))
					return nullptr;

				headers[i] = *src & 0xF;
				headers[i + 1] = *src >> 4;
				++src;

				for (unsigned x = i; x < i + 2; ++x)
				{
					if (headers[x] == 7) {
						// rle
						src = decode_rle(src, end, dst + x * outer_stride, x == 0 ? 0 : dst[(x - 1) * outer_stride + 15 * inner_stride], inner_stride);
						if (SEQ_UNLIKELY(!src))
							return nullptr;
					}
					else if (headers[x] == 15) {
						// raw row
						// check overflow
						if (SEQ_UNLIKELY(end - src < 16))
							return nullptr;
						fast_copy_stridded_0_16(dst + x * outer_stride, src, inner_stride);
						src += 16;
					}
					else {

						cnt = bit_count_0[headers[x]];
						// read compressed column
						if (cnt > 0) {
							// check overflow
							if (SEQ_UNLIKELY(end < src + cnt * 2 + 1))
								return nullptr;

							min = *src++;
#ifdef __BMI2__
							if (SEQ_LIKELY(src < end_fast))
								src = read_16_bits(src, end, column, cnt);
							else
#endif
								src = read_16_bits_slow(src, end, column, cnt);

							if (headers[x] < 8) // type 0
								fast_copy_stridded_16(dst + x * outer_stride, column, min, inner_stride);
							else // type 1
								fast_copyleft_stridded_16(dst + x * outer_stride, column, x == 0 ? 0 : dst[(x - 1) * outer_stride + 15 * inner_stride], min, inner_stride);
						}
						else {
							if (SEQ_UNLIKELY(src >= end))
								return nullptr;

							min = *src++;
							if (headers[x] < 8) // type 0
								fast_memset_stridded_16(dst + x * outer_stride, min, inner_stride);
							else  // type 1
								fast_copyleft_inner_stridded_16(dst + x * outer_stride, x == 0 ? 0 : dst[(x - 1) * outer_stride + 15 * inner_stride], min, inner_stride);

						}
					}
				}
			}



			return src;
		}

	}//end namespace detail


	SEQ_HEADER_ONLY_EXPORT_FUNCTION unsigned block_bound(unsigned BPP)
	{
		return 256 * BPP + BPP + (BPP / 2 + (BPP & 1));
	}


	SEQ_HEADER_ONLY_EXPORT_FUNCTION unsigned block_decode_256(const void* SEQ_RESTRICT _src, unsigned size, unsigned BPP, unsigned block_count, void* SEQ_RESTRICT _dst)
	{
		const unsigned char* src = reinterpret_cast<const unsigned char*>(_src);
		const unsigned char* saved = src;
		const unsigned char* end = src + size;
		const unsigned header_len = (BPP >> 1) + (BPP & 1);
		unsigned outer_stride = BPP * 16;
		unsigned inner_stride = BPP;

		// check for minimum size
		if (SEQ_UNLIKELY(size < ((BPP / 2 + BPP % 2) + BPP)))
			return SEQ_ERROR_SRC_OVERFLOW;

		for (unsigned bcount = 0; bcount < block_count; ++bcount)
		{
			unsigned char* dst = reinterpret_cast<unsigned char*>(_dst) + bcount * 256 * BPP;
			const unsigned char* anchor = (src);
			src += header_len;

			// Check for the same value repeated 256 times
			unsigned j;
			for (j = 0; j < header_len; ++j) {
				if (anchor[j] != __SEQ_BLOCK_ALL_SAME)
					break;
			}
			if (j == header_len) {
				//decode all same
				for (unsigned i = 0; i < 256; ++i) {
					unsigned char* d = dst + i * BPP;
					for (unsigned b = 0; b < BPP; ++b)
						*d++ = src[b];
				}
				return static_cast<unsigned>(src + BPP - saved);
			}

			for (unsigned i = 0; i < BPP; ++i)
			{
				unsigned char block_header = (anchor[i >> 1] >> (4 * (i & 1))) & 0xF;

				switch (block_header)
				{
				case __SEQ_BLOCK_ALL_RAW:
					src = detail::decode_RAW(src, dst + i, inner_stride, outer_stride, end);
					break;
				case __SEQ_BLOCK_ALL_SAME:
					src = detail::decode_SAME(src, dst + i, inner_stride, outer_stride, end);
					break;
				case __SEQ_BLOCK_NORMAL:
					src = detail::decodeBlock(src, dst + i, inner_stride, outer_stride, end);
					break;
				default:
					return SEQ_ERROR_CORRUPTED_DATA;
					break;
				}

			}
		}

		return  static_cast<unsigned>(src - saved);

	}


}// end namespace seq


#endif //__SSE4_1__
