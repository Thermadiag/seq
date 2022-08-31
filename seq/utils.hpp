#pragma once

/** @file */

#include <memory>
#include <chrono>

#include "type_traits.hpp"


namespace seq
{

	/// @brief Memory Layout Management for containers like seq::sequence or seq::tiered_vector
	enum LayoutManagement
	{
		OptimizeForSpeed, //! Use more memory to favor speed
		OptimizeForMemory //! Use as few memory as possible
	};

	
	/// @brief Convenient random access iterator on a constant value
	template<class T>
	class cvalue_iterator
	{
		using alloc_traits = std::allocator_traits<std::allocator<T> >;
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = typename alloc_traits::difference_type;
		using size_type = typename alloc_traits::size_type;
		using pointer = typename alloc_traits::const_pointer;
		using reference = const value_type&;

		cvalue_iterator(size_type _pos = 0) : pos(_pos) {}
		cvalue_iterator(size_type _pos, const T& _value) :pos(_pos), value(_value) {}

		reference operator*() const noexcept {
			return value;
		}
		pointer operator->() const noexcept {
			return std::pointer_traits<pointer>::pointer_to(**this);
		}
		cvalue_iterator& operator++() noexcept {
			++pos;
			return *this;
		}
		cvalue_iterator operator++(int) noexcept {
			cvalue_iterator _Tmp = *this;
			++(*this);
			return _Tmp;
		}
		cvalue_iterator& operator--() noexcept {
			//TODO: check decrement
			--pos;
			return *this;
		}
		cvalue_iterator operator--(int) noexcept {
			cvalue_iterator _Tmp = *this;
			--(*this);
			return _Tmp;
		}
		bool operator==(const cvalue_iterator& other) const noexcept {
			return pos == other.pos;
		}
		bool operator!=(const cvalue_iterator& other) const noexcept {
			return pos != other.pos;
		}
		cvalue_iterator& operator+=(difference_type diff)noexcept {
			pos += diff;
			return *this;
		}
		cvalue_iterator& operator-=(difference_type diff)noexcept {
			pos -= diff;
			return *this;
		}
		const value_type& operator[](difference_type diff) const noexcept {
			return value;
		}
		
		T value;
		size_type pos;
	};

	template < class T>
	cvalue_iterator< T> operator+(const cvalue_iterator< T>& it, typename cvalue_iterator< T>::difference_type diff)noexcept {
		cvalue_iterator< T> res = it;
		return res += diff;
	}
	template < class T>
	cvalue_iterator< T> operator-(const cvalue_iterator< T>& it, typename cvalue_iterator< T>::difference_type diff)noexcept {
		cvalue_iterator< T> res = it;
		return res -= diff;
	}
	template < class T>
	typename cvalue_iterator< T>::difference_type operator-(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2)noexcept {
		return it1.pos - it2.pos;
	}
	template < class T>
	bool operator<(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2)noexcept {
		return it1.pos < it2.pos;
	}
	template < class T>
	bool operator>(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2) noexcept {
		return it1.pos > it2.pos;
	}
	template < class T>
	bool operator<=(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2)noexcept {
		return it1.pos <= it2.pos;
	}
	template < class T>
	bool operator>=(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2)noexcept {
		return it1.pos >= it2.pos;
	}

	/// @brief Simply call p->~T(), used as a replacement to std::allocator::destroy() which was removed in C++20
	template<class T>
	inline void destroy_ptr(T* p)
	{
		p->~T();
	}

	/// @brief Simply call new (p) T(...), used as a replacement to std::allocator::construct() which was removed in C++20
	template<class T, class... Args >
	inline void construct_ptr(T* p, Args&&... args)
	{
		new (p) T(std::forward<Args>(args)...);
	}


	namespace detail
	{
		// Extract the key on a std::pair or a Key value 
		template<class Key, class T>
		struct ExtractKey
		{
			using key_type = Key;
			using value_type = T;
			using mapped_type = typename T::second_type;
			SEQ_ALWAYS_INLINE static const key_type& key(const value_type& value) noexcept { return value.first; }
			template<class U>
			SEQ_ALWAYS_INLINE static const U& key(const U& value) noexcept { return value; }
		};
		template<class T>
		struct ExtractKey<T, T>
		{
			using key_type = T;
			using value_type = T;
			using mapped_type = T;
			SEQ_ALWAYS_INLINE static const T& key(const T& value) noexcept { return value; }
			template<class U>
			SEQ_ALWAYS_INLINE static const U& key(const U& value) noexcept { return value; }
		};

		// Build a std::pair or Key value from forwarded arguments
		template<class T, class... Args >
		struct BuildValue
		{
			using type = T;
			SEQ_ALWAYS_INLINE static T build(Args&&... args) { return T(std::forward<Args>(args)...); }
		};
		template<class T>
		struct BuildValue<T, T>
		{
			using type = const T&;
			SEQ_ALWAYS_INLINE static const T& build(const T& v) { return v; }
		};
		template<class T>
		struct BuildValue<T, T&>
		{
			using type = const T&;
			SEQ_ALWAYS_INLINE static const T& build(const T& v) { return v; }
		};
		template<class T>
		struct BuildValue<T, T&&>
		{
			using type = T&&;
			SEQ_ALWAYS_INLINE static T&& build(T&& v) { return std::move(v); }
		};
		template<class T>
		struct BuildValue<T, const T&>
		{
			using type = const T&;
			SEQ_ALWAYS_INLINE static const T& build(const T& v) { return v; }
		};


		// Helper class to detect is_transparent typedef in hash functor or comparison functor

		template <class T>
		struct make_void {
			using type = void;
		};

		template <class T, class = void>
		struct has_is_transparent : std::false_type {};

		template <class T>
		struct has_is_transparent<T,
			typename make_void<typename T::is_transparent>::type>
			: std::true_type {};
	}




	namespace detail
	{

		template<class L1, class L2, bool IsL1>
		struct CallLambda
		{
			template <class... Args>
			using return_type = decltype(std::declval<L1&>()(std::declval<Args>()...));

			template <class... Args>
			return_type<Args ...> operator()(const L1& l1, const L2& , Args&&... args) const
			{
				return l1(std::forward<Args>(args)...);
			}
		};
		template<class L1, class L2>
		struct CallLambda<L1, L2, false>
		{
			template <class... Args>
			using return_type = decltype(std::declval<L2&>()(std::declval<Args>()...));

			template <class... Args>
			return_type<Args ...> operator()(const L1& , const L2& l2, Args&&... args) const
			{
				return l2(std::forward<Args>(args)...);
			}
		};
	}

	/// @brief Simulation of C++17 if constexpr
	template<bool IsL1, class L1, class L2, class... Args>
	decltype(std::declval<detail::CallLambda<L1, L2, IsL1>&>()(std::declval<L1&>(), std::declval<L2&>(),std::declval<Args>()...))
		constexpr_if( const L1& l1,  const L2& l2, Args&&... args)
	{
		return detail::CallLambda<L1, L2, IsL1>{}(l1,l2, std::forward<Args>(args)...);
	}



}//end namespace seq