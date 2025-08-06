#ifndef SEQ_COMPOSITE_ITERATOR_HPP
#define SEQ_COMPOSITE_ITERATOR_HPP

#include <iterator>
#include <tuple>

#include "type_traits.hpp"
#include "bits.hpp"


namespace seq
{
	namespace detail
	{
		
		template<typename T>
		struct decay_tuple;

		template<typename... Ts>
		struct decay_tuple<std::tuple<Ts...>>
		{
			using type = std::tuple<std::decay_t<Ts>...>;
		};

		// forward declaration
		template<class Tuple>
		struct tuple_val;

		template<class Tuple, class... T, size_t... I>
		auto move_helper(std::tuple<T...>& t, std::index_sequence<I...>)
		{
			return Tuple(std::move(std::get<I>(t))...);
		}

		template<class Tuple, class... T>
		auto move_tuple(std::tuple<T...>& t)
		{
			return move_helper<Tuple, T...>(t, std::make_index_sequence<sizeof...(T)>{});
		}

		// Tuple of reference used by zip_iterator as reference
		template<class Tuple>
		struct tuple_ref : Tuple
		{
			using base_type = Tuple;
			using tuple_type_no_ref = typename decay_tuple<Tuple>::type;
			using value_type = tuple_val<tuple_type_no_ref>;

			template<class... Refs>
			tuple_ref(Refs&&... refs) noexcept
			  : Tuple(std::forward<Refs>(refs)...)
			{
			}

			template<class... Args>
			tuple_ref& operator=(const std::tuple<Args...>& other) noexcept(noexcept(std::declval<Tuple&>() = std::declval<std::tuple<Args...>&>()))
			{
				// Base tuple assignment
				base_type::operator=(other);
				return *this;
			}

			template<class... Args>
			tuple_ref& operator=(std::tuple<Args...>&& other) noexcept(noexcept(std::declval<Tuple&>() = std::declval<std::tuple<Args...>&&>()))
			{
				using Other = std::tuple<Args...>;
				if constexpr (std::is_same_v < Other, value_type>)
					// Base tuple move assignment
					base_type::operator=(std::move(other));
				if constexpr (std::is_same_v<Other, tuple_ref<Tuple>> || std::is_same_v < Other, tuple_type_no_ref>)
					// Force moving all ements
					base_type::operator=(move_tuple<tuple_type_no_ref>(std::move(other)));
				else
					// Standard forwarding
					base_type::operator=(std::forward<std::tuple<Args...>>(other));
				return *this;
			}
		};

		// Tuple of value used by zip_iterator as value_type
		template<class Tuple>
		struct tuple_val : Tuple
		{
			using base_type = Tuple;
			using value_type = typename decay_tuple<Tuple>::type;

			tuple_val() noexcept(noexcept(Tuple())) = default;
			tuple_val(const tuple_val&) noexcept(std::is_nothrow_copy_constructible_v<Tuple>) = default;
			tuple_val(tuple_val&&) noexcept(std::is_nothrow_move_constructible_v<Tuple>) = default;
			template<class... Args>
			tuple_val(Args&&... args) noexcept(noexcept(Tuple(std::forward<Args>(args)...)))
			  : Tuple(std::forward<Args>(args)...)
			{
			}
			template<class Other>
			tuple_val(tuple_ref<Other>&& other) noexcept(std::is_nothrow_move_constructible_v<Tuple>)
			  : Tuple(move_tuple<Tuple>(std::move(other)))
			{
			}
			template<class Other>
			tuple_val(const tuple_ref<Other>& other) noexcept(std::is_nothrow_copy_constructible_v<Tuple>)
			  : Tuple(other)
			{
			}
			tuple_val& operator=(const tuple_val&) noexcept(std::is_nothrow_copy_assignable_v<Tuple>) = default;
			tuple_val& operator=(tuple_val&&) noexcept(std::is_nothrow_move_assignable_v<Tuple>) = default;

