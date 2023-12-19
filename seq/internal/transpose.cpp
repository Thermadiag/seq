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



#include "transpose.h"

#ifdef __SSE4_1__

namespace seq
{

	//Transposition
	//http://pzemtsov.github.io/2014/10/01/how-to-transpose-a-16x16-matrix.html
	//https://github.com/pzemtsov/article-e1-cache/blob/master/sse.h

	/** Combine together four fields of 2 bits each, in lower to high order.
	* Used in 128 and 256 bits shuffles and permutations
	* @param n0 constant integer value of size 2 bits (not checked)
	* @param n1 constant integer value of size 2 bits (not checked)
	* @param n2 constant integer value of size 2 bits (not checked)
	* @param n3 constant integer value of size 2 bits (not checked) (guys, was it really so necessary to write these comments?)
	* @return combined 8-bit value where lower 2 bits contain n0 and high 2 bits contain n3 (format used by __mm_shuffle_ps/SHUFPS)
	*/
#define combine_4_2bits(n0, n1, n2, n3) (n0 + (n1<<2) + (n2<<4) + (n3<<6))
	// ------ General shuffles and permutations

	/** shuffles two 128-bit registers according to four 2-bit constants defining positions.
	* @param x   A0    A1    A2    A3    (each element a 32-bit float)
	* @param y   C0    C1    C2    C3    (each element a 32-bit float)
	* @return    A[n0] A[n1] C[n2] C[n3]
	* Note that positions 0, 1 are only filled with data from x, positions 2, 3 only with data from y.
	* Components of a single vector can be shuffled in any order by using this function with x and inself
	* (see __mm_shuffle_ps intrinsic and SHUFPS instruction)
	*/
#define _128_shuffle(x, y, n0, n1, n2, n3) _mm_shuffle_ps(x, y, combine_4_2bits (n0, n1, n2, n3))
	/** shuffles two 128-bit integer registers according to four 2-bit constants defining positions.
	* @param x   A0    A1    A2    A3    (each element a 32-bit float)
	* @param y   C0    C1    C2    C3    (each element a 32-bit float)
	* @return    A[n0] A[n1] C[n2] C[n3]
	* Note that positions 0, 1 are only filled with data from x, positions 2, 3 only with data from y.
	* Components of a single vector can be shuffled in any order by using this function with x and inself
	* (see __mm_shuffle_ps intrinsic and SHUFPS instruction)
	*/
#define _128i_shuffle(x, y, n0, n1, n2, n3) _mm_castps_si128(_128_shuffle(_mm_castsi128_ps(x), _mm_castsi128_ps(y), n0, n1, n2, n3))

