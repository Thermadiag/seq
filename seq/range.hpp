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

#ifndef SEQ_RANGE_HPP
#define SEQ_RANGE_HPP

/** @file */

/**\defgroup range iterator_range: define iterable ranges over arithmetic values or containers

The range module defines the range() function that returns an iterable object over a container, an iterator range or an arithmetic range.
It is mainly used within the format module for the join() function.



*/

#include <iterator>

#include "type_traits.hpp"

namespace seq
{
	namespace detail
	{
		/// @brief Iterator over an integral value
		template<class Integral>
		struct IntegralIterator
		{
			using value_type = Integral;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			Integral val;
			IntegralIterator() {}
			IntegralIterator(Integral v) : val(v) {}
			auto operator++() noexcept -> IntegralIterator& {
				++val;
				return *this;
			}
			auto operator++(int) noexcept -> IntegralIterator {
				IntegralIterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { return val; }
			auto operator->() const noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			bool operator==(const IntegralIterator& it) const noexcept { return val == it.val; }
			bool operator!=(const IntegralIterator& it) const noexcept { return val != it.val; }
		};
		/// @brief Iterator over an integral value
		template<class Type>
		struct ArithmeticIterator
		{
			using value_type = Type;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			Type val;
			Type incr;
			ArithmeticIterator() {}
			ArithmeticIterator(Type v, Type i) : val(v), incr(i) {
				SEQ_ASSERT_DEBUG(i != 0, "invalid zero increment");
			}
			auto operator++() noexcept -> ArithmeticIterator& {
				val += incr;
				return *this;
			}
			auto operator++(int) noexcept -> ArithmeticIterator {
				ArithmeticIterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { return val; }
			auto operator->() const noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			bool operator==(const ArithmeticIterator& it) const noexcept { return !(*this != it); }
			bool operator!=(const ArithmeticIterator& it) const noexcept { return incr > 0 ? (val < it.val) : (val > it.val); }
		};
		/// @brief Iterate N times over a given iterator
		template<class Iter>
		struct NIterator
		{
			using value_type = typename std::iterator_traits<Iter>::value_type;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			Iter iter;
			size_t count;
			NIterator(Iter it, size_t c) : iter(it), count(c) {}
			auto operator++() noexcept -> NIterator& {
				++iter;
				++count;
				return *this;
			}
			auto operator++(int) noexcept -> NIterator {
				NIterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { return *iter; }
			auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			bool operator==(const NIterator& it) const noexcept { return count == it.count || iter == it.iter; }
			bool operator!=(const NIterator& it) const noexcept { return count != it.count && iter != it.iter; }

		};
		/// @brief Iterate N times over a given iterator
		template<class Iter>
		struct NIteratorRef
		{
			using value_type = typename std::iterator_traits<Iter>::value_type;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			Iter* iter;
			size_t count;
			NIteratorRef(Iter* it, size_t c) : iter(it), count(c) {}
			auto operator++() noexcept -> NIteratorRef& {
				++(*iter);
				++count;
				return *this;
			}
			auto operator++(int) noexcept -> NIteratorRef {
				NIteratorRef _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { return *(*iter); }
			auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			bool operator==(const NIteratorRef& it) const noexcept { return count == it.count || *iter == *it.iter; }
			bool operator!=(const NIteratorRef& it) const noexcept { return count != it.count && *iter != *it.iter; }

		};
		/// @brief Iterate over 2 iterators to create a std::pair
		template<class Iter1, class Iter2>
		struct ZipIterator
		{
			using value_type1 = typename std::iterator_traits<Iter1>::value_type;
			using value_type2 = typename std::iterator_traits<Iter2>::value_type;
			using value_type = std::pair<value_type1, value_type2>;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = value_type;
			Iter1 iter1;
			Iter2 iter2;
			ZipIterator(Iter1 it1, Iter2 it2) : iter1(it1), iter2(it2) {}
			auto operator++() noexcept -> ZipIterator& {
				++iter1;
				++iter2;
				return *this;
			}
			auto operator++(int) noexcept -> ZipIterator {
				ZipIterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { 
				return std::make_pair(*iter1,*iter2); 
			}
			bool operator==(const ZipIterator& it) const noexcept { 
				return iter1 == it.iter1 || iter2 == it.iter2; 
			}
			bool operator!=(const ZipIterator& it) const noexcept { 
				return iter1 != it.iter1 && iter2 != it.iter2; 
			}

		};
		/// @brief Iterate N times over a given iterator with boundary wrapping
		template<class Iter>
		struct NIteratorWrap
		{
			using value_type = typename std::iterator_traits<Iter>::value_type;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			Iter iter;
			Iter begin;
			Iter end;
			size_t count;
			NIteratorWrap(size_t c) : count(c) {}
			NIteratorWrap(Iter it, Iter en, size_t c) : iter(it), begin(it), end(en), count(c) {}
			auto operator++() noexcept -> NIteratorWrap& {
				if (++iter == end) 
					iter = begin;
				++count;
				return *this;
			}
			auto operator++(int) noexcept -> NIteratorWrap {
				NIteratorWrap _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { return *(iter); }
			auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			bool operator==(const NIteratorWrap& it) const noexcept { return count == it.count; }
			bool operator!=(const NIteratorWrap& it) const noexcept { return count != it.count; }

		};
		/// @brief Iterate N times over a given iterator with boundary wrapping
		template<class T>
		struct NIteratorRepeat
		{
			using value_type = T;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const T*;
			using reference = const T&;
			T value;
			size_t count;
			NIteratorRepeat(size_t c) : count(c) {}
			NIteratorRepeat(const T & val, size_t) : value(val), count(0) {}
			NIteratorRepeat( T&& val, size_t) : value(std::move(val)), count(0) {}
			auto operator++() noexcept -> NIteratorRepeat& {
				++count;
				return *this;
			}
			auto operator++(int) noexcept -> NIteratorRepeat {
				NIteratorRepeat _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { return value; }
			auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			bool operator==(const NIteratorRepeat& it) const noexcept { return count == it.count; }
			bool operator!=(const NIteratorRepeat& it) const noexcept { return count != it.count; }
		};
		/// @brief Iterate over a given iterator of value type std::pair and flatten the result
		template<class Iter>
		struct FlattenPair
		{
			using pair_type = typename std::iterator_traits<Iter>::value_type;
			using value_type = typename std::remove_const<typename pair_type::first_type>::type;
			static_assert(std::is_same<value_type, typename pair_type::second_type>::value, "FlattenPair only works on pair<T,T>");

			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			Iter iter;
			bool is_first;
			FlattenPair(Iter it) : iter(it), is_first(true){}
			auto operator++() noexcept -> FlattenPair& {
				is_first = !is_first;
				if(is_first)
					++iter;
				return *this;
			}
			auto operator++(int) noexcept -> FlattenPair {
				FlattenPair _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator*() const noexcept -> reference { return is_first ? (*iter).first : (*iter).second; }
			auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			bool operator==(const FlattenPair& it) const noexcept { return iter == it.iter ; }
			bool operator!=(const FlattenPair& it) const noexcept { return iter != it.iter ; }

		};
		
	} // end namespace detail



