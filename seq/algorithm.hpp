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

#include <iterator>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <vector>

namespace seq
{
	/// @brief Buffer object used by stable_sort() and inplace_merge()
	/// @tparam Iter
	template<class Iter>
	struct buffer
	{
		Iter first;
		size_t size = 0;
	};

	namespace algo_detail
	{
		using default_sort_buffer = buffer<void*>;
	}

	//
	// Default buffer sizes expressed in input size ratio
	//

	/// @brief Default sort buffer size, uses input size/2 bytes
	static constexpr algo_detail::default_sort_buffer default_buffer{ nullptr, std::numeric_limits<size_t>::max() };
	/// @brief Medium sort buffer size, uses input size/16 bytes
	static constexpr algo_detail::default_sort_buffer medium_buffer{ nullptr, std::numeric_limits<size_t>::max() - 1u };
	/// @brief Small sort buffer size, uses input size/64 bytes
	static constexpr algo_detail::default_sort_buffer small_buffer{ nullptr, std::numeric_limits<size_t>::max() - 2u };
	/// @brief Tiny sort buffer size, uses input size/128 bytes
	static constexpr algo_detail::default_sort_buffer tiny_buffer{ nullptr, std::numeric_limits<size_t>::max() - 3u };
	/// @brief Null buffer, uses (slow) bufferless merge sort
	static constexpr algo_detail::default_sort_buffer null_buffer{ nullptr, 0 };

	namespace algo_detail
	{
		// Check if iterator is random access
		template<class Iter>
		struct is_random_access : std::is_same<std::random_access_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>
		{
		};

		template<class Iter, class IterCat = typename std::iterator_traits<Iter>::iterator_category>
		struct IterPos
		{
			Iter iter;
			std::ptrdiff_t pos = 0;

			SEQ_ALWAYS_INLINE IterPos() noexcept {}
			SEQ_ALWAYS_INLINE IterPos(const Iter& it, std::ptrdiff_t p ) noexcept
			  : iter(it)
			  , pos(p)
			{
			}
			IterPos(const IterPos&) = default;
			IterPos& operator=(const IterPos&) = default;
			SEQ_ALWAYS_INLINE void incr() noexcept
			{
				++pos;
				++iter;
			}
			SEQ_ALWAYS_INLINE void decr() noexcept
			{
				--pos;
				--iter;
			}
			template<class Diff>
			SEQ_ALWAYS_INLINE void incr(Diff d) noexcept
			{
				pos += static_cast<std::ptrdiff_t>(d);
				std::advance(iter, d);
			}
			SEQ_ALWAYS_INLINE auto diff(const IterPos& r) const noexcept { return pos - r.pos; }
			SEQ_ALWAYS_INLINE bool less(const IterPos& r) const noexcept { return pos < r.pos; }
			SEQ_ALWAYS_INLINE bool less_eq(const IterPos& r) const noexcept { return pos <= r.pos; }
			SEQ_ALWAYS_INLINE bool greater(const IterPos& r) const noexcept { return pos > r.pos; }
			SEQ_ALWAYS_INLINE bool greater_eq(const IterPos& r) const noexcept { return pos >= r.pos; }
		};
		template<class Iter>
		struct IterPos<Iter, std::random_access_iterator_tag>
		{
			Iter iter;
			SEQ_ALWAYS_INLINE IterPos() noexcept {}
			SEQ_ALWAYS_INLINE IterPos(const Iter& it, std::ptrdiff_t ) noexcept
			  : iter(it)
			{
			}
			IterPos(const IterPos&) noexcept= default;
			IterPos& operator=(const IterPos&) noexcept = default;

			SEQ_ALWAYS_INLINE void incr() noexcept { ++iter; }
			SEQ_ALWAYS_INLINE void decr() noexcept { --iter; }
			template<class Diff>
			SEQ_ALWAYS_INLINE void incr(Diff d) noexcept
			{
				iter += d;
			}
			SEQ_ALWAYS_INLINE auto diff(const IterPos& r) const noexcept { return iter - r.iter; }
			SEQ_ALWAYS_INLINE bool less(const IterPos& r) const noexcept { return iter < r.iter; }
			SEQ_ALWAYS_INLINE bool less_eq(const IterPos& r) const noexcept { return iter <= r.iter; }
			SEQ_ALWAYS_INLINE bool greater(const IterPos& r) const noexcept { return iter > r.iter; }
			SEQ_ALWAYS_INLINE bool greater_eq(const IterPos& r) const noexcept { return iter >= r.iter; }
		};

		/// @brief Iterator wrapper that provides a random access category interface
		/// Just used to ease implementation without c++17 if constexpr;
		template<class Iter>
		struct IterWrapper : public IterPos<Iter>
		{
			using base_type = IterPos<Iter>;
			using value_type = typename std::iterator_traits<Iter>::value_type;
			using iterator_category = typename std::iterator_traits<Iter>::iterator_category;
			using difference_type = typename std::iterator_traits<Iter>::difference_type;
			using pointer = typename std::iterator_traits<Iter>::pointer;
			using reference = typename std::iterator_traits<Iter>::reference;