	static inline __m128i transpose_4x4(__m128i m)
	{
		return _mm_shuffle_epi8(m, _mm_setr_epi8(0, 4, 8, 12,
			1, 5, 9, 13,
			2, 6, 10, 14,
			3, 7, 11, 15));
	}
	static inline void transpose_4x4_dwords(const __m128i& w0, const __m128i& w1,
		__m128i const& w2, const __m128i& w3,
		__m128i& r0, __m128i& r1,
		__m128i& r2, __m128i& r3)
	{
		// 0  1  2  3
		// 4  5  6  7
		// 8  9  10 11
		// 12 13 14 15

		__m128i x0 = _128i_shuffle(w0, w1, 0, 1, 0, 1); // 0 1 4 5
		__m128i x1 = _128i_shuffle(w0, w1, 2, 3, 2, 3); // 2 3 6 7
		__m128i x2 = _128i_shuffle(w2, w3, 0, 1, 0, 1); // 8 9 12 13
		__m128i x3 = _128i_shuffle(w2, w3, 2, 3, 2, 3); // 10 11 14 15

		_mm_store_si128(&r0, _128i_shuffle(x0, x2, 0, 2, 0, 2));
		_mm_store_si128(&r1, _128i_shuffle(x0, x2, 1, 3, 1, 3));
		_mm_store_si128(&r2, _128i_shuffle(x1, x3, 0, 2, 0, 2));
		_mm_store_si128(&r3, _128i_shuffle(x1, x3, 1, 3, 1, 3));
	}
	/*static inline void transpose_16x16(
		__m128i& x0, __m128i& x1, __m128i& x2, __m128i& x3,
		__m128i& x4, __m128i& x5, __m128i& x6, __m128i& x7,
		__m128i& x8, __m128i& x9, __m128i& x10, __m128i& x11,
		__m128i& x12, __m128i& x13, __m128i& x14, __m128i& x15)
	{
		__m128i w00, w01, w02, w03;
		__m128i w10, w11, w12, w13;
		__m128i w20, w21, w22, w23;
		__m128i w30, w31, w32, w33;

		transpose_4x4_dwords(x0, x1, x2, x3, w00, w01, w02, w03);
		transpose_4x4_dwords(x4, x5, x6, x7, w10, w11, w12, w13);
		transpose_4x4_dwords(x8, x9, x10, x11, w20, w21, w22, w23);
		transpose_4x4_dwords(x12, x13, x14, x15, w30, w31, w32, w33);
		w00 = transpose_4x4(w00);
		w01 = transpose_4x4(w01);
		w02 = transpose_4x4(w02);
		w03 = transpose_4x4(w03);
		w10 = transpose_4x4(w10);
		w11 = transpose_4x4(w11);
		w12 = transpose_4x4(w12);
		w13 = transpose_4x4(w13);
		w20 = transpose_4x4(w20);
		w21 = transpose_4x4(w21);
		w22 = transpose_4x4(w22);
		w23 = transpose_4x4(w23);
		w30 = transpose_4x4(w30);
		w31 = transpose_4x4(w31);
		w32 = transpose_4x4(w32);
		w33 = transpose_4x4(w33);
		transpose_4x4_dwords(w00, w10, w20, w30, x0, x1, x2, x3);
		transpose_4x4_dwords(w01, w11, w21, w31, x4, x5, x6, x7);
		transpose_4x4_dwords(w02, w12, w22, w32, x8, x9, x10, x11);
		transpose_4x4_dwords(w03, w13, w23, w33, x12, x13, x14, x15);
	}*/

	SEQ_HEADER_ONLY_EXPORT_FUNCTION void transpose_16x16(const __m128i* in, __m128i* out)
	{
		__m128i w00, w01, w02, w03;
		__m128i w10, w11, w12, w13;
		__m128i w20, w21, w22, w23;
		__m128i w30, w31, w32, w33;

		transpose_4x4_dwords(in[0], in[1], in[2], in[3], w00, w01, w02, w03);
		transpose_4x4_dwords(in[4], in[5], in[6], in[7], w10, w11, w12, w13);
		transpose_4x4_dwords(in[8], in[9], in[10], in[11], w20, w21, w22, w23);
		transpose_4x4_dwords(in[12], in[13], in[14], in[15], w30, w31, w32, w33);
		w00 = transpose_4x4(w00);
		w01 = transpose_4x4(w01);
		w02 = transpose_4x4(w02);
		w03 = transpose_4x4(w03);
		w10 = transpose_4x4(w10);
		w11 = transpose_4x4(w11);
		w12 = transpose_4x4(w12);
		w13 = transpose_4x4(w13);
		w20 = transpose_4x4(w20);
		w21 = transpose_4x4(w21);
		w22 = transpose_4x4(w22);
		w23 = transpose_4x4(w23);
		w30 = transpose_4x4(w30);
		w31 = transpose_4x4(w31);
		w32 = transpose_4x4(w32);
		w33 = transpose_4x4(w33);
		transpose_4x4_dwords(w00, w10, w20, w30, out[0], out[1], out[2], out[3]);
		transpose_4x4_dwords(w01, w11, w21, w31, out[4], out[5], out[6], out[7]);
		transpose_4x4_dwords(w02, w12, w22, w32, out[8], out[9], out[10], out[11]);
		transpose_4x4_dwords(w03, w13, w23, w33, out[12], out[13], out[14], out[15]);
	}





	namespace detail
	{
		static SEQ_ALWAYS_INLINE __m128i setr_epi8(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15)
		{
			return _mm_setr_epi8(static_cast<char>(a0), static_cast<char>(a1), static_cast<char>(a2), static_cast<char>(a3), static_cast<char>(a4), static_cast<char>(a5), static_cast<char>(a6), static_cast<char>(a7), static_cast<char>(a8), static_cast<char>(a9), static_cast<char>(a10), static_cast<char>(a11), static_cast<char>(a12), static_cast<char>(a13), static_cast<char>(a14), static_cast<char>(a15));
		}
		static SEQ_ALWAYS_INLINE void extract1Byte(const char* src, hse_array_type* out_arrays)
		{
			memcpy(out_arrays, src, 256);
		}
		