	///iterator_range class used to join iterables
	template<class Iter>
	class iterator_range
	{
		Iter first;
		Iter last;

	public:
		using value_type = typename std::iterator_traits<Iter>::value_type;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = typename std::iterator_traits<Iter>::pointer;
		using reference = typename std::iterator_traits<Iter>::reference;
		using iterator = Iter;
		using const_iterator = Iter;

		iterator_range(Iter f, Iter l) : first(f), last(l) {}

		Iter begin() const noexcept { return first; }
		Iter end() const noexcept { return last; }
		Iter cbegin() const noexcept { return first; }
		Iter cend() const noexcept { return last; }

		// Implicit conversion to container type
		template<class Container, class = typename std::enable_if<is_iterable<Container>::value, void>::type >
		operator Container() const {
			return Container(begin(), end());
		}

		// Implicit conversion to std::pair<T,T>
		template<class T>
		operator std::pair<T, T>() const {
			std::pair<T, T> res;
			auto b = begin();
			if (b != last) {
				res.first = static_cast<T>(*b);
				++b;
				if (b != last)
					res.second = static_cast<T>(*b);
			}
			return res;
		}

		// Comparison operators
		template<class Iter2>
		bool operator!=(const iterator_range<Iter2>& other) const {
			auto it = begin();
			auto oit = other.begin();
			auto e = end();
			auto oe = other.end();

			for (; it != e && oit != oe; ++it, ++oit) {
				if (*it != *oit)
					return true;
			}
			return it != e || oit != oe;
		}
		template<class Iter2>
		bool operator==(const iterator_range<Iter2>& other) const {
			return !(*this != other);
		}
	};



	/// @brief Build and return an iterable object over the range [first,last)
	/// @tparam Iter iterator type
	template<class Iter, class = typename std::enable_if<!std::is_arithmetic<Iter>::value, void>::type>
	auto range(Iter first, Iter last) noexcept -> iterator_range<Iter>
	{
		return iterator_range<Iter>(first, last);
	}