			template<class... Args>
			tuple_val& operator=(const std::tuple<Args...>& other) noexcept(noexcept(std::declval<Tuple&>() = std::declval<std::tuple<Args...>&>()))
			{
				base_type::operator=(other);
				return *this;
			}
			template<class... Args>
			tuple_val& operator=(std::tuple<Args...>&& other) noexcept(noexcept(std::declval<Tuple&>() = std::declval<std::tuple<Args...>&&>()))
			{
				using Other = std::tuple<Args...>;
				if constexpr (std::is_same_v < Other, tuple_ref<Other>>)
					base_type::operator=(move_tuple<Tuple>(std::move(other)));
				else
					base_type::operator=(std::forward<std::tuple<Args...>>(other));
				return *this;
			}
		};

		template<class Tuple>
		void swap(tuple_ref<Tuple>& left, tuple_ref<Tuple>& right) noexcept(noexcept(left.swap(right)))
		{
			left.swap(right);
		}
		template<class Tuple>
		void swap(tuple_ref<Tuple>&& left, tuple_ref<Tuple>&& right) noexcept(noexcept(left.swap(right)))
		{
			left.swap(right);
		}

		template<class Tuple1, class Tuple2>
		tuple_ref<decltype(std::tuple_cat(std::declval<Tuple1&>(), std::declval<Tuple2&>()))> tuple_ref_cat(const tuple_ref<Tuple1>& l, const tuple_ref<Tuple2>& r) noexcept;

		template<class Tuple1, class Tuple2>
		tuple_val<decltype(std::tuple_cat(std::declval<Tuple1&>(), std::declval<Tuple2&>()))> tuple_val_cat(const tuple_val<Tuple1>& l, const tuple_val<Tuple2>& r) noexcept;


		template<class Cat>
		constexpr size_t value_for_category() noexcept
		{
			if constexpr (std::is_same_v<Cat, std::input_iterator_tag>)
				return 1;
			else if constexpr (std::is_same_v<Cat, std::forward_iterator_tag>)
				return 3;
			else if constexpr (std::is_same_v<Cat, std::bidirectional_iterator_tag>)
				return 7;
			else if constexpr (std::is_same_v<Cat, std::random_access_iterator_tag>)
				return 15;
#ifdef SEQ_HAS_CPP_20
			else if constexpr (std::is_same_v<Cat, std::contiguous_iterator_tag>)
				return 15;
#endif
			else
				return 0;
		}

		template<size_t Val>
		constexpr auto category_for_value() noexcept
		{
			static constexpr size_t index = Val ? static_bit_scan_reverse<Val>::value + 1 : 0;
			if constexpr (index == 1) return std::input_iterator_tag{};
			else if constexpr (index == 2) return std::forward_iterator_tag{};
			else if constexpr (index == 3) return std::bidirectional_iterator_tag{};
			else if constexpr (index == 4) return std::random_access_iterator_tag{};
			else return int{};
		}

		template<class Cat1, class Cat2>
		struct merge_iterator_category
		{
			using type = decltype(category_for_value<value_for_category<Cat1>() & value_for_category<Cat2>()>());
		};

		template<class Tuple, size_t N = std::tuple_size<Tuple>::value>
		struct CompositeIter
		{
			static constexpr size_t pos = std::tuple_size<Tuple>::value - N;
			using this_iterator = typename std::tuple_element<pos, Tuple>::type;
			using this_category = typename std::iterator_traits<this_iterator>::iterator_category;
			using this_value_type = tuple_val<std::tuple<typename std::iterator_traits<this_iterator>::value_type>>;
			using this_reference = tuple_ref<std::tuple<typename std::iterator_traits<this_iterator>::reference>>;

			using value_type = decltype(tuple_val_cat(std::declval<this_value_type&>(), std::declval<typename CompositeIter<Tuple, N - 1>::value_type&>()));
			using reference = decltype(tuple_ref_cat(std::declval<this_reference&>(), std::declval<typename CompositeIter<Tuple, N - 1>::reference&>()));
			using iterator_category = typename merge_iterator_category<this_category, typename CompositeIter<Tuple, N - 1>::iterator_category>::type;
			using pointer = value_type*;
			using difference_type = std::ptrdiff_t;