			IterWrapper() noexcept = default;
			IterWrapper(const IterWrapper&) noexcept = default;
			IterWrapper(IterWrapper&&) noexcept = default;
			IterWrapper& operator=(const IterWrapper&) noexcept = default;
			IterWrapper& operator=(IterWrapper&&) noexcept = default;
			SEQ_ALWAYS_INLINE IterWrapper(Iter it, std::ptrdiff_t pos) noexcept
			  : base_type(it, pos)
			{
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return const_cast<reference>(*this->iter); }
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> IterWrapper&
			{
				base_type::incr();
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
				base_type::decr();
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
				base_type::incr(diff);
				return *this;
			}
			template<class Diff>
			SEQ_ALWAYS_INLINE auto operator-=(Diff diff) noexcept -> IterWrapper&
			{
				base_type::incr(-static_cast<difference_type>(diff));
				return *this;
			}
			template<class Diff>
			SEQ_ALWAYS_INLINE reference operator[](Diff d) const noexcept
			{
				return *((*this) + d);
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
			auto d1 = it1.iter - it2.iter;
			auto d2 = it1.diff(it2);
			return it1.diff(it2);
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
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator<(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2) noexcept
		{
			return it1.less(it2);
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator>(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2) noexcept
		{
			return it1.greater(it2);
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator<=(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2) noexcept
		{
			return it1.less_eq(it2);
		}
		template<class Iter>
		SEQ_ALWAYS_INLINE auto operator>=(const IterWrapper<Iter>& it1, const IterWrapper<Iter>& it2) noexcept
		{
			return it1.greater_eq(it2);
		}


		//
		// For noexcept specifier
		//

		template<class T, class Cmp>
		struct NothrowSort
		{
			// Check if sorting given type with given comparator does not throw
			static constexpr bool value = std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value && std::is_nothrow_constructible<T>::value &&
						      noexcept(std::declval<Cmp&>()(std::declval<T&>(), std::declval<T&>()));
		};
		template<class Iter, class Cmp>
		struct NothrowSortIter : public NothrowSort<typename std::iterator_traits<Iter>::value_type, Cmp>
		{
		};

		//
		// Several low level sorting helper functions
		//

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
			SEQ_ASSERT_DEBUG(std::distance(f0, f1) == n0, "");
			SEQ_ASSERT_DEBUG(std::is_sorted(f0, std::next(f0, n0), r) && std::is_sorted(f1, std::next(f1, n1), r), "");
			SEQ_ASSERT_DEBUG(n0 > 0, "");
			SEQ_ASSERT_DEBUG(n1 > 0, "");

			f0_0 = f0;
			n0_0 = n0 >> 1;
			f0_1 = f0;
			f0_1 = std::next(f0_1, n0_0);
			f1_1 = std::lower_bound(f1, std::next(f1, n1), *f0_1, r);
			f1_0 = std::rotate(f0_1, f1, f1_1);
			n0_1 = std::distance(f0_1, f1_0);
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
			SEQ_ASSERT_DEBUG(std::distance(f0, f1) == n0, "");
			SEQ_ASSERT_DEBUG(std::is_sorted(f0, std::next(f0, n0), r) && std::is_sorted(f1, std::next(f1, n1), r), "");
			SEQ_ASSERT_DEBUG(n0 > 0, "");
			SEQ_ASSERT_DEBUG(n1 > 0, "");

			f0_0 = f0;
			n0_1 = n1 >> 1;
			f1_1 = f1;
			f1_1 = std::next(f1_1, n0_1);
			f0_1 = std::upper_bound(f0, std::next(f0, n0), *f1_1, r);
			++f1_1;
			f1_0 = std::rotate(f0_1, f1, f1_1);
			n0_0 = std::distance(f0_0, f0_1);
			n1_0 = n0 - n0_0;
			n1_1 = (n1 - n0_1) - 1;
		}

		template<class It>
		struct is_reverse_iterator : std::false_type
		{
		};
		template<class It>
		struct is_reverse_iterator<std::reverse_iterator<It>> : std::true_type
		{
		};

		template<bool Overlap, class Iter1, class Iter2, class Out, class Cmp>
		inline Out merge_move_forward(Iter1 first1, Iter1 end1, Iter2 first2, Iter2 end2, Out out, Cmp c) noexcept(NothrowSortIter<Iter1, Cmp>::value)
		{
			// Merge 2 range forward
			SEQ_ASSERT_DEBUG(std::is_sorted(first1, end1, c), "");
			SEQ_ASSERT_DEBUG(std::is_sorted(first2, end2, c), "");
			SEQ_DEBUG_ONLY(Out dst = out;)

			typename std::iterator_traits<Iter1>::value_type* tmp1;
			typename std::iterator_traits<Iter2>::value_type* tmp2;

			while (first2 != end2) {
				tmp2 = &(*first2);
				while (first1 != end1 && !c(*tmp2, *(tmp1 = &(*first1)))) {
					*out = std::move(*tmp1);
					++out;
					++first1;
				}
				if (first1 == end1)
					break;

				*out = std::move(*tmp2);
				++out;
				++first2;

				tmp1 = &(*first1);
				while (first2 != end2 && c(*(tmp2 = &(*first2)), *tmp1)) {
					*out = std::move(*tmp2);
					++out;
					++first2;
				}
				*out = std::move(*tmp1);
				++out;
				++first1;
			}

			out = std::move(first1, end1, out);
			if (Overlap && iter_equal(first2, out)) { // TODO: find another way (might dereference end)
				// The last range is already inplace:
				// just advance output iterator if not
				// a std::reverse_iterator (used by
				// merge_move_backward which do not use the result).
				if (!is_reverse_iterator<Out>::value)
					out = std::next(out, std::distance(first2, end2));
			}
			else
				out = std::move(first2, end2, out);

			SEQ_ASSERT_DEBUG(std::is_sorted(dst, out, c), "");
			return out;
		}

		template<class Iter1, class Iter2, class Out, class Cmp>
		inline void merge_move_forward_unbalanced(Iter1 first1, Iter1 end1, Iter2 first2, Iter2 end2, Out out, Cmp c) noexcept(NothrowSortIter<Iter1, Cmp>::value)
		{
			// Left is way smaller than right
			for (auto it = first1; it != end1; ++it) {
				if (first2 != end2) {
					while (it != end1 && !c(*first2, *it))
						*out++ = std::move(*it++);
					if (it == end1)
						break;

					auto found = std::lower_bound(first2, end2, *it, c);
					out = std::move(first2, found, out);
					first2 = found;
				}
				*out++ = std::move(*it);
			}
		}
		template<class Iter1, class Iter2, class Out, class Cmp>
		inline void merge_move_backward_unbalanced(Iter1 first1, Iter1 last1, Iter2 first2, Iter2 last2, Out out_end, Cmp c) noexcept(NothrowSortIter<Iter1, Cmp>::value)
		{
			merge_move_forward_unbalanced(std::make_reverse_iterator(last2),
						      std::make_reverse_iterator(first2),
						      std::make_reverse_iterator(last1),
						      std::make_reverse_iterator(first1),
						      std::make_reverse_iterator(out_end),
						      [c](const auto& a, const auto& b) { return c(b, a); });
		}

		template<bool Overlap, class Iter1, class Iter2, class Out, class Cmp>
		void merge_move_backward(Iter1 first1, Iter1 last1, Iter2 first2, Iter2 last2, Out out_end, Cmp c) noexcept(NothrowSortIter<Iter1, Cmp>::value)
		{
			merge_move_forward<Overlap>(std::make_reverse_iterator(last2),
						    std::make_reverse_iterator(first2),
						    std::make_reverse_iterator(last1),
						    std::make_reverse_iterator(first1),
						    std::make_reverse_iterator(out_end),
						    [c](const auto& a, const auto& b) { return c(b, a); });
		}

		/* template<class Iter, class Cmp, class B>
		static void merge_with_buffer(Iter first, size_t n0, Iter middle, size_t n1, Iter e1, Cmp r, B buffer) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Inplace merge 2 ranges using provided buffer
			if (n0 <= n1) {
				auto blast = std::move(first, middle, buffer.first);
				if (is_random_access<Iter>::value && n0 * 32 < n1)
					// unbalanced merge
					merge_move_forward_unbalanced(buffer.first, blast, middle, e1, first, r);
				else
					merge_move_forward<true>(buffer.first, blast, middle, e1, first, r);
			}
			else {
				auto last = e1;
				auto blast = std::move(middle, last, buffer.first);
				if (is_random_access<Iter>::value && n1 * 32 < n0)
					merge_move_backward_unbalanced(first, middle, buffer.first, blast, last, r);
				else
					merge_move_backward<true>(first, middle, buffer.first, blast, last, r);
			}
			SEQ_ASSERT_DEBUG(std::is_sorted(first, std::next(middle, n1), r), "");
		}*/
		template<class Iter, class Cmp, class B>
		static void merge_with_buffer(Iter first, size_t n0, Iter middle, size_t n1, Iter e1, Cmp r, B buffer) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Inplace merge 2 ranges using provided buffer
			if (n0 <= n1) {
				auto blast = std::move(first, middle, buffer.first);
				if (is_random_access<Iter>::value && n0 * 32 < n1)
					// unbalanced merge
					merge_move_forward_unbalanced(std::make_move_iterator(buffer.first), std::make_move_iterator(blast), std::make_move_iterator(middle), std::make_move_iterator(e1), first, r);
				else
					merge_move_forward<true>(std::make_move_iterator(buffer.first), std::make_move_iterator(blast), std::make_move_iterator(middle), std::make_move_iterator(e1), first, r);
			}
			else {
				auto last = e1;
				auto blast = std::move(middle, last, buffer.first);
				if (is_random_access<Iter>::value && n1 * 32 < n0)
					merge_move_backward_unbalanced(std::make_move_iterator(first), std::make_move_iterator(middle), std::make_move_iterator(buffer.first), std::make_move_iterator(blast), last, r);
				else
					merge_move_backward<true>(std::make_move_iterator(first), std::make_move_iterator(middle), std::make_move_iterator(buffer.first), std::make_move_iterator(blast), last, r);
			}
			SEQ_ASSERT_DEBUG(std::is_sorted(first, std::next(middle, n1), r), "");
		}

		template<class Iter, class Cmp, class B>
		static void merge_adaptive_n(Iter f0, size_t n0, Iter f1, size_t n1, Iter e1, Cmp r, B buffer) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Inplace merge with buffer, first published by Dudzin'sky and Dydek in 1981 IPL 12(1):5-8
			// Implementation from: https://www.jmeiners.com/efficient-programming-with-components/15_merge_inplace.html

			SEQ_ASSERT_DEBUG(std::distance(f0, f1) == n0, "");
			SEQ_ASSERT_DEBUG(std::is_sorted(f0, std::next(f0, n0), r), "");
			SEQ_ASSERT_DEBUG(std::is_sorted(f1, std::next(f1, n1), r), "");

			if (!n0 || !n1 || !r(*f1, *std::prev(f1)))
				// One of the 2 ranges is empty, or already sorted
				return;
			if (r(*std::prev(e1), *f0)) {
				std::rotate(f0, f1, e1);
				return;
			}
			if (n0 <= buffer.size || n1 <= buffer.size)
				// We have enough buffer: merge
				return merge_with_buffer(f0, n0, f1, n1, e1, r, buffer);

			// Rotate left or right range
			Iter f0_0, f0_1, f1_0, f1_1;
			size_t n0_0, n0_1, n1_0, n1_1;
			if (n0 < n1)
				merge_inplace_left_subproblem(f0, n0, f1, n1, f0_0, n0_0, f0_1, n0_1, f1_0, n1_0, f1_1, n1_1, r);
			else
				merge_inplace_right_subproblem(f0, n0, f1, n1, f0_0, n0_0, f0_1, n0_1, f1_0, n1_0, f1_1, n1_1, r);

			// Recurse on each range
			merge_adaptive_n(f0_0, n0_0, f0_1, n0_1, std::next(f0_1, n0_1), r, buffer);
			merge_adaptive_n(f1_0, n1_0, f1_1, n1_1, std::next(f1_1, n1_1), r, buffer);
			SEQ_ASSERT_DEBUG(std::is_sorted(f0, e1), "");
		}

