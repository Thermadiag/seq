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
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto hash_bytes_murmur64(const std::uint8_t* ptr, size_t len) noexcept -> size_t
	{
		static constexpr std::uint64_t m = 14313749767032793493ULL;
		static constexpr std::uint64_t seed = 3782874213ULL;
		static constexpr std::uint64_t r = 47ULL;

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
	/// This version reads the input buffer by chunks of sizeof(size_t).
	/// @param ptr input buffer
	/// @param len input buffer size
	/// @return computed hash value
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto hash_bytes_fnv1a(const unsigned char* ptr, size_t size)noexcept -> size_t
	{
		static constexpr size_t FNV_offset_basis = sizeof(size_t) == 8 ? 14695981039346656037ULL : 2166136261U;
		static constexpr size_t FNV_prime = sizeof(size_t) == 8 ? 1099511628211ULL : 16777619U;

		size_t h = FNV_offset_basis;

		const std::uint8_t* end = ptr + size - (sizeof(size_t) - 1);
		while (ptr < end) {
			auto k = read_size_t(ptr);
			h ^= k;
			h *= FNV_prime;
			ptr += sizeof(size_t);
		}
		switch (size & 7U) {
		case 7U:
			h ^= static_cast<size_t>(ptr[6U]);
			h *= FNV_prime;
			SEQ_FALLTHROUGH();
		case 6U:
			h ^= static_cast<size_t>(ptr[5U]);
			h *= FNV_prime;
			SEQ_FALLTHROUGH();
		case 5U:
			h ^= static_cast<size_t>(ptr[4U]);
			h *= FNV_prime;
			SEQ_FALLTHROUGH();
		case 4U:
			h ^= static_cast<size_t>(ptr[3U]);
			h *= FNV_prime;
			SEQ_FALLTHROUGH();
		case 3U:
			h ^= static_cast<size_t>(ptr[2U]);
			h *= FNV_prime;
			SEQ_FALLTHROUGH();
		case 2U:
			h ^= static_cast<size_t>(ptr[1U]);
			h *= FNV_prime;
			SEQ_FALLTHROUGH();
		case 1U:
			h ^= static_cast<size_t>(ptr[0U]);
			h *= FNV_prime;
			SEQ_FALLTHROUGH();
		default:
			break;
		}
		return h;
	}

	/// @brief Compute a hash value for input data using the fnv1a algorithm.
	/// @param ptr input buffer
	/// @param len input buffer size
	/// @return computed hash value
	SEQ_HEADER_ONLY_EXPORT_FUNCTION auto hash_bytes_fnv1a_slow(const unsigned char* ptr, size_t size)noexcept -> size_t
	{
		static constexpr size_t FNV_offset_basis = sizeof(size_t) == 8 ? 14695981039346656037ULL : 2166136261U;
		static constexpr size_t FNV_prime = sizeof(size_t) == 8 ? 1099511628211ULL : 16777619U;

		size_t h = FNV_offset_basis;

		for (size_t i = 0; i < size; ++i) {
			auto k = static_cast<size_t>(ptr[i]);
			h ^= k;
			h *= FNV_prime;
		}
		return h;
	}

}