			template<class Ref, class... Args>
			static Ref deref(Tuple& tuple, Args&&... args) noexcept(noexcept(*std::declval<this_iterator&>()))
			{
				return CompositeIter<Tuple, N - 1>::template deref<Ref>(tuple, std::forward<Args>(args)..., *std::get<pos>(tuple));
			}
			static void incr(Tuple& t) noexcept(noexcept(++std::declval<this_iterator&>()))
			{
				++std::get<pos>(t);
				CompositeIter<Tuple, N - 1>::incr(t);
			}
			static void decr(Tuple& t) noexcept(noexcept(--std::declval<this_iterator&>()))
			{
				--std::get<pos>(t);
				CompositeIter<Tuple, N - 1>::decr(t);
			}
			static void incr(Tuple& t, difference_type d) noexcept(noexcept(std::declval<this_iterator&>() += 1))
			{
				std::get<pos>(t) += d;
				CompositeIter<Tuple, N - 1>::incr(t, d);
			}
			static difference_type distance(const Tuple& first,
									  const Tuple& last) noexcept(noexcept(std::distance(std::declval<this_iterator&>(), std::declval<this_iterator&>())))
			{
				return std::distance(std::get<0>(first), std::get<0>(last));
			}
		};

		template<class Tuple>
		struct CompositeIter<Tuple, 1>
		{
			static constexpr size_t pos = std::tuple_size<Tuple>::value - 1;
			using this_iterator = typename std::tuple_element<pos, Tuple>::type;
			using iterator_category = typename std::iterator_traits<this_iterator>::iterator_category;
			using value_type = tuple_val<std::tuple<typename std::iterator_traits<this_iterator>::value_type>>;
			using reference = tuple_ref<std::tuple<typename std::iterator_traits<this_iterator>::reference>>;
			using pointer = value_type*;
			using difference_type = std::ptrdiff_t;

			template<class Ref, class... Args>
			static Ref deref(Tuple& tuple, Args&&... args) noexcept(noexcept(*std::declval<this_iterator&>()))
			{
				return Ref(std::forward<Args>(args)..., *std::get<pos>(tuple));
			}
			static void incr(Tuple& t) noexcept(noexcept(++std::declval<this_iterator&>())) { ++std::get<pos>(t); }
			static void decr(Tuple& t) noexcept(noexcept(--std::declval<this_iterator&>())) { --std::get<pos>(t); }
			static void incr(Tuple& t, difference_type d) noexcept(noexcept(std::declval<this_iterator&>() += 1)) { std::get<pos>(t) += d; }
			static difference_type distance(const Tuple& first,
									  const Tuple& last) noexcept(noexcept(std::distance(std::declval<this_iterator&>(), std::declval<this_iterator&>())))
			{
				return std::distance(std::get<0>(first), std::get<0>(last));
			}
		};

	}

	template<class Tuple>
	class zip_iterator
	{
		using composite = detail::CompositeIter<Tuple>;

	public:
		Tuple tuple;

		using iterator_category = typename composite::iterator_category;
		using value_type = typename composite::value_type;
		using reference = typename composite::reference;
		using pointer = typename composite::pointer;
		using difference_type = typename composite::difference_type;
		static constexpr size_t iter_value = detail::value_for_category<iterator_category>();

		zip_iterator() noexcept(noexcept(Tuple())) {}
		zip_iterator(const Tuple& t)
		  : tuple(t)
		{
		}
		zip_iterator(Tuple&& t) noexcept(std::is_nothrow_move_constructible<Tuple>::value)
		  : tuple(std::move(t))
		{
		}

		zip_iterator(const zip_iterator&) = default;
		zip_iterator(zip_iterator&&) = default;
		zip_iterator& operator=(const zip_iterator&) noexcept(std::is_nothrow_move_assignable<Tuple>::value) = default;
		zip_iterator& operator=(zip_iterator&&) noexcept(std::is_nothrow_move_constructible<Tuple>::value) = default;

