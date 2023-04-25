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

#ifndef SEQ_TINY_STRING_HPP
#define SEQ_TINY_STRING_HPP



/** @file */



#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <climits>


#include "hash.hpp"
#include "type_traits.hpp"
#include "utils.hpp"


#ifdef SEQ_HAS_CPP_17
#include <string_view>
#endif

#undef min
#undef max




namespace seq
{

	/// @brief Allocator type for tiny string view
	template<class Char>
	struct view_allocator
	{
		typedef Char value_type;
		typedef Char* pointer;
		typedef const Char* const_pointer;
		typedef Char& reference;
		typedef const Char& const_reference;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::true_type;

		template <class Other>
		struct rebind {
			using other = view_allocator<Other>;
		};

		constexpr view_allocator() noexcept {}
		constexpr view_allocator(const view_allocator&) noexcept = default;
		template <class Other>
		constexpr view_allocator(const view_allocator<Other>&) noexcept {}
		~view_allocator() = default;
		view_allocator& operator=(const view_allocator&) = default;

		void deallocate(Char* , const size_t ) {}
		Char* allocate(const size_t ) {return nullptr;}
		Char* allocate(const size_t , const void*) {return nullptr;}
		size_t max_size() const noexcept {return static_cast<size_t>(-1) / sizeof(Char);}
	};

	// forward declaration
	template<class Char, class Traits, class Allocator, size_t MaxStaticSize>
	class tiny_string;


	template<class Char>
	using basic_tstring_view = tiny_string<Char, std::char_traits<Char>, view_allocator<Char>, 0>;

	/// @brief Base string view typedef, similar to std::string_view.
	using tstring_view = basic_tstring_view<char>;
	using wtstring_view = basic_tstring_view<wchar_t>;

	/// @brief Base string typedef, similar to std::string. Equivalent to tiny_string<0, std::allocator<char>>.
	using tstring = tiny_string<char, std::char_traits<char>, std::allocator<char>, 0 >;
	using wtstring = tiny_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t>, 0 >;


	


	namespace detail
	{
		/*-************************************
		*  Common functions
		**************************************/
		
		
		//Function missing on msvc and gcc/mingw
		template<class Char>
		inline auto string_memrchr(const Char* s, Char c, size_t n) noexcept -> const Char* {
			const Char* p = s;
			for (p += n; n > 0; n--) {
				if (*--p == c) {
					return (p);
				}
			}

			return nullptr;
		}

		inline auto count_approximate_common_bytes(const void* _pIn, const void* _pMatch, const void* _pInLimit) noexcept -> size_t
		{
			static constexpr size_t stepsize = sizeof(size_t);
			const char* pIn = static_cast<const char*>(_pIn);
			const char* pMatch = static_cast<const char*>(_pMatch);
			const char* pInLimit = static_cast<const char*>(_pInLimit);
			const char* pStart = pIn;

			while (SEQ_LIKELY(pIn < pInLimit - (stepsize - 1))) {
				size_t const diff = read_size_t(pMatch) ^ read_size_t(pIn);
				if (diff == 0u) { pIn += stepsize; pMatch += stepsize; continue; }
				return static_cast<size_t>(pIn - pStart);
			}

			if ((stepsize == 8) && (pIn < (pInLimit - 3)) && (read_32(pMatch) == read_32(pIn))) { pIn += 4; pMatch += 4; }
			if ((pIn < (pInLimit - 1)) && (read_16(pMatch) == read_16(pIn))) { pIn += 2; pMatch += 2; }
			if ((pIn < pInLimit) && (*pMatch == *pIn)) { pIn++;}
			return static_cast<size_t>(pIn - pStart);
		}

		template<class Traits, class Char>
		SEQ_ALWAYS_INLINE int traits_string_compare(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			using unsigned_char = typename std::make_unsigned<Char>::type;
			if (l1 == 0) return l2 == 0 ? 0 : -1;
			if (l2 == 0) return l1 == 0 ? 0 : 1;
			if (static_cast<unsigned_char>(*v1) < static_cast<unsigned_char>(*v2)) return -1;
			if (static_cast<unsigned_char>(*v1) > static_cast<unsigned_char>(*v2)) return 1;
			int r = Traits::compare (v1, v2, std::min(l1, l2));
			if (r == 0) return l1 < l2 ? -1 : (l1 > l2 ? 1 : 0);
			return r;
		} 

	
		SEQ_ALWAYS_INLINE auto string_inf(const char* v1, const char* e1, const char* v2, const char* e2) noexcept -> bool
		{
			//comparison works on unsigned !!!!
			const char* end = v1 + std::min(e1 - v1, e2 - v2);
			while ((v1 < end - (8 - 1))) {
				std::uint64_t r1 = read_BE_64(v1);
				std::uint64_t r2 = read_BE_64(v2);
				if (r1 != r2) { return r1 < r2;}
				v1 += 8;
				v2 += 8;
			}
			if ((v1 < (end - 3))) {
				std::uint32_t r1 = read_BE_32(v1);
				std::uint32_t r2 = read_BE_32(v2);
				if (r1 != r2) { return r1 < r2;}
				v1 += 4; v2 += 4;
			}
			
			while (v1 != end) {
				if (*v1 != *v2) {
					return static_cast<unsigned char>(*v1) < static_cast<unsigned char>(*v2);}
				++v1; ++v2;
			}
			return v1 == e1 && v2 != e2;
		}

		SEQ_ALWAYS_INLINE bool string_inf(const char* v1, size_t l1, const char* v2, size_t l2) noexcept
		{

			if (l1 == 0) return l2 != 0;
			if (l2 == 0) return false;
			if (*v1 != *v2) { return static_cast<unsigned char>(*v1) < static_cast<unsigned char>(*v2); }

#ifdef __clang__
			
			// This approach is faster with clang compared to gcc and msvc
			int cmp = std::char_traits<char>::compare(v1, v2, std::min(l1, l2));
			return cmp < 0 ? true : (cmp > 0 ? false : (l1 < l2));
#else
			return string_inf(v1, v1 + l1, v2, v2 + l2);
#endif
		}
		template<class Traits, class Char>
		SEQ_ALWAYS_INLINE bool traits_string_inf(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			if (sizeof(Char) == 1)
				return string_inf(reinterpret_cast<const char*>(v1), l1, reinterpret_cast<const char*>(v2), l2);

			using unsigned_char = typename std::make_unsigned<Char>::type;
			if (l1 == 0) return l2 != 0;
			if (l2 == 0) return false;
			if (*v1 != *v2) { return static_cast<unsigned_char>(*v1) < static_cast<unsigned_char>(*v2); }
			int cmp = Traits::compare(v1, v2, std::min(l1, l2));
			return cmp < 0 ? true : (cmp > 0 ? false : (l1 < l2));
		}

