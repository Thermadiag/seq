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

#ifndef SEQ_NET_SORT_HPP
#define SEQ_NET_SORT_HPP

#include "bits.hpp"
#include "type_traits.hpp"

#include <iterator>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <vector>

#define SEQ_ALGO_ASSERT_DEBUG(condition, msg) SEQ_ASSERT_DEBUG(condition, msg)

namespace seq
{
	/// @brief Buffer object used by net_sort(), net_sort_size() and inplace_merge()
	template<class Iter>
	struct buffer
	{
		Iter first;
		size_t size = 0;
	};

	namespace algo_detail
	{
		// Unspecified length
		static constexpr size_t Unspecified = (size_t)-1;

		/// @brief Iterator wrapper for bidirectional iterator
		template<class Iter>
		class IterWrapper
		{
			template<class Diff>
			SEQ_ALWAYS_INLINE void increment_iter(Iter& it, Diff d)
			{
				if constexpr (is_random_access<Iter>::value)
					it += d;
				else
					std::advance(it, d);
			}

		public:
			using value_type = typename std::iterator_traits<Iter>::value_type;
			using iterator_category = typename std::iterator_traits<Iter>::iterator_category;
			using difference_type = typename std::iterator_traits<Iter>::difference_type;
			using reference = typename std::iterator_traits<Iter>::reference;
			using pointer = typename std::iterator_traits<Iter>::pointer;

			Iter iter;
			difference_type pos;