		static inline void  extract2Bytes_sse3(const char* src, hse_array_type* out_arrays)
		{
			static const __m128i sh0 = setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			static const __m128i sh1 = setr_epi8(1, 3, 5, 7, 9, 11, 13, 15, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			
			for (int yy = 0; yy < 16; ++yy)
			{
				const char* row = src + yy * 32;
				__m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(row));
				__m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(row + 16));

				__m128i val0 = _mm_shuffle_epi8(v0, sh0);
				val0 = _mm_or_si128(val0, _mm_slli_si128(_mm_shuffle_epi8(v1, sh0), 8));

				__m128i val1 = _mm_shuffle_epi8(v0, sh1);
				val1 = _mm_or_si128(val1, _mm_slli_si128(_mm_shuffle_epi8(v1, sh1), 8));

				_mm_store_si128(reinterpret_cast<__m128i*>(out_arrays[0][yy].i8), val0);
				_mm_store_si128(reinterpret_cast<__m128i*>(out_arrays[1][yy].i8), val1);
			}
		}
		
		static inline void  extract4Bytes_sse3(const char* src, hse_array_type* out_arrays)
		{
			static const __m128i sh0 = setr_epi8(0, 4, 8, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			static const __m128i sh1 = setr_epi8(1, 5, 9, 13, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			static const __m128i sh2 = setr_epi8(2, 6, 10, 14, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			static const __m128i sh3 = setr_epi8(3, 7, 11, 15, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			

			for (int yy = 0; yy < 16; ++yy)
			{
				const char* row = src + yy * 64;
				__m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(row));
				__m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(row + 16));
				__m128i v2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(row + 32));
				__m128i v3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(row + 48));

				__m128i val0 = _mm_shuffle_epi8(v0, sh0);
				val0 = _mm_or_si128( val0 , _mm_slli_si128( _mm_shuffle_epi8(v1, sh0), 4));
				val0 = _mm_or_si128(val0, _mm_slli_si128(_mm_shuffle_epi8(v2, sh0), 8));
				val0 = _mm_or_si128(val0, _mm_slli_si128(_mm_shuffle_epi8(v3, sh0), 12));

				__m128i val1 = _mm_shuffle_epi8(v0, sh1);
				val1 = _mm_or_si128(val1, _mm_slli_si128(_mm_shuffle_epi8(v1, sh1), 4));
				val1 = _mm_or_si128(val1, _mm_slli_si128(_mm_shuffle_epi8(v2, sh1), 8));
				val1 = _mm_or_si128(val1, _mm_slli_si128(_mm_shuffle_epi8(v3, sh1), 12));

				__m128i val2 = _mm_shuffle_epi8(v0, sh2);
				val2 = _mm_or_si128(val2, _mm_slli_si128(_mm_shuffle_epi8(v1, sh2), 4));
				val2 = _mm_or_si128(val2, _mm_slli_si128(_mm_shuffle_epi8(v2, sh2), 8));
				val2 = _mm_or_si128(val2, _mm_slli_si128(_mm_shuffle_epi8(v3, sh2), 12));

				__m128i val3 = _mm_shuffle_epi8(v0, sh3);
				val3 = _mm_or_si128(val3, _mm_slli_si128(_mm_shuffle_epi8(v1, sh3), 4));
				val3 = _mm_or_si128(val3, _mm_slli_si128(_mm_shuffle_epi8(v2, sh3), 8));
				val3 = _mm_or_si128(val3, _mm_slli_si128(_mm_shuffle_epi8(v3, sh3), 12));

				_mm_store_si128(reinterpret_cast<__m128i*>(out_arrays[0][yy].i8), val0);
				_mm_store_si128(reinterpret_cast<__m128i*>(out_arrays[1][yy].i8), val1);
				_mm_store_si128(reinterpret_cast<__m128i*>(out_arrays[2][yy].i8), val2);
				_mm_store_si128(reinterpret_cast<__m128i*>(out_arrays[3][yy].i8), val3);
			}
		}

