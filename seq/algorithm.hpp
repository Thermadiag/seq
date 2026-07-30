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

#ifndef SEQ_ALGORITHM_HPP
#define SEQ_ALGORITHM_HPP

#include "net_sort.hpp"
#include "internal/concurrent_hash_table.hpp"

namespace seq
{

	namespace algo_detail
	{
		// Hash function working on pointer
		template<class Key, class Hash>
		struct HashPtr 
		{
			const Hash* h;
			size_t operator()(const Key* k) const noexcept { return (*h)(*k); }
		};
		// Comparison function working on pointer
		template<class Key, class Equal>
		struct EqualPtr 
		{
			const Equal* eq;
			bool operator()(const Key* l, const Key* r) const noexcept { return (*eq)(*l, *r); }
		};
	}

	/// @brief Remove duplicate elements from input range.
	///
	/// Like std::unique, seq::unique removes duplicate elements
	/// from input range in a stable way.
	///
	/// Unlike std::unique, seq::unique does not need the input range
	/// to be sorted at all. Instead, it uses a hash table to assess
	/// if an element is a duplicate value or not. The hash table is
	/// a swiss table also used by seq::concurrent_set/map.
	///
	/// seq::unique works on forward iterator, but is faster for
	/// random access iterator as the hash table can be reserved
	/// up front.
	///
	/// It is possible to pass a custom hash function and comparison
	/// function for non trival element type.
	///
	template<class Iter, class Hash = hasher<typename std::iterator_traits<Iter>::value_type>, class Equal = std::equal_to<>>
	Iter unique(Iter first, Iter last, const Hash& h = {}, const Equal& eq = {})
	{
		using namespace algo_detail;
		using Key = typename std::iterator_traits<Iter>::value_type;
		using HashFn = HashPtr<Key, Hash>;
		using EqualFn = EqualPtr<Key, Equal>;
		using TableType = seq::detail::ConcurrentHashTable<Key*, Key*, HashFn, EqualFn, std::allocator<Key*>, no_concurrency>;

		if (first == last)
			return last;

		TableType set{ HashFn{ &h }, EqualFn{ &eq } };

		if constexpr (is_random_access<Iter>::value)
			set.reserve(std::distance(first, last));

		// Loop over elements until with find a duplicate
		for (; first != last; ++first) {
			if (!set.emplace(std::addressof(*first)))
				break;
		}

		// Check no duplicates
		if (first == last)
			return last;

		auto loc = first++;
		for (; first != last; ++first) {
			if (!set.emplace(std::addressof(*first))) {
				continue;
			}
			*loc++ = std::move(*first);
		}
		return loc;
	}

} // end namespace seq

#endif