			SEQ_ALWAYS_INLINE IterWrapper(Iter it = Iter(), difference_type p = 0) 
			  : iter(it)
			  , pos(p)
			{
			}
			SEQ_ALWAYS_INLINE auto operator*() const -> reference { return (*this->iter); }
			SEQ_ALWAYS_INLINE auto operator++() -> IterWrapper&
			{
				++iter;
				++pos;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) -> IterWrapper
			{
				IterWrapper it = *this;
				++(*this);
				return it;
			}
			SEQ_ALWAYS_INLINE auto operator--() -> IterWrapper&
			{
				--iter;
				--pos;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) -> IterWrapper
			{
				IterWrapper it = *this;
				--(*this);
				return it;
			}
			template<class Diff>
			SEQ_ALWAYS_INLINE auto operator+=(Diff diff) -> IterWrapper&
			{
				pos += static_cast<difference_type>(diff);
				increment_iter(iter, static_cast<difference_type>(diff));
				return *this;
			}
			template<class Diff>
			SEQ_ALWAYS_INLINE auto operator-=(Diff diff) -> IterWrapper&
			{
				pos -= static_cast<difference_type>(diff);
				increment_iter(iter, -static_cast<difference_typet>(diff));
				return *this;
			}
		};
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator+(const IterWrapper<Iter>& it, typename IterWrapper<Iter>::difference_type diff)
		{
			IterWrapper<Iter> res = it;
			res += diff;
			return res;
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator-(const IterWrapper<Iter>& it, typename IterWrapper<Iter>::difference_type diff)
		{
			IterWrapper<Iter> res = it;
			res -= diff;
			return res;
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator-(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2)
		{
			return it1.pos - it2.pos;
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator==(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2)
		{
			return it1.iter == it2.iter;
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator!=(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2)
		{
			return it1.iter != it2.iter;
		}



		// similar to std::next, but use ++ operator when no distance is specified
		template<class Iter>
		static SEQ_ALWAYS_INLINE auto iter_next(Iter it) 
		{
			return ++it;
		}
		template<class Iter, class Diff>
		static SEQ_ALWAYS_INLINE auto iter_next(Iter it, Diff d) 
		{
			return it + d;
		}

		// similar to std::prev, but use -- operator when no distance is specified
		template<class Iter>
		static SEQ_ALWAYS_INLINE auto iter_prev(Iter it) 
		{
			return --it;
		}
		template<class Iter, class Diff>
		static SEQ_ALWAYS_INLINE auto iter_prev(Iter it, Diff d) 
		{
			return it - d;
		}

		template<class Iter>
		static SEQ_ALWAYS_INLINE auto unwrap_iter(Iter it) 
		{
			return it;
		}
		template<class Iter>
		static SEQ_ALWAYS_INLINE auto unwrap_iter(std::move_iterator<Iter> it) 
		{
			return unwrap_iter(it.base());
		}
		template<class Iter>
		static SEQ_ALWAYS_INLINE auto unwrap_iter(std::reverse_iterator<Iter> it) 
		{
			return unwrap_iter(it.base());
		}

		// compare iterators for equality without triggering compile error
		template<class Iter1, class Iter2>
		static SEQ_ALWAYS_INLINE bool iter_equal(Iter1 it1, Iter2 it2) 
		{
			using type1 = decltype(unwrap_iter(it1));
			using type2 = decltype(unwrap_iter(it2));
			if constexpr (std::is_same_v<type1, type2>)
				return unwrap_iter(it1) == unwrap_iter(it2);
			else
				return false;
		}

		// similar to std::distance with an overload for IterWrapper that supports subtracting 2 iterators
		template<class Iter>
		static SEQ_ALWAYS_INLINE auto iter_distance(Iter first, Iter last) 
		{
			return std::distance(first, last);
		}
		template<class Iter>
		static SEQ_ALWAYS_INLINE auto iter_distance(const IterWrapper<Iter>& first, const IterWrapper<Iter>& last) 
		{
			return last - first;
		}

		template<class Iter>
		static SEQ_ALWAYS_INLINE auto wrap_iter(Iter it, typename std::iterator_traits<Iter>::difference_type d = 0)
		{
			if constexpr (is_random_access<Iter>::value) {
				(void)d;
				return it;
			}
			else
				return IterWrapper<Iter>(it, d);
		}

		template<class Iter, class Cmp>
		static void
		merge_inplace_left(Iter f0, size_t n0, Iter f1, size_t n1, Iter& f0_0, size_t& n0_0, Iter& f0_1, size_t& n0_1, Iter& f1_0, size_t& n1_0, Iter& f1_1, size_t& n1_1, Cmp& r) 
		{
			// Subroutine of inplace_merge_n
			SEQ_ALGO_ASSERT_DEBUG((size_t)iter_distance(f0, f1) == n0, "");
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(f0, iter_next(f0, n0), r) && std::is_sorted(f1, iter_next(f1, n1), r), "");
			SEQ_ALGO_ASSERT_DEBUG(n0 > 0, "");
			SEQ_ALGO_ASSERT_DEBUG(n1 > 0, "");

			f0_0 = f0;
			n0_0 = n0 >> 1;
			f0_1 = f0;
			f0_1 = iter_next(f0_1, n0_0);
			f1_1 = std::lower_bound(f1, iter_next(f1, n1), *f0_1, r);
			f1_0 = std::rotate(f0_1, f1, f1_1);
			n0_1 = iter_distance(f0_1, f1_0);
			++f1_0;
			n1_0 = (n0 - n0_0) - 1;
			n1_1 = n1 - n0_1;
		}

		template<class Iter, class Cmp>
		static void
		merge_inplace_right(Iter f0, size_t n0, Iter f1, size_t n1, Iter& f0_0, size_t& n0_0, Iter& f0_1, size_t& n0_1, Iter& f1_0, size_t& n1_0, Iter& f1_1, size_t& n1_1, Cmp& r) 
		{
			// Subroutine of inplace_merge_n
			SEQ_ALGO_ASSERT_DEBUG((size_t)iter_distance(f0, f1) == n0, "");
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(f0, iter_next(f0, n0), r) && std::is_sorted(f1, iter_next(f1, n1), r), "");
			SEQ_ALGO_ASSERT_DEBUG(n0 > 0, "");
			SEQ_ALGO_ASSERT_DEBUG(n1 > 0, "");

			f0_0 = f0;
			n0_1 = n1 >> 1;
			f1_1 = f1;
			f1_1 = iter_next(f1_1, n0_1);
			f0_1 = std::upper_bound(f0, iter_next(f0, n0), *f1_1, r);
			++f1_1;
			f1_0 = std::rotate(f0_1, f1, f1_1);
			n0_0 = iter_distance(f0_0, f0_1);
			n1_0 = n0 - n0_0;
			n1_1 = (n1 - n0_1) - 1;
		}

		template<class Iter, class Out>
		static SEQ_ALWAYS_INLINE Out copy_internal(Iter begin, Iter end, Out out)
		{
			// direct std::copy call
			return std::copy((begin), (end), (out));
		}
		template<class Iter, class Out>
		static SEQ_ALWAYS_INLINE Out copy_internal(std::move_iterator<Iter> begin, std::move_iterator<Iter> end, Out out)
		{
			// Let the compiler decide to use memmove if necesary
			return std::move((begin.base()), (end.base()), (out));
		}

		template<bool Overlap, class Iter1, class Iter2, class Out, class Cmp>
		static Out merge_forward(Iter1 first1, Iter1 end1, Iter2 first2, Iter2 end2, Out out, Cmp& c)
		{
			// Merge 2 range forward
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(first1, end1, c), "");
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(first2, end2, c), "");
			SEQ_DEBUG_ONLY(Out dst = out;)

			if constexpr (is_random_access<Iter1>::value && is_random_access<Iter2>::value) {

				// Check for unbalanced merge
				auto s1 = iter_distance(first1, end1);
				auto s2 = iter_distance(first2, end2);

				if (s1 < s2 / 32) {
					// Left is way smaller than right
					for (; first1 != end1; ++first1) {
						if (first2 != end2) {
							while (first1 != end1 && !c(*first2, *first1))
								*out++ = (*first1++);
							if (first1 == end1)
								break;

							auto found = std::lower_bound(first2, end2, *first1, c);
							out = copy_internal(first2, found, out);
							first2 = found;
						}
						*out++ = (*first1);
					}
					goto end;
				}
				else if (s2 < s1 / 32) {
					// Right is way smaller than left
					for (; first2 != end2; ++first2) {
						if (first1 != end1) {
							while (first2 != end2 && c(*first2, *first1))
								*out++ = (*first2++);
							if (first2 == end2)
								break;

							auto found = std::upper_bound(first1, end1, *first2, c);
							out = copy_internal(first1, found, out);
							first1 = found;
						}
						*out++ = (*first2);
					}
					goto end;
				}
			}

			// More efficient merge than std::merge (usually)

			while (first2 != end2) {
				while (first1 != end1 && !c(*first2, *first1)) {

					*out = (*first1);
					++out;
					++first1;
				}

				if (first1 == end1)
					break;

				*out = (*first2);
				++out;
				++first2;

				while (first2 != end2 && c(*first2, *first1)) {

					*out = (*first2);
					++out;
					++first2;
				}

				*out = (*first1);
				++out;
				++first1;
			}

		end:
			out = copy_internal(first1, end1, out);
			if (Overlap && iter_equal(first2, out)) {
				// The last range is already inplace:
				// just advance output iterator if not
				// a std::reverse_iterator (used by
				// merge_backward which do not use the result).
				if constexpr (!is_reverse_iterator<Out>::value)
					out = iter_next(out, iter_distance(first2, end2));
			}
			else
				out = copy_internal(first2, end2, out);

			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(dst, out, c), "");
			return out;
		}

		template<class Iter, class Out, class Cmp>
		static SEQ_ALWAYS_INLINE std::pair<bool, bool> merge_tails(Iter* first, Iter* second, Out& out_left, Out& out_right, Cmp& c)
		{
			// Merge tails and advance
			bool left_order = c(*first[1], *first[0]);
			bool right_order = !c(*second[1], *second[0]);
			*out_left = std::move(*first[left_order]);
			*out_right = std::move(*second[right_order]);
			++out_left;
			--out_right;
			first[1] += left_order;
			first[0] += !left_order;
			second[1] -= right_order;
			second[0] -= !right_order;
			return { left_order, right_order };
		}

