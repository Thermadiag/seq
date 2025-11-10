#ifndef TESTS_HPP
#define TESTS_HPP

#include <cstdint>
#include <functional>
#include <type_traits>

#include <seq/type_traits.hpp>


namespace test_detail
{
	struct Unused
	{
		static inline std::int64_t& get_count()
		{
			static std::int64_t count = 0;
			return count;
		}
	};
	static inline std::int64_t& get_count()
	{
		return Unused::get_count();
	}
}

template<class T, bool Relocatable = true>
class TestDestroy
{
	static_assert(std::is_arithmetic_v<T>, "TestDestroy only supports arithmetic types");
	
	T value;

public:
	static std::int64_t count() {
		return test_detail::get_count();
	}
	TestDestroy() 
	:value(){
		++test_detail::get_count();
	}
	TestDestroy(const T& val)
		:value(val) {
		++test_detail::get_count();
	}
	template<class U>
	TestDestroy(const U& val, typename std::enable_if<std::is_arithmetic_v<U>,void>::type* =nullptr)
		:value(static_cast<T>(val)) {
		++test_detail::get_count();
	}
	TestDestroy(const TestDestroy& val)
		:value(val.value) {
		++test_detail::get_count();
	}
	TestDestroy( TestDestroy&& val) noexcept
		:value(std::move(val.value)) {
		++test_detail::get_count();
	}
	~TestDestroy() noexcept {
		--test_detail::get_count();
	}
	TestDestroy& operator=(const T& other) {
		value = other;
		return *this;
	}
	TestDestroy& operator=( T&& other) noexcept {
		value = std::move(other);
		return *this;
	}
	TestDestroy& operator=(const TestDestroy& other) {
		value = other.value;
		return *this;
	}
	TestDestroy& operator=( TestDestroy&& other) {
		value = std::move( other.value);
		return *this;
	}
	TestDestroy& operator++() {
		++value;
		return *this;
	}
	TestDestroy& operator++(int) {
		TestDestroy t = *this;
		++value;
		return t;
	}
	TestDestroy& operator--() {
		--value;
		return *this;
	}
	TestDestroy& operator--(int) {
		TestDestroy t = *this;
		--value;
		return t;
	}
	operator T() const { return value; }
	const T& val() const { return value; }
};

template<class T, bool R> inline bool operator==(const TestDestroy<T,R>& l, const TestDestroy<T, R>& r)  { return l.val() == r.val(); }
template<class T, bool R> inline bool operator!=(const TestDestroy<T, R>& l, const TestDestroy<T, R>& r)  { return l.val() != r.val(); }
template<class T, bool R> inline bool operator<(const TestDestroy<T, R>& l, const TestDestroy<T, R>& r)  { return l.val() < r.val(); }
template<class T, bool R> inline bool operator>(const TestDestroy<T, R>& l, const TestDestroy<T, R>& r)  { return l.val() > r.val(); }
template<class T, bool R> inline bool operator<=(const TestDestroy<T, R>& l, const TestDestroy<T, R>& r)  { return l.val() <= r.val(); }
template<class T, bool R> inline bool operator>=(const TestDestroy<T, R>& l, const TestDestroy<T, R>& r)  { return l.val() >= r.val(); }

template<class T, bool R> inline TestDestroy<T, R> operator*(const TestDestroy<T, R>& v, T val) { return TestDestroy<T, R>(v.val() * val); }

namespace seq
{
	template<class T, bool R>
	struct is_relocatable<TestDestroy<T,R> > {
		static constexpr bool value = is_relocatable<T>::value && R;
	};
}

namespace std
{
	template<class T, bool R>
	struct hash<TestDestroy<T,R>>
	{
		size_t operator()(const TestDestroy<T,R>& v) const {
			return std::hash<T>{}(static_cast<T>(v));
		}
	};
}





template<class T>
struct CountAlloc
{
	typedef T value_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using is_always_equal = std::false_type;
	using propagate_on_container_swap = std::true_type;
	using propagate_on_container_copy_assignment = std::true_type;
	using propagate_on_container_move_assignment = std::true_type;

	template <class Other>
	struct rebind {
		using other = CountAlloc<Other>;
	};

	std::shared_ptr<std::int64_t> d_count;

	CountAlloc() :d_count(new std::int64_t(0)) {}
	CountAlloc(const CountAlloc& other)
		:d_count(other.d_count) {}
	template <class Other>
	CountAlloc(const CountAlloc<Other>& other) 
		: d_count(other.d_count) {}
	~CountAlloc() {}
	CountAlloc& operator=(const CountAlloc& other) {
		d_count = other.d_count;
		return *this;
	}

	bool operator==(const CountAlloc& other) const { return d_count == other.d_count; }
	bool operator!=(const CountAlloc& other) const { return d_count != other.d_count; }

	void deallocate(T* p, const size_t count) {
		std::allocator<T>{}.deallocate(p, count);
		(*d_count) -= count * sizeof(T);
	}
	T* allocate(const size_t count) {
		T* p = std::allocator<T>{}.allocate(count);
		(*d_count) += count * sizeof(T);
		return p;
	}
	T* allocate(const size_t count, const void*) { return allocate(count); }
	size_t max_size() const noexcept { return static_cast<size_t>(-1) / sizeof(T); }
};

template<class T>
std::int64_t get_alloc_bytes(const CountAlloc<T>& al)
{
	return *al.d_count;
}
template<class T>
std::int64_t get_alloc_bytes(const std::allocator<T>& )
{
	return 0;
}

#endif