		auto operator++() noexcept(noexcept(composite::incr(std::declval<Tuple&>()))) -> zip_iterator&
		{
			composite::incr(tuple);
			return *this;
		}
		auto operator++(int) noexcept(noexcept(composite::incr(std::declval<Tuple&>()))) -> zip_iterator
		{
			zip_iterator _Tmp = *this;
			++(*this);
			return _Tmp;
		}

		auto operator--() noexcept(noexcept(composite::decr(std::declval<Tuple&>()))) -> zip_iterator&
		{
			static_assert(iter_value >= 3, "operator--: invalid iterator category");
			composite::decr(tuple);
			return *this;
		}
		auto operator--(int) noexcept(noexcept(composite::decr(std::declval<Tuple&>()))) -> zip_iterator
		{
			zip_iterator _Tmp = *this;
			--(*this);
			return _Tmp;
		}
		reference operator[](difference_type diff) const noexcept(noexcept(composite::incr(std::declval<Tuple&>(), 1)))
		{
			static_assert(iter_value >= 15, "operator[]: invalid iterator category");
			zip_iterator t = *this;
			composite::incr(t.tuple, diff);
			return *t;
		}
		auto operator*() const noexcept(noexcept(composite::template deref<reference>(std::declval<Tuple&>())))
		{
			return composite::template deref<reference>(const_cast<Tuple&>(tuple));
		}

		auto operator+=(difference_type diff) noexcept(noexcept(composite::incr(std::declval<Tuple&>(), 1))) -> zip_iterator&
		{
			static_assert(iter_value >= 15, "operator+=: invalid iterator category");
			composite::incr(tuple, diff);
			return *this;
		}
		auto operator-=(difference_type diff) noexcept(noexcept(composite::incr(std::declval<Tuple&>(), 1))) -> zip_iterator&
		{
			static_assert(iter_value >= 15, "operator-=: invalid iterator category");
			composite::incr(tuple, -diff);
			return *this;
		}
	};

	template<class Tuple>
	auto operator+(const zip_iterator<Tuple>& it,
					 typename zip_iterator<Tuple>::difference_type diff) noexcept(noexcept(std::declval<zip_iterator<Tuple>&>() += 1))
	{
		zip_iterator<Tuple> res = it;
		res += diff;
		return res;
	}
	template<class Tuple>
	auto operator+(typename zip_iterator<Tuple>::difference_type diff,
					 const zip_iterator<Tuple>& it) noexcept(noexcept(std::declval<zip_iterator<Tuple>&>() += 1))
	{
		zip_iterator<Tuple> res = it;
		res += diff;
		return res;
	}
	template<class Tuple>
	auto operator-(const zip_iterator<Tuple>& it,
					 typename zip_iterator<Tuple>::difference_type diff) noexcept(noexcept(std::declval<zip_iterator<Tuple>&>() -= 1))
	{
		zip_iterator<Tuple> res = it;
		res -= diff;
		return res;
	}
	template<class Tuple>
	auto operator-(typename zip_iterator<Tuple>::difference_type diff,
					 const zip_iterator<Tuple>& it) noexcept(noexcept(std::declval<zip_iterator<Tuple>&>() -= 1))
	{
		zip_iterator<Tuple> res = it;
		res -= diff;
		return res;
	}