		template<class Key, class Iter, class Out, class Cmp>
		static Out merge_move(Iter first1, Iter last1, Iter first2, Iter last2, Out out, Cmp c) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{

			// Merge 2 sorted ranges to given output.
			// Uses the fastest available method: standard forward merge
			// or branchless merge from both ends for random access iterators.

			using type = typename std::iterator_traits<Iter>::value_type;
			using key_type = typename std::conditional<std::is_same<Key, void>::value, type, Key>::type;
			static constexpr bool bidirectional = is_random_access<Iter>::value && std::is_trivially_move_assignable<type>::value;

			if SEQ_CONSTEXPR (bidirectional) {

				// Branchless merge from both ends
				// Only faster for trivial types where a CMOV instruction might be used
				ptrdiff_t dist1 = std::distance(first1, last1);
				ptrdiff_t dist2 = std::distance(first2, last2);

				{

					Iter first[2] = { first1, first2 };
					Iter second[2] = { std::prev(last1), std::prev(last2) };

					Out out_left = out;
					Out res = std::next(out, dist1 + dist2);
					Out out_right = std::prev(res);

					if (dist1 < 128 && dist1 == dist2) {

						while (dist1-- != 0) {
							const int left_order = c(*first[1], *first[0]);
							const int right_order = !c(*second[1], *second[0]);
							*out_left = std::move(*first[left_order]);
							*out_right = std::move(*second[right_order]);
							++out_left;
							--out_right;
							first[1] += left_order;
							first[0] += !left_order;
							second[1] -= right_order;
							second[0] -= !right_order;
						}

						SEQ_ASSERT_DEBUG(std::is_sorted(out, res, c), "");
						return res;
					}

					const ptrdiff_t iter_count = std::min(dist1, dist2);
					const ptrdiff_t stop = iter_count / 16;
					ptrdiff_t order = 0;
					ptrdiff_t count = 0;

					bool prev_left_order = true;
					bool prev_right_order = true;
					if ((first[0] < second[0]) && (first[1] < second[1])) {
						prev_left_order = c(*first[1], *first[0]);
						prev_right_order = !c(*second[1], *second[0]);
						*out_left = std::move(*first[prev_left_order]);
						*out_right = std::move(*second[prev_right_order]);
						++out_left;
						--out_right;
						first[1] += prev_left_order;
						first[0] += !prev_left_order;
						second[1] -= prev_right_order;
						second[0] -= !prev_right_order;

						while (((first[0] < second[0]) && (first[1] < second[1]))) {
							const bool left_order = c(*first[1], *first[0]);
							const bool right_order = !c(*second[1], *second[0]);
							*out_left = std::move(*first[left_order]);
							*out_right = std::move(*second[right_order]);
							++out_left;
							--out_right;
							first[1] += left_order;
							first[0] += !left_order;
							second[1] -= right_order;
							second[0] -= !right_order;

							if (count < stop) {
								order += left_order == prev_left_order;
								order += right_order == prev_right_order;
								prev_left_order = left_order;
								prev_right_order = right_order;
								if (++count == stop) {
									if (order > stop)
										break;
								}
							}
						}

						// Finish with merge_move_forward to merge
						// the middle range
						merge_move_forward<false>(first[0], ++second[0], first[1], ++second[1], out_left, c);
						SEQ_ASSERT_DEBUG(std::is_sorted(out, res, c), "");
						return res;
					}
				}
			}

			// Standard forward merge
			return merge_move_forward<false>(first1, last1, first2, last2, out, c);
		}