		template<class Traits, class Char>
		SEQ_ALWAYS_INLINE bool traits_string_sup(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return traits_string_inf<Traits>(v2, l2, v1, l1);
		}

		
	
		
		inline auto string_inf_equal(const char* v1, size_t l1, const char* v2, size_t l2)noexcept -> bool
		{
			return !string_inf(v2, l2, v1, l1);
		}
		template<class Traits, class Char>
		inline bool traits_string_inf_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return !traits_string_inf<Traits>(v2, l2, v1, l1);
		}
		template<class Traits, class Char>
		inline bool traits_string_sup_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return traits_string_inf_equal<Traits>(v2, l2, v1, l1);
		}
		


		SEQ_ALWAYS_INLINE auto string_equal(const char* pIn, const char* pMatch, const char* pInLimit)  noexcept -> bool {
			static constexpr size_t stepsize = sizeof(size_t);
			while ((pIn < pInLimit - (stepsize - 1))) {
				if (read_size_t(pMatch) != read_size_t(pIn)) { return false;}
				pIn += stepsize; pMatch += stepsize;
			}
			while (pIn != pInLimit) {
				if (*pIn != *pMatch) { return false;}
				++pIn; ++pMatch;
			} 
			return true;

		}
		
		template<class Traits, class Char>
		SEQ_ALWAYS_INLINE bool traits_string_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return (l1 == l2) && string_equal(reinterpret_cast<const char*>(v1), reinterpret_cast<const char*>(v2), reinterpret_cast<const char*>(v1) + l1 * sizeof(Char));
		}

		template<class Traits, class Char>
		inline auto traits_string_find_first_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (pos >= size) 
				return npos;
			const Char* end = data + size;
			if (sizeof(Char) > 1 || size * n < 256U)
			{
				const Char* send = s + n;
				for (const Char* p = data + pos; p != end; ++p)
					for (const Char* m = s; m != send; ++m)
						if (*m == *p) return  static_cast<size_t>(p - data);
			}
			else {
				unsigned char buff[256 / CHAR_BIT];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) {
					unsigned char v = static_cast<unsigned char>(s[i]);
					buff[v / CHAR_BIT] |= 1U << (v % CHAR_BIT);
				}
				for (const Char* p = data + pos; p != end; ++p) {
					unsigned char v = static_cast<unsigned char>(*p);
					if ((buff[v / CHAR_BIT] >> (v % CHAR_BIT)) & 1U)
						return  static_cast<size_t>(p - data);
				}

			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_find_last_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (size == 0) return npos;
			if (pos >= size) pos = size - 1;
			const Char* p = data;
			if (sizeof(Char) > 1 || size * n < 256U)
			{
				const Char* send = s + n;
				for (const Char* in = p + pos; in >= p; --in) {
					for (const Char* m = s; m != send; ++m)
						if (*m == *in) return  static_cast<size_t>(in - p);
				}
			}
			else
			{
				unsigned char buff[256 / CHAR_BIT];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) {
					unsigned char v = static_cast<unsigned char>(s[i]);
					buff[v / CHAR_BIT] |= 1U << (v % CHAR_BIT);
				}
				for (const Char* in = p + pos; in >= p; --in) {
					unsigned char v = static_cast<unsigned char>(*in);
					if ((buff[v / CHAR_BIT] >> (v % CHAR_BIT)) & 1U)
						return  static_cast<size_t>(in - p);
				}
			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_find_first_not_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			const Char* end = data + size;
			const Char* send = s + n;
			for (const Char* p = data + pos; p != end; ++p) {
				const Char* m = s;
				for (; m != send; ++m)
					if (*m == *p) break;
				if (m == send)
					return static_cast<size_t>(p - data);
			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_find_last_not_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (size == 0) return npos;
			if (pos >= size) pos = size - 1;
			const Char* p = data;
			const Char* send = s + n;
			for (const Char* in = p + pos; in >= p; --in) {
				const Char* m = s;
				for (; m != send; ++m)
					if (*m == *in) break;
				if (m == send)
					return static_cast<size_t>(in - p);
			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_find(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (n > size || pos + n > size || n == 0) 
				return npos;

			const Char* in = data + pos;
			const Char* end = in + (size - pos - n) + 1;
			Char c = *s;
			for (;;) {
				in = Traits::find(in, static_cast<size_t>(end - in), c);//static_cast<const Char*>(memchr(in, c, static_cast<size_t>(end - in)));
				if (!in) return npos;

				//start searching, count_approximate_common_bytes returns (usually) an underestimation of the common bytes, except if equal
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == (n - 1) *sizeof(Char))
					return  static_cast<size_t>(in - data);
				//in += common +1;
				++in;
			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_rfind(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (n > size || pos < n || n == 0) return npos;
			if (pos > size) pos = size;
			const Char* beg = data;
			const Char* in = std::min(beg + pos, data + size - n);
			Char c = *s;
			for (;;) {
				in = string_memrchr(beg, c, static_cast<size_t>(in - beg + 1));
				if (!in) return npos;
				//start searching
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == (n - 1) *sizeof(Char))
					return  static_cast<size_t>(in - data);
				--in;
			}
			return npos;
		}


		template<class Char>
		struct NoneSSO
		{
			size_t not_sso : 1;
			size_t exact_size : 1;
			size_t size : sizeof(size_t) * 8 - 2;
			Char* data;
		};

		template<class Char, size_t MaxSSO =( (sizeof(NoneSSO<Char>) - alignof(Char))/sizeof(Char) - 1) >
		struct SSO
		{
			static_assert(alignof(Char) == sizeof(Char), "invalid char type alignment");
			unsigned char not_sso : 1;
			unsigned char size : 7;
			Char data[MaxSSO + 1];
		};
		
		
		template<class Char, size_t MaxSSO>
		struct string_proxy
		{
			union {
				SSO<Char,MaxSSO> sso;
				NoneSSO< Char> non_sso;
			} d_union;
		};


		template<class Char, size_t MaxSSO, class Allocator>
		struct string_internal : private Allocator
		{
			// internal storage for tiny_string and allocator
			static constexpr size_t struct_size = sizeof(string_proxy<Char,MaxSSO>);
			union U {
				SSO<Char, MaxSSO> sso;
				NoneSSO<Char> non_sso;
			};
			static constexpr size_t union_mem_size = sizeof(U);
			U d_union;

			string_internal():Allocator(){ }
			string_internal(const Allocator & al) :Allocator(al) { }
			string_internal( Allocator&& al) :Allocator(std::move(al)) { }
			auto allocate(size_t n) -> Char* {
				return this->Allocator::allocate(n);
			}
			void deallocate(Char* p, size_t n) {
				this->Allocator::deallocate(p, n);
			}
			auto get_allocator() -> Allocator &  { return *this; }
			auto get_allocator() const -> const Allocator& { return *this; }
			void set_allocator(const Allocator& al) { static_cast<Allocator&>(*this) = al; }
			void set_allocator( Allocator&& al) { static_cast<Allocator&>(*this) = std::move(al); }

			SEQ_ALWAYS_INLINE void swap(string_internal& other)
			{
				if SEQ_CONSTEXPR(!std::is_same<std::allocator<Char>, Allocator>::value)
					swap_allocator(get_allocator(), other.get_allocator());

				std::swap(d_union, other.d_union);
				
			}
		
		};


		template<class Char, size_t MaxSSO>
		struct string_internal<Char, MaxSSO, view_allocator<Char> >
		{
			// internal storage for tiny_string and view_allocator (tstring_view objects)
			const Char* data;
			size_t size;

			string_internal(const Char* d = nullptr, size_t s = 0)
			:data(d), size(s){} 

			auto allocate(size_t n) -> Char* {
				throw std::bad_alloc();
				return NULL;
			}
			void deallocate(Char*  /*unused*/, size_t  /*unused*/) {}
			auto get_allocator() const -> view_allocator<Char> { return view_allocator<Char>(); }
			void set_allocator(const view_allocator<Char>& /*unused*/) {  }
			void set_allocator(view_allocator<Char>&& /*unused*/) {  }

			void swap(string_internal& other)
			{
				std::swap(data, other.data);
				std::swap(size, other.size);
			}
		};

	


	}



	/// @brief String class with a similar interface and requirements than std::string
	/// @tparam MaxStaticSize maximum static size for Small String Optimization
	/// @tparam Allocator allocator type
	/// 
	/// 
	/// seq::tiny_string is a string class similar to std::string but aiming at greater performances when used in containers.
	/// It provides a customizable Small String Optimization (SSO) much like boost::small_vector, where the maximum static size
	/// before an allocation is triggered is defined at compile time.
	/// 
	/// seq::tiny_string contains some preallocated elements in-place, which can avoid the use of dynamic storage allocation when 
	/// the actual number of elements is below that preallocated threshold (MaxStaticSize parameter).
	/// 
	/// seq::tiny_string only supports characters of type char, not wchar_t.
	/// 
	/// 
	/// Motivation
	/// ----------
	/// 
	/// Why another string class? I started writing tiny_string to provide a small and relocatable string class that can be used within seq::cvector.
	/// Indeed, gcc std::string implementation is not relocatable as it stores a pointer to its internal data for small strings. In addition, most std::string implementations have a size of 32 bytes 
	/// (at least msvc and gcc ones), which was a lot for my compressed container. Therefore, I started working on a string implementation with the following specificities:
	/// -	Relocatable,
	/// -	Small footprint (16 bytes on 64 bits machine if possible),
	/// -	Customizable Small String Optimization (SSO),
	/// -	Higly optimized for small strings (fast copy/move assignment and fast comparison operators).
	/// 
	/// It turns out that such string implementation makes all flat containers (like std::vector or std::deque) faster (at least for small strings) as it greatly reduces cache misses.
	/// The Customizable Small String Optimization is also very convenient to avoid unnecessary memory allocations for different workloads.
	/// 
	/// 
	/// Size and bookkeeping
	/// --------------------
	/// 
	/// By default, a tiny_string contains enough room to store a 15 bytes string, therefore a length of 14 bytes for null terminated strings.
	/// For small strings (below the preallocated threshold), tiny_string only store one additional byte for bookkeeping: 7 bits for string length
	/// and 1 bit to tell if the string is allocated in-place or on the heap. It means that the default tiny_string size is 16 bytes, which is half
	/// the size of std::string on gcc and msvc. This small footprint is what makes tiny_string very fast on flat containers like std::vector ot std::deque,
	/// while node based container (like std::map) are less impacted. Note that this tiny size is only reach when using an empty allocator class like std::allocator<char>. 
	/// 
	/// When the tiny_string grows beyong the preallocated threshold, memory is allocated on the heap based on provided allocator, and the bookkeeping part is divided as follow:
	///		-	still 1 bit to tell is the memory is heap allocated or not,
	///		-	1 bit to tell if the string capacity is exactly equal to the string size + 1 (as tiny_string is always null terminated),
	///		-	sizeof(size_t)*8 - 2 bits to store the string size. Therefore, the maximum size of tiny_string is slightly lower than std::string.
	///		-	a pointer (4 to 8 bytes) to the actual memory chunk.
	/// 
	/// seq::tiny_string does not store the memory capacity, and always use a grow factor of 2. The capacity is always deduced from the string length 
	/// using compiler intrinsics (like _BitScanReverse on msvc). In some cases (like copy construction), the allocated capacity is the same as the string length,
	/// in which case a 1 bit flag is set to track this information.
	/// 
	/// The global typedef seq::tstring is provided for convenience, and is equivalent to seq::tiny_string<0,std::allocator<char>>.
	/// 
	/// Static size
	/// -----------
	/// 
	/// The maximum preallocated space is specified as a template parameter (MaxStaticSize).
	/// By default, this value is set to 0, meaning that the tiny_string only uses 2 word of either 32 or 64 bits depending on the architecture.
	/// Therefore, the maximum in-place capacity is either 7 or 15 bytes.
	/// 
	/// The maximum preallocated space can be increased up to 126 bytes. To have a tiny_string of 32 bytes like std::string on gcc and msvc, you could use, for instance, tiny_string<28>.
	/// In this case, the maximum string size (excluding null-terminated character) to use SSO would be 30 bytes (!).
	/// 
	/// 
	/// Relocatable type
	/// ----------------
	/// 
	/// seq::tiny_string is relocatable, meaning that it does not store pointer to internal data.
	/// Relocatable types can be used more efficiently in containers that are aware of this property. For instance, seq::devector,
	/// seq::tiered_vector and seq::flat_map are faster when working with relocatable types, as the process to move one object from a memory layout
	/// about to be destroyed to a new one can be accomplished through a simple memcpy.
	/// 
	/// Msvc implementation of std::string is also relocatable, while gcc implementation is not as it stores a pointer to its internal data
	/// for small strings.
	/// 
	/// Within the seq library, a relocatable type must statify seq::is_relocatable<type>::value == true.
	/// 
	/// Note that tiny_string is only relocatable if the allocator itself is relocatable (which is the case for the default std::allocator<char>).
	/// 
	/// Interface
	/// ---------
	/// 
	/// seq::tiny_string provides the same interface as std::string.
	/// Functions working on other strings like find(), compare()... are overloaded to also work on std::string.
	/// The comparison operators are also overloaded to work with std::string.
	/// 
	/// seq::tiny_string provides the following additional members for convenience:
	///		-	join(): merge several strings with a common separator
	///		-	split(): split string based on separator
	///		-	replace(): replace a string by another one
	///		-	convert(): convert the string to another type using streams
	///		-	sprintf(): format string
	/// 
	/// seq::tiny_string also works with std::istream/std::ostream exactly like std::string.
	/// A specialization of std::hash is provided for tiny_string types which relies on murmurhash2. This specialization is transparent and
	/// supports hashing std::string, tiny_string and const char*.
	/// 
	/// seq::tiny_string provides the same invalidation rules as std::string as well as the same exception guarantees.
	/// 
	/// The main difference compared to std::string is memory deallocation. As tiny_string does not store the capacity internally,
	/// its capacity is deduced from the size and must be the closest greater or equal power of 2 (except for a few situations where the capacity is excatly the length +1).
	/// Therefore, tiny_string must release memory when its size decreases due, for instance, to calls to tiny_string::pop_back().
	/// Likewise, shrink_to_fit() and reserve() are no-ops.
	/// 
	/// 
	/// String view
	/// -----------
	/// 
	/// seq::tiny_string is specialized for seq::view_allocator in order to provide a std::string_view like class.
	/// It is provided for compilers that do not support (yet) std::string_view, and provides a similar interface.
	/// 
	/// The global typedef seq::tstring_view is provided  for convenience, and is equivalent to seq::tiny_string<0,seq::view_allocator>.
	/// tstring_view is also hashable and can be compared to other tiny_string types as well as std::string.
	/// 
	/// 
	/// Performances
	/// ------------
	/// 
	/// All tiny_string members have been optimized to match or outperform (for small strings) most std::string implementations. They have been benchmarked against
	/// gcc (10.0.1) and msvc (14.20) for members compare(), find(), rfind(), find_first_of(), find_last_of(), find_first_not_of() and find_last_not_of().
	/// Comparison operators <=> are usually faster than std::string.
	/// 
	/// However, tiny_string is usually slower for back insertion with push_back(). The pop_back() member is also slower than msvc and gcc implementations.
	/// 
	/// tiny_string is usually faster when used inside flat containers simply because its size is smaller than std::string (32 bytes on gcc and msvc).
	/// The following table shows the performances of tiny_string against std::string for sorting a vector of 1M random short string (size = 14, where both tiny_string
	/// and std::string use SSO) and 1M random longer strings (size = 200, both use heap allocation). Tested with gcc 10.1.0 (-O3) for msys2 on Windows 10,
	/// using Intel(R) Core(TM) i7-10850H at 2.70GHz.
	/// 
	/// String class       | sort small (std::less) | sort small (tstring::less) | sort wide (std::less) | sort wide (tstring::less) |
	/// -------------------|------------------------|----------------------------|-----------------------|---------------------------|
	/// std::string        |          160 ms        |          122 ms            |       382 ms          |         311 ms            |
	/// seq::tiny_string   |          112 ms        |          112 ms            |       306 ms          |         306 ms            |
	/// 
	/// This benchmark is available in file 'seq/benchs/bench_tiny_string.hpp'.
	/// Note that tiny_string always uses its own comparison function. We can see that the comparison function of tiny_string is faster than the default one used
	/// by std::string. Even when using tiny_string comparator, std::string remains slightly slower due to the tinier memory footprint of tiny_string.
	/// 
	/// The difference is wider on msvc (14.20):
	/// 
	/// String class       | sort small (std::less) | sort small (tstring::less) | sort wide (std::less) | sort wide (tstring::less) |
	/// -------------------|------------------------|----------------------------|-----------------------|---------------------------|
	/// std::string        |          281 ms        |          264 ms            |       454 ms          |         441 ms            |
	/// seq::tiny_string   |          176 ms        |          176 ms            |       390 ms          |         390 ms            |
	///
	/// 
	/// 
	template<class Char, class Traits = std::char_traits<Char>, class Allocator = std::allocator<Char>, size_t MaxStaticSize = 0>
	class tiny_string
	{
		static constexpr size_t _size_of_internal = sizeof(detail::string_proxy<Char, 0>);
		static constexpr size_t _max_capacity = ((_size_of_internal - alignof(Char)) / sizeof(Char));
		static constexpr size_t _max_size = _max_capacity-1;
		static constexpr size_t _real_max_size = _max_size > MaxStaticSize ? _max_size : MaxStaticSize;
		static constexpr size_t _real_max_capacity = _real_max_size + 1;
		static constexpr size_t _bytes_for_capacity = alignof(Char) + sizeof(Char) * _real_max_capacity;
		static constexpr size_t _size_t_for_bytes = _bytes_for_capacity / sizeof(size_t) + (_bytes_for_capacity % sizeof(size_t) ? 1 : 0);
		static constexpr size_t sso_max_capacity = ((_size_t_for_bytes * sizeof(size_t)) - alignof(Char)) / sizeof(Char);
		static constexpr size_t sso_max_size = sso_max_capacity -1;

		static constexpr size_t first_allocated_capacity = sso_max_capacity < 4 ? 8 : sso_max_capacity < 8 ? 16 : sso_max_capacity < 16 ? 32 : sso_max_capacity < 32 ? 64 : 128;

		using internal_data = detail::string_internal<Char, sso_max_size,Allocator>;
		using this_type = tiny_string<Char,Traits,Allocator, MaxStaticSize>;

		//static constexpr size_t sso_max_capacity = ((sizeof(typename internal_data::U) - alignof(Char)) / sizeof(Char));

		internal_data d_data;

		template<class C, class T, class Al, size_t S>
		friend class tiny_string;
	
		//is it a small string
		SEQ_ALWAYS_INLINE bool is_sso() const noexcept{
			return d_data.d_union.sso.not_sso == 0; 
		}
		SEQ_ALWAYS_INLINE bool is_sso(size_t len) const noexcept { return len < (sso_max_capacity); }
		void set_size_internal(size_t len) noexcept {
			// set internal size for sso and non sso cases
			if (is_sso(len)) {
				d_data.d_union.sso.not_sso = 0;
				d_data.d_union.sso.size = len;
			}
			else {
				d_data.d_union.non_sso.not_sso = 1;
				d_data.d_union.non_sso.size = len;
			}
		}
		SEQ_ALWAYS_INLINE  auto size_internal() const noexcept -> size_t 
		{ 
			return is_sso() ? d_data.d_union.sso.size : d_data.d_union.non_sso.size; 
		}
		auto capacity_internal() const noexcept -> size_t {
			// returns the capacity
			return is_sso() ? 
				sso_max_capacity: 
				(d_data.d_union.non_sso.exact_size ? 
					d_data.d_union.non_sso.size+1 : 
					(d_data.d_union.non_sso.size < first_allocated_capacity ?
						first_allocated_capacity :
						(1ULL << (1ULL + (bit_scan_reverse_64(d_data.d_union.non_sso.size))))));
		}
		auto capacity_for_length(size_t len) const noexcept -> size_t {
			//returns the capacity for given length
			return is_sso(len) ? 
				sso_max_capacity : 
				(len < first_allocated_capacity ?
					first_allocated_capacity :
					(1ULL << (1ULL + (bit_scan_reverse_64(len)))));
		}

		void resize_uninitialized(size_t len, bool keep_old, bool exact_size = false) 
		{
			// resize string without initializeing memory.
			// take care of switch from sso to non sso (and opposite).
			// keep old data if keep_old is true.
			// allocate the exact amount of memory if exact_size is true.
			size_t old_size = size();
			if (len == old_size) return;

			if (is_sso(len)) {
				//from non sso to sso
				if (!is_sso()) {
					Char* ptr = d_data.d_union.non_sso.data;
					if (keep_old) 
						memcpy(d_data.d_union.sso.data, ptr, len * sizeof(Char));
					d_data.deallocate(ptr,capacity_internal());
				}
				/*else {
					//sso to sso: nothing to do
				}*/
				memset(d_data.d_union.sso.data + len, 0, (sizeof(d_data.d_union.sso.data) - (len) * sizeof(Char)) );
				d_data.d_union.sso.not_sso = 0;
				d_data.d_union.sso.size = static_cast<unsigned char>(len);
			}
			//non sso, might throw
			else {
				//from sso to non sso
				if (is_sso()) {
					Char* ptr;
					if(exact_size)
						ptr = d_data.allocate(len+1);
					else {
						size_t capacity = capacity_for_length(len);
						ptr = d_data.allocate(capacity);
					}
					if (keep_old) 
						memcpy(ptr, d_data.d_union.sso.data, d_data.d_union.sso.size * sizeof(Char));
					ptr[len] = 0;
					d_data.d_union.non_sso.data = ptr;
					d_data.d_union.non_sso.exact_size = exact_size;
				
					//clear additionals
					size_t start = static_cast<size_t>( reinterpret_cast<char*>((&d_data.d_union.non_sso.data) + 1) - reinterpret_cast<char*>(this) );
					memset(reinterpret_cast<char*>(this) + start, 0, sizeof(*this) - start);
				
				}
				//from non sso to non sso
				else {
					size_t current_capacity = capacity_internal();
					size_t new_capacity = capacity_for_length(len);
					if (current_capacity != new_capacity) {
						Char* ptr;
						if(exact_size)
							ptr = d_data.allocate(len+1);
						else
							ptr = d_data.allocate(new_capacity);
						if (keep_old)
							memcpy(ptr, d_data.d_union.non_sso.data, std::min(len, size_internal()) * sizeof(Char));
						//memset(ptr + len, 0, new_capacity - len); //reset the whole end to 0
						d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal());
						d_data.d_union.non_sso.exact_size = exact_size;
						d_data.d_union.non_sso.data = ptr;
					}
					d_data.d_union.non_sso.data[len] = 0;
				}
				d_data.d_union.non_sso.not_sso = 1;
				d_data.d_union.non_sso.size = len;
			}
		}

		template<class Iter, class Cat>
		void assign_cat(Iter first, Iter last, Cat /*unused*/)
		{
			// assign range for non random access iterators
			tiny_string tmp;
			while (first != last) {
				tmp.push_back(*first);
				++first;
			}
			*this = std::move(tmp);
		}
		template<class Iter>
		void assign_cat(Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
		{
			// assign range for random access iterators
			resize_uninitialized(static_cast<size_t>(last - first),false,true);
			std::copy(first, last, data());
		}

		template<class Iter, class Cat>
		void insert_cat(size_t pos, Iter first, Iter last, Cat /*unused*/)
		{
			// insert range for non random access iterators
			SEQ_ASSERT_DEBUG(pos <= size(), "invalid insert position");
			if (first == last)
				return;

			// push back, might throw
			size_type prev_size = size();
			for (; first != last; ++first)
				push_back(*first);
			// Might throw, fine
			std::rotate(begin() + pos, begin() + prev_size, end());
		}

		template<class Iter>
		void insert_cat(size_t pos, Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
		{
			// insert range for random access iterators
			SEQ_ASSERT_DEBUG(pos <= size(), "invalid insert position");
			if (first == last)
				return;

			if (pos < size() / 2) {
				size_type to_insert = static_cast<size_t>(last - first);
				// Might throw, fine
				resize_front(size() + to_insert);
				iterator beg = begin();
				// Might throw, fine
				Char* p = data();
				std::for_each(p+to_insert, p + to_insert + pos, [&](Char v) {*beg = v; ++beg; });
				std::for_each(p + pos, p + pos + to_insert, [&](Char& v) {v = *first++; });
			}
			else {
				// Might throw, fine
				size_type to_insert = static_cast<size_t>(last - first);
				resize(size() + to_insert);
				Char* p = data();
				size_t s = size();
				std::copy_backward(p + pos, p + (s - to_insert), p+s);
				std::copy(first, last, p + pos);
			}
		}

		SEQ_ALWAYS_INLINE bool isPow2(size_t len) const noexcept {
			// check if len is a power of 2
			return ((len - 1) & len) == 0;
		}
		SEQ_ALWAYS_INLINE bool isNextPow2(size_t len) const noexcept {
			// check if len is a power of 2
			return ((len + 1) & len) == 0;
		}

		void extend_buffer_for_push_back()
		{
			// extend buffer on push_back for non SSO case
			size_t new_capacity = capacity_for_length(d_data.d_union.non_sso.size + 1);
			// might throw, fine
			Char* ptr = d_data.allocate(new_capacity);
			memcpy(ptr, d_data.d_union.non_sso.data, d_data.d_union.non_sso.size * sizeof(Char));
			d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal());
			d_data.d_union.non_sso.data = ptr;
			d_data.d_union.non_sso.exact_size = 0;
		}
		void push_back_sso(Char c)
		{
			if (d_data.d_union.sso.size < max_static_size) {
				// SSO push back
				d_data.d_union.sso.data[d_data.d_union.sso.size++] = c;
			}
			else {
				// switch from sso to non sso, might throw
				resize_uninitialized(d_data.d_union.sso.size + 1, true);
				d_data.d_union.non_sso.data[d_data.d_union.non_sso.size - 1] = c;
			}
		}

		void push_back_complex(Char c)
		{
			// complex push_back, when we reach a power of 2 or the transition between SSO / non SSO
			if (SEQ_UNLIKELY(is_sso())) {
				push_back_sso(c);
			}
			else {
				extend_buffer_for_push_back();
				Char* p = d_data.d_union.non_sso.data + d_data.d_union.non_sso.size++;
				p[0] = c;
				p[1] = 0;
			}
		}

		void pop_back_to_sso() 
		{
			// pop_back(), transition from non SSO to SSO
			Char* ptr = d_data.d_union.non_sso.data;
			size_t cap = capacity_internal();
			d_data.d_union.sso.size = static_cast<unsigned char>(d_data.d_union.non_sso.size);
			d_data.d_union.sso.not_sso = 0;
			memcpy(d_data.d_union.sso.data, ptr, (max_static_size + 1)*sizeof(Char));
			d_data.deallocate(ptr, cap);
		}

		void erase_internal(size_t first, size_t last)
		{
			SEQ_ASSERT_DEBUG(first <= last, "erase: invalid positions");
			SEQ_ASSERT_DEBUG(last <= size(), "erase: invalid last position");
			if (first == last) return;
			size_type space_before = first;
			size_type space_after = size() - last;
			size_type s = size();
			Char* p = data();
			if (space_before < space_after) {
				std::copy_backward(p, p + first, p + last);
				resize_front(s - (last - first));
			}
			else {
				std::copy(p+ last,p+s,p + first);
				resize(s - (last - first));
			}
		}


		template <class Iter>
		void replace_cat(size_t pos, size_t len, Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
		{
			size_t input_size = (last - first);
			size_t new_size = size() - len + input_size;
			if (!is_sso() && !is_sso(new_size) && capacity_internal() == capacity_for_length(new_size)) {
				//do everything inplace
				if (input_size != len) 
					memmove(data() + pos + input_size, data() + pos + len, size() - (pos + len));

				// copy input
				std::copy(first, last, data() + pos);

				d_data.d_union.non_sso.data[new_size] = 0;
				d_data.d_union.non_sso.size = new_size;
				return;
			}

			//otherwise, create a new string and fill it (might throw)
			tiny_string other(new_size, 0);
			Char* op = other.data();
			//first part
			memcpy(op, data(), pos * sizeof(Char));
			//middle part
			std::copy(first, last, op + pos);
			//last part
			memcpy(op + pos + input_size, data() + pos + len, (size() - (pos + len)) * sizeof(Char));

			swap(other);
		}
		template <class Iter, class Cat>
		void replace_cat(size_t pos, size_t len, Iter first, Iter last, Cat c)
		{
			//otherwise, create a new string and fill it
			tiny_string other;
			//first part
			other.append(data(), pos);
			//middle part
			other.append(first, last);
			//last part
			other.append(data() + pos + len, size() - (pos + len));
			swap(other);
		}
		template <class Iter>
		void replace_internal(size_t pos, size_t len, Iter first, Iter last)
		{
			// replace portion of the string
			replace_cat(pos, len, first, last, typename std::iterator_traits<Iter>::iterator_category());
		}

		auto initialize(size_t size) -> Char*
		{
			// intialize the string for given size
			memset(&d_data.d_union, 0, sizeof(d_data.d_union));
			if (is_sso(size))
			{
				d_data.d_union.sso.not_sso = 0;
				d_data.d_union.sso.size = static_cast<unsigned char>(size);
				return d_data.d_union.sso.data;
			}
			else {
				//might throw, fine
				d_data.d_union.non_sso.data = d_data.allocate(size + 1);
				d_data.d_union.non_sso.not_sso = 1;
				d_data.d_union.non_sso.size = size;
				d_data.d_union.non_sso.exact_size = 1;
				d_data.d_union.non_sso.data[size] = 0;
				return d_data.d_union.non_sso.data;
			}
		}


		SEQ_ALWAYS_INLINE bool no_resize_non_sso(size_t low, size_t high)
		{
			//for non sso mode only, when growing (with append or push_back), tells if the internal buffer must grow
			//equivalent to capacity_for_length(new_size) != capacity_internal()
			return high < first_allocated_capacity || !((low ^ high) > low);
		}



	public:
		static_assert(MaxStaticSize < 127, "tiny_string maximum static size is limited to 126 elements");
		static_assert(alignof(Char) == sizeof(Char), "invalid char type alignment");
		static_assert(sizeof(Char) <= 4, "invalid char type");

		// Maximum string length to use SSO
		static constexpr size_t max_static_size = sso_max_capacity - 1;
		static constexpr size_t npos = static_cast<size_t>( -1);

		using traits_type = Traits;
		using value_type = Char;
		using reference = Char&;
		using pointer = Char*;
		using const_reference = const Char&;
		using const_pointer = const Char*;
		using iterator = Char*;
		using const_iterator = const Char*;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type = size_t;
		using difference_type = std::intptr_t;
		using allocator_type = Allocator;
		

	
		/// @brief Default constructor
		tiny_string()
		{
			memset(&d_data.d_union, 0, sizeof(d_data.d_union));
		}
		/// @brief Construct from allocator object
		explicit tiny_string(const Allocator& al)
			:d_data(al)
		{
			memset(&d_data.d_union, 0, sizeof(d_data.d_union));
		}
		/// @brief Construct from a null-terminated string and an allocator object
		tiny_string(const Char* data, const Allocator& al = Allocator())
			:d_data(al)
		{
			size_t len = Traits::length(data);
			memcpy(initialize(len), data, len * sizeof(Char));
		}
		/// @brief Construct from a string, its size and an allocator object
		tiny_string(const Char* data, size_t n, const Allocator& al = Allocator())
			:d_data(al)
		{
			memcpy(initialize(n), data, n * sizeof(Char));
		}
		/// @brief Construct by filling with n copy of character c and an allocator object
		tiny_string(size_t n, Char c, const Allocator& al = Allocator())
			:d_data(al)
		{
			std::fill_n(initialize(n), n, c);
		}
		/// @brief Construct from a std::string and an allocator object
		template<class Al>
		tiny_string(const std::basic_string<Char,Traits,Al>& str, const Allocator& al = Allocator())
			:d_data(al)
		{
			memcpy(initialize(str.size()), str.data(), str.size()*sizeof(Char));
		}


	#ifdef SEQ_HAS_CPP_17
		template<class Tr>
		tiny_string(const std::basic_string_view<Char,Tr>& str, const Allocator& al = Allocator())
			:d_data(al)
		{
			memcpy(initialize(str.size()), str.data(), str.size() * sizeof(Char));
		}
	#endif



		/// @brief Copy constructor
		tiny_string(const tiny_string& other)
			:d_data(copy_allocator(other.d_data.get_allocator()))
		{
			if (other.is_sso())
				// for SSO and read only string 
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			else {
				d_data.d_union.non_sso.not_sso = 1;
				d_data.d_union.non_sso.size = other.d_data.d_union.non_sso.size;
				d_data.d_union.non_sso.exact_size = 1;
				d_data.d_union.non_sso.data = d_data.allocate(d_data.d_union.non_sso.size + 1);
				memcpy(d_data.d_union.non_sso.data, other.d_data.d_union.non_sso.data,( d_data.d_union.non_sso.size + 1) * sizeof(Char));
			}
		}
		/// @brief Copy constructor with custom allocator
		tiny_string(const tiny_string& other, const Allocator& al)
			:d_data(al)
		{
			if (other.is_sso())
				// for SSO and read only string 
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			else {
				d_data.d_union.non_sso.not_sso = 1;
				d_data.d_union.non_sso.size = other.d_data.d_union.non_sso.size;
				d_data.d_union.non_sso.exact_size = 1;
				d_data.d_union.non_sso.data = d_data.allocate(d_data.d_union.non_sso.size + 1);
				memcpy(d_data.d_union.non_sso.data, other.d_data.d_union.non_sso.data, (d_data.d_union.non_sso.size + 1) * sizeof(Char));
			}
		}
		/// @brief Construct by copying the content of another tiny_string
		template<size_t OtherMaxSS, class OtherAlloc>
		tiny_string(const tiny_string<Char, Traits, OtherAlloc, OtherMaxSS>& other)
			:d_data() 
		{
			memcpy(initialize(other.size()), other.data(), other.size() * sizeof(Char));
		}
		/// @brief Construct by copying a sub-part of other
		tiny_string(const tiny_string& other, size_t pos, size_t len, const Allocator& al = Allocator())
			:d_data(al)
		{
			if (len == npos || pos + len > other.size())
				len = other.size() - pos;
			memcpy(initialize(len), other.data() + pos, len * sizeof(Char));
		}
		/// @brief Construct by copying a sub-part of other
		tiny_string(const tiny_string& other, size_t pos, const Allocator& al = Allocator())
			:d_data(al)
		{
			size_t len = other.size() - pos;
			memcpy(initialize(len), other.data() + pos, len * sizeof(Char));
		}
		/// @brief Move constructor
		SEQ_ALWAYS_INLINE tiny_string(tiny_string&& other) noexcept
			:d_data(std::move(other.d_data.get_allocator()))
		{
			memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			if (!is_sso())
				memset(&other.d_data.d_union, 0, sizeof(d_data.d_union));
		}
		/// @brief Move constructor with custom allocator
		tiny_string(tiny_string&& other, const Allocator& al) noexcept
			:d_data(al)
		{
			memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			if (!is_sso()) {
				if (!std::allocator_traits<Allocator>::is_always_equal::value && al != other.get_allocator()) {
					memcpy(initialize(other.size()), other.data(), other.size() * sizeof(Char));
				}
				else {
					memset(&other.d_data.d_union, 0, sizeof(d_data.d_union));
				}
			}
		}
		/// @brief Construct by copying the range [first,last)
		template<class Iter>
		tiny_string(Iter first, Iter last, const Allocator& al = Allocator())
			:d_data(al)
		{

			memset(&d_data.d_union, 0, sizeof(d_data.d_union));
			assign_cat(first, last, typename std::iterator_traits<Iter>::iterator_category());
		}
		/// @brief Construct from initializer list
		tiny_string(std::initializer_list<Char> il, const Allocator& al = Allocator())
			:d_data(al)
		{
			memset(&d_data.d_union, 0, sizeof(d_data.d_union));
			assign(il);
		}

		~tiny_string()
		{
			if (!is_sso()) {
				d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal());
			}
		}

		/// @brief Returns the internal character storage 
		SEQ_ALWAYS_INLINE auto data() noexcept -> Char* { return is_sso() ? d_data.d_union.sso.data : d_data.d_union.non_sso.data; }
		/// @brief Returns the internal character storage 
		SEQ_ALWAYS_INLINE auto data() const noexcept -> const Char* { return is_sso() ? d_data.d_union.sso.data : d_data.d_union.non_sso.data; }
		/// @brief Returns the internal character storage 
		SEQ_ALWAYS_INLINE auto c_str() const noexcept -> const Char* { return data(); }
		/// @brief Returns the string size (without the trailing null character)
		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_t { return size_internal(); }
		/// @brief Returns the string size (without the trailing null character)
		SEQ_ALWAYS_INLINE auto length() const noexcept -> size_t { return size(); }
		/// @brief Returns the string maximum size
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_t { return (1ULL << (sizeof(size_t) * 8ULL - 2ULL)) - 1ULL; }
		/// @brief Returns the string current capacity
		SEQ_ALWAYS_INLINE auto capacity() const noexcept -> size_t { return capacity_internal() - 1ULL; }
		/// @brief Returns true if the string is empty, false otherwise
		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return size() == 0; }
		/// @brief Returns the string allocator
		SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> const allocator_type & { return d_data.get_allocator(); }
		SEQ_ALWAYS_INLINE auto get_allocator() noexcept -> allocator_type & { return d_data.get_allocator(); }


		/// @brief Assign the range [first,last) to this string
		template<class Iter>
		auto assign(Iter first, Iter last) -> tiny_string&
		{
			assign_cat(first, last, typename std::iterator_traits<Iter>::iterator_category());
			return *this;
		}
		/// @brief Assign the content of other to this string
		auto assign(const tiny_string& other) -> tiny_string&
		{
			if (is_sso() && other.is_sso())
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			else {
				if SEQ_CONSTEXPR(assign_alloc<Allocator>::value)
				{
					if (get_allocator() != other.get_allocator()) {
						clear();
					}
				}
				assign_allocator(get_allocator(), other.get_allocator());

				resize_uninitialized(other.size(), false, true);
				memcpy(this->data(), other.c_str(), other.size() * sizeof(Char));
			}
			return *this;
		}
		/// @brief Assign the content of other to this string
		template<class OtherAlloc, size_t OtherMaxStaticSize>
		auto assign(const tiny_string<Char, Traits, OtherAlloc, OtherMaxStaticSize>& other) -> tiny_string&
		{
			resize_uninitialized(other.size(), false, true);
			memcpy(this->data(), other.c_str(), other.size() * sizeof(Char));
			return *this;
		}
		/// @brief Assign the content of other to this string
		template<class Al>
		auto assign(const std::basic_string<Char, Traits, Al>& other) -> tiny_string&
		{
			resize_uninitialized(other.size(), false, true);
			memcpy(this->data(), other.c_str(), other.size() * sizeof(Char));
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Assign the content of other to this string
		auto assign(const std::basic_string_view<Char, Traits>& other) -> tiny_string&
		{
			resize_uninitialized(other.size(), false, true);
			memcpy(this->data(), other.data(), other.size() * sizeof(Char));
			return *this;
		}
	#endif

		/// @brief Assign a sub-part of other to this string
		auto assign(const tiny_string& str, size_t subpos, size_t sublen) -> tiny_string& 
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			resize_uninitialized(sublen, false,true);
			memcpy(this->data(), str.data() + subpos, sublen * sizeof(Char));
			return *this;
		}
		/// @brief Assign a null-terminated buffer to this string
		auto assign(const Char* s) -> tiny_string&
		{
			size_t len = Traits::length(s);
			resize_uninitialized(len, false, true);
			memcpy(this->data(), s, len * sizeof(Char));
			return *this;
		}
		/// @brief Assign a buffer to this string
		auto assign(const Char* s, size_t n) -> tiny_string&
		{
			resize_uninitialized(n, false, true);
			memcpy(this->data(), s, n * sizeof(Char));
			return *this;
		}
		/// @brief Reset the string by n copies of c
		auto assign(size_t n, Char c) -> tiny_string&
		{
			resize_uninitialized(n, false,true);
			std::fill_n(this->data(), n, c);
			return *this;
		}
		/// @brief Assign to this string the initializer list il
		auto assign(std::initializer_list<Char> il) -> tiny_string&
		{
			return assign(il.begin(), il.end());
		}
		/// @brief Move the content of other to this string. Equivalent to tiny_string::operator=.
		SEQ_ALWAYS_INLINE auto assign(tiny_string&& other) noexcept -> tiny_string&
		{
			d_data.swap(other.d_data);
			return *this;
		}

		/// @brief Resize the string.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		void resize(size_t n) 
		{
			resize(n, 0);
		}
		/// @brief Resize the string.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		/// If n is bigger than size(), pad with (n-size()) copies of c.
		void resize(size_t n, Char c)
		{
			size_t old_size = size();
			if (old_size == n)
				return;
			resize_uninitialized(n, true);
			if (n > old_size)
				std::fill_n(data() + old_size, n - old_size, c);
		}

		/// @brief Resize the string from the front.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		void resize_front(size_t n)
		{
			resize_front(n, 0);
		}
		/// @brief Resize the string from the front.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		/// If n is bigger than size(), pad at the front with (n-size()) copies of c.
		void resize_front(size_t n, Char c)
		{
			size_t old_size = size();
			if (old_size == n)
				return;

			if (!is_sso() && !is_sso(n) && capacity_internal() == capacity_for_length(n)) {
				//no alloc/dealloc required
				Char* p = data();
				if (n > old_size) {
					memmove(p + n - old_size, p, old_size * sizeof(Char));
					std::fill_n(p, n - old_size, c);
				}
				else {
					memmove(p, p + old_size - n, n * sizeof(Char));
				}
				d_data.d_union.non_sso.size = n;
				d_data.d_union.non_sso.data[n] = 0;
				return;
			}

			tiny_string other(n,0);
			if (n > old_size) {
				std::fill_n(other.data(), n - old_size, c);
				memcpy(other.data() + n - old_size, data(), old_size * sizeof(Char));
			}
			else {
				memcpy(other.data() , data() + old_size -n, n * sizeof(Char));
			}
			swap(other);
		}

		/// @brief Swap the content of this string with other
		SEQ_ALWAYS_INLINE void swap(tiny_string& other) 
		{
			// Do not check for same address as this is counter productive
			d_data.swap(other.d_data);
		}

		/// @brief Clear the string and deallocate memory for wide strings
		void clear() noexcept 
		{
			resize_uninitialized(0, false);
		}
		/// @brief Does nothing (see tiny_string documentation)
		void shrink_to_fit() { } //no-op
		/// @brief Does nothing (see tiny_string documentation)
		void reserve(size_t) { } //no-op

		/// @brief Returns an iterator to the first element of the container.
		SEQ_ALWAYS_INLINE auto begin() noexcept -> iterator { return data(); }
		/// @brief Returns an iterator to the first element of the container.
		SEQ_ALWAYS_INLINE auto begin() const noexcept -> const_iterator { return data(); }
		/// @brief Returns an iterator to the first element of the container.
		SEQ_ALWAYS_INLINE auto cbegin() const noexcept -> const_iterator { return data(); }
	
		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_ALWAYS_INLINE auto end() noexcept -> iterator { return is_sso() ? d_data.d_union.sso.data + d_data.d_union.sso.size : d_data.d_union.non_sso.data + d_data.d_union.non_sso.size; }
		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_ALWAYS_INLINE auto end() const noexcept -> const_iterator { return is_sso() ? d_data.d_union.sso.data + d_data.d_union.sso.size : d_data.d_union.non_sso.data + d_data.d_union.non_sso.size; }
		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_ALWAYS_INLINE auto cend() const noexcept -> const_iterator { return end(); }
	
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }

		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto crend() const noexcept -> const_reverse_iterator { return rend(); }

		/// @brief Returns the character at pos, throw std::out_of_range for invalid position.
		SEQ_ALWAYS_INLINE Char at(size_t pos) const
		{
			if (pos >= size()) throw std::out_of_range("");
			return data()[pos];
		}
		/// @brief Returns the character at pos, throw std::out_of_range for invalid position.
		SEQ_ALWAYS_INLINE Char&at(size_t pos)
		{
			if (pos >= size()) throw std::out_of_range("");
			return data()[pos];
		}

		/// @brief Returns the character at pos
		SEQ_ALWAYS_INLINE Char operator[](size_t pos) const noexcept
		{
			SEQ_ASSERT_DEBUG(pos < size(), "invalid position");
			return data()[pos];
		}
		/// @brief Returns the character at pos
		SEQ_ALWAYS_INLINE Char& operator[](size_t pos) noexcept
		{
			SEQ_ASSERT_DEBUG(pos < size(), "invalid position");
			return data()[pos];
		}

		/// @brief Returns the last character of the string
		SEQ_ALWAYS_INLINE Char back() const noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return is_sso() ? d_data.d_union.sso.data[d_data.d_union.sso.size-1] : d_data.d_union.non_sso.data[d_data.d_union.non_sso.size-1]; 
		}
		/// @brief Returns the last character of the string
		SEQ_ALWAYS_INLINE Char & back() noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container"); 
			return is_sso() ? d_data.d_union.sso.data[d_data.d_union.sso.size - 1] : d_data.d_union.non_sso.data[d_data.d_union.non_sso.size - 1]; 
		}
		/// @brief Returns the first character of the string
		SEQ_ALWAYS_INLINE Char front() const noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container"); 
			return is_sso() ? d_data.d_union.sso.data[0] : d_data.d_union.non_sso.data[0]; 
		}
		/// @brief Returns the first character of the string
		SEQ_ALWAYS_INLINE Char& front() noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container"); 
			return is_sso() ? d_data.d_union.sso.data[0] : d_data.d_union.non_sso.data[0]; 
		}
	
		/// @brief Append character to the back of the string
		void push_back(Char c)
		{
			if (SEQ_LIKELY(!is_sso() && !(d_data.d_union.non_sso.exact_size || isNextPow2(d_data.d_union.non_sso.size )))) {
				Char* p = d_data.d_union.non_sso.data + d_data.d_union.non_sso.size++;
				p[0] = c;
				p[1] = 0;
				return;
			}
			
			push_back_complex(c);
		}
		/// @brief Removes the last character of the string
		void pop_back()
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_back on an empty string!");

			if (is_sso()) {
				d_data.d_union.sso.data[--d_data.d_union.sso.size] = 0;
			}
			else {
				if (SEQ_UNLIKELY(d_data.d_union.non_sso.exact_size || isPow2(d_data.d_union.non_sso.size))) {
					resize_uninitialized(d_data.d_union.non_sso.size - 1, true);
				}
				else {
					d_data.d_union.non_sso.data[--d_data.d_union.non_sso.size] = 0;
					if (SEQ_UNLIKELY(d_data.d_union.non_sso.size == max_static_size)) {
						//back to sso
						pop_back_to_sso();
					}
				}
			}
		}

		/// @brief Append the content of str to this string
		template<class Al, size_t Ss>
		auto append(const tiny_string<Char, Traits ,Al, Ss>& str) -> tiny_string&
		{
			return append(str.data() ,str.size());
		}

		/// @brief Append the content of str to this string
		template<class Al>
		auto append(const std::basic_string<Char, Traits, Al>& str) -> tiny_string&
		{
			return append(str.data(), str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Append the content of str to this string
		auto append(const std::basic_string_view<Char, Traits>& str) -> tiny_string&
		{
			return append(str.data(), str.size());
		}
	#endif

		/// @brief Append the sub-part of str to this string
		template<size_t Ss, class Al>
		auto append(const tiny_string<Char,Traits,Al, Ss>& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}
		/// @brief Append the sub-part of str to this string
		template<class Al>
		auto append(const std::basic_string<Char, Traits, Al>& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Append the sub-part of str to this string
		auto append(const std::basic_string_view<Char, Traits>& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}
	#endif


		/// @brief Append a null-terminated buffer to this string
		auto append(const Char* s) -> tiny_string&
		{
			return append(s, Traits::length(s));
		}
		
		/// @brief Append buffer content to this string
		auto append(const Char* s, size_t n) -> tiny_string&
		{
			if (SEQ_UNLIKELY(n == 0)) return *this;
			size_t old_size = size();
			size_t new_size = old_size + n;
			//try to avoid a costly resize_uninitialized
			if (is_sso() || !no_resize_non_sso(old_size, new_size) || d_data.d_union.non_sso.exact_size) {
				resize_uninitialized(new_size, true);
			}
			else {
				d_data.d_union.non_sso.size = new_size;
			}
			Char* d = data();
			memcpy(d + old_size, s, n * sizeof(Char));
			d[new_size] = 0;
			return *this;
		}
		/// @brief Append n copies of character c to the string
		auto append(size_t n, Char c) -> tiny_string&
		{
			if (SEQ_UNLIKELY(n == 0)) return *this;
			size_t old_size = size();
			size_t new_size = old_size + n;
			//try to avoid a costly resize_uninitialized
			if (is_sso() || !no_resize_non_sso(old_size, new_size) || d_data.d_union.non_sso.exact_size) 
				resize_uninitialized(new_size, true);
			else {
				d_data.d_union.non_sso.size = new_size;
			}
			Char* d = data();
			std::fill_n(d + old_size, n, c);
			d[new_size] = 0;
			return *this;
		}
		/// @brief Append the content of the range [first,last) to this string
		template <class Iter>
		auto append(Iter first, Iter last) -> tiny_string&
		{
			if (first == last) return *this;
			if (std::is_same<typename std::iterator_traits<Iter>::iterator_category, std::random_access_iterator_tag>::value) {
				size_t n = std::distance( first, last);
				size_t old_size = size();
				size_t new_size = old_size + n;
				//try to avoid a costly resize_uninitialized
				if (is_sso() || !no_resize_non_sso(old_size, new_size) || d_data.d_union.non_sso.exact_size) 
					resize_uninitialized(new_size, true);
				else {
					d_data.d_union.non_sso.size = old_size + n;
				}
				std::copy(first, last, data() + old_size);
				data()[new_size] = 0;
			}
			else {
				while (first != last) {
					push_back(*first);
					++first;
				}
			}
			return *this;
		}
		/// @brief Append the content of the range [il.begin(),il.end()) to this string
		auto append(std::initializer_list<Char> il)  -> tiny_string&
		{
			return append(il.begin(), il.end());
		}


	
		/// @brief Inserts the content of str at position pos
		template<size_t S, class AL>
		auto insert(size_t pos, const tiny_string<Char,Traits,AL, S>& str) -> tiny_string&
		{
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}
		/// @brief Inserts a sub-part of str at position pos
		template<class Al>
		auto insert(size_t pos, const std::basic_string<Char, Traits, Al>& str) -> tiny_string&
		{
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Inserts a sub-part of str at position pos
		auto insert(size_t pos, const std::basic_string_view<Char, Traits>& str) -> tiny_string&
		{
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}
	#endif

		/// @brief Inserts a sub-part of str at position pos
		auto insert(size_t pos, const tiny_string& str, size_t subpos, size_t sublen = npos)  -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			insert(begin() + pos, str.begin() + subpos, str.begin() + subpos + sublen);
			return *this;
		}
		/// @brief Inserts a null-terminated buffer at position pos
		auto insert(size_t pos, const Char* s)  -> tiny_string&
		{
			//avoid multiple push_back
			size_t len = Traits::length(s);
			insert(begin() + pos, s, s + len);
			return *this;
		}
		/// @brief Inserts buffer at position pos
		auto insert(size_t pos, const Char* s, size_t n) -> tiny_string&
		{
			insert(begin() + pos, s, s+n);
			return *this;
		}
		/// @brief Inserts n copies of character c at position pos
		auto insert(size_t pos, size_t n, Char c)  -> tiny_string&
		{
			insert(begin() + pos, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n));
			return *this;
		}
		/// @brief Inserts n copies of character c at position p
		auto insert(const_iterator p, size_t n, Char c) -> iterator
		{
			return insert(p, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n));
		}
		/// @brief Inserts character c at position p
		auto insert(const_iterator p, Char c) -> iterator
		{
			return insert(p, &c, (&c) + 1);
		}
		/// @brief Inserts the content of the range [first,last) at position p
		template <class InputIterator>
		auto insert(const_iterator p, InputIterator first, InputIterator last) -> iterator
		{
			size_t pos = static_cast<size_t>(p - cbegin());
			insert_cat(pos, first, last, typename std::iterator_traits<InputIterator>::iterator_category());
			return begin() + pos;
		}
		/// @brief Inserts the content of the range [il.begin(),il.end()) at position p
		auto insert(const_iterator p, std::initializer_list<Char> il) -> iterator
		{
			size_t pos = p - cbegin();
			insert_cat(pos, il.begin(), il.end(), std::random_access_iterator_tag());
			return begin() + pos;
		}

		/// @brief Removes up to sublen character starting from subpos
		auto erase(size_t subpos , size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > size())
				sublen = size() - subpos;
			erase_internal(subpos, subpos + sublen);
			return *this;
		}
		/// @brief Remove character at position p
		auto erase(const_iterator p) -> iterator
		{
			size_type f = static_cast<size_t>(p - begin());
			erase_internal(f, f+1);
			return begin() + f;
		}
		/// @brief Removes the range [first,last)
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			size_type f = static_cast<size_t>(first - begin());
			erase_internal(f, static_cast<size_t>(last - begin()));
			return begin() + f;
		}


		/// @brief Replace up to len character starting from pos by str
		template< class Al>
		auto replace(size_t pos, size_t len, const std::basic_string<Char, Traits, Al>& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Replace up to len character starting from pos by str
		auto replace(size_t pos, size_t len, const std::basic_string_view<Char, Traits>& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}
	#endif


		/// @brief Replace characters in the range [i1,i2) by str
		template<class Al>
		auto replace(const_iterator i1, const_iterator i2, const std::basic_string<Char, Traits, Al>& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Replace characters in the range [i1,i2) by str
		auto replace(const_iterator i1, const_iterator i2, const std::basic_string_view<Char, Traits>& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}
	#endif


		/// @brief Replace up to len character starting from pos by str
		template<class Al, size_t S>
		auto replace(size_t pos, size_t len, const tiny_string<Char,Traits,Al,S>& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by str
		template<class Al, size_t S>
		auto replace(const_iterator i1, const_iterator i2, const tiny_string<Char,Traits, Al,S>& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}


		/// @brief Replace up to len character starting from pos by a sub-part of str
		template<class Al, size_t S>
		auto replace(size_t pos, size_t len, const tiny_string<Char,Traits,Al,S>& str,
			size_t subpos, size_t sublen)  -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			if (sublen == npos || subpos + sublen > str.size())
				sublen = size() - subpos;
			replace_internal(pos, len, str.begin() + subpos, str.begin() + subpos + sublen);
			return *this;
		}
		/// @brief Replace up to len character starting from pos by a sub-part of str
		template<class Al>
		auto replace(size_t pos, size_t len, const std::basic_string<Char, Traits, Al>& str,
			size_t subpos, size_t sublen) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			if (sublen == npos || subpos + sublen > str.size())
				sublen = size() - subpos;
			replace_internal(pos, len, str.begin() + subpos, str.begin() + subpos + sublen);
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Replace up to len character starting from pos by a sub-part of str
		auto replace(size_t pos, size_t len, const std::basic_string_view<Char, Traits>& str,
			size_t subpos, size_t sublen) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			if (sublen == npos || subpos + sublen > str.size())
				sublen = size() - subpos;
			replace_internal(pos, len, str.begin() + subpos, str.begin() + subpos + sublen);
			return *this;
		}
	#endif

		/// @brief Replace up to len character starting from pos by s
		auto replace(size_t pos, size_t len, const Char* s) -> tiny_string&
		{
			size_t l = Traits::length(s);
			replace_internal(pos, len, s, s+l);
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by null-terminated string s
		auto replace(const_iterator i1, const_iterator i2, const Char* s) -> tiny_string&
		{
			size_t l = Traits::length (s);
			replace_internal(i1 - cbegin(), i2-i1, s, s + l);
			return *this;
		}
		/// @brief Replace up to len character starting from pos by buffer s of size n
		auto replace(size_t pos, size_t len, const Char* s, size_t n)  -> tiny_string&
		{
			replace_internal(pos,len, s, s + n);
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by buffer s of size n
		auto replace(const_iterator i1, const_iterator i2, const Char* s, size_t n) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, s, s + n);
			return *this;
		}

		/// @brief Replace up to len character starting from pos by n copies of c
		auto replace(size_t pos, size_t len, size_t n, Char c)  -> tiny_string&
		{
			replace_internal(pos, len, cvalue_iterator<Char>(0,c), cvalue_iterator<Char>(n));
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by n copies of c
		auto replace(const_iterator i1, const_iterator i2, size_t n, Char c) -> tiny_string&
		{
			replace_internal(i1-cbegin(), i2-i1, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n));
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by the range [first,last)
		template <class InputIterator>
		auto replace(const_iterator i1, const_iterator i2,
			InputIterator first, InputIterator last)  -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, first, last);
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by the range [il.begin(),il.end())
		auto replace(const_iterator i1, const_iterator i2, std::initializer_list<Char> il)  -> tiny_string&
		{
			return replace(i1, i2, il.begin(), il.end());
		}

	
		/// @brief Replace any sub-string _from of size n1 by the string _to of size n2, starting to position start
		auto replace(const Char* _from, size_t n1, const Char* _to, size_t n2, size_t start = 0) -> size_t
		{
			size_t res = 0;
			size_t start_pos = start;
			while ((start_pos = find(_from, start_pos,n1)) != std::string::npos) {
				replace(start_pos, n1, _to, n2);
				start_pos += n2; // Handles case where 'to' is a substring of 'from'
				++res;
			}
			return res;
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		auto replace(const Char* _from, const Char* _to, size_t start = 0) -> size_t
		{
			return replace(_from, Traits::length(_from), _to, Traits::length(_to), start);
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		template<size_t S1, class Al1, size_t S2, class Al2>
		auto replace(const tiny_string<Char,Traits,Al1,S1>& _from, const tiny_string<Char,Traits,Al2,S2>& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(),start);
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		template<class Al1, class Al2>
		auto replace(const std::basic_string<Char, Traits, Al1>& _from, const std::basic_string<Char, Traits, Al2>& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(),start);
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		auto replace(const std::basic_string_view<Char, Traits>& _from, const std::basic_string_view<Char, Traits>& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(), start);
		}
	#endif

	
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const Char* str, size_t n, size_t start = 0) const noexcept  -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(str,start,n); offset != npos; offset = find(str, offset + n,n))
				++count;
			return count;
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const Char* str, size_t start = 0) const noexcept  -> size_t
		{
			return count(str, Traits::length(str),start);
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		template<class Al, size_t S>
		auto count(const tiny_string<Char,Traits,Al,S>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(),start);
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		template<class Al>
		auto count(const std::basic_string<Char, Traits, Al>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(),start);
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const std::basic_string_view<Char, Traits>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
	#endif


		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(Char c, size_t start = 0) const noexcept  -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(c,start); offset != npos; offset = find(c,offset + 1))
				++count;
			return count;
		}


		/// @brief Copies a substring [pos, pos+len) to character string pointed to by s.
		/// Returns the number of copied characters.
		auto copy(Char* s, size_t len, size_t pos = 0) const -> size_t
		{
			if (pos > size()) throw std::out_of_range("tiny_string::copy out of range");
			if (len == npos || pos + len > size())
				len = size() - pos;
			memcpy(s, data() + pos, len * sizeof(Char));
			return len;
		}

		// @brief Returns a sub-part of this string as a tstring_view object
		auto substr(size_t pos, size_t len = npos) const -> tiny_string<Char, Traits, view_allocator<Char>, 0>
		{
			if (pos > size()) throw std::out_of_range("tiny_string::substr out of range");
			if (len == npos || pos + len > size())
				len = size() - pos;
			return tiny_string<Char, Traits, view_allocator<Char>, 0>(begin() + pos, len);
		}

		template<class Al, size_t S>
		auto find(const tiny_string<Char,Traits,Al,S>& str, size_t pos = 0) const noexcept -> size_t 
		{
			return find(str.data(), pos, str.size());
		}
		template<class Al>
		auto find(const std::basic_string<Char, Traits, Al>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		auto find(const std::basic_string_view<Char, Traits>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
	#endif

		auto find(const Char* s, size_t pos = 0) const -> size_t
		{
			return find(s, pos, Traits::length(s));
		}
		auto find(const Char* s, size_t pos, size_type n) const -> size_t
		{
			return detail::traits_string_find<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find(Char c, size_t pos = 0) const noexcept -> size_t
		{
			const Char* p = Traits::find(data() + pos, size() - pos, c);
			return p == NULL ? npos : p - begin();
		}

		template<class Al, size_t S>
		auto rfind(const tiny_string<Char,Traits,Al,S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
		template<class Al>
		auto rfind(const std::basic_string<Char, Traits, Al>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		size_t rfind(const std::basic_string_view<Char, Traits>& str, size_t pos = npos) const noexcept
		{
			return rfind(str.data(), pos, str.size());
		}
	#endif

		auto rfind(const Char* s, size_t pos = npos) const -> size_t
		{
			return rfind(s, pos, Traits::length(s));
		}
		auto rfind(const Char* s, size_t pos, size_type n) const -> size_t
		{
			return detail::traits_string_rfind<Traits>(data(), pos, size(), s, n, npos);
		}
		auto rfind(Char c, size_t pos = npos) const noexcept -> size_t
		{
			if (pos >= size()) pos = size()-1;
			const Char* p = detail::string_memrchr(data(), c, pos + 1);
			return p == NULL ? npos : p - data();
		}


		auto find_first_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_first_of<Traits>(data(), pos, size(), s, n, npos);
		}
		template<class Al, size_t S>
		auto find_first_of(const tiny_string<Char,Traits,Al,S>& str, size_t pos = 0) const noexcept -> size_t 
		{
			return find_first_of(str.data(), pos, str.size());
		}
		auto find_first_of(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(s, pos, Traits::length(s));
		}
		auto find_first_of(Char c, size_t pos = 0) const noexcept -> size_t
		{
			return find(c, pos);
		}

		template<class Al, size_t S>
		auto find_last_of(const tiny_string<Char,Traits,Al,S>& str, size_t pos = npos) const noexcept -> size_t 
		{
			return find_last_of(str.data(), pos, str.size());
		}
		auto find_last_of(const Char* s, size_t pos = npos) const noexcept -> size_t 
		{
			return find_last_of(s, pos, Traits::length(s));
		}
		auto find_last_of(const Char* s, size_t pos, size_t n = npos) const noexcept -> size_t
		{
			return detail::traits_string_find_last_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_last_of(Char c, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(c, pos);
		}

		template<class Al, size_t S>
		auto find_first_not_of(const tiny_string<Char,Traits,Al,S>& str, size_t pos = 0) const noexcept -> size_t 
		{
			return find_first_not_of(str.data(), pos, str.size());
		}
		auto find_first_not_of(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(s, pos, Traits::length(s));
		}
		auto find_first_not_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_first_not_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_first_not_of(Char c, size_t pos = 0) const noexcept -> size_t
		{
			const Char* e = end();
			for (const Char* p = data() + pos; p != e; ++p)
				if (*p != c) return p - data();
			return npos;
		}

		template<class Al, size_t S>
		auto find_last_not_of(const tiny_string<Char,Traits,Al,S>& str, size_t pos = npos) const noexcept -> size_t 
		{
			return find_last_not_of(str.data(), pos, str.size());
		}
		auto find_last_not_of(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(s, pos, Traits::length(s));
		}
		auto find_last_not_of(const Char* s, size_t pos, size_t n = npos) const noexcept -> size_t
		{
			return detail::traits_string_find_last_not_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_last_not_of(Char c, size_t pos = npos) const noexcept -> size_t
		{
			if (size() == 0) return npos;
			if (pos >= size()) pos = size() - 1;
			const Char* p = data();
			for (const Char* in = p + pos; in >= p; --in)
				if (*in != c)
					return in - p;
			return npos;
		}

		template<class Al, size_t S>
		auto compare(const tiny_string<Char,Traits,Al,S>& str) const noexcept -> int 
		{
			return compare(0, size(), str.data(), str.size());
		}
		template<class Al, size_t S>
		auto compare(size_t pos, size_t len, const tiny_string<Char, Traits, Al, S>& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}
		template<class Al>
		auto compare(size_t pos, size_t len, const std::basic_string<Char, Traits, Al>& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		auto compare(size_t pos, size_t len, const std::basic_string_view<Char, Traits>& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}
	#endif
		template<class Al, size_t S>
		auto compare(size_t pos, size_t len, const tiny_string<Char, Traits, Al, S>& str, size_t subpos, size_t sublen) const noexcept -> int
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return compare(pos, len, str.data() + subpos, sublen);
		}
		auto compare(const Char* s) const noexcept -> int
		{
			return compare(0, size(), s);
		}
		auto compare(size_t pos, size_t len, const Char* s) const noexcept -> int
		{
			return compare(pos, len, s, Traits::length(s));
		}
		auto compare(size_t pos, size_t len, const Char* _s, size_t n) const noexcept -> int
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			return detail::traits_string_compare<Traits>(data() + pos, len, _s, n);
		}


		auto operator=(const tiny_string& other) -> tiny_string& 
		{
			if (this != std::addressof(other)) 
				assign(other);
			return *this;
		} 
		template<class Al, size_t S>
		auto operator=(const tiny_string<Char,Traits, Al,S>& other) -> tiny_string&
		{
			return assign(other);
		}
		SEQ_ALWAYS_INLINE auto operator=(tiny_string && other) noexcept -> tiny_string&
		{
			// Do not test for same address, which is counter productive
			d_data.swap( other.d_data);
			return *this;
		}
		auto operator=(const Char* other) -> tiny_string&
		{
			return assign(other);
		}
		auto operator=(Char c) -> tiny_string&
		{
			if (!is_sso())
				d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal());
			memset(&d_data.d_union, 0, sizeof(d_data.d_union));
			d_data.d_union.sso.not_sso = 0;
			d_data.d_union.sso.size = 1;
			d_data.d_union.sso.data[0] = c;
			return *this;
		}
		template<class Al>
		auto operator=(const std::basic_string<Char, Traits, Al>& other) -> tiny_string&
		{
			return assign(other);
		}

	#ifdef SEQ_HAS_CPP_17
		auto operator=(const std::basic_string_view<Char, Traits>& other) -> tiny_string&
		{
			return assign(other);
		}
	#endif

		auto operator=( std::initializer_list<Char> il) -> tiny_string&
		{
			return assign(il);
		}

		template<class Al,size_t S>
		auto operator+= (const tiny_string<Char,Traits,Al,S>& str) -> tiny_string& { return append(str); }
		template<class Al>
		auto operator+= (const std::basic_string<Char, Traits, Al>& str) -> tiny_string& { return append(str); }
		auto operator+= (const Char* s) -> tiny_string& { return append(s); }
		auto operator+= (Char c) -> tiny_string& { return append(c); }
		auto operator+= (std::initializer_list<Char> il) -> tiny_string& { return append(il); }

	#ifdef SEQ_HAS_CPP_17
		auto operator+= (const std::basic_string_view<Char, Traits>& str) -> tiny_string& { return append(str); }
	#endif

		/// @brief Implicit conversion to std::string
		template< class Al>
		operator std::basic_string<Char, Traits, Al>() const {
			return std::basic_string<Char, Traits, Al>(data(), size());
		}
	};





	/// @brief Specialization of tiny_string for string views.
	/// You should use the global typedef tstring_view equivalent to tiny_string<0,view_allocator>.
	/// Provides a similar interface to std::string_view.
	/// See tiny_string documentation for more details.
	template<class Char, class Traits>
	struct tiny_string<Char,Traits, view_allocator<Char>,0>
	{
		using internal_data = detail::string_internal<Char, 0, view_allocator<Char>>;
		using this_type = tiny_string<Char, Traits, view_allocator<Char>, 0>;

		static constexpr size_t sso_max_capacity = ((sizeof(internal_data) - alignof(Char)) / sizeof(Char));

		internal_data d_data;

		//is it a small string
		SEQ_ALWAYS_INLINE bool is_sso() const noexcept { return 0; }
		SEQ_ALWAYS_INLINE bool is_sso(size_t  /*unused*/) const noexcept { return 0; }
		auto size_internal() const noexcept -> size_t { return d_data.size; }
	
		SEQ_ALWAYS_INLINE bool isPow2(size_t len) const noexcept {
			return static_cast<int>(((len - static_cast<size_t>(1)) & len) == 0);
		}


	public:
		static constexpr size_t max_static_size = sso_max_capacity - 1;
		static constexpr size_t npos = static_cast<size_t>(-1);

		using traits_type = std::char_traits<Char>;
		using value_type = Char;
		using reference = Char&;
		using pointer = Char*;
		using const_reference = const Char&;
		using const_pointer = const Char*;
		using iterator = const Char*;
		using const_iterator = const Char*;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type = size_t;
		using difference_type = std::intptr_t;
		using allocator_type = view_allocator<Char>;
		

		auto data() const noexcept -> const Char* { return d_data.data; }
		auto c_str() const noexcept -> const Char* { return data(); }
		auto size() const noexcept -> size_t { return size_internal(); }
		auto length() const noexcept -> size_t { return size(); }
		static auto max_size() noexcept -> size_t { return std::numeric_limits<size_t>::max(); }
		auto empty() const noexcept -> bool { return size() == 0; }
		auto get_allocator() const noexcept -> allocator_type { return d_data.get_allocator(); }

		tiny_string()
			:d_data(nullptr,0) {}
	
		tiny_string(const Char* data)
			:d_data(data, Traits::length(data)) {}
	
		tiny_string(const Char* data, size_t n)
			:d_data(data, n) {}
	
		template<class Al>
		tiny_string(const std::basic_string<Char, Traits, Al>& str)
			:d_data(str.data(), str.size()) {}

	#ifdef SEQ_HAS_CPP_17
		tiny_string(const std::basic_string_view<Char, Traits>& str)
			:d_data(str.data(), str.size()) {}
	#endif
	
		template<class Al, size_t S>
		tiny_string(const tiny_string<Char, Traits,Al,S>& other)
			: d_data(other.data(), other.size()) {}

		tiny_string(const tiny_string& other)
			: d_data(other.d_data) {}
	
		template<class Iter>
		tiny_string(Iter first, Iter last)
			: d_data(&(*first), last - first) {}

		auto operator=(const tiny_string& other) -> tiny_string&
		{
			d_data = other.d_data;
			return *this;
		}
		template<class A, size_t S>
		auto operator=(const tiny_string<Char,Traits,A,S>& other) -> tiny_string&
		{
			d_data = internal_data(other.data(),other.size());
			return *this;
		}
		template<class Al>
		auto operator=(const std::basic_string<Char, Traits, Al>& other) -> tiny_string&
		{
			d_data = internal_data(other.data(), other.size());
			return *this;
		}
		auto operator=(const Char* other) -> tiny_string&
		{
			d_data = internal_data(other, Traits::length (other));
			return *this;
		}
#ifdef SEQ_HAS_CPP_17
		auto operator=(const std::basic_string_view<Char, Traits>& other) -> tiny_string&
		{
			d_data = internal_data(other.data(), other.size());
			return *this;
		}
#endif
	
		SEQ_ALWAYS_INLINE void swap(tiny_string& other)
		{
			d_data.swap( other.d_data);
		}

		auto begin() const noexcept -> const_iterator { return data(); }
		auto cbegin() const noexcept -> const_iterator { return data(); }
		auto end() const noexcept -> const_iterator { return d_data.data + d_data.size; }
		auto cend() const noexcept -> const_iterator { return d_data.data + d_data.size; }
		auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		auto crend() const noexcept -> const_reverse_iterator { return rend(); }

		auto at(size_t pos) const -> Char
		{
			if (pos >= size()) { throw std::out_of_range("");}
			return data()[pos];
		}
		auto operator[](size_t pos) const noexcept -> Char
		{
			return data()[pos];
		}

		auto back() const noexcept -> Char { return data()[size() - 1]; }
		auto front() const noexcept -> Char { return data()[0]; }

		/**
		* Convenient function, returns the count of non-overlapping occurrences of 'str'
		*/
		auto count(const Char* str, size_t n, size_t start = 0) const noexcept -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(str, start, n); offset != npos; offset = find(str, offset + n, n))
				++count;
			return count;
		}
		auto count(const Char* str, size_t start = 0) const noexcept -> size_t
		{
			return count(str, Traits::length(str), start);
		}
		auto count(const tiny_string& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
		template<class Al>
		auto count(const std::basic_string<Char, Traits, Al>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}

	#ifdef SEQ_HAS_CPP_17
		auto count(const std::basic_string_view<Char, Traits>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
	#endif

		auto count(Char c, size_t start = 0) const noexcept -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(c, start); offset != npos; offset = find(c, offset + 1))
				++count;
			return count;
		}


		auto copy(Char* s, size_t len, size_t pos = 0) const -> size_t
		{
			if (pos > size()) { throw std::out_of_range("tiny_string::copy out of range");}
			if (len == npos || pos + len > size()) {
				len = size() - pos;
			}		
			memcpy(s, data() + pos, len*sizeof(Char));
			return len;
		}

		auto substr(size_t pos = 0, size_t len = npos) const -> tiny_string
		{
			if (pos > size()) { throw std::out_of_range("tiny_string::substr out of range");}
			if (len == npos || pos + len > size()) {
				len = size() - pos;
			}
			return {begin() + pos, len};
		}

		template<class Al, size_t S>
		auto find(const tiny_string<Char,Traits,Al,S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
		template<class Al>
		auto find(const std::basic_string<Char, Traits, Al>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		auto find(const std::basic_string_view<Char, Traits>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
	#endif

		auto find(const Char* s, size_t pos = 0) const -> size_t
		{
			return find(s, pos, Traits::length(s));
		}
		auto find(const Char* s, size_t pos, size_type n) const -> size_t
		{
			return detail::traits_string_find<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find(Char c, size_t pos = 0) const noexcept -> size_t
		{
			const Char* p = Traits::find(data() + pos, size() - pos, c);
			return p == nullptr ? npos : static_cast<size_t>(p - begin());
		}

		template<class A, size_t S>
		auto rfind(const tiny_string<Char,Traits,A,S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
		template<class Al>
		auto rfind(const std::basic_string<Char, Traits, Al>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		auto rfind(const std::basic_string_view<Char, Traits>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
	#endif

		auto rfind(const Char* s, size_t pos = npos) const -> size_t
		{
			return rfind(s, pos, Traits::length(s));
		}
		auto rfind(const Char* s, size_t pos, size_type n) const -> size_t
		{
			return detail::traits_string_rfind(data(), pos, size(), s, n, npos);
		}
		auto rfind(Char c, size_t pos = npos) const noexcept -> size_t
		{
			if (pos >= size()) 
				pos = size() - 1;
			const Char* p = detail::string_memrchr(data(), c, pos + 1); 
			return p == nullptr ? npos : static_cast<size_t>(p - data());
		}



		auto find_first_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_first_of<Traits>(data(), pos, size(), s, n, npos);
		}
		template<class A, size_t S>
		auto find_first_of(const tiny_string< Char,Traits,A,S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(str.data(), pos, str.size());
		}
		auto find_first_of(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(s, pos, Traits::length(s));
		}
		auto find_first_of(Char c, size_t pos = 0) const noexcept -> size_t
		{
			return find(c, pos);
		}

		template<class A, size_t S>
		auto find_last_of(const tiny_string<Char,Traits,A,S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_of(str.data(), pos, str.size());
		}
		auto find_last_of(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_of(s, pos, Traits::length(s));
		}
		auto find_last_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_last_of(data(), pos, size(), s, n, npos);
		}
		auto find_last_of(Char c, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(c, pos);
		}

		template<class A, size_t S>
		auto find_first_not_of(const tiny_string<Char, Traits, A, S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(str.data(), pos, str.size());
		}
		auto find_first_not_of(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(s, pos, Traits::length(s));
		}
		auto find_first_not_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_first_not_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_first_not_of(Char c, size_t pos = 0) const noexcept -> size_t
		{
			const Char* e = end();
			for (const Char* p = data() + pos; p != e; ++p) {
				if (*p != c) { return static_cast<size_t>(p - data());}
			}
			return npos;
		}

		template<class A, size_t S>
		auto find_last_not_of(const tiny_string<Char,Traits,A,S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(str.data(), pos, str.size());
		}
		auto find_last_not_of(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(s, pos, Traits::length(s));
		}
		auto find_last_not_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_last_not_of(data(), pos, size(), s, n, npos);
		}
		auto find_last_not_of(Char c, size_t pos = npos) const noexcept -> size_t
		{
			if (size() == 0) { return npos;}
			if (pos >= size()) { pos = size() - 1;}
			const Char* p = data();
			for (const Char* in = p + pos; in >= p; --in) {
				if (*in != c) {
					return static_cast<size_t>(in - p);
				}
			}
			return npos;
		}

		template<class A, size_t S>
		auto compare(const tiny_string<Char, Traits, A, S>& str) const noexcept -> int
		{
			return compare(0, size(), str.data(), str.size());
		}
		template<class Al>
		auto compare(size_t pos, size_t len, const std::basic_string<Char, Traits, Al>& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		int compare(size_t pos, size_t len, const std::basic_string_view<Char, Traits>& str) const noexcept
		{
			return compare(pos, len, str.data(), str.size());
		}
	#endif
		template<class A, size_t S>
		auto compare(size_t pos, size_t len, const tiny_string<Char, Traits, A, S>& str, size_t subpos, size_t sublen) const noexcept -> int
		{
			if (sublen == npos || subpos + sublen > str.size()) {
				sublen = str.size() - subpos;
	}
			return compare(pos, len, str.data() + subpos, sublen);
		}
		auto compare(const Char* s) const noexcept -> int
		{
			return compare(0, size(), s);
		}
		auto compare(size_t pos, size_t len, const Char* s) const noexcept -> int
		{
			return compare(pos, len, s, Traits::length(s));
		}
		auto compare(size_t pos, size_t len, const Char* _s, size_t n) const noexcept -> int
		{
			
			if (len == npos || pos + len > size())
				len = size() - pos;
			return detail::traits_string_compare<Traits>(data() + pos, len, _s, n);
		}

		/// @brief Implicit conversion to std::string
		template<class Al>
		operator std::basic_string<Char, Traits, Al>() const {
			return std::basic_string<Char, Traits, Al>(data(), size());
		}
	};











	/**********************************
	* Comparison operators
	* ********************************/

	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_ALWAYS_INLINE auto operator== (const tiny_string<Char,Traits,A1,S1>& lhs, const tiny_string<Char, Traits,A2,S2>& rhs) noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator== (const Char* lhs, const tiny_string<Char,Traits,Al,S>& rhs) noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator== (const tiny_string<Char,Traits,Al,S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator== (const std::basic_string<Char, Traits, Al>&lhs, const tiny_string<Char,Traits,Al2,S>& rhs)noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator== (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

	#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator== (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char,Traits, Al,S>& rhs)noexcept {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator== (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	#endif



	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_ALWAYS_INLINE auto operator!= (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator!= (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator!= (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator!= (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator!= (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return !(lhs == rhs);
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator!= (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator!= (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return !(lhs == rhs);
	}
#endif



	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_ALWAYS_INLINE auto operator< (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator< (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator< (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator< (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator< (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator< (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator< (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif



	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_ALWAYS_INLINE auto operator<= (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator<= (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator<= (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator<= (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator<= (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	bool operator<= (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	bool operator<= (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif



	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_ALWAYS_INLINE auto operator> (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator> (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator> (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator> (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator> (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator> (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator> (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif


	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_ALWAYS_INLINE auto operator>= (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator>= (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE auto operator>= (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator>= (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_ALWAYS_INLINE auto operator>= (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator>= (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_ALWAYS_INLINE bool operator>= (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif



	////////////////////////////////////////////////////////////////
	// Concatenation operators
	////////////////////////////////////////////////////////////////

	namespace detail
	{
		template<class T>
		struct GetStaticSize
		{
			static constexpr size_t value = 0;
		};
		template<class Char,class Traits,class Al, size_t S>
		struct GetStaticSize<tiny_string<Char,Traits,Al,S> >
		{
			static constexpr size_t value = S;
		};
		template<class S1, class S2>
		struct FindStaticSize
		{
			static constexpr size_t value = GetStaticSize<S1>::value > GetStaticSize<S2>::value ? GetStaticSize<S1>::value : GetStaticSize<S2>::value;
		};
		template<class Al1, class Al2>
		struct FindAllocator
		{
			using allocator_type = Al1;
			static auto select(const Al1& al1, const Al2& /*unused*/) -> Al1 { return al1; }
		};
		template<class Char>
		struct FindAllocator<view_allocator<Char>, view_allocator<Char> >
		{
			using allocator_type = std::allocator<Char>;
			static auto select(const view_allocator<Char>& /*unused*/, const view_allocator<Char>& /*unused*/) -> std::allocator<Char> {return {};}
		} ;
		template<class Al1, class Char>
		struct FindAllocator<Al1, view_allocator<Char> >
		{
			using allocator_type = Al1;
			static auto select(const Al1& al1, const view_allocator<Char>& /*unused*/) -> Al1 { return al1; }
		};
		template<class Char, class Al2>
		struct FindAllocator< view_allocator<Char>, Al2>
		{
			using allocator_type = Al2;
			static auto select(const view_allocator<Char>&  /*unused*/, const Al2& al2) -> Al2 { return al2; }
		};
		template<class Char, class Traits, class S1, class S2, class Al=void>
		struct FindReturnType
		{
			using type = tiny_string<Char,Traits, typename FindAllocator<Al, Al>::allocator_type, FindStaticSize<S1, S2>::value >;
		};
		template<class Char, class Traits, class S1, class S2>
		struct FindReturnType<Char,Traits,S1,S2,void>
		{
			using type = tiny_string<Char, Traits, typename FindAllocator<typename S1::allocator_type, typename S2::allocator_type>::allocator_type, FindStaticSize<S1, S2>::value >;
		};
	} //end namespace detail

	template<class Char, class Traits, class Al, size_t Size, class Al2, size_t Size2>
	SEQ_ALWAYS_INLINE auto operator+ (const tiny_string<Char,Traits,Al,Size>& lhs, const tiny_string<Char, Traits, Al2, Size2>& rhs) -> typename detail::FindReturnType<Char,Traits,tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al2, Size2> >::type
	{
		using find_alloc = detail::FindAllocator<Al, Al2>;
		using ret_type = typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al2, Size2> >::type;
		ret_type ret(lhs.size() + rhs.size(), static_cast<Char>(0),
			std::allocator_traits<typename ret_type::allocator_type>::select_on_container_copy_construction(find_alloc::select(lhs.get_allocator(), rhs.get_allocator())));
		memcpy(ret.data(), lhs.data(), lhs.size()*sizeof(Char));
		memcpy(ret.data() + lhs.size(), rhs.data(), rhs.size()*sizeof(Char));
		return ret;
	}

	template<class Char, class Traits, size_t Size, class Al, class Al2>
	SEQ_ALWAYS_INLINE auto operator+ (const tiny_string<Char,Traits, Al,Size>& lhs, const std::basic_string<Char, Traits, Al2>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return lhs + tstring_view(rhs.data(), rhs.size());
	}
	#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, size_t Size, class Al>
	SEQ_ALWAYS_INLINE auto operator+ (const tiny_string<Char, Traits, Al, Size>& lhs, const std::basic_string_view<Char, Traits>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return lhs + tstring_view(rhs.data(), rhs.size());
	}
	#endif

	template<class Char, class Traits, size_t Size, class Al>
	SEQ_ALWAYS_INLINE auto operator+ (const tiny_string<Char, Traits, Al, Size>& lhs, const Char* rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return lhs + tstring_view(rhs);
	}
	template<class Char, class Traits, size_t Size, class Al>
	SEQ_ALWAYS_INLINE auto operator+ (const tiny_string<Char, Traits, Al, Size>& lhs,  char rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return lhs + tstring_view(&rhs,1);
	}

	template<class Char, class Traits, size_t Size, class Al, class Al2>
	SEQ_ALWAYS_INLINE auto operator+ (const std::basic_string<Char, Traits, Al2>& lhs, const tiny_string<Char,Traits, Al,Size>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return tstring_view(lhs.data(), lhs.size()) + rhs;
	}
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, size_t Size, class Al>
	SEQ_ALWAYS_INLINE auto operator+ (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, Size>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size>  >::type
	{
		return tstring_view(lhs.data(), lhs.size()) + rhs;
	}
#endif


	template<class Char, class Traits, size_t Size, class Al>
	SEQ_ALWAYS_INLINE auto operator+ (const Char* lhs, const tiny_string<Char, Traits, Al, Size>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size>  >::type
	{
		return tstring_view(lhs) + rhs;
	}
	template<class Char, class Traits, size_t Size, class Al>
	SEQ_ALWAYS_INLINE auto operator+ (Char lhs, const tiny_string<Char, Traits, Al, Size>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size>  >::type
	{
		return tstring_view(&lhs,1) + rhs;
	}

	/// @brief Returns the string data (const char*) for given string object
	template<class Char, class Traits, class Al>
	inline auto string_data(const std::basic_string<Char, Traits, Al>& str) -> const Char* { return str.data(); }
	template<class Char, class Traits, class Al, size_t S>
	inline auto string_data(const tiny_string<Char,Traits,Al,S>& str) -> const Char* { return str.data(); }
	template<class Char>
	inline auto string_data(const Char* str) -> const Char* { return str; }
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	inline auto string_data(const std::basic_string_view<Char, Traits>& str) -> const Char* { return str.data(); }
#endif

	/// @brief Returns the string size for given string object
	template<class Char, class Traits, class Al>
	inline auto string_size(const std::basic_string<Char, Traits, Al>& str) -> size_t { return str.size(); }
	template<class Char, class Traits, class Al, size_t S>
	inline auto string_size(const tiny_string<Char,Traits,Al,S>& str) -> size_t { return str.size(); }
	template<class Char>
	inline auto string_size(const Char* str) -> size_t { return std::char_traits<Char>::length(str); }
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	inline auto string_size(const std::basic_string_view<Char, Traits>& str) -> size_t { return str.size(); }
#endif




	/// @brief Detect tiny_string
	template<class T>
	struct is_tiny_string : std::false_type {};
	template<class Char, class Traits, class Al, size_t S>
	struct is_tiny_string<tiny_string<Char,Traits,Al,S> > : std::true_type {};

	/// @brief Detect tiny_string, std::string, but not tstring_view
	template<class T>
	struct is_allocated_string : std::false_type {};
	template<class Char, class Traits, class Al, size_t S>
	struct is_allocated_string<tiny_string<Char, Traits, Al, S>> 
	{
		static constexpr bool value = !std::is_same<Al, view_allocator<Char> >::value;
	};
	template<class Char, class Traits, class Allocator >
	struct is_allocated_string<std::basic_string<Char, Traits, Allocator>> : std::true_type {} ;


	template< class C > 
	struct is_char : std::integral_constant<bool, std::is_same<C, char>::value || std::is_same<C, char16_t>::value || std::is_same<C, char32_t>::value || std::is_same<C, wchar_t>::value> {};
	
	/// @brief Detect all possible string types (std::string, tstring, tstring_view, std::string_view, const char*, char*
	template<class T>
	struct is_generic_string : std::false_type {};
	template<class T>
	struct is_generic_string<T*> : is_char<T> {} ;
	template<class T>
	struct is_generic_string<const T*> : is_char<T> {} ;
	template<class Char, class Traits ,class Allocator >
	struct is_generic_string<std::basic_string<Char,Traits,Allocator> > : std::true_type {} ;
	template<class Char, class Traits, class Al, size_t S>
	struct is_generic_string<tiny_string<Char,Traits,Al,S>> : std::true_type {};
#ifdef SEQ_HAS_CPP_17
	template<class Char,class Traits>
	struct is_generic_string<std::basic_string_view<Char, Traits> > : std::true_type {};
#endif

	/// @brief Detect all possible string types (std::string, tstring, tstring_view, std::string_view, const char*, char*
	template<class T>
	struct is_generic_char_string : std::false_type {};
	template<>
	struct is_generic_char_string<char*> : std::true_type {};
	template<>
	struct is_generic_char_string<const char*> : std::true_type {};
	template< class Traits, class Allocator >
	struct is_generic_char_string<std::basic_string<char, Traits, Allocator> > : std::true_type {};
	template< class Traits, class Al, size_t S>
	struct is_generic_char_string<tiny_string<char, Traits, Al, S>> : std::true_type {};
#ifdef SEQ_HAS_CPP_17
	template< class Traits>
	struct is_generic_char_string<std::basic_string_view<char, Traits> > : std::true_type {};
#endif

	/// @brief Detect tstring_view or std::string_view
	template<class T>
	struct is_string_view : std::false_type {};
	template<class Char, class Traits>
	struct is_string_view<tiny_string<Char,Traits, view_allocator<Char> ,0> > : std::true_type {} ;
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	struct is_string_view<std::basic_string_view<Char, Traits> > : std::true_type {};
#endif


	/// @brief Detect generic string view: tstring_view, std::string_view, char*, const char*
	template<class T>
	struct is_generic_string_view : std::false_type {};
	template<class Char, class Traits>
	struct is_generic_string_view<tiny_string<Char, Traits, view_allocator<Char>, 0> > : std::true_type {} ;
	template<class Char>
	struct is_generic_string_view<Char*> : is_char<Char> {} ;
	template<class Char>
	struct is_generic_string_view<const Char*> : is_char<Char> {} ;
	#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	struct is_generic_string_view<std::basic_string_view<Char, Traits> > : std::true_type {};
	#endif



	// Specialization of is_relocatable

	template<class Char>
	struct is_relocatable<view_allocator<Char> > : std::true_type {} ;

	template<class Char, class Traits, class Al, size_t S>
	struct is_relocatable<tiny_string<Char,Traits,Al,S> > : is_relocatable<Al> {};
	
	
	
	

	/**********************************
	* Reading/writing from/to streams
	* ********************************/

	template<class Elem,class Traits,size_t Size, class Alloc>
	inline auto operator>>(std::basic_istream<Elem, Traits>& iss, tiny_string<Elem,Traits, Alloc,Size>& str) 
		-> typename std::enable_if<!std::is_same<Alloc, view_allocator<Elem> >::value, std::basic_istream<Elem, Traits> >::type	&
	{	// extract a string
		typedef std::ctype<Elem> c_type;
		typedef std::basic_istream<Elem, Traits> stream_type;
		typedef tiny_string<Elem, Traits, Alloc, Size> string_type;
		typedef typename string_type::size_type size_type;

		std::ios_base::iostate state = std::ios_base::goodbit;
		bool changed = false;
		const typename stream_type::sentry ok(iss);

		if (ok)
		{	// state okay, extract characters
			const c_type& ctype_fac = std::use_facet< c_type >(iss.getloc());
			str.clear();

			try{
				size_type size = 0 < iss.width()
				&& static_cast<size_type>(iss.width()) < str.max_size()
				? static_cast<size_type>(iss.width()) : str.max_size();
			typename Traits::int_type _Meta = iss.rdbuf()->sgetc();

			for (; 0 < size; --size, _Meta = iss.rdbuf()->snextc())
				if (Traits::eq_int_type(Traits::eof(), _Meta))
				{	// end of file, quit
					state |= std::ios_base::eofbit;
					break;
				}
				else if (ctype_fac.is(c_type::space,
					Traits::to_char_type(_Meta)))
					break;	// whitespace, quit
				else
				{	// add character to string
					str.append(1, Traits::to_char_type(_Meta));
					changed = true;
				}

			}
			catch (...) {
				iss.setstate(std::ios_base::badbit);
			}
		}

		iss.width(0);
		if (!changed)
			state |= std::ios_base::failbit;
		iss.setstate(state);
		return (iss);
	}

	template<class Elem, class Traits, size_t Size, class Alloc>
	inline	auto operator<<(std::basic_ostream<Elem, Traits>& oss, const tiny_string<Elem,Traits, Alloc,Size>& str) -> std::basic_ostream<Elem, Traits>&
	{	// insert a string
		typedef std::basic_ostream<Elem, Traits> myos;
		typedef tiny_string<Elem, Traits, Alloc, Size> mystr;
		typedef typename mystr::size_type mysizt;

		std::ios_base::iostate state = std::ios_base::goodbit;
		mysizt size = str.size();
		mysizt pad = oss.width() <= 0 || static_cast<mysizt>(oss.width()) <= size
			? 0 : static_cast<mysizt>(oss.width()) - size;
		const typename myos::sentry sok(oss);

		if (!sok)
			state |= std::ios_base::badbit;
		else
		{	// state okay, insert characters
			try {
				if ((oss.flags() & std::ios_base::adjustfield) != std::ios_base::left)
					for (; 0 < pad; --pad)	// pad on left
						if (Traits::eq_int_type(Traits::eof(),
							oss.rdbuf()->sputc(oss.fill())))
						{	// insertion failed, quit
							state |= std::ios_base::badbit;
							break;
						}

				if (state == std::ios_base::goodbit
					&& oss.rdbuf()->sputn(str.c_str(), static_cast<std::streamsize>(size))
					!= static_cast<std::streamsize>(size))
					state |= std::ios_base::badbit;
				else
					for (; 0 < pad; --pad)	// pad on right
						if (Traits::eq_int_type(Traits::eof(),
							oss.rdbuf()->sputc(oss.fill())))
						{	// insertion failed, quit
							state |= std::ios_base::badbit;
							break;
						}
				oss.width(0);
			}
			catch (...)
			{
				oss.setstate(std::ios_base::badbit);
				throw;
			}
		}

		oss.setstate(state);
		return (oss);
	}


}//end namespace seq




namespace std
{
	// Overload std::swap for tiny_string. Illegal considering C++ standard, but works on all tested compilers and more efficient than the standard std::swap.
	template<class Char, class Traits, class Allocator, size_t Size>
	SEQ_ALWAYS_INLINE void swap(seq::tiny_string<Char, Traits, Allocator, Size>& a, seq::tiny_string<Char, Traits, Allocator, Size>& b)
	{
		a.swap(b);
	}

	template<class Char, class Traits, class Allocator, size_t Size>
	class hash< seq::tiny_string<Char, Traits, Allocator, Size> >
	{
	public:
		using is_transparent = std::true_type;
		auto operator()(const seq::tiny_string<Char, Traits, Allocator, Size>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast<const uint8_t*>(str.data()), str.size() * sizeof(Char));
		}
		template<size_t S, class Al>
		auto operator()(const seq::tiny_string<Char, Traits, Al, S>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast< const uint8_t*>(str.data()), str.size() * sizeof(Char));
		}
		template< class Al>
		auto operator()(const std::basic_string<Char, Traits, Al>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast< const uint8_t*>(str.data()), str.size() * sizeof(Char));
		}
		auto operator()(const Char * str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast<const uint8_t*>(str), Traits::length(str) * sizeof(Char));
		}

#ifdef SEQ_HAS_CPP_17
		auto operator()(const std::basic_string_view<Char, Traits>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast<const uint8_t*>(str.data()), str.size() * sizeof(Char));
		}
#endif
	};

} //end namespace std



/*#include "memory.hpp"

namespace seq
{
	namespace detail
	{
		using string_pool_type = parallel_object_pool<char, std::allocator<char>, 0, pow_object_allocation<1024, 32> >;

		static inline string_pool_type& get_string_pool()
		{
			static string_pool_type pool;
			return pool;
		}

		class string_allocator {
		public:
			
			using value_type = char;
			typedef char* pointer;
			typedef const char* const_pointer;
			typedef char& reference;
			typedef const char& const_reference;
			using size_type = size_t;
			using difference_type = ptrdiff_t;

			using propagate_on_container_move_assignment = std::true_type;
			using is_always_equal = std::true_type;

			template <class _Other>
			struct rebind {
				using other = string_allocator;
			};

			char* address(char& _Val) const noexcept {
				return std::addressof(_Val);
			}

			const char* address(const char& _Val) const noexcept {
				return std::addressof(_Val);
			}

			constexpr string_allocator() noexcept {}
			constexpr string_allocator(const string_allocator&) noexcept = default;
			~string_allocator() = default;
			string_allocator& operator=(const string_allocator&) = default;

			void deallocate(char* ptr, const size_t count) {
				get_string_pool().deallocate(ptr, count);
			}

			char* allocate( size_t count) {
				return get_string_pool().allocate(count);
			}

			char* allocate(size_t count, const void*) {
				return get_string_pool().allocate(count);
			}

			template <class _Objty, class... _Types>
			void construct(_Objty* const _Ptr, _Types&&... _Args) {
				::new (_Ptr) _Objty(std::forward<_Types>(_Args)...);
			}

			template <class _Uty>
			void destroy(_Uty* const _Ptr) {
				_Ptr->~_Uty();
			}

			size_t max_size() const noexcept {
				return static_cast<size_t>(-1) ;
			}
		};
	}

	using ftstring = tiny_string<0, detail::string_allocator>;
}
*/



#endif
