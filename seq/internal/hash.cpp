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


#include "../hash.hpp"

namespace seq
{

	/// @brief Compute a hash value for input data using the murmurhash2 algorithm.
	/// @param ptr input buffer
	/// @param len input buffer size
	/// @return computed hash value
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto hash_bytes_murmur64(const void* _ptr, size_t len) noexcept -> size_t
	{
		static constexpr std::uint64_t m = 14313749767032793493ULL;
		static constexpr std::uint64_t seed = 3782874213ULL;
		static constexpr std::uint64_t r = 47ULL;

		const unsigned char* ptr = static_cast<const unsigned char*>(_ptr);
		std::uint64_t h = seed ^ (len * m);
		const std::uint8_t* end = ptr + len - (sizeof(std::uint64_t) - 1);
		while (ptr < end) {
			auto k = read_64(ptr);

			k *= m;
			k ^= k >> r;
			k *= m;

			h ^= k;
			h *= m;

			ptr += sizeof(std::uint64_t);
		}

		switch (len & 7U) {
		case 7U:
			h ^= static_cast<std::uint64_t>(ptr[6U]) << 48U;
			SEQ_FALLTHROUGH();
		case 6U:
			h ^= static_cast<std::uint64_t>(ptr[5U]) << 40U;
			SEQ_FALLTHROUGH();
		case 5U:
			h ^= static_cast<std::uint64_t>(ptr[4U]) << 32U;
			SEQ_FALLTHROUGH();
		case 4U:
			h ^= static_cast<std::uint64_t>(ptr[3U]) << 24U;
			SEQ_FALLTHROUGH();
		case 3U:
			h ^= static_cast<std::uint64_t>(ptr[2U]) << 16U;
			SEQ_FALLTHROUGH();
		case 2U:
			h ^= static_cast<std::uint64_t>(ptr[1U]) << 8U;
			SEQ_FALLTHROUGH();
		case 1U:
			h ^= static_cast<std::uint64_t>(ptr[0U]);
			h *= m;
			SEQ_FALLTHROUGH();
		default:
			break;
		}

		h ^= h >> r;
		h *= m;
		h ^= h >> r;
		return static_cast<size_t>(h);
	}

	/// @brief Compute a hash value for input data using the fnv1a algorithm.
	/// @param ptr input buffer
	/// @param len input buffer size
	/// @return computed hash value
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto hash_bytes_fnv1a(const void* _ptr, size_t size)noexcept -> size_t
	{
		static constexpr size_t FNV_offset_basis = sizeof(size_t) == 8 ? 14695981039346656037ULL : 2166136261U;
		static constexpr size_t FNV_prime = sizeof(size_t) == 8 ? 1099511628211ULL : 16777619U;

		const unsigned char* ptr = static_cast<const unsigned char*>(_ptr);
		size_t h = FNV_offset_basis;

		for (size_t i = 0; i < size; ++i) {
			auto k = static_cast<size_t>(ptr[i]);
			h ^= k;
			h *= FNV_prime;
		}
		return h;
	}





	namespace detail
	{

		/**
		 * Function builds an unsigned 64-bit value out of remaining bytes in a
		 * message, and pads it with the "final byte". This function can only be
		 * called if less than 8 bytes are left to read. The message should be "long",
		 * permitting Msg[ -3 ] reads.
		 *
		 * @param Msg Message pointer, alignment is unimportant.
		 * @param MsgLen Message's remaining length, in bytes; can be 0.
		 * @return Final byte-padded value from the message.
		 */

#define _HU64(v) static_cast<uint64_t>(v)

		static SEQ_ALWAYS_INLINE uint64_t kh_lpu64ec_l3(const uint8_t* const Msg,
			const size_t MsgLen)
		{
			const int ml8 = static_cast<int>(MsgLen * 8);
			if (MsgLen < 4) {
				const uint8_t* const Msg3 = Msg + MsgLen - 3;
				const uint64_t m = _HU64(Msg3[0]) | _HU64(Msg3[1]) << 8 | _HU64(Msg3[2]) << 16;
				return(_HU64(1) << ml8 | m >> (24 - ml8));
			}

			const uint64_t mh = read_LE_32(Msg + MsgLen - 4);
			const uint64_t ml = read_LE_32(Msg);
			return(_HU64(1) << ml8 | ml | (mh >> (64 - ml8)) << 32);
		}