		template<class Iter, class Cmp>
		SEQ_ALWAYS_INLINE Iter insertion_sort_n(Iter begin, unsigned count, Cmp l) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Standard in-place insertion sort working on bidirectional iterators,
			// but using a number of values to sort instead of an end iterator.
			// Stop early if number of performed moves is > MaxMove.

			if SEQ_UNLIKELY (count < 2)
				return count == 0 ? begin : std::next(begin);

			auto cur = begin;
			auto prev = cur++;
			for (; count > 1; --count) {
				if (l(*cur, *prev)) {
					auto sift = cur;
					auto tmp = std::move(*sift);
					do {
						*sift = std::move(*prev);
						--sift;
					} while (sift != begin && l(tmp, *(--prev)));
					*sift = std::move(tmp);
				}
				prev = cur++;
			}
			SEQ_ASSERT_DEBUG(std::is_sorted(begin, cur, l), "");
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
				std::reverse(start_equal, begin);
			}

		reverse_full:
			// Reverse the full sequence.
			// Equal ranges will get back their natural orders.
			std::reverse(start, end);
			SEQ_ASSERT_DEBUG(std::is_sorted(start, end, [l](const auto& a, const auto& b) { return l(a, b); }), "");
		}

		template<class Iter>
		struct PartitionResult
		{
			Iter last;
			Iter second_half;
			size_t size;
		};
		template<class Iter, class Pred, class Buffer>
		PartitionResult<Iter> stable_partition_size(Iter begin, size_t size, Pred pr, Buffer buf)
		{
			if (buf.size == 0)
				return PartitionResult<Iter>{ begin, begin, 0 };

			auto end_buf = std::next(buf.first, buf.size);
			auto buf_it = buf.first;

			Iter start = begin;
			size_t remaining = size;

			while (remaining && pr(*start)) {
				// skip left elements
				++start;
				--remaining;
			}

			// Skip right elements
			if SEQ_CONSTEXPR (is_random_access<Iter>::value) {
				auto last = begin + (size - 1);
				while (last != begin && !pr(*last)) {
					--last;
					--remaining;
				}
			}

			Iter insert = start;
			if (remaining) {
				// move right element to buffer
				*buf_it++ = std::move(*start++);
				--remaining;
			}
			else
				return PartitionResult<Iter>{ start, start, size - remaining };

			while (remaining && buf_it != end_buf) {
				while (remaining && pr(*start)) {
					*insert = std::move(*start);
					++insert;
					++start;
					--remaining;
				}

				if (!remaining || buf_it == end_buf)
					goto finish;

				*buf_it = std::move(*start);
				++buf_it;
				++start;
				--remaining;

				while (remaining && buf_it != end_buf && !pr(*start)) {
					*buf_it = std::move(*start);
					++buf_it;
					++start;
					--remaining;
				}
			}

		finish:
			auto last = std::move(buf.first, buf_it, insert);
			return PartitionResult<Iter>{ last, insert, size - remaining };
		}