	template<class Tuple>
	auto operator-(const zip_iterator<Tuple>& it1,
					 const zip_iterator<Tuple>& it2) noexcept(noexcept(detail::CompositeIter<Tuple>::distance(std::declval<Tuple&>(), std::declval<Tuple&>())))
	{
		using composite = detail::CompositeIter<Tuple>;
		return static_cast<typename zip_iterator<Tuple>::difference_type>(composite::distance(it2.tuple, it1.tuple));
	}
	template<class Tuple>
	auto operator==(const zip_iterator<Tuple>& it1, const zip_iterator<Tuple>& it2) noexcept(noexcept(std::declval<Tuple&>() == std::declval<Tuple&>()))
	{
		return it1.tuple == it2.tuple;
	}
	template<class Tuple>
	auto operator!=(const zip_iterator<Tuple>& it1, const zip_iterator<Tuple>& it2) noexcept(noexcept(std::declval<Tuple&>() != std::declval<Tuple&>()))
	{
		return it1.tuple != it2.tuple;
	}
	template<class Tuple>
	auto operator<(const zip_iterator<Tuple>& it1, const zip_iterator<Tuple>& it2) noexcept(noexcept(std::declval<Tuple&>() < std::declval<Tuple&>()))
	{
		static_assert(zip_iterator<Tuple>::iter_value >= 15, "operator<: invalid iterator category");
		return (it1.tuple) < (it2.tuple);
	}
	template<class Tuple>
	auto operator>(const zip_iterator<Tuple>& it1, const zip_iterator<Tuple>& it2) noexcept(noexcept(std::declval<Tuple&>() > std::declval<Tuple&>()))
	{
		static_assert(zip_iterator<Tuple>::iter_value >= 15, "operator>: invalid iterator category");
		return (it1.tuple) > (it2.tuple);
	}
	template<class Tuple>
	auto operator<=(const zip_iterator<Tuple>& it1, const zip_iterator<Tuple>& it2) noexcept(noexcept(std::declval<Tuple&>() <= std::declval<Tuple&>()))
	{
		static_assert(zip_iterator<Tuple>::iter_value >= 15, "operator<=: invalid iterator category");
		return it1.tuple <= it2.tuple;
	}
	template<class Tuple>
	auto operator>=(const zip_iterator<Tuple>& it1, const zip_iterator<Tuple>& it2) noexcept(noexcept(std::declval<Tuple&>() >= std::declval<Tuple&>()))
	{
		static_assert(zip_iterator<Tuple>::iter_value >= 15, "operator>=: invalid iterator category");
		return it1.tuple >= it2.tuple;
	}

	template<class... Args>
	auto zip_iterators(std::tuple<Args...>&& iters)
	{
		using tuple = typename detail::decay_tuple< std::tuple<Args...> >::type;
		return zip_iterator<tuple>(iters);
	}
	template<class... Args>
	auto zip_iterators(Args&&... args)
	{
		return zip_iterators(std::make_tuple(std::forward<Args>(args)...));
	}

	template<class Iter>
	class iter_range
	{
		Iter d_begin;
		Iter d_end;

	public:
		using iterator = Iter;
		using value_type = typename std::iterator_traits<Iter>::value_type;
		using reference = typename std::iterator_traits<Iter>::reference;
		using pointer = typename std::iterator_traits<Iter>::pointer;
		using difference_type = typename std::iterator_traits<Iter>::difference_type;
		using size_type = difference_type;

		iter_range(const Iter& b = Iter(), const Iter& e = Iter())
		  : d_begin(b)
		  , d_end(e)
		{
		}
		iter_range(const iter_range&) = default;
		iter_range(iter_range&&) noexcept = default;
		iter_range& operator=(const iter_range&) = default;
		iter_range& operator=(iter_range&&) = default;

		Iter begin() const { return d_begin; }
		Iter end() const noexcept { return d_end; }
	};

	namespace detail
	{
		template<class First, class... Ranges>
		auto range_begin(First&& f, Ranges&&... ranges)
		{
			return std::tuple_cat(std::make_tuple(f.begin()), range_begin(std::forward<Ranges>(ranges)...));
		}
		template<class First>
		auto range_begin(First&& f)
		{
			return std::make_tuple(f.begin());
		}
		template<class First, class... Ranges>
		auto range_end(First&& f, Ranges&&... ranges)
		{
			return std::tuple_cat(std::make_tuple(f.end()), range_end(std::forward<Ranges>(ranges)...));
		}
		template<class First>
		auto range_end(First&& f)
		{
			return std::make_tuple(f.end());
		}
	}
	template<class... Ranges>
	auto zip(Ranges&&... ranges)
	{
		auto beg = zip_iterators(detail::range_begin(std::forward<Ranges>(ranges)...));
		auto end = zip_iterators(detail::range_end(std::forward<Ranges>(ranges)...));
		return iter_range<decltype(beg)>(beg, end);
	}

}

#endif
