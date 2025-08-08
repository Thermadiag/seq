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

#ifndef SEQ_ALGORITHM_HPP
#define SEQ_ALGORITHM_HPP

#include "bits.hpp"
#include "type_traits.hpp"
#include "internal/concurrent_hash_table.hpp"

#include <iterator>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <vector>


#define SEQ_ALGO_ASSERT_DEBUG(condition, msg) SEQ_ASSERT_DEBUG(condition, msg)

namespace seq
{
	namespace algo_detail
	{
		// Unspecified length
		static constexpr unsigned Unspecified = (unsigned)-1;

		
		
		

		/// @brief Iterator wrapper for bidirectional iterator
		template<class Iter>
		class IterWrapper 
		{
			template<class Diff>
			void increment_iter(Iter& it, Diff d)
			{
				if constexpr (has_plus_equal<Iter>::value)
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
			ptrdiff_t pos;

			SEQ_ALWAYS_INLINE IterWrapper(Iter it = Iter(), std::ptrdiff_t p = 0) noexcept
			  : iter(it)
			  , pos(p)
			{
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return (*this->iter); }
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> IterWrapper&
			{
				++iter;
				++pos;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> IterWrapper
			{
				IterWrapper it = *this;
				++(*this);
				return it;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> IterWrapper&
			{
				--iter;
				--pos;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> IterWrapper
			{
				IterWrapper it = *this;
				--(*this);
				return it;
			}
			template<class Diff>
			SEQ_ALWAYS_INLINE auto operator+=(Diff diff) noexcept -> IterWrapper&
			{
				pos += static_cast<std::ptrdiff_t>(diff);
				increment_iter(iter, static_cast<std::ptrdiff_t>(diff));
				return *this;
			}
			template<class Diff>
			SEQ_ALWAYS_INLINE auto operator-=(Diff diff) noexcept -> IterWrapper&
			{
				pos -= static_cast<std::ptrdiff_t>(diff);
				increment_iter(iter, -static_cast<std::ptrdiff_t>(diff));
				return *this;
			}
		};
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator+(const IterWrapper<Iter>& it, typename IterWrapper<Iter>::difference_type diff) noexcept
		{
			IterWrapper<Iter> res = it;
			res += diff;
			return res;
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator-(const IterWrapper<Iter>& it, typename IterWrapper<Iter>::difference_type diff) noexcept
		{
			IterWrapper<Iter> res = it;
			res -= diff;
			return res;
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator-(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2) noexcept
		{
			return it1.pos - it2.pos;
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator==(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2) noexcept
		{
			return it1.iter == it2.iter;
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator!=(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2) noexcept
		{
			return it1.iter != it2.iter;
		}
		

		// noexcept specifier for inplace merge/sort
		template<class T, class Cmp>
		struct NothrowSort
		{
			// Check if sorting given type with given comparator does not throw
			static constexpr bool value = std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value && std::is_nothrow_constructible<T>::value &&
						      noexcept(std::declval<Cmp&>()(std::declval<T&>(), std::declval<T&>()));
		};
		// noexcept specifier for inplace merge/sort
		template<class Iter, class Cmp>
		struct NothrowSortIter : public NothrowSort<typename std::iterator_traits<Iter>::value_type, Cmp>
		{
		};

		// Interpret element as bitfield for relocatable types
		template<class T>
		struct Bitfield
		{
			alignas(T) char data[sizeof(T)];
		};
		template<class T>
		SEQ_ALWAYS_INLINE Bitfield<T>& as_bits(T& ref) noexcept
		{
			return (Bitfield<T>&)ref;
		}
		template<class T>
		SEQ_ALWAYS_INLINE Bitfield<T>& as_bits(T&& ref) noexcept
		{
			return (Bitfield<T>&)ref;
		}
		template<class T>
		SEQ_ALWAYS_INLINE T& as_type(Bitfield<T>& d)
		{
			return (T&)d;
		}

		// similar to std::next, but use ++ operator when no distance is specified
		template<class Iter>
		SEQ_ALWAYS_INLINE auto iter_next(Iter it) noexcept
		{
			return ++it;
		}
		template<class Iter, class Diff>
		SEQ_ALWAYS_INLINE auto iter_next(Iter it, Diff d) noexcept
		{
			return it + d;
		}

		// similar to std::prev, but use -- operator when no distance is specified
		template<class Iter>
		SEQ_ALWAYS_INLINE auto iter_prev(Iter it) noexcept
		{
			return --it;
		}
		template<class Iter, class Diff>
		SEQ_ALWAYS_INLINE auto iter_prev(Iter it, Diff d) noexcept
		{
			return it - d;
		}

		// compare iterators for equality without triggering compile error
		template<class Iter1, class Iter2>
		static SEQ_ALWAYS_INLINE bool iter_equal(Iter1 it1, Iter2 it2) noexcept
		{
			return false;
		}
		template<class Iter1>
		static SEQ_ALWAYS_INLINE bool iter_equal(Iter1 it1, Iter1 it2) noexcept
		{
			return it1 == it2;
		}

		// similar to std::distance with an overload for IterWrapper that supports subtracting 2 iterators
		template<class Iter>
		static SEQ_ALWAYS_INLINE auto iter_distance(Iter first, Iter last) noexcept
		{
			return std::distance(first, last);
		}
		template<class Iter>
		static SEQ_ALWAYS_INLINE auto iter_distance(const IterWrapper<Iter>& first, const IterWrapper<Iter>& last) noexcept
		{
			return last - first;
		}
		
		template<class Iter>
		auto wrap_iter(Iter it, ptrdiff_t d = 0)
		{
			if constexpr (is_random_access<Iter>::value)
				return it;
			else
				return IterWrapper<Iter>(it, d);
		}


		template<class Iter, class Cmp>
		static void merge_inplace_left_subproblem(Iter f0,
							  size_t n0,
							  Iter f1,
							  size_t n1,
							  Iter& f0_0,
							  size_t& n0_0,
							  Iter& f0_1,
							  size_t& n0_1,
							  Iter& f1_0,
							  size_t& n1_0,
							  Iter& f1_1,
							  size_t& n1_1,
							  Cmp r) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Subroutine of inplace_merge_n
			SEQ_ALGO_ASSERT_DEBUG(iter_distance(f0, f1) == n0, "");
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
		static void merge_inplace_right_subproblem(Iter f0,
							   size_t n0,
							   Iter f1,
							   size_t n1,
							   Iter& f0_0,
							   size_t& n0_0,
							   Iter& f0_1,
							   size_t& n0_1,
							   Iter& f1_0,
							   size_t& n1_0,
							   Iter& f1_1,
							   size_t& n1_1,
							   Cmp r) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Subroutine of inplace_merge_n
			SEQ_ALGO_ASSERT_DEBUG(iter_distance(f0, f1) == n0, "");
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
		SEQ_ALWAYS_INLINE Out copy_internal(Iter begin, Iter end, Out out)
		{
			// direct std::copy call
			return std::copy((begin), (end), (out));
		}
		template<class Iter, class Out>
		SEQ_ALWAYS_INLINE Out copy_internal(std::move_iterator<Iter> begin, std::move_iterator<Iter> end, Out out)
		{
			// Let the compiler decide to use memmove if necesary
			using type = typename std::iterator_traits<Iter>::value_type;
			return std::move((begin.base()), (end.base()), (out));
		}


		template<bool Overlap, class Iter1, class Iter2, class Out, class Cmp>
		inline Out merge_forward(Iter1 first1, Iter1 end1, Iter2 first2, Iter2 end2, Out out, Cmp c)
		{
			// Merge 2 range forward
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(first1, end1, c), "");
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(first2, end2, c), "");
			SEQ_DEBUG_ONLY(Out dst = out;)

			if constexpr (is_random_access<Iter1>::value && is_random_access<Iter2>::value) {

				// Check for unbalanced merge
				auto s1 = iter_distance(first1, end1);
				auto s2 = iter_distance(first2, end2);

				if (s1 * 32 < s2) {
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
				else if (s2 * 32 < s1) {
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
		static SEQ_ALWAYS_INLINE std::pair<bool, bool> merge_tails(Iter* first, Iter* second, Out& out_left, Out& out_right, Cmp c)
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
		static SEQ_ALWAYS_INLINE void finish_bidirectional_merge(Iter* first, Iter* second, Out out_left, Cmp c)
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
				std::move((first[0]), (++second[0]), ( out_left));
		}

		template<unsigned Count, class Iter, class Out, class Cmp>
		static Out merge_move_bidirectional(Iter first1, Iter last1, Iter first2, Iter last2, Out out, Cmp c, Out* out_end = nullptr) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			using T = typename std::iterator_traits<Iter>::value_type;

			// Merge 2 sorted ranges to given output.
			// Uses the fastest available method: standard forward merge
			// or branchless merge from both ends for random access iterators and relocatable types.

			if constexpr ( is_random_access<Iter>::value  && is_relocatable<T>::value) {

				// Branchless merge from both ends
				// Only truly faster with trivial comparison function,
				// which is usually the case for relocatable types.

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
					
					ptrdiff_t dist1 = iter_distance(first1, last1);
					ptrdiff_t dist2 = iter_distance(first2, last2);

					Out res = out_end ? *out_end : iter_next(out, dist1 + dist2);
					Out out_right = iter_prev(res);

					// Unbalanced merge from both ends.
					// For the first part of the merge (1/16 of the smallest range),
					// check if the order is pseud random. If not, finish with
					// merge_forward().

					const ptrdiff_t iter_count = std::min(dist1, dist2);
					const ptrdiff_t stop = iter_count / 16;
					ptrdiff_t order = 0;
					ptrdiff_t count = 0;

					std::pair<bool, bool> prev_order = { true, true };

					if (first[0] < second[0] && first[1] < second[1]) {

						prev_order = merge_tails(first, second, out_left, out_right, c);

						while (count < stop && first[0] < second[0] && first[1] < second[1]) {
							auto ord = merge_tails(first, second, out_left, out_right, c);
							order += (ptrdiff_t)(ord.first == prev_order.first) + (ptrdiff_t)(ord.second == prev_order.second);
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
		void merge_backward(Iter1 first1, Iter1 last1, Iter2 first2, Iter2 last2, Out out_end, Cmp c) noexcept(NothrowSortIter<Iter1, Cmp>::value)
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
		static void merge_with_buffer(Iter first, size_t n0, Iter middle, size_t n1, Iter e1, Cmp r, B buffer) noexcept(NothrowSortIter<Iter, Cmp>::value)
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
		void rotate_one_right(Iter first, Iter mid, Iter last)
		{
			// Exchanges the range [first, mid) with [mid, last)
			// pre: distance(mid, last) is 1
			using type = typename std::iterator_traits<Iter>::value_type;
			type tmp(std::move(*mid));
			std::move_backward((first), (mid), (last));
			*first = std::move(tmp);
		}

		template<bool FirstChecks, class Iter, class Cmp, class B>
		static void merge_adaptive_n(Iter f0, size_t n0, Iter f1, size_t n1, Iter e1, Cmp r, B buffer) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Inplace merge with buffer, first published by Dudzin'sky and Dydek in 1981 IPL 12(1):5-8
			// Implementation from: https://www.jmeiners.com/efficient-programming-with-components/15_merge_inplace.html

			SEQ_ALGO_ASSERT_DEBUG(iter_distance(f0, f1) == n0, "");
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(f0, iter_next(f0, n0), r), "");
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(f1, iter_next(f1, n1), r), "");

			if constexpr (FirstChecks) {
				// Perform the firsts, easy checks

				if (!n0 || !n1 || !r(*f1, *iter_prev(f1)))
					// One of the 2 ranges is empty, or already sorted
					return;

				if (r(*iter_prev(e1), *f0)) {
					// Simple rotation needed
					std::rotate((f0),(f1), (e1));
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
				merge_inplace_left_subproblem(f0, n0, f1, n1, f0_0, n0_0, f0_1, n0_1, f1_0, n1_0, f1_1, n1_1, r);
			else
				merge_inplace_right_subproblem(f0, n0, f1, n1, f0_0, n0_0, f0_1, n0_1, f1_0, n1_0, f1_1, n1_1, r);

			// Recurse on each range
			merge_adaptive_n<true>(f0_0, n0_0, f0_1, n0_1, iter_next(f0_1, n0_1), r, buffer);
			merge_adaptive_n<true>(f1_0, n1_0, f1_1, n1_1, iter_next(f1_1, n1_1), r, buffer);
			SEQ_ALGO_ASSERT_DEBUG(std::is_sorted(f0, e1), "");
		}

		template<class Iter, class Cmp>
		SEQ_ALWAYS_INLINE Iter insertion_sort_n(Iter begin, unsigned count, Cmp l) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Standard in-place insertion sort working on bidirectional iterators,
			// but using a number of values to sort instead of an end iterator.

			using T= typename std::iterator_traits<Iter>::value_type;
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
		static void reverse_sort(Iter begin, Iter end, Cmp l) noexcept(NothrowSortIter<Iter, Cmp>::value)
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
		static void ping_pong_merge_4(Iter it0, Iter it1, Iter it2, Iter it3, Iter it4, Cmp c, Storage tmp) noexcept(NothrowSortIter<Iter, Cmp>::value)
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
					auto dst = std::move((it2),( it3), middle);
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
		static void ping_pong_merge_3(Iter it0, Iter it1, Iter it2, Iter it3, Cmp c, Storage tmp) noexcept(NothrowSortIter<Iter, Cmp>::value)
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
		static SEQ_ALWAYS_INLINE void merge_sorted_runs_with_buffer(Iter* iters, size_t start, size_t last, Cmp cmp, Buffer buf) noexcept(NothrowSortIter<Iter, Cmp>::value)
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
		SEQ_ALWAYS_INLINE void swap_branchless(T&& a, T&& b, bool b_is_less) noexcept
		{
			// Swap elements based on is_less.
			// Uses branchless swap for relocatable types.

			if constexpr (is_relocatable<T>::value) {
				auto tmp = as_bits(a);
				as_bits(a) = b_is_less ? as_bits(b) : tmp;
				as_bits(b) = b_is_less ? tmp : as_bits(b);
			}
			else {
				using std::swap;
				if (b_is_less)
					swap(a, b);
			}
		}

#define CHECK_2(l, r) swap_branchless(a##l, a##r, cmp(a##r, a##l))
#define CHECK_2_NO_OVERLAPP(a, b, c, d)                                                                                                                                                                \
	CHECK_2(a, b);                                                                                                                                                                                 \
	CHECK_2(c, d)
#define CHECK_4_NO_OVERLAPP(a, b, c, d, e, f, g, h)                                                                                                                                                    \
	CHECK_2_NO_OVERLAPP(a, b, c, d);                                                                                                                                                               \
	CHECK_2_NO_OVERLAPP(e, f, g, h)

		template<class T, class Cmp>
		SEQ_ALWAYS_INLINE void network_sort_8(T&& a0, T&& a1, T&& a2, T&& a3, T&& a4, T&& a5, T&& a6, T&& a7, Cmp cmp)
		{
			CHECK_4_NO_OVERLAPP(0, 1, 2, 3, 4, 5, 6, 7);
			CHECK_4_NO_OVERLAPP(0, 2, 1, 3, 4, 6, 5, 7);
			CHECK_4_NO_OVERLAPP(0, 4, 1, 5, 2, 6, 3, 7);
			CHECK_2_NO_OVERLAPP(2, 4, 3, 5);
			CHECK_2_NO_OVERLAPP(1, 4, 3, 6);
			CHECK_2_NO_OVERLAPP(1, 2, 3, 4);
			CHECK_2(5, 6);
		}

#undef CHECK_2
#undef CHECK_2_NO_OVERLAPP
#undef CHECK_4_NO_OVERLAPP

		template<unsigned N, class Iter, class Cmp>
		Iter atom_sort_8(Iter vals, unsigned count, Cmp cmp) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Sort up to 8 values
			if constexpr (is_random_access<Iter>::value ) {
				if (N == 8 || count == 8) {
					// Sort 8 values using a sorting netork
					network_sort_8(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7], cmp);
					return vals + 8;
				}
			}
			
			return insertion_sort_n(vals, N == Unspecified ? count : N, cmp);
		}

		template<class Iter, class Out, class Cmp>
		Out atom_sort_64(Iter& first, Out out, Cmp c) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Sort 64 values to output

			auto it0 = atom_sort_8<8>(first, 8, c);
			auto it1 = atom_sort_8<8>(it0, 8, c);
			auto it2 = atom_sort_8<8>(it1, 8, c);
			auto it3 = atom_sort_8<8>(it2, 8, c);
			auto it4 = atom_sort_8<8>(it3, 8, c);
			auto it5 = atom_sort_8<8>(it4, 8, c);
			auto it6 = atom_sort_8<8>(it5, 8, c);
			auto it7 = atom_sort_8<8>(it6, 8, c);

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
		Iter sort_128(Iter vals, unsigned count, Cmp c, Buffer buf) noexcept(NothrowSortIter<Iter, Cmp>::value)
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
				unsigned cnt = 1;
				unsigned rem = count;
				Iter src = vals;
				while (rem) {
					auto p = atom_sort_8<Unspecified>(src, std::min(rem, 8u), c);
					rem -= (unsigned)(p - src);
					iters[cnt++] = src = p;
				}
				merge_sorted_runs_with_buffer(iters, 0, cnt - 1, c, buf);
				return src;
			}
		}

		template<unsigned IterCount, class T, class Cmp, class Buffer>
		static std::pair<T, size_t> try_wave_sort(T begin, size_t size, size_t min_dist, Cmp c, Buffer buf) noexcept(NothrowSortIter<T, Cmp>::value)
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
			unsigned cnt = 1;
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

		template<unsigned MaxIters, class Iter, class Cmp, class Fn, class Buff>
		static void generic_merge_sort_internal(Iter begin, Iter end, size_t size, Cmp l, Fn sort_sub_range, Buff buf, size_t min_size = 0) noexcept(NothrowSortIter<Iter, Cmp>::value)
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
		static void merge_sort_internal(Iter begin, size_t size, Cmp l, Buffer buf) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Bottom-up merge sort.
			// Small chunks of up to 128 elements are sorted using insertion sort or sorting netwrok and ping-pong merge.
			// If possible, use wave sort on longer runs.
			// Supports bidirectional iterators.

			if (size < 128) {
				auto r = try_wave_sort<5>(begin, size, size, l, buf);
				if (r.first == begin) 
					//  Failed
					sort_128(begin, (unsigned)size, l, buf);
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
				  unsigned cnt = (unsigned)std::min(remaining, (size_t)128u);
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
			if (buf.size == 0)
				return (count);
			if (buf.size == std::numeric_limits<size_t>::max() - 1)
				return (count / 8u);
			if (buf.size == std::numeric_limits<size_t>::max() - 2)
				return (count / 32u);
			return (count / 128u);
		}

	} // end algo_detail

	/// @brief Buffer object used by net_sort(), net_sort_size() and inplace_merge()
	template<class Iter>
	struct buffer
	{
		Iter first;
		size_t size = 0;
	};

	/// @brief Default sort buffer size, uses input size/2 bytes
	static constexpr buffer<void*> default_buffer{ nullptr, 0 };

	/// @brief Medium sort buffer size, uses input size/16 bytes
	static constexpr buffer<void*> medium_buffer{ nullptr, std::numeric_limits<size_t>::max() - 1u };

	/// @brief Small sort buffer size, uses input size/64 bytes
	static constexpr buffer<void*> small_buffer{ nullptr, std::numeric_limits<size_t>::max() - 2u };

	/// @brief Tiny sort buffer size, uses input size/256 bytes
	static constexpr buffer<void*> tiny_buffer{ nullptr, std::numeric_limits<size_t>::max() - 3u };

	/// @brief Null buffer, uses (slow) bufferless merge sort
	static constexpr buffer<void*> null_buffer{ nullptr, 0 };


	/// @brief Stable merge algorithm similar to std::merge.
	///
	/// This algorithm is usually more efficient than regular
	/// std::merge, at least on msvc. It provides a better
	/// handling of consecutive ordered values, and has a
	/// special case for unbalanced merging (one range is
	/// way smaller than the other).
	///
	template<class Iter1, class Iter2, class Out, class Cmp = std::less<>>
	Out merge(Iter1 first1, Iter1 last1, Iter2 first2, Iter2 last2, Out out, Cmp c = Cmp())
	{
		return algo_detail::merge_forward<false>(first1, last1, first2, last2, out, c);
	}

	/// @brief Inplace stable merge algorithm similar to std::inplace_merge.
	///
	/// The main difference with std::inplace_merge is that this version
	/// uses a user provided buffer for merging. This function only allocate
	/// memory if provided buffer is one of 'default_buffer', 'medium_buffer',
	/// 'small_buffer' or 'tiny_buffer'. 0 sized buffer are supported.
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

		if constexpr (std::is_same<buffer<void*>, Buffer>::value) {
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

	template<class Iter, class Cmp = std::less<>, class Buffer = buffer<void*>>
	void inplace_merge(Iter* iters, size_t count, Cmp c = Cmp(), Buffer buf = Buffer())
	{
		using namespace algo_detail;
		using Key = typename std::iterator_traits<Iter>::value_type;

		if (count <= 2)
			// Nothing to do
			return;

		if constexpr (std::is_same<buffer<void*>, Buffer>::value) {
			// Compute buffer size
			size_t buf_size = sort_buffer_size(buf, std::distance(iters[0], iters[count -1])/2);
			std::vector<Key> buf_(buf_size);
			merge_sorted_runs_with_buffer(iters, 0, count - 1, c, buffer<Key*>{ buf_.data(), buf_.size() });
		}
		else
			// Use provided buffer
			merge_sorted_runs_with_buffer(iters, 0, count - 1, c, buf);
	}

	/// @brief Reverse a range already sorted in descending order while preserving stability.
	///
	/// The full range is reversed except equal values
	/// of which order is preserved.
	///
	template<class Iter, class Cmp = std::less<>>
	void reverse_descending(Iter first, Iter last, Cmp c = Cmp())
	{
		algo_detail::reverse_sort(first, last, c);
	}

	/// @brief Stable merge sort algorithm using an external buffer.
	///
	/// net sort is a merge sort algorithm with the following specificities:
	///
	///		-	Bottom-up merging instead of the more traditional top-down approach,
	///		-	Small blocks of 8 elements are sorted using a sorting network,
	///		-	Bidirectional merging is used for relocatable types,
	///		-	Ping-pong merge is used to merge 4 sorted ranges,
	///		-	Can work without allocating memory through a (potentially null) user provided buffer,
	///		-	Works on bidirectional iterators.
	///
	/// If provided buffer is one of 'seq::default_buffer', 'seq::medium_buffer', 'seq::small_buffer'
	/// or 'seq::tiny_buffer', this function will try to allocate memory.
	///
	/// From my tests on multiple input types, net_sort() is always faster than std::stable_sort().
	///
	/// net_sort_size() and net_sort() work on bidirectional iterators.
	/// Using net_sort_size() instead of net_sort() is faster when the range size is already known.
	///
	/// All credits to scandum (https://github.com/scandum) for its quadsort algorithm from which
	/// I took several ideas (bidirectional merge and ping-pong merge).
	///
	template<class Iter, class Cmp = std::less<>, class Buffer = buffer<void*>>
	void net_sort_size(Iter begin, size_t size, Cmp cmp = Cmp(), Buffer buf = Buffer())
	{
		using namespace algo_detail;
		using Key = typename std::iterator_traits<Iter>::value_type;

		if (size < 16) {
			insertion_sort_n(begin, (unsigned)size, cmp);
			return;
		}

		if constexpr (std::is_same<buffer<void*>, Buffer>::value) { 
			// Compute buffer size
			size_t buf_size = sort_buffer_size(buf, size / 2);
			if (buf_size < 16)
				buf_size = 16;
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
	///		-	Small blocks of 8 elements are sorted using a sorting network,
	///		-	Bidirectional merging is used for relocatable types,
	///		-	Ping-pong merge is used to merge 4 sorted ranges,
	///		-	Can work without allocating memory through a (potentially null) user provided buffer,
	///		-	Works on bidirectional iterators.
	///
	/// If provided buffer is one of 'seq::default_buffer', 'seq::medium_buffer', 'seq::small_buffer'
	/// or 'seq::tiny_buffer', this function will try to allocate memory.
	///
	/// From my tests on multiple input types, net_sort() is always faster than std::stable_sort().
	///
	/// net_sort_size() and net_sort() work on bidirectional iterators.
	/// Using net_sort_size() instead of net_sort() is faster when the range size is already known.
	///
	/// Full credits to scandum (https://github.com/scandum) for its quadsort algorithm from which
	/// I took several ideas (bidirectional merge and ping-pong merge).
	///
	template<class Iter, class Cmp = std::less<>, class Buffer = buffer<void*>>
	void net_sort(Iter begin, Iter end, Cmp cmp = Cmp(), Buffer buffer = Buffer())
	{
		net_sort_size(begin, std::distance(begin, end), cmp, buffer);
	}

	namespace algo_detail
	{
		// Hash function working on pointer
		template<class Key, class Hash>
		struct HashPtr : Hash
		{
			HashPtr(const Hash& h = Hash())
			  : Hash(h)
			{
			}
			size_t operator()(const Key* k) const noexcept { return Hash::operator()(*k); }
		};
		// Comparison function working on pointer
		template<class Key, class Equal>
		struct EqualPtr : Equal
		{
			EqualPtr(const Equal& eq = Equal())
			  : Equal(eq)
			{
			}
			size_t operator()(const Key* l, const Key* r) const noexcept { return Equal::operator()(*l, *r); }
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
	Iter unique(Iter first, Iter last, const Hash& h = Hash(), const Equal& eq = Equal())
	{
		using namespace algo_detail;
		using Key = typename std::iterator_traits<Iter>::value_type;
		using HashFn = HashPtr<Key, Hash>;
		using EqualFn = EqualPtr<Key, Equal>;
		using TableType = seq::detail::ConcurrentHashTable<Key*, Key*, HashFn, EqualFn, std::allocator<Key*>, no_concurrency>;

		if (first == last)
			return last;

		auto start = first;

		TableType set{ HashFn(h), EqualFn(eq) };

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