		template<class Key, class Iter, class Cmp, class Storage>
		static void ping_pong_merge_4(Iter* iters, Cmp c, Storage tmp) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			const bool s0 = !c(*iters[1], *std::prev(iters[1]));
			const bool s1 = !c(*iters[2], *std::prev(iters[2]));
			const bool s2 = !c(*iters[3], *std::prev(iters[3]));
			if (s0 && s1 && s2)
				return;

			Storage middle, end;
			if (!s0)
				middle = merge_move<Key>(iters[0], iters[1], iters[1], iters[2], tmp, c);
			else {
				Storage dst = std::move(iters[0], iters[1], tmp);
				middle = std::move(iters[1], iters[2], dst);
			}
			if (!s2)
				end = merge_move<Key>(iters[2], iters[3], iters[3], iters[4], middle, c);
			else {
				Storage dst = std::move(iters[2], iters[3], middle);
				end = std::move(iters[3], iters[4], dst);
			}
			merge_move<Key>(tmp, middle, middle, end, iters[0], c);
		}
		template<class Key, class Iter, class Cmp, class Storage>
		static void ping_pong_merge_3(Iter* iters, Cmp c, Storage tmp) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			const bool s0 = !c(*iters[1], *std::prev(iters[1]));
			const bool s1 = !c(*iters[2], *std::prev(iters[2]));
			if (s0 && s1)
				return;

			auto middle = tmp;
			if (!s0)
				middle = merge_move<Key>(iters[0], iters[1], iters[1], iters[2], tmp, c);
			else {
				auto dst = std::move(iters[0], iters[1], tmp);
				middle = std::move(iters[1], iters[2], dst);
			}
			merge_move_forward<true>(tmp, middle, iters[2], iters[3], iters[0], c);
		}

		template<class Key, class Iter, class Cmp, class Buffer, class Out = typename std::iterator_traits<Iter>::pointer>
		static SEQ_ALWAYS_INLINE void merge_sorted_runs_with_buffer(Iter* iters, size_t start, size_t last, Cmp cmp, Buffer buf) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Inplace merge already sorted runs represented by an array of iterators.
			// Supports bidirectional iterators.
			// Internally calls merge_adaptive_n with provided buffer.

			auto size = last - start;
			if (size < 2u)
				return;

			if (size <= 4 && (size_t)std::distance(iters[start], iters[last]) <= buf.size) {
				if (size == 4)
					return ping_pong_merge_4<Key>(iters + start, cmp, buf.first);
				if (size == 3)
					return ping_pong_merge_3<Key>(iters + start, cmp, buf.first);
			}

			auto half = size / 2u;
			merge_sorted_runs_with_buffer<Key>(iters, start, start + half, cmp, buf);
			merge_sorted_runs_with_buffer<Key>(iters, start + half, last, cmp, buf);

			size_t l_size = (size_t)std::distance(iters[start], iters[start + half]);
			size_t r_size = (size_t)std::distance(iters[start + half], iters[last]);
			merge_adaptive_n(iters[start], l_size, iters[start + half], r_size, iters[last], cmp, buf);
		}

#define CHECK_2(vals, a, b)                                                                                                                                                                            \
	if (l(vals[b], vals[a]))                                                                                                                                                                       \
	std::swap(vals[b], vals[a])