		static inline void transpose16x16_SSE(const unsigned char* A, unsigned char* B, const int lda, const int ldb) {
			__m128i rows[16];
			for (int i = 0; i < 16; i += 4) {
				rows[i] = _mm_loadu_si128(reinterpret_cast<const __m128i*>( & A[i * lda]));
				rows[i + 1] = _mm_loadu_si128(reinterpret_cast<const __m128i*>( & A[(i + 1) * lda]));
				rows[i + 2] = _mm_loadu_si128(reinterpret_cast<const __m128i*>( & A[(i + 2) * lda]));
				rows[i + 3] = _mm_loadu_si128(reinterpret_cast<const __m128i*>( & A[(i + 3) * lda]));
			}
			//__m128i out[16];
			transpose_16x16(rows, rows);
			for (int i = 0; i < 16; i += 4) {
				_mm_store_si128(reinterpret_cast<__m128i*>( & B[i * ldb]), rows[i]);
				_mm_store_si128(reinterpret_cast<__m128i*>( & B[(i + 1) * ldb]), rows[i + 1]);
				_mm_store_si128(reinterpret_cast<__m128i*>( & B[(i + 2) * ldb]), rows[i + 2]);
				_mm_store_si128(reinterpret_cast<__m128i*>( & B[(i + 3) * ldb]), rows[i + 3]);
			}
		}

		static inline void transpose_block_SSE16x16(const unsigned char* A, unsigned char* B, const int n, const int m) {
			const int block_size = 16;
			for (int i = 0; i < n; i += block_size) {
				for (int j = 0; j < m; j += block_size) {
					int max_i2 = i + block_size < n ? i + block_size : n;
					int max_j2 = j + block_size < m ? j + block_size : m;
					for (int i2 = i; i2 < max_i2; i2 += 16) {
						for (int j2 = j; j2 < max_j2; j2 += 16) {
							transpose16x16_SSE(&A[i2 * m + j2], &B[j2 * n + i2], m, n);
						}
					}
				}
			}
		}

		static inline void transpose16x16_SSE_u(const unsigned char* A, unsigned char* B, const int lda, const int ldb) {
			__m128i rows[16];
			for (int i = 0; i < 16; i += 4) {
				rows[i] = _mm_loadu_si128(reinterpret_cast<const __m128i*>( & A[i * lda]));
				rows[i + 1] = _mm_loadu_si128(reinterpret_cast<const __m128i*>( & A[(i + 1) * lda]));
				rows[i + 2] = _mm_loadu_si128(reinterpret_cast<const __m128i*>( & A[(i + 2) * lda]));
				rows[i + 3] = _mm_loadu_si128(reinterpret_cast<const __m128i*>( & A[(i + 3) * lda]));
			}
			//__m128i out[16];
			transpose_16x16(rows, rows);
			for (int i = 0; i < 16; i += 4) {
				_mm_storeu_si128(reinterpret_cast<__m128i*>( & B[i * ldb]), rows[i]);
				_mm_storeu_si128(reinterpret_cast<__m128i*>( & B[(i + 1) * ldb]), rows[i + 1]);
				_mm_storeu_si128(reinterpret_cast<__m128i*>( & B[(i + 2) * ldb]), rows[i + 2]);
				_mm_storeu_si128(reinterpret_cast<__m128i*>( & B[(i + 3) * ldb]), rows[i + 3]);
			}
		}

		static inline void transpose_block_SSE16x16_u(const unsigned char* A, unsigned char* B, const int n, const int m) {
			const int block_size = 16;
			for (int i = 0; i < n; i += block_size) {
				for (int j = 0; j < m; j += block_size) {
					int max_i2 = i + block_size < n ? i + block_size : n;
					int max_j2 = j + block_size < m ? j + block_size : m;
					for (int i2 = i; i2 < max_i2; i2 += 16) {
						for (int j2 = j; j2 < max_j2; j2 += 16) {
							transpose16x16_SSE_u(&A[i2 * m + j2], &B[j2 * n + i2], m, n);
						}
					}
				}
			}
		}

		static inline std::int64_t read_L_64(const void* src)
		{
			std::int64_t value;
			memcpy(&value, src, sizeof(std::uint64_t));
			return value;
		}

#if defined( __SSE4_1__ ) && defined (SEQ_ARCH_64)// for _mm_extract_epi64