		/**
		 * Function builds an unsigned 64-bit value out of remaining bytes in a
		 * message, and pads it with the "final byte". This function can only be
		 * called if less than 8 bytes are left to read. Can be used on "short"
		 * messages, but MsgLen should be greater than 0.
		 *
		 * @param Msg Message pointer, alignment is unimportant.
		 * @param MsgLen Message's remaining length, in bytes; cannot be 0.
		 * @return Final byte-padded value from the message.
		 */

		static SEQ_ALWAYS_INLINE uint64_t kh_lpu64ec_nz(const uint8_t* const Msg,
			const size_t MsgLen)
		{
			const int ml8 = static_cast<int>(MsgLen * 8);
			if (MsgLen < 4) {
				uint64_t m = Msg[0];
				if (MsgLen > 1) {
					m |= _HU64(Msg[1]) << 8;
					if (MsgLen > 2)
						m |= _HU64(Msg[2]) << 16;
				}
				return(_HU64(1) << ml8 | m);
			}

			const uint64_t mh = read_LE_32(Msg + MsgLen - 4);
			const uint64_t ml = read_LE_32(Msg);
			return(_HU64(1) << ml8 | ml | (mh >> (64 - ml8)) << 32);
		}

		/**
		 * Function builds an unsigned 64-bit value out of remaining bytes in a
		 * message, and pads it with the "final byte". This function can only be
		 * called if less than 8 bytes are left to read. The message should be "long",
		 * permitting Msg[ -4 ] reads.
		 *
		 * @param Msg Message pointer, alignment is unimportant.
		 * @param MsgLen Message's remaining length, in bytes; can be 0.
		 * @return Final byte-padded value from the message.
		 */

		static SEQ_ALWAYS_INLINE uint64_t kh_lpu64ec_l4(const uint8_t* const Msg,
			const size_t MsgLen)
		{
			const int ml8 = static_cast<int>(MsgLen * 8);
			if (MsgLen < 5) {
				const uint64_t m = read_LE_32(Msg + MsgLen - 4);
				return(_HU64(1) << ml8 | m >> (32 - ml8));
			}
			const uint64_t m = read_LE_64(Msg + MsgLen - 8);
			return(_HU64(1) << ml8 | m >> (64 - ml8));
		}

#define KOMIHASH_HASH16( m ) \
	umul128( Seed1 ^ read_LE_64( m ), Seed5 ^ read_LE_64( m + 8 ), &Seed1, &r1h ); \
	Seed5 += r1h; Seed1 ^= Seed5;

#define KOMIHASH_HASHROUND() \
	umul128( Seed1, Seed5, &Seed1, &r2h ); \
	Seed5 += r2h; Seed1 ^= Seed5;

#define KOMIHASH_HASHFIN() \
	umul128( r1h, r2h, &Seed1, &r1h ); \
	Seed5 += r1h; Seed1 ^= Seed5; \
	KOMIHASH_HASHROUND(); \
	return static_cast<size_t>( Seed1 );

		/**
		 * The hashing epilogue function (for internal use).
		 *
		 * @param Msg Pointer to the remaining part of the message.
		 * @param MsgLen Remaining part's length, can be zero.
		 * @param Seed1 Latest Seed1 value.
		 * @param Seed5 Latest Seed5 value.
		 * @return 64-bit hash value.
		 */

		static SEQ_ALWAYS_INLINE uint64_t komihash_epi(const uint8_t* Msg, size_t MsgLen,
			uint64_t Seed1, uint64_t Seed5)
		{
			uint64_t r1h, r2h;

			if (SEQ_LIKELY(MsgLen > 31)) {
				KOMIHASH_HASH16(Msg);
				KOMIHASH_HASH16(Msg + 16);
				Msg += 32;
				MsgLen -= 32;
			}
			if (MsgLen > 15) {
				KOMIHASH_HASH16(Msg);
				Msg += 16;
				MsgLen -= 16;
			}
			if (MsgLen > 7) {
				r2h = Seed5 ^ kh_lpu64ec_l4(Msg + 8, MsgLen - 8);
				r1h = Seed1 ^ read_LE_64(Msg);
			}
			else {
				r1h = Seed1 ^ kh_lpu64ec_l4(Msg, MsgLen);
				r2h = Seed5;
			}
			KOMIHASH_HASHFIN();
		}