#define CHECK_2_NO_OVERLAPP(vals, a, b, c, d)                                                                                                                                                          \
	{                                                                                                                                                                                              \
		bool c0 = l(vals[b], vals[a]);                                                                                                                                                         \
		bool c1 = l(vals[d], vals[c]);                                                                                                                                                         \
		if (c0)                                                                                                                                                                                \
			std::swap(vals[b], vals[a]);                                                                                                                                                        \
		if (c1)                                                                                                                                                                                \
			std::swap(vals[d], vals[c]);                                                                                                                                                        \
	}

#define CHECK_4_NO_OVERLAPP(vals, a, b, c, d, e, f, g, h)                                                                                                                                              \
	{                                                                                                                                                                                              \
		if (l(vals[b], vals[a]))                                                                                                                                                               \
			std::swap(vals[b], vals[a]);                                                                                                                                                        \
		if (l(vals[d], vals[c]))                                                                                                                                                               \
			std::swap(vals[d], vals[c]);                                                                                                                                                        \
		if (l(vals[f], vals[e]))                                                                                                                                                               \
			std::swap(vals[f], vals[e]);                                                                                                                                                        \
		if (l(vals[h], vals[g]))                                                                                                                                                               \
			std::swap(vals[h], vals[g]);                                                                                                                                                        \
	}

		template<class Iter, class Cmp>
		SEQ_ALWAYS_INLINE Iter network_sort_8(Iter vals, Cmp l) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			using namespace std;
			// Sort 4 values using a sorting netork
			CHECK_4_NO_OVERLAPP(vals, 0, 1, 2, 3, 4, 5, 6, 7)
			CHECK_4_NO_OVERLAPP(vals, 0, 2, 1, 3, 4, 6, 5, 7)
			CHECK_4_NO_OVERLAPP(vals, 0, 4, 1, 5, 2, 6, 3, 7)

			CHECK_2_NO_OVERLAPP(vals, 2, 4, 3, 5)
			CHECK_2_NO_OVERLAPP(vals, 1, 4, 3, 6)
			CHECK_2_NO_OVERLAPP(vals, 1, 2, 3, 4)
			CHECK_2(vals, 5, 6);
			return vals + 8;
		}