		static inline void tp128_8x8(const unsigned char* A, unsigned char* B, const int lda, const int ldb)
		{
			//see https://stackoverflow.com/questions/42162270/a-better-8x8-bytes-matrix-transpose-with-sse?rq=1
			static const __m128i pshufbcnst = _mm_set_epi8(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
			__m128i B0, B1, B2, B3, T0, T1, T2, T3;

			B0 = _mm_set_epi64x(read_L_64(A + 1 * lda), read_L_64(A + 0 * lda));//_mm_shuffle_epi8(_mm_loadu_si128((__m128i*)A), sv);
			B1 = _mm_set_epi64x(read_L_64(A + 3 * lda), read_L_64(A + 2 * lda));
			B2 = _mm_set_epi64x(read_L_64(A + 5 * lda), read_L_64(A + 4 * lda));
			B3 = _mm_set_epi64x(read_L_64(A + 7 * lda), read_L_64(A + 6 * lda));

			T0 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(B0), _mm_castsi128_ps(B1), 0b10001000));
			T1 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(B2), _mm_castsi128_ps(B3), 0b10001000));
			T2 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(B0), _mm_castsi128_ps(B1), 0b11011101));
			T3 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(B2), _mm_castsi128_ps(B3), 0b11011101));

			B0 = _mm_shuffle_epi8(T0, pshufbcnst);
			B1 = _mm_shuffle_epi8(T1, pshufbcnst);
			B2 = _mm_shuffle_epi8(T2, pshufbcnst);
			B3 = _mm_shuffle_epi8(T3, pshufbcnst);

			T0 = _mm_unpacklo_epi32(B0, B1);
			T1 = _mm_unpackhi_epi32(B0, B1);
			T2 = _mm_unpacklo_epi32(B2, B3);
			T3 = _mm_unpackhi_epi32(B2, B3);


			*(reinterpret_cast<int64_t*>(B + 0 * ldb)) = _mm_extract_epi64(T0, 0);
			*(reinterpret_cast<int64_t*>(B + 1 * ldb)) = _mm_extract_epi64(T0, 1);
			*(reinterpret_cast<int64_t*>(B + 2 * ldb)) = _mm_extract_epi64(T1, 0);
			*(reinterpret_cast<int64_t*>(B + 3 * ldb)) = _mm_extract_epi64(T1, 1);
			*(reinterpret_cast<int64_t*>(B + 4 * ldb)) = _mm_extract_epi64(T2, 0);
			*(reinterpret_cast<int64_t*>(B + 5 * ldb)) = _mm_extract_epi64(T2, 1);
			*(reinterpret_cast<int64_t*>(B + 6 * ldb)) = _mm_extract_epi64(T3, 0);
			*(reinterpret_cast<int64_t*>(B + 7 * ldb)) = _mm_extract_epi64(T3, 1);
		}

		static inline void transpose_block_SSE8x8(const unsigned char* A, unsigned char* B, const int n, const int m) {
			const int block_size = 8;
			for (int i = 0; i < n; i += block_size) {
				for (int j = 0; j < m; j += block_size) {
					int max_i2 = i + block_size < n ? i + block_size : n;
					int max_j2 = j + block_size < m ? j + block_size : m;
					for (int i2 = i; i2 < max_i2; i2 += 8) {
						for (int j2 = j; j2 < max_j2; j2 += 8) {
							tp128_8x8(&A[i2 * m + j2], &B[j2 * n + i2], m, n);
						}
					}
				}
			}
		}