	/// @brief Build and return an iterable object over the range [first, min(first + count, last))
	/// @tparam Iter iterator type
	template<class Iter, class = typename std::enable_if<!std::is_arithmetic<Iter>::value, void>::type>
	auto range(Iter first, Iter last, size_t count) noexcept -> iterator_range<detail::NIterator<Iter>>
	{
		return iterator_range<detail::NIterator<Iter>>(detail::NIterator<Iter>(first, 0), detail::NIterator<Iter>(last, count));
	}

	/// @brief Build and return an iterable object over the range [first, min(first + count, last))
	/// @tparam Iter iterator type
	template<class Iter, class = typename std::enable_if<!std::is_arithmetic<Iter>::value, void>::type>
	auto range_p(Iter* first, Iter* last, size_t count) noexcept -> iterator_range<detail::NIteratorRef<Iter>>
	{
		return iterator_range<detail::NIteratorRef<Iter>>(detail::NIteratorRef<Iter>(first, 0), detail::NIteratorRef<Iter>(last, count));
	}

	/// @brief Build and return an iterable object over the integral range [first, last) with a step of 1
	/// @tparam Integral integral type
	template<class Integral, class = typename std::enable_if<std::is_integral<Integral>::value, void>::type>
	auto range(Integral first, Integral last) noexcept -> iterator_range<detail::IntegralIterator<Integral> >
	{
		using Iter = detail::IntegralIterator<Integral>;
		return iterator_range<Iter>(Iter(first), Iter(last));
	}

	/// @brief Build and return an iterable object over the integral range [first, last) with given step.
	/// @tparam Arithmetic arithmetic type
	/// It is possible to provide a negative step, in which case first should be greater than last.
	template<class Arithmetic, class = typename std::enable_if<std::is_arithmetic<Arithmetic>::value, void>::type>
	auto range(Arithmetic first, Arithmetic last, Arithmetic step) noexcept -> iterator_range<detail::ArithmeticIterator<Arithmetic> >
	{
		using Iter = detail::ArithmeticIterator<Arithmetic>;
		return iterator_range<Iter>(Iter(first, step), Iter(last, step));
	}

	/// @brief Build and return an iterable object over the range [c.begin(), c.end())
	/// @tparam Iterable container type
	/// @param c Iterable container
	template<class Container>
	auto range(const Container& c) noexcept -> iterator_range<typename Container::const_iterator>
	{
		return iterator_range<typename Container::const_iterator>(c.begin(), c.end());
	}

	/// @brief Build and return an iterable object over the range [lst.begin(), lst.end())
	template<class T>
	auto range(std::initializer_list<T> lst) noexcept -> iterator_range<const T*>
	{
		return iterator_range<const T*>(lst.begin(), lst.end());
	}

	/// @brief Just to avoid deeply nested iterator_range objects
	template<class Iter>
	auto range(const iterator_range<Iter>& r) -> const iterator_range<Iter>&
	{
		return r;
	}

	template<class Iterable1, class Iterable2>
	auto zip(const Iterable1& r1, const Iterable2 & r2) -> iterator_range<detail::ZipIterator<typename Iterable1::const_iterator, typename Iterable2::const_iterator>>
	{
		using Iter1 = typename Iterable1::const_iterator;
		using Iter2 = typename Iterable2::const_iterator;
		using ZipIter = detail::ZipIterator<Iter1, Iter2>;
		return iterator_range<ZipIter>(ZipIter(r1.begin(),r2.begin()), ZipIter(r1.end(), r2.end()));
	}

	template<class Iterable>
	auto cycle(const Iterable & r, size_t count) -> iterator_range<detail::NIteratorWrap<typename Iterable::const_iterator>>
	{
		using Iter = typename Iterable::const_iterator;
		using IterCycle = detail::NIteratorWrap<Iter>;
		return iterator_range<IterCycle>(IterCycle(r.begin(), r.end(), 0), IterCycle(count));
	}

	template<class T>
	auto repeat(T&& value, size_t count) -> iterator_range<detail::NIteratorRepeat<typename std::decay<T>::type> >
	{
		using type = typename std::decay<T>::type;
		using iter = detail::NIteratorRepeat < type>;
		return  iterator_range < iter>(iter(std::forward<T&&>(value),0), iter(count));
	}

	template<class Iterable>
	auto flatten(const Iterable & r) -> iterator_range <detail::FlattenPair<typename Iterable::const_iterator> >
	{
		using Iter = typename Iterable::const_iterator;
		using FlatIter = detail::FlattenPair<Iter>;
		return iterator_range<FlatIter>(FlatIter(r.begin()), FlatIter(r.end()));
	}
	

}//end namespace seq


#endif