#undef CHECK_2
#undef CHECK_2_NO_OVERLAPP
#undef CHECK_4_NO_OVERLAPP

		template<unsigned N, class Key, class Iter, class Cmp>
		Iter atom_sort_8(Iter first, unsigned count, Cmp c) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			using T = typename std::iterator_traits<Iter>::value_type;

			if SEQ_CONSTEXPR (is_random_access<Iter>::value && N == 8)
				return network_sort_8(first, c);
			else
				return insertion_sort_n(first, N == -1 ? count : N, c);
		}

		template<class Key, class Iter, class Out, class Cmp>
		std::pair<Out, unsigned> atom_sort_64(Iter& first, unsigned count, Out out, Cmp c) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			using T = typename std::iterator_traits<Iter>::value_type;

			// Sort up to 64 values
			if (count >= 64) {

				auto it0 = atom_sort_8<8, Key>(first, 8, c);
				auto it1 = atom_sort_8<8, Key>(it0, 8, c);
				auto it2 = atom_sort_8<8, Key>(it1, 8, c);
				auto it3 = atom_sort_8<8, Key>(it2, 8, c);
				auto it4 = atom_sort_8<8, Key>(it3, 8, c);
				auto it5 = atom_sort_8<8, Key>(it4, 8, c);
				auto it6 = atom_sort_8<8, Key>(it5, 8, c);
				auto it7 = atom_sort_8<8, Key>(it6, 8, c);

				auto o0 = merge_move<Key>(first, it0, it0, it1, out, c);
				auto o1 = merge_move<Key>(it1, it2, it2, it3, o0, c);
				auto o2 = merge_move<Key>(it3, it4, it4, it5, o1, c);
				auto o3 = merge_move<Key>(it5, it6, it6, it7, o2, c);
				auto d0 = merge_move<Key>(out, o0, o0, o1, first, c);
				auto d1 = merge_move<Key>(o1, o2, o2, o3, d0, c);
				auto r = merge_move<Key>(first, d0, d0, d1, out, c);
				first = d1;
				return { r, 64u };
			}

			if (count >= 16) {
				auto it0 = atom_sort_8<8, Key>(first, 8, c);
				auto it1 = atom_sort_8<8, Key>(it0, 8, c);
				Out r = merge_move<Key>(first, it0, it0, it1, out, c);
				first = it1;
				return { r, 16u };
			}
			if (count > 8) {
				auto it0 = atom_sort_8<8, Key>(first, 8, c);
				auto it1 = atom_sort_8<-1, Key>(it0, count - 8, c);
				Out r = merge_move<Key>(first, it0, it0, it1, out, c);
				first = it1;
				return { r, count };
			}
			auto it1 = atom_sort_8<-1, Key>(first, count, c);
			Out r = std::move(first, it1, out);
			first = it1;
			return { r, count };
		}

		template<class Key, class Iter, class Out, class Cmp>
		Out sort_out_64(Iter& vals, unsigned count, Out out, Cmp c) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Sort up to 64 values to output
			Out iters[6] = { out };
			unsigned cnt = 1;
			unsigned rem = count;
			Iter src = vals;
			while (rem) {
				auto p = atom_sort_64<Key>(src, rem, out, c);
				rem -= p.second;
				iters[cnt++] = out = p.first;
			}
			merge_sorted_runs_with_buffer<Key>(iters, 0, cnt - 1, c, buffer<Iter>{ vals, count });
			vals = src;
			return out;
		}
		template<class Key, class Iter, class Cmp, class Buffer>
		Iter sort_128(Iter vals, unsigned count, Cmp c, Buffer buf) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// std::stable_sort(vals, vals + count, c);
			// return vals + count;
			if (count == 128) {
				Iter src = vals;
				auto it0 = atom_sort_64<Key>(src, 64, buf.first, c);
				auto it1 = atom_sort_64<Key>(src, 64, it0.first, c);
				return merge_move<Key>(buf.first, it0.first, it0.first, it1.first, vals, c);
			}

			unsigned half = count / 2;
			Iter src = vals;
			auto it0 = sort_out_64<Key>(src, half, buf.first, c);
			auto it1 = sort_out_64<Key>(src, count - half, it0, c);
			return merge_move<Key>(buf.first, it0, it0, it1, vals, c);
		}

		template<unsigned IterCount, class Key, class T, class Cmp, class Buffer>
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

			for (; dist != size && cnt < (IterCount - 1); ++begin, ++prev, ++dist) {
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
				ascending[cnt] = !as;
				iters[cnt++] = begin;
				if (cnt > 1 && begin == std::next(iters[cnt - 2])) {
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
				SEQ_ASSERT_DEBUG(std::is_sorted(iters[i], iters[i + 1], c), "");
			}

			// inplace merge runs
			merge_sorted_runs_with_buffer<Key>(iters, 0u, cnt - 1u, c, buf);
			SEQ_ASSERT_DEBUG(std::is_sorted(start, begin, c), "");
			return { begin, dist };
		}

		template<class Iter, class Cmp>
		std::pair<Iter, size_t> sorted_until(Iter first, Iter last, Cmp c) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			size_t dist = 0;
			// find extent of range that is ordered by predicate
			if (first != last) {
				for (auto next = first; ++next != last; ++first, ++dist) {
					if (c(*next, *first)) {
						last = next;
						++dist;
						break;
					}
				}
			}
			return { last, dist };
		}

		template<unsigned MaxIters, class Key, class Iter, class Cmp, class Fn, class Buff>
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
							// We need to identify the sorted range with sorted_until()

							// Increment it by min_size -1, starting from begin or from end
							auto it = begin;
							if SEQ_CONSTEXPR (!is_random_access<Iter>::value) {
								auto d = std::distance(begin, end);
								if (min_size > d / 2)
									it = std::prev(end, d - min_size + 1);
							}
							if (it == begin)
								it = std::next(begin, min_size - 1);

							// auto it = std::next(begin, min_size - 1);
							auto p = sorted_until(it, end, l);
							r = { p.first, min_size - 1 + p.second };
						}
						else {
							// No need to call sorted_until(), we know
							// the sorted range goes to the end.
							r = { end, remaining };
							SEQ_ASSERT_DEBUG(std::is_sorted(begin, r.first, l), "");
						}
					}
					SEQ_ASSERT_DEBUG(std::is_sorted(begin, r.first, l), "");

					// Store new start position
					// and update remaining elements
					iters[cnt++] = begin = r.first;
					remaining -= r.second;
					cum_dist += r.second;

				} while (remaining != 0 && cnt < MaxIters);

				// Merge all sorted runs, and construct the merge buffer if necessary
				merge_sorted_runs_with_buffer<Key>(iters, 0u, cnt - 1u, l, buf);

				// Update new min size if this is NOT the last chunk
				if (remaining || new_min_size == std::numeric_limits<size_t>::max())
					new_min_size = std::min(new_min_size, cum_dist);

				last_start = iters[0];
				SEQ_ASSERT_DEBUG(std::is_sorted(iters[0], iters[cnt - 1u], l), "");

			} while (remaining != 0);

			// Compute end iterator during the first pass
			if (min_size == 0)
				end = begin;

			if (last_start != start) {
				// We need a next pass if the number of iterators was not enough
				return generic_merge_sort_internal<MaxIters, Key>(start, end, size, l, sort_sub_range, buf, new_min_size);
			}
		}

		template<class Key, class Iter, class Cmp, class Buffer>
		static void merge_sort_internal(Iter begin, size_t size, Cmp l, Buffer buf) noexcept(NothrowSortIter<Iter, Cmp>::value)
		{
			// Standard merge sort.
			// Small chunks of up to 256 elements are sorted using insertion sort ot sorting netwrok (or both).
			// If possible, use wave sort on longer runs.
			//
			// This sorting routine is used by radix_sort() for small sequence sizes (< 2048)
			// or when we cannot allocate memory for the radix tree.
			//
			// If we can allocate memory, this function should be roughly as fast as
			// std::stable sort, and usually faster for almost sorted sequences.
			//
			// Supports bidirectional iterators.

			generic_merge_sort_internal<65, Key>(
			  begin,
			  begin,
			  size,
			  l,
			  [&](Iter b, size_t remaining, auto l) {
				  // Try wave sort first, as it might consume a lot more
				  // than the default 128 elements (possibly the whole sequence)
				  auto r = try_wave_sort<5, Key>(b, remaining, std::min(remaining, (size_t)128u), l, buf);
				  if (r.first != b) {
					  // printf("found %i\n", (int)r.second);
					  //  Success, retrieve new start position and number of sorted elements
					  return r;
				  }

				  // Failure, use sort_128()
				  unsigned cnt = (unsigned)std::min(remaining, (size_t)128u);
				  auto it = sort_128<Key>(b, cnt, l, buf);
				  return std::make_pair(it, (size_t)cnt);
			  },
			  buf);

			SEQ_ASSERT_DEBUG(std::is_sorted(begin, std::next(begin, size), l), "");
		}

		// Create the internal buffer used by merge_sort() or radix_sort()
		template<class Buffer>
		static size_t sort_buffer_size(const Buffer& buf, size_t count) noexcept
		{
			if (buf.size == default_buffer.size || buf.size == 0)
				return (count / 2u);
			if (buf.size == medium_buffer.size)
				return (count / 16u);
			if (buf.size == small_buffer.size)
				return (count / 64u);
			return (count / 128u);
		}

		template<class KeyType, class Iter, class Cmp, class Buffer>
		void merge_sort_size_internal(Iter begin, size_t size, Cmp cmp, Buffer buffer) noexcept(algo_detail::NothrowSortIter<Iter, Cmp>::value)
		{
			using namespace algo_detail;
			using Key = typename std::iterator_traits<Iter>::value_type;

			if (size < 32) {
				insertion_sort_n(begin, size, cmp);
				return;
			}

			if SEQ_CONSTEXPR (std::is_same<default_sort_buffer, Buffer>::value) {
				size_t buf_size = sort_buffer_size(buffer, size);
				if (buf_size < 128)
					buf_size = 128;
				std::vector<Key> buf(buf_size);
				return merge_sort_internal<KeyType>(begin, size, cmp, seq::buffer<Key*>{ buf.data(), buf.size() });
			}
			else {

				if (buffer.size >= 128)
					return merge_sort_internal<KeyType>(begin, size, cmp, buffer);

				// Build buffer if necessary
				std::vector<Key> buf(128);
				merge_sort_internal<KeyType>(begin, size, cmp, seq::buffer<Key*>{ buf.data(), buf.size() });
			}
		}
	}

	//
	// Public API
	//

	/// @brief Stable merge sort algorithm using an external buffer.
	///
	/// If provided buffer is one of 'default_buffer', 'medium_buffer', 'small_buffer'
	/// or 'tiny_buffer', this function will try to allocate memory.
	///
	/// This function performs at most O(N.log(N)) comparisons if enough side
	/// memory is available, O(N.log²(N)) otherwise.
	///
	/// merge_sort_size() is usually faster than std::stable_sort() for almost
	/// sorted inputs or wave-like patterns, and as fast otherwise.
	///
	/// merge_sort_size() and merge_sort() work on bidirectional iterators.
	/// Using merge_sort_size() instead of merge_sort() is faster when the
	/// range size is already known.
	///
	template<class Iter, class Cmp = std::less<>, class Buffer = algo_detail::default_sort_buffer>
	void merge_sort_size(Iter begin, size_t size, Cmp cmp = Cmp(), Buffer buffer = Buffer()) noexcept(algo_detail::NothrowSortIter<Iter, Cmp>::value)
	{
		using namespace algo_detail;
		using Key = typename std::iterator_traits<Iter>::value_type;
		using IterType = IterWrapper<Iter>;
		merge_sort_size_internal<Key>(IterType(begin, 0), size, cmp, buffer);
		//merge_sort_size_internal<Key>(begin, size, cmp, buffer);
	}

	/// @brief Stable merge sort algorithm using an external buffer.
	///
	/// If provided buffer is one of 'default_buffer', 'medium_buffer', 'small_buffer'
	/// or 'tiny_buffer', this function will try to allocate memory.
	///
	/// The function performs at most O(N.log(N)) comparisons if enough side
	/// memory is available, O(N.log²(N)) otherwise.
	///
	/// merge_sort() is usually faster than std::stable_sort() for almost
	/// sorted inputs or wave-like patterns, and as fast otherwise.
	///
	/// merge_sort_size() and merge_sort() work on bidirectional iterators.
	/// Using merge_sort_size() instead of merge_sort() is faster when the
	/// range size is already known.
	///
	template<class Iter, class Cmp = std::less<>, class Buffer = algo_detail::default_sort_buffer>
	void merge_sort(Iter begin, Iter end, Cmp cmp = Cmp(), Buffer buffer = Buffer()) 
	{
		merge_sort_size(begin, std::distance(begin, end), cmp, buffer);
	}

	template<class Iter, class Cmp = std::less<>, class Buffer = algo_detail::default_sort_buffer>
	void merge_sort_stack(Iter begin, Iter end, Cmp cmp = Cmp()) noexcept(algo_detail::NothrowSortIter<Iter, Cmp>::value)
	{
		auto n = std::distance(begin, end);
		if (n < 32)
			algo_detail::insertion_sort_n(begin, n, cmp);
		else {
			using Key = typename std::iterator_traits<Iter>::value_type;
			Key keys[128];
			merge_sort_size(begin, n, cmp, buffer<Key*>(keys, 128));
		}
	}

} // end namespace seq

namespace seq
{
	template<class Iter, class Pred, class Buffer>
	Iter stable_partition(Iter first, Iter last, Pred p, Buffer b)
	{
		auto n = std::distance(first, last);
		if (n <= b.size) {
			auto r = algo_detail::stable_partition_size(first, n, p, b);
			return r.second_half;
		}
		auto middle = first;
		middle = std::next(middle, n / 2);
		auto left = stable_partition(first, middle, p, b);
		auto right = stable_partition(middle, last, p, b);
		return std::rotate(left, middle, right);
	}
}
#endif