#endif

		static inline void extractBytes_generic(std::uint32_t BPP, const char* src, std::uint32_t , detail::hse_array_type* out_arrays)
		{
			char* out = reinterpret_cast<char*>(out_arrays);
			for (unsigned y = 0; y < 256; ++y)
				for (unsigned x = 0; x < BPP; x++) {
					out[x * 256 + y] = *src++;
				}
		}


	}//end namespace detail

	SEQ_HEADER_ONLY_EXPORT_FUNCTION void transpose_256_rows(const char* src, char* aligned_dst, unsigned BPP)
	{
		if (BPP >= 16 && (BPP & 15) == 0) {
			detail::transpose_block_SSE16x16(reinterpret_cast<const unsigned char*>(src), reinterpret_cast<unsigned char*>(aligned_dst), 256, static_cast<int>(BPP));
		}
#if defined( __SSE4_1__) && defined(SEQ_ARCH_64)
		else if (BPP >= 8 && (BPP & 7) == 0) {
			detail::transpose_block_SSE8x8(reinterpret_cast<const unsigned char*>(src), reinterpret_cast<unsigned char*>(aligned_dst), 256, static_cast<int>(BPP));
		}
#endif
		else {
			switch (BPP) {
			case 1: detail::extract1Byte(src, reinterpret_cast<detail::hse_array_type*>(aligned_dst)); break;
			case 2: detail::extract2Bytes_sse3(src,  reinterpret_cast<detail::hse_array_type*>(aligned_dst)); break;
			case 4: detail::extract4Bytes_sse3(src,  reinterpret_cast<detail::hse_array_type*>(aligned_dst)); break;
			default: detail::extractBytes_generic(BPP, src, BPP, reinterpret_cast<detail::hse_array_type*>(aligned_dst)); break;
			}
		}
	}


	SEQ_HEADER_ONLY_EXPORT_FUNCTION void transpose_inv_256_rows(const char* src, char* dst, unsigned BPP)
	{
		if (BPP >= 16 && (BPP & 15) == 0) {
			detail::transpose_block_SSE16x16_u(reinterpret_cast<const unsigned char*>(src), reinterpret_cast<unsigned char*>(dst), static_cast<int>(BPP), 256);
		}
#if defined( __SSE4_1__) && defined(SEQ_ARCH_64)
		else if (BPP >= 8 && (BPP & 7) == 0 ) {
			detail::transpose_block_SSE8x8(reinterpret_cast<const unsigned char*>(src), reinterpret_cast<unsigned char*>(dst), static_cast<int>(BPP), 256);
		}
#endif
		else {
			for (unsigned y = 0; y < BPP; ++y) {
				char* d = dst + y;
				const char* s = src + y * 256;
				for (unsigned x = 0; x < 256; ++x, d += BPP) {
					*d = s[x]; 
				}
			}
		}
	}


	SEQ_HEADER_ONLY_EXPORT_FUNCTION void transpose_generic(const char* src, char* dst, unsigned block_size, unsigned BPP)
	{
		SEQ_ASSERT_DEBUG((block_size & 15) == 0, "block_size must be a multiple of 16");
		if (BPP >= 16 && (BPP & 15) == 0) {
			detail::transpose_block_SSE16x16(reinterpret_cast<const unsigned char*>(src), reinterpret_cast<unsigned char*>(dst), static_cast<int>(block_size), static_cast<int>(BPP));
		}
#if defined( __SSE4_1__) && defined(SEQ_ARCH_64)
		else if (BPP >= 8 && (BPP & 7) == 0) {
			detail::transpose_block_SSE8x8(reinterpret_cast<const unsigned char*>(src), reinterpret_cast<unsigned char*>(dst), static_cast<int>(block_size), static_cast<int>(BPP));
		}
#endif
		else {
			for (unsigned y = 0; y < block_size; ++y)
				for (unsigned x = 0; x < BPP; x++) {
					dst[x * block_size + y] = *src++;
			}
		}
	}
	SEQ_HEADER_ONLY_EXPORT_FUNCTION void transpose_inv_generic(const char* src, char* dst, unsigned block_size, unsigned BPP)
	{
		SEQ_ASSERT_DEBUG((block_size & 15) == 0, "block_size must be a multiple of 16");
		if (BPP >= 16 && (BPP & 15) == 0) {
			detail::transpose_block_SSE16x16_u(reinterpret_cast<const unsigned char*>(src), reinterpret_cast<unsigned char*>(dst), static_cast<int>(BPP), static_cast<int>(block_size));
		}
#if defined( __SSE4_1__) && defined (SEQ_ARCH_64)
		else if (BPP >= 8 && (BPP & 7) == 0) {
			detail::transpose_block_SSE8x8(reinterpret_cast<const unsigned char*>(src), reinterpret_cast<unsigned char*>(dst), static_cast<int>(BPP), static_cast<int>(block_size));
		}
#endif
		else {
			
			for (unsigned y = 0; y < BPP; ++y) {
				char* d = dst + y;
				const char* s = src + y * block_size;
				for (unsigned x = 0; x < block_size; ++x, d += BPP) {
					*d = s[x];
				}
			}
		}
	}


}//end namespace seq


#endif //__SSE4_1__