		template<class Iter, class Out, class Cmp>
		static SEQ_ALWAYS_INLINE void finish_bidirectional_merge(Iter* first, Iter* second, Out out_left, Cmp& c)
		{
			bool finish_left = (second[0] < first[0]);
			bool finish_right = (second[1] < first[1]);

			if (!finish_left && !finish_right) {
				merge_forward<false>(
				  std::make_move_iterator(first[0]), std::make_move_iterator(++second[0]), std::make_move_iterator(first[1]), std::make_move_iterator(++second[1]), out_left, c);
			}
			else if (finish_left)
				std::move((first[1]), (++second[1]), (out_left));
			else if (finish_right)
				std::move((first[0]), (++second[0]), (out_left));
		}

		template<size_t Count, class Iter, class Out, class Cmp>
		static Out merge_move_bidirectional(Iter first1, Iter last1, Iter first2, Iter last2, Out out, Cmp& c, Out* out_end = nullptr) 
		{
			using T = typename std::iterator_traits<Iter>::value_type;

			// Merge 2 sorted ranges to given output.
			// Uses the fastest available method: standard forward merge
			// or branchless merge from both ends for random access iterators and relocatable types.

			if constexpr (is_random_access<Iter>::value && is_relocatable<T>::value) {

				// Branchless merge from both ends
				// Only truly faster with trivial comparison function,
				// which is usually the case for relocatable types.

				using difference_type = typename std::iterator_traits<Iter>::difference_type;

				Out out_left = out;
				Iter first[2] = { first1, first2 };
				Iter second[2] = { iter_prev(last1), iter_prev(last2) };

				if constexpr (Count != Unspecified) {
					Out res = out_end ? *out_end : iter_next(out, Count * 2);
					Out out_right = iter_prev(res);

					while (first[0] < second[0] && first[1] < second[1])
						merge_tails(first, second, out_left, out_right, c);

					finish_bidirectional_merge(first, second, out_left, c);
					SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(out, res, c), "");
					return res;
				}
				else {

					difference_type dist1 = iter_distance(first1, last1);
					difference_type dist2 = iter_distance(first2, last2);

					Out res = out_end ? *out_end : iter_next(out, dist1 + dist2);
					Out out_right = iter_prev(res);

					// Unbalanced merge from both ends.
					// For the first part of the merge (1/16 of the smallest range),
					// check if the order is pseud random. If not, finish with
					// merge_forward().

					const difference_type iter_count = std::min(dist1, dist2);
					const difference_type stop = iter_count / 16;
					difference_type order = 0;
					difference_type count = 0;

					std::pair<bool, bool> prev_order = { true, true };

					if (first[0] < second[0] && first[1] < second[1]) {

						prev_order = merge_tails(first, second, out_left, out_right, c);

						while (count < stop && first[0] < second[0] && first[1] < second[1]) {
							auto ord = merge_tails(first, second, out_left, out_right, c);
							order += (difference_type)(ord.first == prev_order.first) + (difference_type)(ord.second == prev_order.second);
							prev_order = ord;
							++count;
						}
						if (order <= stop + stop / 2) {
							// Balanced merging: keep using bidirectional merge
							while (first[0] < second[0] && first[1] < second[1])
								merge_tails(first, second, out_left, out_right, c);
						}

						// Finish with merge_forward
						finish_bidirectional_merge(first, second, out_left, c);
						SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(out, res, c), "");
						return res;
					}
				}
			}
			// Standard forward merge
			return merge_forward<false>(std::make_move_iterator(first1), std::make_move_iterator(last1), std::make_move_iterator(first2), std::make_move_iterator(last2), out, c);
		}

		template<bool Overlap, class Iter1, class Iter2, class Out, class Cmp>
		static SEQ_ALWAYS_INLINE void merge_backward(Iter1 first1, Iter1 last1, Iter2 first2, Iter2 last2, Out out_end, Cmp& c) 
		{
			// Merge backward implemented in terms of merge_forward
			merge_forward<Overlap>(std::make_reverse_iterator(last2),
					       std::make_reverse_iterator(first2),
					       std::make_reverse_iterator(last1),
					       std::make_reverse_iterator(first1),
					       std::make_reverse_iterator(out_end),
					       [c](const auto& a, const auto& b) { return c(b, a); });
		}

		template<class Iter, class Cmp, class B>
		static SEQ_ALWAYS_INLINE void merge_with_buffer(Iter first, size_t n0, Iter middle, size_t n1, Iter e1, Cmp& r, B& buffer)
		{
			// Inplace merge 2 ranges using provided buffer.
			// Moves as few elements as possible to the temporary buffer.
			if (n0 <= n1) {
				auto blast = std::move((first), (middle), buffer);
				merge_forward<true>(std::make_move_iterator(buffer), std::make_move_iterator(blast), std::make_move_iterator(middle), std::make_move_iterator(e1), first, r);
			}
			else {
				auto last = e1;
				auto blast = std::move((middle), (last), buffer);
				merge_backward<true>(std::make_move_iterator(first), std::make_move_iterator(middle), std::make_move_iterator(buffer), std::make_move_iterator(blast), last, r);
			}
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(first, iter_next(middle, n1), r), "");
		}

		template<class Iter>
		static void rotate_one_right(Iter first, Iter mid, Iter last)
		{
			// Exchanges the range [first, mid) with [mid, last)
			// pre: distance(mid, last) is 1
			using type = typename std::iterator_traits<Iter>::value_type;
			type tmp(std::move(*mid));
			std::move_backward((first), (mid), (last));
			*first = std::move(tmp);
		}

		template<bool FirstChecks, class Iter, class Cmp, class B>
		static void merge_adaptive_n(Iter f0, size_t n0, Iter f1, size_t n1, Iter e1, Cmp& r, B& buffer)
		{
			// Inplace merge with buffer, first published by Dudzin'sky and Dydek in 1981 IPL 12(1):5-8
			// Implementation from: https://www.jmeiners.com/efficient-programming-with-components/15_merge_inplace.html

			SEQ_ALGO_ASSERT_DEBUG((size_t)iter_distance(f0, f1) == n0, "");
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(f0, iter_next(f0, n0), r), "");
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(f1, iter_next(f1, n1), r), "");

			if constexpr (FirstChecks) {
				// Perform the firsts, easy checks

				if (!n0 || !n1 || !r(*f1, *iter_prev(f1)))
					// One of the 2 ranges is empty, or already sorted
					return;

				if (r(*iter_prev(e1), *f0)) {
					// Simple rotation needed
					std::rotate((f0), (f1), (e1));
					return;
				}

				// The following checks come from msvc STL implementation
				// and help a LOT in some situations

				// Increment f0 as long as it is smaller than first value of second range
				for (;;) {
					if (f0 == f1)
						// We reach the end of first range: already in order
						return;
					if (r(*f1, *f0))
						break;
					++f0;
					--n0;
				}

				auto highest = iter_prev(f1);
				do {
					--e1;
					--n1;
					if (f1 == e1) { // rotate only element remaining in right partition to the beginning, without allocating
						rotate_one_right(f0, f1, ++e1);
						return;
					}
				} while (!r(*e1, *highest)); // found that *highest goes in *e1's position

				++e1;
				++n1;
			}

			if (n0 <= buffer.size || n1 <= buffer.size)
				// We have enough buffer: merge
				return merge_with_buffer((f0), n0, (f1), n1, (e1), r, (buffer.first));

			// Rotate left or right range
			Iter f0_0, f0_1, f1_0, f1_1;
			size_t n0_0, n0_1, n1_0, n1_1;
			if (n0 < n1)
				merge_inplace_left(f0, n0, f1, n1, f0_0, n0_0, f0_1, n0_1, f1_0, n1_0, f1_1, n1_1, r);
			else
				merge_inplace_right(f0, n0, f1, n1, f0_0, n0_0, f0_1, n0_1, f1_0, n1_0, f1_1, n1_1, r);

			// Recurse on each range
			merge_adaptive_n<true>(f0_0, n0_0, f0_1, n0_1, iter_next(f0_1, n0_1), r, buffer);
			merge_adaptive_n<true>(f1_0, n1_0, f1_1, n1_1, iter_next(f1_1, n1_1), r, buffer);
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(f0, e1), "");
		}

		template<class Iter, class Cmp>
		Iter insertion_sort_n(Iter begin, size_t count, Cmp& l)
		{
			// Standard in-place insertion sort working on bidirectional iterators,
			// but using a number of values to sort instead of an end iterator.

			using T = typename std::iterator_traits<Iter>::value_type;
			if SEQ_UNLIKELY (count < 2)
				return count == 0 ? begin : iter_next(begin);

			auto cur = begin;
			auto prev = cur++;
			for (; count > 1; --count) {
				if (l(*cur, *prev)) {
					auto sift = cur;
					T tmp = std::move(*sift);
					do {
						*sift = std::move(*prev);
						--sift;
					} while (sift != begin && l(tmp, *(--prev)));
					*sift = std::move(tmp);
				}
				prev = cur++;
			}
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(begin, cur, l), "");
			return cur;
		}

		template<class Iter, class Cmp>
		static void reverse_sort(Iter begin, Iter end, Cmp& l)
		{
			// Inplace reverse range.
			// The range must be sorted in descending order.
			// Stable version of std::reverse.

			if (begin == end)
				return;
			Iter start = begin;
			Iter prev = begin++;
			while (begin != end) {
				// Loop through non equal values
				while (l(*begin, *prev)) {
					prev = begin++;
					if SEQ_UNLIKELY (begin == end)
						goto reverse_full;
				}
				// Find full equal range and reverse it
				Iter start_equal = prev++;
				++begin;
				while (begin != end && !l(*begin, *prev))
					prev = begin++;
				std::reverse((start_equal), (begin));
			}

		reverse_full:
			// Reverse the full sequence.
			// Equal ranges will get back their natural orders.
			std::reverse((start), (end));
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(start, end, [l](const auto& a, const auto& b) { return l(a, b); }), "");
		}

		template<class Iter, class Cmp, class Storage>
		static void ping_pong_merge_4(Iter it0, Iter it1, Iter it2, Iter it3, Iter it4, Cmp& c, Storage& tmp)
		{
			// Ping pong merge 4 sorted ranges using provided buffer.

			if ((size_t)(it4 - it0) <= tmp.size) {
				const bool s0 = !c(*it1, *iter_prev(it1));
				const bool s1 = !c(*it2, *iter_prev(it2));
				const bool s2 = !c(*it3, *iter_prev(it3));
				if (s0 && s1 && s2)
					return;

				decltype(tmp.first) middle, end;
				if (!s0)
					middle = merge_move_bidirectional<Unspecified>((it0), (it1), (it1), (it2), tmp.first, c);
				else {
					auto dst = std::move((it0), (it1), tmp.first);
					middle = std::move((it1), (it2), dst);
				}
				if (!s2)
					end = merge_move_bidirectional<Unspecified>((it2), (it3), (it3), (it4), middle, c);
				else {
					auto dst = std::move((it2), (it3), middle);
					end = std::move((it3), (it4), dst);
				}
				if (c(*middle, *iter_prev(middle)))
					merge_move_bidirectional<Unspecified>(tmp.first, (middle), (middle), (end), it0, c, &it4);
				else {
					auto dst = std::move((tmp.first), (middle), it0);
					std::move((middle), (end), dst);
				}
			}
			else {
				merge_adaptive_n<true>(it0, it1 - it0, it1, it2 - it1, it2, c, tmp);
				merge_adaptive_n<true>(it2, it3 - it2, it3, it4 - it3, it4, c, tmp);
				merge_adaptive_n<true>(it0, it2 - it0, it2, it4 - it2, it4, c, tmp);
			}
		}
		template<class Iter, class Cmp, class Storage>
		static void ping_pong_merge_3(Iter it0, Iter it1, Iter it2, Iter it3, Cmp& c, Storage& tmp) 
		{
			// Ping pong merge 3 sorted ranges using provided buffer.

			if ((size_t)(it2 - it0) <= tmp.size) {
				const bool s0 = !c(*it1, *iter_prev(it1));
				const bool s1 = !c(*it2, *iter_prev(it2));
				if (s0 && s1)
					return;

				auto middle = tmp.first;
				if (!s0)
					middle = merge_move_bidirectional<Unspecified>((it0), (it1), (it1), (it2), tmp.first, c);
				else {
					auto dst = std::move((it0), (it1), tmp.first);
					middle = std::move((it1), (it2), dst);
				}
				if (c(*it2, *iter_prev(middle)))
					merge_forward<true>(std::make_move_iterator(tmp.first), std::make_move_iterator(middle), std::make_move_iterator(it2), std::make_move_iterator(it3), it0, c);
				else
					std::move((tmp.first), (middle), it0);
			}
			else {
				merge_adaptive_n<true>(it0, it1 - it0, it1, it2 - it1, it2, c, tmp);
				merge_adaptive_n<true>(it0, it2 - it0, it2, it3 - it2, it3, c, tmp);
			}
		}

		template<class Iter, class Cmp, class Buffer, class Out = typename std::iterator_traits<Iter>::pointer>
		static void merge_sorted_runs_with_buffer(Iter* iters, size_t start, size_t last, Cmp& cmp, Buffer& buf) 
		{
			// Inplace merge already sorted ranges represented by an array of iterators.
			// Supports bidirectional iterators.
			// Internally calls merge_adaptive_n with provided buffer.

			auto size = last - start;

			if (size > 4) {
				auto quarter = size / 4;
				auto half = quarter * 2;
				auto quarter3 = quarter * 3;
				// merge each 4 ranges
				merge_sorted_runs_with_buffer(iters, start, start + quarter, cmp, buf);
				merge_sorted_runs_with_buffer(iters, start + quarter, start + half, cmp, buf);
				merge_sorted_runs_with_buffer(iters, start + half, start + quarter3, cmp, buf);
				merge_sorted_runs_with_buffer(iters, start + quarter3, last, cmp, buf);
				return ping_pong_merge_4(iters[start], iters[start + quarter], iters[start + half], iters[start + quarter3], iters[last], cmp, buf);
			}

			switch (size) {
				case 4:
					ping_pong_merge_4(iters[start], iters[start + 1], iters[start + 2], iters[start + 3], iters[start + 4], cmp, buf);
					break;
				case 3:
					ping_pong_merge_3(iters[start], iters[start + 1], iters[start + 2], iters[start + 3], cmp, buf);
					break;
				case 2:
					merge_adaptive_n<true>(iters[start], iters[start + 1] - iters[start], iters[start + 1], iters[last] - iters[start + 1], iters[last], cmp, buf);
					break;
				default:
					break;
			}
		}

		template<class T>
		static SEQ_ALWAYS_INLINE void swap_branchless(T& a, T& b, bool b_is_less) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_constructible_v<T>)
		{
			// Swap elements based on is_less.
			// Uses branchless swap for small trivial types.
			
			using std::swap;
			if (b_is_less)
				swap(a, b);
			
		}

		template<class Iter, class Cmp, class Buffer>
		static void small_sort_8(Iter it, Cmp& cmp, Buffer& b)
		{
			swap_branchless(*it, it[1], cmp(it[1], *it));
			swap_branchless(it[2], it[3], cmp(it[3], it[2]));
			swap_branchless(it[4], it[5], cmp(it[5], it[4]));
			swap_branchless(it[6], it[7], cmp(it[7], it[6]));

			ping_pong_merge_4(it, it + 2, it + 4, it + 6, it + 8, cmp, b);
		}

		template<size_t N, class Iter, class Cmp, class Buffer>
		static SEQ_ALWAYS_INLINE Iter atom_sort_8(Iter vals, size_t count, Cmp& cmp, Buffer& b) 
		{
			// Sort up to 8 values

			if constexpr (is_random_access<Iter>::value && N == 8) {
				if (count == 8) {
					// Sort exactly 8 values 
					small_sort_8(vals, cmp, b);
					return vals + 8;
				}
			}

			return insertion_sort_n(vals, N == Unspecified ? count : N, cmp);
		}

		template<class Iter, class Out, class Cmp>
		static Out atom_sort_64(Iter& first, Out out, Cmp& c)
		{
			// Sort 64 values to output

			
			using type = std::decay_t<typename std::iterator_traits<Iter>::value_type>;
			type buff[8];
			buffer<type*> b{ buff, 8 };


			auto it0 = atom_sort_8<8>(first, 8, c, b);
			auto it1 = atom_sort_8<8>(it0, 8, c, b);
			auto it2 = atom_sort_8<8>(it1, 8, c, b);
			auto it3 = atom_sort_8<8>(it2, 8, c, b);
			auto it4 = atom_sort_8<8>(it3, 8, c, b);
			auto it5 = atom_sort_8<8>(it4, 8, c, b);
			auto it6 = atom_sort_8<8>(it5, 8, c, b);
			auto it7 = atom_sort_8<8>(it6, 8, c, b);

			auto o0 = merge_move_bidirectional<8>(first, it0, it0, it1, out, c);
			auto o1 = merge_move_bidirectional<8>(it1, it2, it2, it3, o0, c);
			auto o2 = merge_move_bidirectional<8>(it3, it4, it4, it5, o1, c);
			auto o3 = merge_move_bidirectional<8>(it5, it6, it6, it7, o2, c);
			auto d0 = merge_move_bidirectional<16>(out, o0, o0, o1, first, c);
			auto d1 = merge_move_bidirectional<16>(o1, o2, o2, o3, d0, c);
			auto r = merge_move_bidirectional<32>(first, d0, d0, d1, out, c);
			first = d1;
			return r;
		}

		template<class Iter, class Cmp, class Buffer>
		static Iter sort_128(Iter vals, size_t count, Cmp& c, Buffer& buf)
		{
			// Sort inplace up to 128 values using provided buffer

			if (count == 128 && buf.size >= 128) {
				auto src = (vals);
				auto it0 = atom_sort_64((src), (buf.first), c);
				auto it1 = atom_sort_64((src), (it0), c);
				return merge_move_bidirectional<64>((buf.first), (it0), (it0), (it1), vals, c);
			}
			else {
				// Buffer too small or less than 128 elements
				Iter iters[17] = { vals };
				size_t cnt = 1;
				size_t rem = count;
				Iter src = vals;
				while (rem) {
					auto p = atom_sort_8<Unspecified>(src, std::min<size_t>(rem, 8u), c, buf);
					rem -= (size_t)(p - src);
					iters[cnt++] = src = p;
				}
				merge_sorted_runs_with_buffer(iters, 0, cnt - 1, c, buf);
				return src;
			}
		}

		template<size_t IterCount, class T, class Cmp, class Buffer>
		static std::pair<T, size_t> try_wave_sort(T begin, size_t size, size_t min_dist, Cmp& c, Buffer& buf)
		{
			// Attempt to sort the range [begin,end).
			// Find consecutive sorted runs (ascending or descending), up to IterCount-1.
			// Stop when reaching end iterator or runs limit.
			// Supports bidirectional iterators.

			if SEQ_UNLIKELY (size == 0)
				return { begin, 0 };

			T start = begin;
			T prev = begin;
			T iters[IterCount] = { begin++ };
			bool ascending[IterCount];
			size_t cnt = 1;
			size_t dist = 1;

			if (size == 1)
				return { begin, 1 };

			ascending[0] = !c(*begin, *prev);
			prev = begin;
			++begin;
			++dist;

			for (; dist != size; ++begin, ++prev, ++dist) {
				// Find consecutive ascending or descending runs
				const bool as = ascending[cnt - 1];
				if (as) {
					for (; dist != size && !c(*begin, *prev); ++dist)
						prev = begin++;
				}
				else {
					for (; dist != size && !c(*prev, *begin); ++dist)
						prev = begin++;
				}

				// Stop before adding the start of a new sorted range,
				// or the last range will have a size of 1...
				if (cnt == (IterCount - 1))
					break;

				ascending[cnt] = !as;
				iters[cnt++] = begin;
				if (cnt > 1 && begin == iter_next(iters[cnt - 2])) {
					ascending[cnt - 2] = !as;
					--cnt;
				}
				if SEQ_UNLIKELY (dist == size)
					break;
			}

			if (dist < min_dist)
				// We were not able to sort up to min_dist elements,
				// returns the start iterator to notify this failure.
				return { start, 0 };

			// add last iterator
			if (iters[cnt - 1u] != begin)
				iters[cnt++] = begin;

			// reverse descending ranges
			for (size_t i = 0; i < cnt - 1; ++i) {
				if (!ascending[i])
					// stable reverse
					reverse_sort(iters[i], iters[i + 1], c);
			}

			// inplace merge runs
			merge_sorted_runs_with_buffer(iters, 0u, cnt - 1u, c, buf);
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(start, begin, c), "");
			return { begin, dist };
		}

		template<size_t MaxIters, class Iter, class Cmp, class Fn, class Buff>
		static void generic_merge_sort_internal(Iter begin, Iter end, size_t size, Cmp& l, Fn sort_sub_range, Buff buf, size_t min_size = 0)
		{
			// Generic merge sort that uses a custom sort function for small chunks.
			// Supports bidirectional iterators.

			size_t remaining = size;
			Iter start = begin;
			Iter last_start = begin;
			size_t new_min_size = std::numeric_limits<size_t>::max();

			do {
				size_t cnt = 1;
				size_t cum_dist = 0;
				Iter iters[MaxIters] = { begin };

				do {
					// Sort any number of elements (up to remaining).
					// sort_sub_range must return a std::pair<Iter,size_t>.
					std::pair<Iter, size_t> r;
					if (min_size == 0)
						// First pass: sort input chunk
						r = sort_sub_range(begin, remaining, l);
					else {
						// Next passes : identify sorted range
						if (remaining >= min_size) {
							// We need to identify the sorted range with is_sorted_until()

							// Increment it by min_size -1, starting from begin or from end
							auto it = begin;

							if constexpr (!is_random_access<Iter>::value) {
								size_t d = (size_t)iter_distance(begin, end);
								if (min_size > d / 2)
									it = iter_prev(end, d - min_size + 1);
							}
							if (it == begin)
								it = iter_next(begin, min_size - 1);

							auto p = std::is_sorted_until((it), (end), l);
							r = { p, min_size - 1 + (size_t)iter_distance(it, p) };
						}
						else {
							// No need to call is_sorted_until(), we know
							// the sorted range goes to the end.
							r = { end, remaining };
							SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(begin, r.first, l), "");
						}
					}
					SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(begin, r.first, l), "");

					// Store new start position
					// and update remaining elements

					iters[cnt++] = begin = r.first;
					remaining -= r.second;
					cum_dist += r.second;

				} while (remaining != 0 && cnt < MaxIters);

				if (remaining == 0 && cnt > 1 && iters[cnt - 1] == iters[cnt - 2]) {
					// Special case when remaining is 0: the last iterator might be equal to the previous one.
					// We must remove it to avoid range of size 0
					--cnt;
				}

				// Merge all sorted runs
				merge_sorted_runs_with_buffer(iters, 0u, cnt - 1u, l, buf);

				// Update new min size if this is NOT the last chunk
				if (remaining || new_min_size == std::numeric_limits<size_t>::max()) {
					new_min_size = std::min(new_min_size, cum_dist);
				}

				last_start = iters[0];
				SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(iters[0], iters[cnt - 1u], l), "");

			} while (remaining != 0);

			// Compute end iterator during the first pass
			if (min_size == 0)
				end = begin;

			if (last_start != start) {
				// We need a next pass if the number of iterators was not enough
				return generic_merge_sort_internal<MaxIters>(start, end, size, l, sort_sub_range, buf, new_min_size);
			}
		}

		template<class Iter, class Cmp, class Buffer>
		static void merge_sort_internal(Iter begin, size_t size, Cmp& l, Buffer buf)
		{
			// Bottom-up merge sort.
			// Small chunks of up to 128 elements are sorted using insertion sort or sorting network and ping-pong merge.
			// If possible, use wave sort on longer runs.
			// Supports bidirectional iterators.

			if (size < 128) {
				auto r = try_wave_sort<5>(begin, size, size, l, buf);
				if (r.first == begin)
					//  Failed
					sort_128(begin,size, l, buf);
				return;
			}

			generic_merge_sort_internal<65>(
			  begin,
			  begin,
			  size,
			  l,
			  [&](Iter b, size_t remaining, auto l) {
				  // Try wave sort first, as it might consume a lot more
				  // than the default 128 elements (possibly the whole sequence)
				  auto r = try_wave_sort<5>(b, remaining, std::min(remaining, (size_t)128u), l, buf);
				  if (r.first != b) {
					  //  Success, retrieve new start position and number of sorted elements
					  return r;
				  }

				  // Failure, use sort_128()
				  size_t cnt = std::min(remaining, (size_t)128u);
				  auto it = sort_128(b, cnt, l, buf);
				  return std::make_pair(it, (size_t)cnt);

			  },
			  buf);

			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(begin, iter_next(begin, size), l), "");
		}

		// Create the internal buffer used by net_sort()
		template<class Buffer>
		static size_t sort_buffer_size(const Buffer& buf, size_t count) noexcept
		{
			// Default buffer
			if (buf.size == 0)
				return (count);

			// No buffer
			if (buf.size == std::numeric_limits<size_t>::max())
				return 0;

			size_t ret = 0;
			if (buf.size == std::numeric_limits<size_t>::max() - 1)
				// Medium buffer
				ret = (count / 8u);
			else if (buf.size == std::numeric_limits<size_t>::max() - 2)
				// Small buffer
				ret = (count / 32u);
			else
				// Tiny buffer
				ret = (count / 128u);

			if (ret == 0) 
				ret = std::min((size_t)16, count);
			
			return ret;
		}

	} // end algo_detail

	

	/// @brief Default sort buffer size, uses input size/2 elements
	static constexpr buffer<void*> default_buffer{ nullptr, 0 };

	/// @brief Medium sort buffer size, uses input size/16 elements
	static constexpr buffer<void*> medium_buffer{ nullptr, std::numeric_limits<size_t>::max() - 1u };

	/// @brief Small sort buffer size, uses input size/64 elements
	static constexpr buffer<void*> small_buffer{ nullptr, std::numeric_limits<size_t>::max() - 2u };

	/// @brief Tiny sort buffer size, uses input size/256 elements
	static constexpr buffer<void*> tiny_buffer{ nullptr, std::numeric_limits<size_t>::max() - 3u };

	/// @brief Null buffer, uses (slow) bufferless merge sort
	static constexpr buffer<void*> null_buffer{ nullptr, std::numeric_limits<size_t>::max() };

	/// @brief Stable merge algorithm similar to std::merge.
	///
	/// This algorithm is usually more efficient than regular
	/// std::merge, at least on msvc. It provides a better
	/// handling of consecutive ordered values, and has a
	/// special case for unbalanced merging (one range is
	/// way smaller than the other).
	///
	template<class Iter1, class Iter2, class Out, class Cmp = std::less<>>
	SEQ_ALWAYS_INLINE Out merge(Iter1 first1, Iter1 last1, Iter2 first2, Iter2 last2, Out out, Cmp c = Cmp())
	{
		return algo_detail::merge_forward<false>(first1, last1, first2, last2, out, c);
	}

	/// @brief Inplace stable merge algorithm similar to std::inplace_merge.
	///
	/// The main difference with std::inplace_merge is that this version
	/// uses a user provided buffer for merging. This function only allocate
	/// memory if provided buffer is one of 'default_buffer', 'medium_buffer',
	/// 'small_buffer' or 'tiny_buffer'. 0 sized buffer are supported (but way slower).
	///
	template<class Iter, class Cmp = std::less<>, class Buffer = buffer<void*>>
	void inplace_merge(Iter first, Iter middle, Iter last, Cmp c = Cmp(), Buffer buf = Buffer())
	{
		using namespace algo_detail;
		using Key = typename std::iterator_traits<Iter>::value_type;

		if (middle == first || middle == last)
			return;

		// Perform the firsts, easy checks
		if (!c(*middle, *std::prev(middle)))
			// One of the 2 ranges is empty, or already sorted
			return;

		if (c(*std::prev(last), *first)) {
			// Simple rotation needed
			std::rotate(first, middle, last);
			return;
		}

		// The following checks come from msvc STL implementation
		// and help a LOT in some situations

		// Increment first as long as it is smaller than first value of second range
		for (;;) {
			if (first == middle)
				// We reach the end of first range: already in order
				return;
			if (c(*middle, *first))
				break;
			++first;
		}

		auto highest = iter_prev(middle);
		do {
			--last;
			if (middle == last) { // rotate only element remaining in right partition to the beginning, without allocating
				rotate_one_right(first, middle, ++last);
				return;
			}
		} while (!c(*last, *highest)); // found that *highest goes in *last's position

		++last;

		// Now go through merge_adaptive_n

		auto s1 = (size_t)std::distance(first, middle);
		auto s2 = (size_t)std::distance(middle, last);

		if constexpr (std::is_same_v<buffer<void*>, Buffer>) {
			// Compute buffer size
			size_t min_size = std::min(s1, s2);
			size_t buf_size = sort_buffer_size(buf, min_size);
			std::vector<Key> buf_(buf_size);
			return merge_adaptive_n<false>(wrap_iter(first), s1, wrap_iter(middle, s1), s2, wrap_iter(last, s1 + s2), c, buffer<Key*>{ buf_.data(), buf_.size() });
		}
		else
			// Use provided buffer
			return merge_adaptive_n<false>(wrap_iter(first), s1, wrap_iter(middle, s1), s2, wrap_iter(last, s1 + s2), c, buf);
	}

	/// @brief Inplace merge count-1 consecutive sorted ranges.
	///
	/// The first argument is an array of count iterators representing count-1 consecutive sorted ranges.
	///
	/// This function only allocate memory if provided buffer is one of 'default_buffer', 'medium_buffer',
	/// 'small_buffer' or 'tiny_buffer'. 0 sized buffer are supported (but way slower).
	///
	template<class Iter, class Cmp = std::less<>, class Buffer = buffer<void*>>
	void inplace_merge(Iter* iters, size_t count, Cmp c = Cmp(), Buffer buf = Buffer())
	{
		using namespace algo_detail;
		using Key = typename std::iterator_traits<Iter>::value_type;

		if (count <= 2)
			// Nothing to do
			return;

		if constexpr (std::is_same_v<buffer<void*>, Buffer>) {
			// Compute buffer size
			size_t buf_size = sort_buffer_size(buf, std::distance(iters[0], iters[count - 1]) / 2);
			std::vector<Key> buf_(buf_size);
			merge_sorted_runs_with_buffer(iters, 0, count - 1, c, buffer<Key*>{ buf_.data(), buf_.size() });
		}
		else
			// Use provided buffer
			merge_sorted_runs_with_buffer(iters, 0, count - 1, c, buf);
	}

	/// @brief Reverse a range already sorted in descending order while preserving stability.
	///
	/// The full range is reversed except equal values which order is preserved.
	///
	template<class Iter, class Cmp = std::less<>>
	SEQ_ALWAYS_INLINE void reverse_descending(Iter first, Iter last, Cmp c = Cmp())
	{
		algo_detail::reverse_sort(first, last, c);
	}

	/// @brief Stable merge sort algorithm using an external buffer.
	///
	/// net sort is a merge sort algorithm with the following specificities:
	///
	///		-	Bottom-up merging instead of the more traditional top-down approach,
	///		-	Bidirectional merging is used for relocatable types,
	///		-	Ping-pong merge is used to merge 4 sorted ranges,
	///		-	Can work without allocating memory through a (potentially empty) user provided buffer,
	///		-	Works on bidirectional iterators.
	///
	/// If provided buffer is one of 'seq::default_buffer', 'seq::medium_buffer', 'seq::small_buffer'
	/// or 'seq::tiny_buffer', this function will try to allocate memory.
	///
	/// From my tests on multiple input types, net_sort() is usually faster than std::stable_sort().
	///
	/// net_sort_size() and net_sort() work on bidirectional iterators.
	/// Using net_sort_size() instead of net_sort() is faster when the range size is already known.
	/// 
	/// Underlying type must be default constructible and move assignable/constructible. 
	///
	/// All credits to scandum (https://github.com/scandum) for its quadsort algorithm from which
	/// I took several ideas (bidirectional merge and ping-pong merge).
	///
	template<class Iter, class Cmp = std::less<>, class Buffer = buffer<void*>>
	void net_sort_size(Iter begin, size_t size, Cmp cmp = Cmp(), Buffer buf = Buffer())
	{
		using namespace algo_detail;
		using Key = typename std::iterator_traits<Iter>::value_type;
		using Cat = typename std::iterator_traits<Iter>::iterator_category;
		
		static_assert(std::is_base_of_v<std::bidirectional_iterator_tag, Cat>, "net_sort requires at least a bidirectional iterator");

		if constexpr (!std::is_same_v<Buffer, buffer<void*>>) {
			using BufferIter = decltype(buf.first);
			using BufferValue = typename std::iterator_traits<BufferIter>::value_type;
			static_assert(std::is_same_v<BufferValue, Key>);
		}

		if (size < 16) {
			insertion_sort_n(begin, size, cmp);
			return;
		}

		if constexpr (std::is_same_v<buffer<void*>, Buffer>) {
			// Compute buffer size
			size_t buf_size = sort_buffer_size(buf, size / 2);
			std::vector<Key> buf_(buf_size);
			return merge_sort_internal(wrap_iter(begin, 0), size, cmp, seq::buffer<Key*>{ buf_.data(), buf_.size() });
		}
		else {
			// Use provided buffer
			return merge_sort_internal(wrap_iter(begin, 0), size, cmp, buf);
		}
	}

	/// @brief Stable merge sort algorithm using an external buffer.
	///
	/// net sort is a merge sort algorithm with the following specificities:
	///
	///		-	Bottom-up merging instead of the more traditional top-down approach,
	///		-	Bidirectional merging is used for relocatable types,
	///		-	Ping-pong merge is used to merge 4 sorted ranges,
	///		-	Can work without allocating memory through a (potentially null) user provided buffer,
	///		-	Works on bidirectional iterators.
	///
	/// If provided buffer is one of 'seq::default_buffer', 'seq::medium_buffer', 'seq::small_buffer'
	/// or 'seq::tiny_buffer', this function will try to allocate memory.
	///
	/// From my tests on multiple input types, net_sort() is usually faster than std::stable_sort().
	///
	/// net_sort_size() and net_sort() work on bidirectional iterators.
	/// Using net_sort_size() instead of net_sort() is faster when the range size is already known.
	///
	/// Underlying type must be default constructible and move assignable/constructible. 
	/// 
	/// Full credits to scandum (https://github.com/scandum) for its quadsort algorithm from which
	/// I took several ideas (bidirectional merge and ping-pong merge).
	///
	template<class Iter, class Cmp = std::less<>, class Buffer = buffer<void*>>
	SEQ_ALWAYS_INLINE void net_sort(Iter begin, Iter end, Cmp cmp = Cmp(), Buffer buffer = Buffer())
	{
		net_sort_size(begin, std::distance(begin, end), cmp, buffer);
	}

} // end namespace seq

#endif