		static SEQ_ALWAYS_INLINE uint64_t komihash_long(const uint8_t* Msg, size_t MsgLen,
			uint64_t Seed1, uint64_t Seed5)
		{
			if (MsgLen > 63)
			{
				uint64_t Seed2 = 1354286222620113816ull;
				uint64_t Seed3 = 11951381506893904140ull;
				uint64_t Seed4 = 719472657908900949ull;
				uint64_t Seed6 = 17340704221724641189ull;
				uint64_t Seed7 = 10258850193283144468ull;
				uint64_t Seed8 = 8175790239553258206ull;
				uint64_t r1h, r2h, r3h, r4h;

				do
				{
					SEQ_PREFETCH(Msg);
					umul128(Seed1 ^ read_LE_64(Msg), Seed5 ^ read_LE_64(Msg + 32), &Seed1, &r1h);
					umul128(Seed2 ^ read_LE_64(Msg + 8), Seed6 ^ read_LE_64(Msg + 40), &Seed2, &r2h);
					umul128(Seed3 ^ read_LE_64(Msg + 16), Seed7 ^ read_LE_64(Msg + 48), &Seed3, &r3h);
					umul128(Seed4 ^ read_LE_64(Msg + 24), Seed8 ^ read_LE_64(Msg + 56), &Seed4, &r4h);

					Msg += 64;
					MsgLen -= 64;
					Seed5 += r1h;
					Seed6 += r2h;
					Seed7 += r3h;
					Seed8 += r4h;
					Seed2 ^= Seed5;
					Seed3 ^= Seed6;
					Seed4 ^= Seed7;
					Seed1 ^= Seed8;
				} while (SEQ_LIKELY(MsgLen > 63));

				Seed5 ^= Seed6 ^ Seed7 ^ Seed8;
				Seed1 ^= Seed2 ^ Seed3 ^ Seed4;
			}
			return static_cast<size_t>(komihash_epi(Msg, MsgLen, Seed1, Seed5));
		}

	}
	/**
	 Strip down version of KOMIHASH hash function. 
	 See https://github.com/avaneev/komihash/tree/main for more details.
	 */
	SEQ_HEADER_ONLY_EXPORT_FUNCTION size_t hash_bytes_komihash(const void* Msg0, size_t MsgLen) noexcept
	{
		const uint8_t* Msg = reinterpret_cast<const uint8_t*>(Msg0);
		uint64_t Seed1 = 131429069690128604ull;
		uint64_t Seed5 = 5688864720084962249ull;
		uint64_t r1h, r2h;

		SEQ_PREFETCH(Msg);

		if ((MsgLen < 16))
		{
			r1h = Seed1;
			r2h = Seed5;
			if (MsgLen > 7) {
				r2h ^= detail::kh_lpu64ec_l3(Msg + 8, MsgLen - 8);
				r1h ^= read_LE_64(Msg);
			}
			else if (SEQ_LIKELY(MsgLen != 0))
				r1h ^= detail::kh_lpu64ec_nz(Msg, MsgLen);
			KOMIHASH_HASHFIN();
		}

		if ((MsgLen < 32))
		{
			KOMIHASH_HASH16(Msg);

			if (MsgLen > 23) {
				r2h = Seed5 ^ detail::kh_lpu64ec_l4(Msg + 24, MsgLen - 24);
				r1h = Seed1 ^ read_LE_64(Msg + 16);
			}
			else {
				r1h = Seed1 ^ detail::kh_lpu64ec_l4(Msg + 16, MsgLen - 16);
				r2h = Seed5;
			}
			KOMIHASH_HASHFIN();
		}

		return static_cast<size_t>(detail::komihash_long(Msg, MsgLen, Seed1, Seed5));
	}
}
