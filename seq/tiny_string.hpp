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


#ifdef SEQ_HAS_CPP_17
#include <string_view>
#endif

#include <vector>
#include <string>
#include <algorithm>


#include "hash.hpp"
#include "type_traits.hpp"
#include "utils.hpp"

#undef min
#undef max


namespace utils
{
	//Function missing on msvc and gcc/mingw
	inline auto my_memrchr(const void* s, int c, size_t n) -> void* {
		const auto* p = static_cast<const unsigned char*>(s);
		for (p += n; n > 0; n--) {
			if (*--p == c) {
				return const_cast<unsigned char*>(p);
			}
		}

		return nullptr;
	}

}
#if defined( WIN32 ) || defined(_WIN32)
#define memrchr utils::my_memrchr
#endif



namespace seq
{

	/// @brief Allocator type for tiny string view
	struct view_allocator {} ;

	// forward declaration
	template<size_t MaxStaticSize, class Allocator>
	class tiny_string;

	/// @brief Base string view typedef, similar to std::string_view. Equivalent to tiny_string<0, view_allocator>.
	using tstring_view = tiny_string<0, view_allocator>;
	/// @brief Base string typedef, similar to std::string. Equivalent to tiny_string<0, std::allocator<char>>.
	using tstring = tiny_string<0, std::allocator<char> >;


	


	namespace detail
	{
		/*-************************************
		*  Common functions
		**************************************/
		
		


		inline auto count_approximate_common_bytes(const char* pIn, const char* pMatch, const char* pInLimit) -> size_t
		{
			static constexpr size_t stepsize = sizeof(size_t);
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

	
		inline auto string_inf(const char* v1, const char* e1, const char* v2, const char* e2)noexcept -> bool
		{
			//comparison works on unsigned !!!!
		
			if (*v1 != *v2) { return static_cast<unsigned char>(*v1) < static_cast<unsigned char>(*v2);}

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
	
		inline auto string_compare(const unsigned char* v1, size_t l1, const unsigned char* v2, size_t l2)noexcept -> int
		{
			const unsigned char* end = v1 + std::min(l1,l2);
			while ((v1 < end - (8 - 1))) {
				std::uint64_t r1 = read_BE_64(v1);
				std::uint64_t r2 = read_BE_64(v2);
				if (r1 != r2) { return r1 < r2 ? -1 : 1;}
				v1 += 8;
				v2 += 8;
			}
			if ((v1 < (end - 3))) {
				std::uint32_t r1 = read_BE_32(v1);
				std::uint32_t r2 = read_BE_32(v2);
				if (r1 != r2) { return r1 < r2 ? -1 : 1;}
				v1 += 4; v2 += 4;
			}
			while (v1 != end) {
				if (*v1 != *v2) {
					return static_cast<unsigned char>(*v1) < static_cast<unsigned char>(*v2) ? -1 : 1;}
				++v1; ++v2;
			}
			return (l1 < l2) ? -1 : ((l1 == l2) ? 0 : 1);
		}
	
		inline auto string_inf_equal(const char* v1, const char* e1, const char* v2, const char* e2)noexcept -> bool
		{
			return !string_inf(v2, e2, v1, e1);
		}


		inline auto string_equal(const char* pIn, const char* pMatch, const char* pInLimit)  noexcept -> bool {
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
		inline auto string_equal(const char* s1, size_t l1, const char* s2, size_t l2) noexcept -> bool {
			return (l1 == l2) && string_equal(s1, s2, s1 +l1);
		}


		template<class T>
		static inline auto get_size(const T& obj) -> size_t { return obj.size(); }
		static inline auto get_size(const char* obj) -> size_t { return strlen(obj); }
		template<class T>
		static inline auto get_data(const T& obj) -> const char* { return obj.data(); }
		static inline auto get_data(const char* obj) -> const char* { return obj; }

		template<class String1, class String2, class Iter>
		void join(String1 & out, const String2 & sep, Iter first, Iter last) 
		{
			out.clear();
			if (first == last)
				return;

			size_t this_size = sep.size();
			out = *first++;
			for (; first != last; ++first) {
				size_t prev_size = out.size();
				size_t fsize = get_size(*first);
				size_t new_size = prev_size + this_size + fsize;
				out.reserve(new_size);
				out.append(get_data(sep), this_size);
				out.append(get_data(*first), fsize);
			}
		}


		/// @internal streambuf working on a const char*
		struct membuf : std::streambuf {
			membuf(char const* base, size_t size) {
				char* p(const_cast<char*>(base));
				this->setg(p, p, p + size);
			}
		};
		/// Input stream working on a memory buffer, only used for reading from a tiny_string
		struct ibufferstream : virtual detail::membuf, std::istream {
			ibufferstream(char const* base, size_t size)
				: membuf(base, size)
				, std::istream(static_cast<std::streambuf*>(this)) {
			}
		};


		template<size_t MaxSSO = 14>
		struct SSO
		{
			unsigned char not_sso : 1;
			unsigned char size : 7;
			char data[MaxSSO + 1];
		};
		struct NoneSSO
		{
			size_t not_sso : 1;
			size_t exact_size : 1;
			size_t size : sizeof(size_t) * 8 - 2;
			char* data;
		};
		struct NoneSSOProxy
		{
			size_t first;
			char* second;
		};

		template<size_t MaxSSO, class Allocator>
		struct string_proxy
			//: private Allocator
		{
			union {
				SSO<MaxSSO> sso;
				NoneSSO non_sso;
			} d_union;
		};


		template<size_t MaxSSO, class Allocator>
		struct string_internal : private Allocator
		{
			// internal storage for tiny_string and allocator
			static constexpr size_t struct_size = sizeof(string_proxy<MaxSSO, Allocator>);
			static constexpr size_t sso_size = struct_size - 2;
			union {
				SSO<sso_size> sso;
				NoneSSO non_sso;
			} d_union;

			string_internal():Allocator(){ }
			string_internal(const Allocator & al) :Allocator(al) { }
			string_internal( Allocator&& al) :Allocator(std::move(al)) { }
			auto allocate(size_t n) -> char* {
				return this->Allocator::allocate(n);
			}
			void deallocate(char* p, size_t n) {
				this->Allocator::deallocate(p, n);
			}
			auto get_allocator() -> Allocator &  { return *this; }
			auto get_allocator() const -> const Allocator& { return *this; }
			void set_allocator(const Allocator& al) { static_cast<Allocator&>(*this) = al; }
			void set_allocator( Allocator&& al) { static_cast<Allocator&>(*this) = std::move(al); }

			void swap(string_internal& other)
			{
				std::swap(d_union, other.d_union);
				swap_allocator(get_allocator(), other.get_allocator());
			}
		
		};


		template< size_t MaxSSO>
		struct string_internal<MaxSSO, view_allocator >
		{
			// internal storage for tiny_string and view_allocator (tstring_view objects)
			static constexpr size_t struct_size = sizeof(string_proxy<MaxSSO, std::allocator<char> >);
			static constexpr size_t sso_size = struct_size - 2;
			const char* data;
			size_t size;

			string_internal(const char * d = nullptr, size_t s = 0)
			:data(d), size(s){} 

			auto allocate(size_t n) -> char* {
				throw std::bad_alloc();
				return NULL;
			}
			void deallocate(char*  /*unused*/, size_t  /*unused*/) {}
			auto get_allocator() const -> view_allocator { return view_allocator(); }
			void set_allocator(const view_allocator& /*unused*/) {  }
			void set_allocator(view_allocator&& /*unused*/) {  }

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
	template<size_t MaxStaticSize = 0, class Allocator = std::allocator<char> >
	class tiny_string
	{
		using internal_data = detail::string_internal<MaxStaticSize,Allocator>;
		using this_type = tiny_string<MaxStaticSize,Allocator>;
		static constexpr size_t sso_max_capacity = internal_data::sso_size + 1;
		internal_data d_data;

		template<size_t Ss, class Al>
		friend class tiny_string;
	
		//is it a small string
		SEQ_ALWAYS_INLINE bool is_sso() const noexcept{ return d_data.d_union.sso.not_sso == 0; }
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
		auto size_internal() const noexcept -> size_t { return is_sso() ? d_data.d_union.sso.size : d_data.d_union.non_sso.size; }
		auto capacity_internal() const noexcept -> size_t {
			// returns the capacity
			return is_sso() ? 
				sso_max_capacity: 
				(d_data.d_union.non_sso.exact_size ? 
					d_data.d_union.non_sso.size+1 : 
					(d_data.d_union.non_sso.size < 32 ?
						32 :
						(1ULL << (1ULL + (bit_scan_reverse_64(d_data.d_union.non_sso.size))))));
		}
		auto capacity_for_length(size_t len) const noexcept -> size_t {
			//returns the capacity for given length
			return is_sso(len) ? 
				sso_max_capacity : 
				(len < 32 ? 
					32 : 
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
					char* ptr = d_data.d_union.non_sso.data;
					if (keep_old) 
						memcpy(d_data.d_union.sso.data, ptr, len);
					d_data.deallocate(ptr,capacity_internal());
				}
				/*else {
					//sso to sso: nothing to do
				}*/
				memset(d_data.d_union.sso.data + len, 0, sizeof(d_data.d_union.sso.data) - (len));
				d_data.d_union.sso.not_sso = 0;
				d_data.d_union.sso.size = len;
			}
			//non sso, might throw
			else {
				//from sso to non sso
				if (is_sso()) {
					char* ptr;
					if(exact_size)
						ptr = d_data.allocate(len+1);
					else {
						size_t capacity = capacity_for_length(len);
						ptr = d_data.allocate(capacity);
					}
					if (keep_old) 
						memcpy(ptr, d_data.d_union.sso.data, d_data.d_union.sso.size);
					ptr[len] = 0;
					d_data.d_union.non_sso.data = ptr;
					d_data.d_union.non_sso.exact_size = exact_size;
				
					//clear additionals
					size_t start = reinterpret_cast<char*>((&d_data.d_union.non_sso.data) + 1) - reinterpret_cast<char*>(this);
					memset(reinterpret_cast<char*>(this) + start, 0, sizeof(*this) - start);
				
				}
				//from non sso to non sso
				else {
					size_t current_capacity = capacity_internal();
					size_t new_capacity = capacity_for_length(len);
					if (current_capacity != new_capacity) {
						char* ptr;
						if(exact_size)
							ptr = d_data.allocate(len+1);
						else
							ptr = d_data.allocate(new_capacity);
						if (keep_old)
							memcpy(ptr, d_data.d_union.non_sso.data, std::min(len, size_internal()));
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
			resize_uninitialized(last - first,false,true);
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
				size_type to_insert = last - first;
				// Might throw, fine
				resize_front(size() + to_insert);
				iterator beg = begin();
				// Might throw, fine
				char* p = data();
				std::for_each(p+to_insert, p + to_insert + pos, [&](char v) {*beg = v; ++beg; });
				std::for_each(p + pos, p + pos + to_insert, [&](char & v) {v = *first++; });
			}
			else {
				// Might throw, fine
				size_type to_insert = last - first;
				resize(size() + to_insert);
				char* p = data();
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
			char* ptr = d_data.allocate(new_capacity);
			memcpy(ptr, d_data.d_union.non_sso.data, d_data.d_union.non_sso.size);
			d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal());
			d_data.d_union.non_sso.data = ptr;
			d_data.d_union.non_sso.exact_size = 0;
		}
		void push_back_sso(char c)
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

		void push_back_complex(char c)
		{
			// complex push_back, when we reach a power of 2 or the transition between SSO / non SSO
			if (SEQ_UNLIKELY(is_sso())) {
				push_back_sso(c);
			}
			else {
				extend_buffer_for_push_back();
				char* p = d_data.d_union.non_sso.data + d_data.d_union.non_sso.size++;
				p[0] = c;
				p[1] = 0;
			}
		}

		void pop_back_to_sso() 
		{
			// pop_back(), transition from non SSO to SSO
			char* ptr = d_data.d_union.non_sso.data;
			size_t cap = capacity_internal();
			d_data.d_union.sso.size = d_data.d_union.non_sso.size;
			d_data.d_union.sso.not_sso = 0;
			memcpy(d_data.d_union.sso.data, ptr, max_static_size + 1);
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
			char* p = data();
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
			char* op = other.data();
			//first part
			memcpy(op, data(), pos);
			//middle part
			std::copy(first, last, op + pos);
			//last part
			memcpy(op + pos + input_size, data() + pos + len, size() - (pos + len));

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

		auto initialize(size_t size) -> char*
		{
			// intialize the string for given size
			memset(&d_data.d_union, 0, sizeof(d_data.d_union));
			if (is_sso(size))
			{
				d_data.d_union.sso.not_sso = 0;
				d_data.d_union.sso.size = size;
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
			return high < 32 || !((low ^ high) > low);
		}



	public:
		static_assert(MaxStaticSize < 127, "tiny_string maximum static size is limited to 126 bytes");

		// Maximum string length to use SSO
		static constexpr size_t max_static_size = sso_max_capacity - 1;
		static constexpr size_t npos = -1;

		using traits_type = std::char_traits<char>;
		using value_type = char;
		using reference = char&;
		using pointer = char*;
		using const_reference = const char&;
		using const_pointer = const char*;
		using iterator = char*;
		using const_iterator = const char*;
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
		tiny_string(const char* data, const Allocator& al = Allocator())
			:d_data(al)
		{
			size_t len = strlen(data);
			memcpy(initialize(len), data, len);
		}
		/// @brief Construct from a string, its size and an allocator object
		tiny_string(const char* data, size_t n, const Allocator& al = Allocator())
			:d_data(al)
		{
			memcpy(initialize(n), data, n);
		}
		/// @brief Construct by filling with n copy of character c and an allocator object
		tiny_string(size_t n, char c, const Allocator& al = Allocator())
			:d_data(al)
		{
			memset(initialize(n), c, n);
		}
		/// @brief Construct from a std::string and an allocator object
		tiny_string(const std::string& str, const Allocator& al = Allocator())
			:d_data(al)
		{
			memcpy(initialize(str.size()), str.data(), str.size());
		}


	#ifdef SEQ_HAS_CPP_17
		tiny_string(const std::string_view& str, const Allocator& al = Allocator())
			:d_data(al)
		{
			memcpy(initialize(str.size()), str.data(), str.size());
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
				memcpy(d_data.d_union.non_sso.data, other.d_data.d_union.non_sso.data, d_data.d_union.non_sso.size + 1);
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
				memcpy(d_data.d_union.non_sso.data, other.d_data.d_union.non_sso.data, d_data.d_union.non_sso.size + 1);
			}
		}
		/// @brief Construct by copying the content of another tiny_string
		template<size_t OtherMaxSS, class OtherAlloc>
		tiny_string(const tiny_string<OtherMaxSS, OtherAlloc>& other)
			:d_data()
		{
			memcpy(initialize(other.size()), other.data(), other.size());
		}
		/// @brief Construct by copying a sub-part of other
		tiny_string(const tiny_string& other, size_t pos, size_t len, const Allocator& al = Allocator())
			:d_data(al)
		{
			if (len == npos || pos + len > other.size())
				len = other.size() - pos;
			memcpy(initialize(len), other.data() + pos, len);
		}
		/// @brief Construct by copying a sub-part of other
		tiny_string(const tiny_string& other, size_t pos, const Allocator& al = Allocator())
			:d_data(al)
		{
			size_t len = other.size() - pos;
			memcpy(initialize(len), other.data() + pos, len);
		}
		/// @brief Move constructor
		tiny_string(tiny_string&& other) noexcept
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
					memcpy(initialize(other.size()), other.data(), other.size());
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
		tiny_string(std::initializer_list<char> il, const Allocator& al = Allocator())
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
		SEQ_ALWAYS_INLINE auto data() noexcept -> char* { return is_sso() ? d_data.d_union.sso.data : d_data.d_union.non_sso.data; }
		/// @brief Returns the internal character storage 
		SEQ_ALWAYS_INLINE auto data() const noexcept -> const char* { return is_sso() ? d_data.d_union.sso.data : d_data.d_union.non_sso.data; }
		/// @brief Returns the internal character storage 
		SEQ_ALWAYS_INLINE auto c_str() const noexcept -> const char* { return data(); }
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
				memcpy(this->data(), other.c_str(), other.size());
			}
			return *this;
		}
		/// @brief Assign the content of other to this string
		template<size_t OtherMaxStaticSize, class OtherAlloc>
		auto assign(const tiny_string<OtherMaxStaticSize, OtherAlloc>& other) -> tiny_string&
		{
			resize_uninitialized(other.size(), false, true);
			memcpy(this->data(), other.c_str(), other.size());
			return *this;
		}
		/// @brief Assign the content of other to this string
		auto assign(const std::string& other) -> tiny_string& 
		{
			resize_uninitialized(other.size(), false, true);
			memcpy(this->data(), other.c_str(), other.size());
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Assign the content of other to this string
		auto assign(const std::string_view& other) -> tiny_string&
		{
			resize_uninitialized(other.size(), false, true);
			memcpy(this->data(), other.data(), other.size());
			return *this;
		}
	#endif

		/// @brief Assign a sub-part of other to this string
		auto assign(const tiny_string& str, size_t subpos, size_t sublen) -> tiny_string& 
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			resize_uninitialized(sublen, false,true);
			memcpy(this->data(), str.data() + subpos, sublen);
			return *this;
		}
		/// @brief Assign a null-terminated buffer to this string
		auto assign(const char* s) -> tiny_string&
		{
			size_t len = strlen(s);
			resize_uninitialized(len, false, true);
			memcpy(this->data(), s, len);
			return *this;
		}
		/// @brief Assign a buffer to this string
		auto assign(const char* s, size_t n) -> tiny_string&
		{
			resize_uninitialized(n, false, true);
			memcpy(this->data(), s, n);
			return *this;
		}
		/// @brief Reset the string by n copies of c
		auto assign(size_t n, char c) -> tiny_string& 
		{
			resize_uninitialized(n, false,true);
			memset(this->data(), c, n);
			return *this;
		}
		/// @brief Assign to this string the initializer list il
		auto assign(std::initializer_list<char> il) -> tiny_string& 
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
		void resize(size_t n, char c)
		{
			size_t old_size = size();
			if (old_size == n)
				return;
			resize_uninitialized(n, true);
			if(n > old_size)
				memset(data()+ old_size, c, n - old_size);
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
		void resize_front(size_t n, char c)
		{
			size_t old_size = size();
			if (old_size == n)
				return;

			if (!is_sso() && !is_sso(n) && capacity_internal() == capacity_for_length(n)) {
				//no alloc/dealloc required
				char* p = data();
				if (n > old_size) {
					memmove(p + n - old_size, p, old_size);
					memset(p, c, n - old_size);
				}
				else {
					memmove(p, p + old_size - n, n);
				}
				d_data.d_union.non_sso.size = n;
				d_data.d_union.non_sso.data[n] = 0;
				return;
			}

			tiny_string other(n,0);
			if (n > old_size) {
				memset(other.data(), c, n - old_size);
				memcpy(other.data() + n - old_size, data(), old_size);
			}
			else {
				memcpy(other.data() , data() + old_size -n, n);
			}
			swap(other);
		}

		/// @brief Swap the content of this string with other
		SEQ_ALWAYS_INLINE void swap(tiny_string& other) 
		{
			if (this != std::addressof(other)) 
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
		SEQ_ALWAYS_INLINE char at(size_t pos) const
		{
			if (pos >= size()) throw std::out_of_range("");
			return data()[pos];
		}
		/// @brief Returns the character at pos, throw std::out_of_range for invalid position.
		SEQ_ALWAYS_INLINE char &at(size_t pos)
		{
			if (pos >= size()) throw std::out_of_range("");
			return data()[pos];
		}

		/// @brief Returns the character at pos
		SEQ_ALWAYS_INLINE char operator[](size_t pos) const noexcept 
		{
			SEQ_ASSERT_DEBUG(pos < size(), "invalid position");
			return data()[pos];
		}
		/// @brief Returns the character at pos
		SEQ_ALWAYS_INLINE char &operator[](size_t pos) noexcept
		{
			SEQ_ASSERT_DEBUG(pos < size(), "invalid position");
			return data()[pos];
		}

		/// @brief Returns the last character of the string
		SEQ_ALWAYS_INLINE char back() const noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return is_sso() ? d_data.d_union.sso.data[d_data.d_union.sso.size-1] : d_data.d_union.non_sso.data[d_data.d_union.non_sso.size-1]; 
		}
		/// @brief Returns the last character of the string
		SEQ_ALWAYS_INLINE char &back() noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container"); 
			return is_sso() ? d_data.d_union.sso.data[d_data.d_union.sso.size - 1] : d_data.d_union.non_sso.data[d_data.d_union.non_sso.size - 1]; 
		}
		/// @brief Returns the first character of the string
		SEQ_ALWAYS_INLINE char front() const noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container"); 
			return is_sso() ? d_data.d_union.sso.data[0] : d_data.d_union.non_sso.data[0]; 
		}
		/// @brief Returns the first character of the string
		SEQ_ALWAYS_INLINE char &front() noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container"); 
			return is_sso() ? d_data.d_union.sso.data[0] : d_data.d_union.non_sso.data[0]; 
		}
	
		/// @brief Append character to the back of the string
		void push_back(char c)
		{
			if (SEQ_LIKELY(!is_sso() && !(d_data.d_union.non_sso.exact_size || isNextPow2(d_data.d_union.non_sso.size )))) {
				char* p = d_data.d_union.non_sso.data + d_data.d_union.non_sso.size++;
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
		template<size_t Ss, class Al>
		auto append(const tiny_string<Ss,Al>& str) -> tiny_string&
		{
			return append(str.data() ,str.size());
		}

		/// @brief Append the content of str to this string
		auto append(const std::string& str) -> tiny_string&
		{
			return append(str.data(), str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Append the content of str to this string
		auto append(const std::string_view& str) -> tiny_string&
		{
			return append(str.data(), str.size());
		}
	#endif

		/// @brief Append the sub-part of str to this string
		template<size_t Ss, class Al>
		auto append(const tiny_string<Ss,Al>& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}
		/// @brief Append the sub-part of str to this string
		auto append(const std::string& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Append the sub-part of str to this string
		auto append(const std::string_view& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}
	#endif


		/// @brief Append a null-terminated buffer to this string
		auto append(const char* s) -> tiny_string&
		{
			return append(s, strlen(s));
		}
		
		/// @brief Append buffer content to this string
		auto append(const char* s, size_t n) -> tiny_string&
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
			char* d = data();
			memcpy(d + old_size, s, n);	
			d[new_size] = 0;
			return *this;
		}
		/// @brief Append n copies of character c to the string
		auto append(size_t n, char c) -> tiny_string&
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
			char* d = data();
			memset(d + old_size, c, n);
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
		auto append(std::initializer_list<char> il)  -> tiny_string&
		{
			return append(il.begin(), il.end());
		}


	
		/// @brief Inserts the content of str at position pos
		template<size_t S2, class AL2>
		auto insert(size_t pos, const tiny_string<S2,AL2>& str) -> tiny_string&
		{
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}
		/// @brief Inserts a sub-part of str at position pos
		auto insert(size_t pos, const std::string& str) -> tiny_string&
		{
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Inserts a sub-part of str at position pos
		auto insert(size_t pos, const std::string_view& str) -> tiny_string&
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
		auto insert(size_t pos, const char* s)  -> tiny_string&
		{
			//avoid multiple push_back
			size_t len = strlen(s);
			insert(begin() + pos, s, s + len);
			return *this;
		}
		/// @brief Inserts buffer at position pos
		auto insert(size_t pos, const char* s, size_t n) -> tiny_string&
		{
			insert(begin() + pos, s, s+n);
			return *this;
		}
		/// @brief Inserts n copies of character c at position pos
		auto insert(size_t pos, size_t n, char c)  -> tiny_string&
		{
			insert(begin() + pos, cvalue_iterator<char>(0, c), cvalue_iterator<char>(n));
			return *this;
		}
		/// @brief Inserts n copies of character c at position p
		auto insert(const_iterator p, size_t n, char c) -> iterator
		{
			return insert(p, cvalue_iterator<char>(0, c), cvalue_iterator<char>(n));
		}
		/// @brief Inserts character c at position p
		auto insert(const_iterator p, char c) -> iterator
		{
			return insert(p, &c, (&c) + 1);
		}
		/// @brief Inserts the content of the range [first,last) at position p
		template <class InputIterator>
		auto insert(const_iterator p, InputIterator first, InputIterator last) -> iterator
		{
			size_t pos = p - cbegin();
			insert_cat(pos, first, last, typename std::iterator_traits<InputIterator>::iterator_category());
			return begin() + pos;
		}
		/// @brief Inserts the content of the range [il.begin(),il.end()) at position p
		auto insert(const_iterator p, std::initializer_list<char> il) -> iterator
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
			size_type f = p - begin();
			erase_internal(f, f+1);
			return begin() + f;
		}
		/// @brief Removes the range [first,last)
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			size_type f = first - begin();
			erase_internal(f, last - begin());
			return begin() + f;
		}


		/// @brief Replace up to len character starting from pos by str
		auto replace(size_t pos, size_t len, const std::string& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Replace up to len character starting from pos by str
		auto replace(size_t pos, size_t len, const std::string_view& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}
	#endif


		/// @brief Replace characters in the range [i1,i2) by str
		auto replace(const_iterator i1, const_iterator i2, const std::string& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Replace characters in the range [i1,i2) by str
		auto replace(const_iterator i1, const_iterator i2, const std::string_view& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}
	#endif


		/// @brief Replace up to len character starting from pos by str
		template<size_t S, class Al>
		auto replace(size_t pos, size_t len, const tiny_string<S,Al>& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by str
		template<size_t S, class Al>
		auto replace(const_iterator i1, const_iterator i2, const tiny_string<S, Al>& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}


		/// @brief Replace up to len character starting from pos by a sub-part of str
		template<size_t S, class Al>
		auto replace(size_t pos, size_t len, const tiny_string<S,Al>& str,
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
		auto replace(size_t pos, size_t len, const std::string& str,
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
		auto replace(size_t pos, size_t len, const std::string_view& str,
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
		auto replace(size_t pos, size_t len, const char* s) -> tiny_string&
		{
			size_t l = strlen(s);
			replace_internal(pos, len, s, s+l);
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by null-terminated string s
		auto replace(const_iterator i1, const_iterator i2, const char* s) -> tiny_string&
		{
			size_t l = strlen(s);
			replace_internal(i1 - cbegin(), i2-i1, s, s + l);
			return *this;
		}
		/// @brief Replace up to len character starting from pos by buffer s of size n
		auto replace(size_t pos, size_t len, const char* s, size_t n)  -> tiny_string&
		{
			replace_internal(pos,len, s, s + n);
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by buffer s of size n
		auto replace(const_iterator i1, const_iterator i2, const char* s, size_t n) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, s, s + n);
			return *this;
		}

		/// @brief Replace up to len character starting from pos by n copies of c
		auto replace(size_t pos, size_t len, size_t n, char c)  -> tiny_string&
		{
			replace_internal(pos, len, cvalue_iterator<char>(0,c), cvalue_iterator<char>(n));
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by n copies of c
		auto replace(const_iterator i1, const_iterator i2, size_t n, char c) -> tiny_string&
		{
			replace_internal(i1-cbegin(), i2-i1, cvalue_iterator<char>(0, c), cvalue_iterator<char>(n));
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
		auto replace(const_iterator i1, const_iterator i2, std::initializer_list<char> il)  -> tiny_string&
		{
			return replace(i1, i2, il.begin(), il.end());
		}

	
		/// @brief Replace any sub-string _from of size n1 by the string _to of size n2, starting to position start
		auto replace(const char* _from, size_t n1, const char* _to, size_t n2, size_t start = 0) -> size_t
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
		auto replace(const char* _from, const char* _to, size_t start = 0) -> size_t
		{
			return replace(_from, strlen(_from), _to, strlen(_to), start);
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		template<size_t S1, class Al1, size_t S2, class Al2>
		auto replace(const tiny_string<S1,Al1>& _from, const tiny_string<S2,Al2>& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(),start);
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		auto replace(const std::string& _from, const std::string& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(),start);
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		auto replace(const std::string_view& _from, const std::string_view& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(), start);
		}
	#endif

	
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const char* str, size_t n, size_t start = 0) const noexcept  -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(str,start,n); offset != npos; offset = find(str, offset + n,n))
				++count;
			return count;
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const char* str, size_t start = 0) const noexcept  -> size_t
		{
			return count(str, strlen(str),start);
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		template<size_t S, class Al>
		auto count(const tiny_string<S,Al>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(),start);
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const std::string& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(),start);
		}

	#ifdef SEQ_HAS_CPP_17
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const std::string_view& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
	#endif


		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(char c, size_t start = 0) const noexcept  -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(c,start); offset != npos; offset = find(c,offset + 1))
				++count;
			return count;
		}


		/// @brief Split the string based on the substring match of size n. 
		/// Computed strings are stored in output itreator out.
		/// If keep_empty_strings is false, empty strings are not added to the output iterator.
		///
		/// @return New past-the-end output iterator
		template<class Iter>
		auto split(const char* match, size_t n, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			size_t start_pos = 0;
			size_t previous_pos = 0;
			while ((start_pos = find(match, start_pos,n)) != std::string::npos) {
				if (previous_pos != start_pos || keep_empty_strings) {
					*out = (substr(previous_pos, start_pos - previous_pos));
					++out;
				}
				start_pos += n;
				previous_pos = start_pos;
			}

			if (previous_pos != size() || keep_empty_strings)
				*out++ = (substr(previous_pos));
			return out;
		}
		/// @brief Split the string based on the substring match. 
		/// Computed strings are stored in output itreator out.
		/// If keep_empty_strings is false, empty strings are not added to the output iterator.
		///
		/// @return New past-the-end output iterator
		template<class Iter>
		auto split(const char* match, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			return split(match, strlen(match), out, keep_empty_strings);
		}
		/// @brief Split the string based on the substring match. 
		/// Computed strings are stored in output itreator out.
		/// If keep_empty_strings is false, empty strings are not added to the output iterator.
		///
		/// @return New past-the-end output iterator
		template<size_t S, class Al,class Iter>
		auto split(const tiny_string<S,Al> & match, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			return split(match.data(), match.size(), out, keep_empty_strings);
		}
		/// @brief Split the string based on the substring match. 
		/// Computed strings are stored in output itreator out.
		/// If keep_empty_strings is false, empty strings are not added to the output iterator.
		///
		/// @return New past-the-end output iterator
		template<class Iter>
		auto split(const std::string& match, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			return split(match.data(), match.size(), out, keep_empty_strings);
		}

	#ifdef SEQ_HAS_CPP_17
		template<class Iter>
		auto split(const std::string_view& match, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			return split(match.data(), match.size(), out, keep_empty_strings);
		}
	#endif
	
		/// @brief Split the string based on the substring match. 
		/// If keep_empty_strings is false, empty strings are not added to the output.
		/// @return vector of tstring_view
		auto split(const char* match, size_t n, bool keep_empty_strings = true) const -> std::vector<tstring_view>;
		/// @brief Split the string based on the substring match. 
		/// If keep_empty_strings is false, empty strings are not added to the output.
		/// @return vector of tstring_view
		auto split(const char* match, bool keep_empty_strings = true) const -> std::vector<tstring_view>;
		/// @brief Split the string based on the substring match. 
		/// If keep_empty_strings is false, empty strings are not added to the output.
		/// @return vector of tstring_view
		template<size_t S, class Al>
		auto split(const tiny_string<S,Al>& match, bool keep_empty_strings = true) const -> std::vector<tstring_view>;
		/// @brief Split the string based on the substring match. 
		/// If keep_empty_strings is false, empty strings are not added to the output.
		/// @return vector of tstring_view
		auto split(const std::string& match, bool keep_empty_strings = true) const -> std::vector<tstring_view>;

	#ifdef SEQ_HAS_CPP_17
		auto split(const std::string_view& match, bool keep_empty_strings = true) const -> std::vector<tstring_view>;
	#endif

		/// @brief Use this string to join multiple string objects into out.
		/// out string contains the concatenation of the range [first,last) separated by this string content.
		template<class String, class Iter>
		void join(String& out, Iter first, Iter last) const
		{
			detail::join(out, *this, first, last);
		}
		/// @brief Use this string to join multiple string objects into out.
		/// out string contains the concatenation of the range [il.begin(),il.end()) separated by this string content.
		template<class String, class T>
		void join(String& out, std::initializer_list<T> il) const
		{
			detail::join(out, *this, il.begin(), il.end());
		}
		/// @brief Use this string to join multiple string objects into out.
		/// Returns the concatenation of the range [first,last) separated by this string content.
		template<class Iter>
		auto join(Iter first, Iter last) const -> tiny_string 
		{
			tiny_string res;
			detail::join(res, *this, first, last);
			return res;
		}
		/// @brief Use this string to join multiple string objects into out.
		/// Returns the concatenation of the range [il.begin(),il.end()) separated by this string content.
		template<class T>
		auto join(std::initializer_list<T> il) -> tiny_string
		{
			return join(il.begin(), il.end());
		}

		/// @brief std::sprintf equivalent writing to this string. 
		template <class... Args>
		auto sprintf(const char *format, Args&&... args) -> tiny_string & 
		{
			size_t cap = capacity();
			size_t n = std::snprintf(data(), cap + 1, format, std::forward<Args>(args)...);
			if (n <= cap) {
				if (is_sso()) d_data.d_union.sso.size = n;
				else d_data.d_union.non_sso.size = n;
			}
			else {
				tiny_string tmp(n,0);
				std::snprintf(tmp.data(), tmp.capacity() + 1, format, std::forward<Args>(args)...);
				swap(tmp);
			}
			return *this;
		}

		/// @brief Convert this string to given type using std::istream.
		/// If the convertion failed and ok is not NULL, ok is set to false, true otherwise.
		template<class T>
		auto convert(bool* ok = nullptr) const noexcept -> T 
		{
			detail::ibufferstream iss(data(), size());
			T tmp;
			iss >> tmp;
			if(ok) *ok = !iss.fail();
			return tmp;
		}

		/// @brief Copies a substring [pos, pos+len) to character string pointed to by s.
		/// Returns the number of copied characters.
		auto copy(char* s, size_t len, size_t pos = 0) const -> size_t
		{
			if (pos > size()) throw std::out_of_range("tiny_string::copy out of range");
			if (len == npos || pos + len > size())
				len = size() - pos;
			memcpy(s, data() + pos, len);
			return len;
		}

		// @brief Returns a sub-part of this string as a tstring_view object
		auto substr(size_t pos, size_t len = npos) const -> tstring_view;

		template<size_t S, class Al>
		auto find(const tiny_string<S,Al>& str, size_t pos = 0) const noexcept -> size_t 
		{
			return find(str.data(), pos, str.size());
		}
		auto find(const std::string& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		auto find(const std::string_view& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
	#endif

		auto find(const char* s, size_t pos = 0) const -> size_t
		{
			return find(s, pos, strlen(s));
		}
		auto find(const char* s, size_t pos, size_type n) const -> size_t
		{
			size_t this_size = size();
			if (n > this_size || pos + n > this_size || n==0) return npos;

			const char* in = data() + pos;
			const char* end = in + (size() - pos - n) +1;
			char c = *s;
			for(;;) {
				in = static_cast<const char*>(memchr(in, c, end - in));
				if (!in) return npos;
			
				//start searching, count_approximate_common_bytes returns (usually) an underestimation of the common bytes, except if equal
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if(common == n-1)
					return in - begin();
				//in += common +1;
				++in;
			}
			return npos;
		}
		auto find(char c, size_t pos = 0) const noexcept -> size_t 
		{
			const char* p = static_cast<const char*>(memchr(data() + pos, c, size() - pos));
			return p == NULL ? npos : p - begin();
		}

		template<size_t S, class Al>
		auto rfind(const tiny_string<S,Al>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
		auto rfind(const std::string& str, size_t pos = npos) const noexcept -> size_t 
		{
			return rfind(str.data(), pos, str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		size_t rfind(const std::string_view& str, size_t pos = npos) const noexcept
		{
			return rfind(str.data(), pos, str.size());
		}
	#endif

		auto rfind(const char* s, size_t pos = npos) const -> size_t
		{
			return rfind(s, pos, strlen(s));
		}
		auto rfind(const char* s, size_t pos, size_type n) const -> size_t 
		{
			size_t this_size = size();
			if (n > this_size || pos < n || n==0) return npos;
			if (pos > this_size) pos = this_size;
			const char* beg = data();
			const char* in = std::min(beg + pos, end()-n) ;
			char c = *s;
			for (;;) {
				in = static_cast<const char*>(memrchr(beg, c, in - beg +1));
				if (!in) return npos;
				//start searching
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == n - 1)
					return in - begin();
				--in;
			}
			return npos;
		}
		auto rfind(char c, size_t pos = npos) const noexcept -> size_t 
		{
			if (pos >= size()) pos = size()-1;
			const char* p = static_cast<const char*>(memrchr(data(), c, pos+1));
			return p == NULL ? npos : p - data();
		}


		auto find_first_of(const char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			const char* end = this->end();
			if (size() < 512) {
				const char* send = s + n;
				for (const char* p = data() + pos; p != end; ++p)
					for (const char* m = s; m != send; ++m)
						if (*m == *p) return p - data();
			}
			else {
				char buff[256];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) buff[static_cast<unsigned char>(s[i])] = 1;
				for (const char* p = data() + pos; p != end; ++p)
					if (buff[static_cast<unsigned char>(*p)])
						return p - data();
			}
			return npos;
		}
		template<size_t S, class Al>
		auto find_first_of(const tiny_string<S,Al>& str, size_t pos = 0) const noexcept -> size_t 
		{
			return find_first_of(str.data(), pos, str.size());
		}
		auto find_first_of(const char* s, size_t pos = 0) const noexcept -> size_t 
		{
			return find_first_of(s, pos, strlen(s));
		}
		auto find_first_of(char c, size_t pos = 0) const noexcept -> size_t
		{
			return find(c, pos);
		}

		template<size_t S, class Al>
		auto find_last_of(const tiny_string<S,Al>& str, size_t pos = npos) const noexcept -> size_t 
		{
			return find_last_of(str.data(), pos, str.size());
		}
		auto find_last_of(const char* s, size_t pos = npos) const noexcept -> size_t 
		{
			return find_last_of(s, pos, strlen(s));
		}
		auto find_last_of(const char* s, size_t pos, size_t n = npos) const noexcept -> size_t 
		{
			if (size() == 0) return npos;
			if (pos >= size()) pos = size() - 1;
			const char* p = data();
			if (size() < 512) {
				const char* send = s + n;
				for (const char* in = p + pos; in >= p; --in) {
					for (const char* m = s; m != send; ++m)
						if (*m == *in) return in - p;
				}
			}
			else {
				char buff[256];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) buff[static_cast<unsigned char>(s[i])] = 1;
				for (const char* in = p + pos; in >= p; --in) {
					if (buff[static_cast<unsigned char>(*in)])
						return in - p;
				}
			}
			return npos;
		}
		auto find_last_of(char c, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(c, pos);
		}

		template<size_t S, class Al>
		auto find_first_not_of(const tiny_string<S,Al>& str, size_t pos = 0) const noexcept -> size_t 
		{
			return find_first_not_of(str.data(), pos, str.size());
		}
		auto find_first_not_of(const char* s, size_t pos = 0) const noexcept -> size_t 
		{
			return find_first_not_of(s, pos, strlen(s));
		}
		auto find_first_not_of(const char* s, size_t pos, size_t n) const noexcept -> size_t 
		{
			const char* end = this->end();
			const char* send = s + n;
			for (const char* p = data() + pos; p != end; ++p) {
				const char* m = s;
				for (; m != send; ++m)
					if (*m == *p) break;
				if (m == send)
					return p - data();
			}
			return npos;
		}
		auto find_first_not_of(char c, size_t pos = 0) const noexcept -> size_t 
		{
			const char* e = end();
			for (const char* p = data() + pos; p != e; ++p)
				if (*p != c) return p - data();
			return npos;
		}

		template<size_t S, class Al>
		auto find_last_not_of(const tiny_string<S,Al>& str, size_t pos = npos) const noexcept -> size_t 
		{
			return find_last_not_of(str.data(), pos, str.size());
		}
		auto find_last_not_of(const char* s, size_t pos = npos) const noexcept -> size_t 
		{
			return find_last_not_of(s, pos, strlen(s));
		}
		auto find_last_not_of(const char* s, size_t pos, size_t n = npos) const noexcept -> size_t 
		{
			if (size() == 0) return npos;
			if (pos >= size()) pos = size() - 1;
			const char* p = data();
			const char* send = s + n;
			for (const char* in = p + pos; in >= p; --in) {
				const char* m = s;
				for (; m != send; ++m)
					if (*m == *in) break;
				if (m == send)
					return in - p;
			}
			return npos;
		}
		auto find_last_not_of(char c, size_t pos = npos) const noexcept -> size_t 
		{
			if (size() == 0) return npos;
			if (pos >= size()) pos = size() - 1;
			const char* p = data();
			for (const char* in = p + pos; in >= p; --in)
				if (*in != c)
					return in - p;
			return npos;
		}

		template<size_t S, class Al>
		auto compare(const tiny_string<S,Al>& str) const noexcept -> int 
		{
			return compare(0, size(), str.data(), str.size());
		}
		auto compare(size_t pos, size_t len, const tiny_string& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}
		auto compare(size_t pos, size_t len, const std::string& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		auto compare(size_t pos, size_t len, const std::string_view& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}
	#endif

		auto compare(size_t pos, size_t len, const tiny_string& str, size_t subpos, size_t sublen) const noexcept -> int
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return compare(pos, len, str.data() + subpos, sublen);
		}
		auto compare(const char* s) const noexcept -> int
		{
			return compare(0, size(), s);
		}
		auto compare(size_t pos, size_t len, const char* s) const noexcept -> int
		{
			return compare(pos, len, s, strlen(s));
		}
		auto compare(size_t pos, size_t len, const char* _s, size_t n) const noexcept -> int
		{
			if (len == npos || pos + len > size())
				len = size() - pos;

			//comparison works on unsigned !!!!
			const unsigned char* p = reinterpret_cast<const unsigned char*>(data()) + pos;
			const unsigned char* s = reinterpret_cast<const unsigned char*>(_s);
			/*if (*p < *s) return -1;
			if (*p > *s) return 1;
			return detail::string_compare(p, len, s, n);*/
			if (*p < *s) return -1;
			if (*p > *s) return 1;
			int r = memcmp(p , s , std::min(len, n) );
			if (r == 0) return len < n ? -1 : (len > n ? 1 : 0);
			return r;
		}


		auto operator=(const tiny_string& other) -> tiny_string& 
		{
			if (this != std::addressof(other)) 
				assign(other);
			return *this;
		} 
		template<size_t OtherMSS,class OtherAlloc>
		auto operator=(const tiny_string<OtherMSS, OtherAlloc>& other) -> tiny_string&
		{
			return assign(other);
		}
		SEQ_ALWAYS_INLINE auto operator=(tiny_string && other) noexcept -> tiny_string&
		{
			if (this != std::addressof(other)) 
				d_data.swap( other.d_data);
			return *this;
		}
		auto operator=(const char * other) -> tiny_string&
		{
			return assign(other);
		}
		auto operator=( char c) -> tiny_string&
		{
			if (!is_sso())
				d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal());
			memset(&d_data.d_union, 0, sizeof(d_data.d_union));
			d_data.d_union.sso.not_sso = 0;
			d_data.d_union.sso.size = 1;
			d_data.d_union.sso.data[0] = c;
			return *this;
		}
		auto operator=(const std::string & other) -> tiny_string&
		{
			return assign(other);
		}

	#ifdef SEQ_HAS_CPP_17
		auto operator=(const std::string_view& other) -> tiny_string&
		{
			return assign(other);
		}
	#endif

		auto operator=( std::initializer_list<char> il) -> tiny_string&
		{
			return assign(il);
		}

		template<size_t Ss, class Al>
		auto operator+= (const tiny_string<Ss,Al>& str) -> tiny_string& { return append(str); }
		auto operator+= (const std::string& str) -> tiny_string& { return append(str); }
		auto operator+= (const char* s) -> tiny_string& { return append(s); }
		auto operator+= (char c) -> tiny_string& { return append(c); }
		auto operator+= (std::initializer_list<char> il) -> tiny_string& { return append(il); }

	#ifdef SEQ_HAS_CPP_17
		auto operator+= (const std::string_view& str) -> tiny_string& { return append(str); }
	#endif

		/// @brief Implicit conversion to std::string
		operator std::string() const {
			return std::string(data(), size());
		}
	};





	/// @brief Specialization of tiny_string for string views.
	/// You should use the global typedef tstring_view equivalent to tiny_string<0,view_allocator>.
	/// Provides a similar interface to std::string_view.
	/// See tiny_string documentation for more details.
	template<>
	struct tiny_string<0,view_allocator>
	{
		using internal_data = detail::string_internal<0, view_allocator>;
		using this_type = tiny_string<0, view_allocator>;
		static constexpr size_t sso_max_capacity = internal_data::sso_size + 1;//sizeof(internal_data) - 1;
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
		static constexpr size_t npos = -1;

		using traits_type = std::char_traits<char>;
		using value_type = char;
		using reference = char&;
		using pointer = char*;
		using const_reference = const char&;
		using const_pointer = const char*;
		using iterator = const char*;
		using const_iterator = const char*;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type = size_t;
		using difference_type = std::intptr_t;
		using allocator_type = view_allocator;
		

		auto data() const noexcept -> const char* { return d_data.data; }
		auto c_str() const noexcept -> const char* { return data(); }
		auto size() const noexcept -> size_t { return size_internal(); }
		auto length() const noexcept -> size_t { return size(); }
		static auto max_size() noexcept -> size_t { return std::numeric_limits<size_t>::max(); }
		auto empty() const noexcept -> bool { return size() == 0; }
		auto get_allocator() const noexcept -> allocator_type { return d_data.get_allocator(); }

		tiny_string()
			:d_data(NULL,0) {}
	
		tiny_string(const char* data)
			:d_data(data, strlen(data)) {}
	
		tiny_string(const char* data, size_t n)
			:d_data(data, n) {}
	
		tiny_string(const std::string& str)
			:d_data(str.data(), str.size()) {}

	#ifdef SEQ_HAS_CPP_17
		tiny_string(const std::string_view& str)
			:d_data(str.data(), str.size()) {}
	#endif
	
		template<size_t OtherMaxSS, class OtherAlloc>
		tiny_string(const tiny_string<OtherMaxSS, OtherAlloc>& other)
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
		template<size_t OtherMaxSS, class OtherAlloc>
		auto operator=(const tiny_string<OtherMaxSS, OtherAlloc>& other) -> tiny_string&
		{
			d_data = internal_data(other.data(),other.size());
			return *this;
		}
		auto operator=(const std::string& other) -> tiny_string&
		{
			d_data = internal_data(other.data(), other.size());
			return *this;
		}
		auto operator=(const char* other) -> tiny_string&
		{
			d_data = internal_data(other, strlen(other));
			return *this;
		}
#ifdef SEQ_HAS_CPP_17
		auto operator=(const std::string_view& other) -> tiny_string&
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

		auto at(size_t pos) const -> char
		{
			if (pos >= size()) { throw std::out_of_range("");}
			return data()[pos];
		}
		auto operator[](size_t pos) const noexcept -> char
		{
			return data()[pos];
		}

		auto back() const noexcept -> char { return data()[size() - 1]; }
		auto front() const noexcept -> char { return data()[0]; }

		/**
		* Convenient function, returns the count of non-overlapping occurrences of 'str'
		*/
		auto count(const char* str, size_t n, size_t start = 0) const noexcept -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(str, start, n); offset != npos; offset = find(str, offset + n, n))
				++count;
			return count;
		}
		auto count(const char* str, size_t start = 0) const noexcept -> size_t
		{
			return count(str, strlen(str), start);
		}
		auto count(const tiny_string& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
		auto count(const std::string& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}

	#ifdef SEQ_HAS_CPP_17
		auto count(const std::string_view& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
	#endif

		auto count(char c, size_t start = 0) const noexcept -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(c, start); offset != npos; offset = find(c, offset + 1))
				++count;
			return count;
		}

		template<class Iter>
		auto split(const char* match, size_t n, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			size_t start_pos = 0;
			size_t previous_pos = 0;
			while ((start_pos = find(match, start_pos, n)) != std::string::npos) {
				if (previous_pos != start_pos || keep_empty_strings) {
					*out = (substr(previous_pos, start_pos - previous_pos));
					++out;
				}
				start_pos += n;
				previous_pos = start_pos;
			}

			if (previous_pos != size() || keep_empty_strings)
				*out++ = (substr(previous_pos));
			return out;
		}
		template<class Iter>
		auto split(const char* match, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			return split(match, strlen(match), out, keep_empty_strings);
		}
		template<class Iter>
		auto split(const tiny_string& match, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			return split(match.data(), match.size(), out, keep_empty_strings);
		}
		template<class Iter>
		auto split(const std::string& match, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			return split(match.data(), match.size(), out, keep_empty_strings);
		}

	#ifdef SEQ_HAS_CPP_17
		template<class Iter>
		auto split(const std::string_view& match, Iter out, bool keep_empty_strings = true) const -> Iter
		{
			return split(match.data(), match.size(), out, keep_empty_strings);
		}
	#endif

		auto split(const char* match, size_t n, bool keep_empty_strings = true) const -> std::vector<tstring_view>
		{
			std::vector<tstring_view> res ;
			split(match, n, std::back_inserter< std::vector<tstring_view> >(res), keep_empty_strings);
			return res;
		}
		auto split(const char* match, bool keep_empty_strings = true) const -> std::vector<tstring_view>
		{
			return split(match, strlen(match), keep_empty_strings);
		}
		auto split(const tiny_string& match, bool keep_empty_strings = true) const -> std::vector<tstring_view>
		{
			return split(match.data(), match.size(), keep_empty_strings);
		}
		auto split(const std::string& match, bool keep_empty_strings = true) const -> std::vector<tstring_view>
		{
			return split(match.data(), match.size(), keep_empty_strings);
		}

	#ifdef SEQ_HAS_CPP_17
		auto split(const std::string_view& match, bool keep_empty_strings = true) const -> std::vector<tstring_view>
		{
			return split(match.data(), match.size(), keep_empty_strings);
		}
	#endif


		template<class String, class Iter>
		void join(String& out, Iter first, Iter last) const
		{
			detail::join(out, *this, first, last);
		}
		template<class String, class T>
		void join(String& out, std::initializer_list<T> il) const
		{
			detail::join(out, *this, il.begin(), il.end());
		}
		template<class Iter>
		auto join(Iter first, Iter last) const -> tstring
		{
			tstring res;
			detail::join(res, *this, first, last);
			return res;
		}
		template<class T>
		auto join(std::initializer_list<T> il) -> tstring
		{
			return join(il.begin(), il.end());
		}



		auto copy(char* s, size_t len, size_t pos = 0) const -> size_t
		{
			if (pos > size()) { throw std::out_of_range("tiny_string::copy out of range");}
			if (len == npos || pos + len > size()) {
				len = size() - pos;
			}		
			memcpy(s, data() + pos, len);
			return len;
		}

		auto substr(size_t pos = 0, size_t len = npos) const -> tstring_view
		{
			if (pos > size()) { throw std::out_of_range("tiny_string::substr out of range");}
			if (len == npos || pos + len > size()) {
				len = size() - pos;
			}
			return {begin() + pos, len};
		}

		auto find(const tiny_string& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
		auto find(const std::string& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		auto find(const std::string_view& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
	#endif

		auto find(const char* s, size_t pos = 0) const -> size_t
		{
			return find(s, pos, strlen(s));
		}
		auto find(const char* s, size_t pos, size_type n) const -> size_t
		{
			size_t this_size = size();
			if (n > this_size || pos + n > this_size || n == 0) { return npos;}

			const char* in = data() + pos;
			const char* end = in + (size() - pos - n) + 1;
			char c = *s;
			for (;;) {
				in = static_cast<const char*>(memchr(in, c, end - in));
				if (in == nullptr) { return npos;}

				//start searching
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == n - 1) {
					return in - begin();
				}
				//in += common + 1;
				++in;
			}
			return npos;
		}
		auto find(char c, size_t pos = 0) const noexcept -> size_t
		{
			const char* p = static_cast<const char*>(memchr(data() + pos, c, size() - pos));
			return p == nullptr ? npos : p - begin();
		}


		auto rfind(const tiny_string& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
		auto rfind(const std::string& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		auto rfind(const std::string_view& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
	#endif

		auto rfind(const char* s, size_t pos = npos) const -> size_t
		{
			return rfind(s, pos, strlen(s));
		}
		auto rfind(const char* s, size_t pos, size_type n) const -> size_t
		{
			size_t this_size = size();
			if (n > this_size || pos < n || n == 0) { return npos;
			}
			if (pos > this_size) { pos = this_size;
			}
			const char* beg = data();
			const char* in = std::min(beg + pos, end() - n);
			char c = *s;
			for (;;) {
				in = static_cast<const char*>(memrchr(beg, c, in - beg + 1));
				if (in == nullptr) { return npos;
				}
				//start searching
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == n - 1) {
					return in - begin();
				}
				--in;
			}
			return npos;
		}
		auto rfind(char c, size_t pos = npos) const noexcept -> size_t
		{
			if (pos >= size()) { pos = size() - 1;
			}
			const char* p = static_cast<const char*>(memrchr(data(), c, pos + 1));
			return p == nullptr ? npos : p - data();
		}



		auto find_first_of(const char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			const char* end = this->end();
			if (size() < 512) {
				const char* send = s + n;
				for (const char* p = data() + pos; p != end; ++p) {
					for (const char* m = s; m != send; ++m) {
						if (*m == *p) { return p - data();}
					}
				}
			}
			else {
				char buff[256];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) { buff[static_cast<unsigned char>(s[i])] = 1;}
				for (const char* p = data() + pos; p != end; ++p) {
					if (buff[static_cast<unsigned char>(*p)] != 0) {
						return p - data();
						}
					}
			}
			return npos;
		}
		auto find_first_of(const tiny_string& str, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(str.data(), pos, str.size());
		}
		auto find_first_of(const char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(s, pos, strlen(s));
		}
		auto find_first_of(char c, size_t pos = 0) const noexcept -> size_t
		{
			return find(c, pos);
		}


		auto find_last_of(const tiny_string& str, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_of(str.data(), pos, str.size());
		}
		auto find_last_of(const char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_of(s, pos, strlen(s));
		}
		auto find_last_of(const char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			if (size() == 0) { return npos;
	}
			if (pos >= size()) { pos = size() - 1;
	}
			const char* p = data();
			if (size() < 512) {
				const char* send = s + n;
				for (const char* in = p + pos; in >= p; --in) {
					for (const char* m = s; m != send; ++m) {
						if (*m == *in) { return in - p;
	}
	}
				}
			}
			else {
				char buff[256];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) { buff[static_cast<unsigned char>(s[i])] = 1;
	}
				for (const char* in = p + pos; in >= p; --in) {
					if (buff[static_cast<unsigned char>(*in)] != 0) {
						return in - p;
	}
				}
			}
			return npos;
		}
		auto find_last_of(char c, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(c, pos);
		}


		auto find_first_not_of(const tiny_string& str, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(str.data(), pos, str.size());
		}
		auto find_first_not_of(const char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(s, pos, strlen(s));
		}
		auto find_first_not_of(const char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			const char* end = this->end();
			const char* send = s + n;
			for (const char* p = data() + pos; p != end; ++p) {
				const char* m = s;
				for (; m != send; ++m) {
					if (*m == *p) { break;}
				}
				if (m == send) {
					return p - data();
				}
			}
			return npos;
		}
		auto find_first_not_of(char c, size_t pos = 0) const noexcept -> size_t
		{
			const char* e = end();
			for (const char* p = data() + pos; p != e; ++p) {
				if (*p != c) { return p - data();}
			}
			return npos;
		}


		auto find_last_not_of(const tiny_string& str, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(str.data(), pos, str.size());
		}
		auto find_last_not_of(const char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(s, pos, strlen(s));
		}
		auto find_last_not_of(const char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			if (size() == 0) { return npos;}
			if (pos >= size()) { pos = size() - 1;}
			const char* p = data();
			const char* send = s + n;
			for (const char* in = p + pos; in >= p; --in) {
				const char* m = s;
				for (; m != send; ++m) {
					if (*m == *in) { break;}
				}
				if (m == send) {
					return in - p;
				}
			}
			return npos;
		}
		auto find_last_not_of(char c, size_t pos = npos) const noexcept -> size_t
		{
			if (size() == 0) { return npos;}
			if (pos >= size()) { pos = size() - 1;}
			const char* p = data();
			for (const char* in = p + pos; in >= p; --in) {
				if (*in != c) {
					return in - p;
				}
			}
			return npos;
		}


		auto compare(const tiny_string& str) const noexcept -> int
		{
			return compare(0, size(), str.data(), str.size());
		}
		auto compare(size_t pos, size_t len, const tiny_string& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}
		auto compare(size_t pos, size_t len, const std::string& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}

	#ifdef SEQ_HAS_CPP_17
		int compare(size_t pos, size_t len, const std::string_view& str) const noexcept
		{
			return compare(pos, len, str.data(), str.size());
		}
	#endif

		auto compare(size_t pos, size_t len, const tiny_string& str, size_t subpos, size_t sublen) const noexcept -> int
		{
			if (sublen == npos || subpos + sublen > str.size()) {
				sublen = str.size() - subpos;
	}
			return compare(pos, len, str.data() + subpos, sublen);
		}
		auto compare(const char* s) const noexcept -> int
		{
			return compare(0, size(), s);
		}
		auto compare(size_t pos, size_t len, const char* s) const noexcept -> int
		{
			return compare(pos, len, s, strlen(s));
		}
		auto compare(size_t pos, size_t len, const char* _s, size_t n) const noexcept -> int
		{
			if (len == npos || pos + len > size()) {
				len = size() - pos;
			}

			//comparison works on unsigned !!!!
			const unsigned char* p = reinterpret_cast<const unsigned char*>(data()) + pos;
			const auto* s = reinterpret_cast<const unsigned char*>(_s);
		
			if (*p < *s) { return -1;}
			if (*p > *s) { return 1;}
			int r = memcmp(p, s, std::min(len, n));
			if (r == 0) { return len < n ? -1 : (len > n ? 1 : 0);}
			return r;
		}

		/// @brief Implicit conversion to std::string
		operator std::string() const {
			return std::string(data(), size());
		}
	};







	template<size_t Ss, class Al>
	auto tiny_string<Ss,Al>::split(const char* match, size_t n, bool keep_empty_strings ) const -> std::vector<tstring_view>
	{
		std::vector<tstring_view> res;
		split(match, n, std::back_inserter< std::vector<tstring_view> >(res), keep_empty_strings);
		return res;
	}

	template<size_t Ss, class Al>
	auto tiny_string<Ss, Al>::split(const char* match, bool keep_empty_strings ) const -> std::vector<tstring_view>
	{
		return split(match, strlen(match), keep_empty_strings);
	}

	template<size_t Ss, class Al>
	template<size_t S2, class Al2>
	auto tiny_string<Ss, Al>::split(const tiny_string<S2, Al2>& match, bool keep_empty_strings ) const -> std::vector<tstring_view>
	{
		return split(match.data(), match.size(), keep_empty_strings);
	}

	template<size_t Ss, class Al>
	auto tiny_string<Ss, Al>::split(const std::string& match, bool keep_empty_strings ) const -> std::vector<tstring_view>
	{
		return split(match.data(), match.size(), keep_empty_strings);
	}

	#ifdef SEQ_HAS_CPP_17
	template<size_t Ss, class Al>
	auto tiny_string<Ss, Al>::split(const std::string_view& match, bool keep_empty_strings) const -> std::vector<tstring_view>
	{
		return split(match.data(), match.size(), keep_empty_strings);
	}
	#endif

	template<size_t Ss, class Al>
	auto tiny_string<Ss, Al>::substr(size_t pos , size_t len ) const -> tstring_view
	{
		if (pos > size()) throw std::out_of_range("tiny_string::substr out of range");
		if (len == npos || pos + len > size())
			len = size() - pos;
		return tstring_view(begin() + pos, len);
	}









	/**********************************
	* Comparison operators
	* ********************************/

	template<size_t Size, class Al, size_t Size2, class Al2>
	auto operator== (const tiny_string<Size,Al>& lhs, const tiny_string<Size2, Al2>& rhs) noexcept -> bool {
		return detail::string_equal(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<size_t Size, class Al>
	auto operator== (const char* lhs, const tiny_string<Size,Al>& rhs) noexcept -> bool {
		return detail::string_equal(lhs, strlen(lhs), rhs.data(), rhs.size());
	}
	template<size_t Size, class Al>
	auto operator== (const tiny_string<Size,Al>& lhs, const char* rhs) noexcept -> bool {
		return detail::string_equal(lhs.data(), lhs.size(), rhs, strlen(rhs));
	}
	template<size_t Size, class Al>
	auto operator== (const std::string &lhs, const tiny_string<Size,Al>& rhs)noexcept -> bool {
		return detail::string_equal(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<size_t Size, class Al>
	auto operator== (const tiny_string<Size,Al>& lhs, const std::string & rhs) noexcept -> bool {
		return detail::string_equal(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

	#ifdef SEQ_HAS_CPP_17
	template<size_t Size, class Al>
	bool operator== (const std::string_view& lhs, const tiny_string<Size, Al>& rhs)noexcept {
		return detail::string_equal(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<size_t Size, class Al>
	bool operator== (const tiny_string<Size, Al>& lhs, const std::string_view& rhs) noexcept {
		return detail::string_equal(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	#endif



	template<size_t Size, class Al, size_t Size2, class Al2>
	SEQ_ALWAYS_INLINE bool operator!= (const tiny_string<Size,Al>& lhs, const tiny_string<Size2,Al2>& rhs) noexcept {
		return !(lhs == rhs);
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator!= (const char* lhs, const tiny_string<Size,Al>& rhs) noexcept {
		return !(lhs == rhs);
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator!= (const tiny_string<Size,Al>& lhs, const char* rhs) noexcept {
		return !(lhs == rhs);
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator!= (const std::string &lhs, const tiny_string<Size,Al>& rhs)noexcept {
		return !(lhs == rhs);
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator!= (const tiny_string<Size,Al>& lhs, const std::string& rhs) noexcept {
		return !(lhs == rhs);
	}
	#ifdef SEQ_HAS_CPP_17
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator!= (const std::string_view& lhs, const tiny_string<Size, Al>& rhs)noexcept {
		return !(lhs == rhs);
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator!= (const tiny_string<Size, Al>& lhs, const std::string_view& rhs) noexcept {
		return !(lhs == rhs);
	}
	#endif


	template<size_t Size, class Al, size_t Size2, class Al2>
	SEQ_ALWAYS_INLINE bool operator< (const tiny_string<Size,Al>& lhs, const tiny_string<Size2,Al2>& rhs) noexcept {
		return detail::string_inf(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator< (const char* lhs, const tiny_string<Size,Al>& rhs) noexcept {
		return detail::string_inf(lhs, lhs+strlen(lhs), rhs.begin(), rhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator< (const tiny_string<Size,Al>& lhs, const char* rhs) noexcept {
		return detail::string_inf(lhs.begin(), lhs.end(), rhs, rhs+strlen(rhs));
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator< (const std::string &lhs, const tiny_string<Size,Al>& rhs)noexcept {
		return detail::string_inf(lhs.data(), lhs.data() + lhs.size(), rhs.begin(), rhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator< (const tiny_string<Size,Al>& lhs, const std::string& rhs) noexcept {
		return detail::string_inf(lhs.begin(), lhs.end(), rhs.data(), rhs.data() + rhs.size());
	}
	#ifdef SEQ_HAS_CPP_17
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator< (const std::string_view& lhs, const tiny_string<Size, Al>& rhs)noexcept {
		return detail::string_inf(lhs.data(), lhs.data() + lhs.size(), rhs.begin(), rhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator< (const tiny_string<Size, Al>& lhs, const std::string_view& rhs) noexcept {
		return detail::string_inf(lhs.begin(), lhs.end(), rhs.data(), rhs.data() + rhs.size());
	}
	#endif


	template<size_t Size, class Al, size_t Size2, class Al2>
	SEQ_ALWAYS_INLINE bool operator<= (const tiny_string<Size,Al>& lhs, const tiny_string<Size2,Al2>& rhs) noexcept {
		return detail::string_inf_equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator<= (const char* lhs, const tiny_string<Size,Al>& rhs) noexcept {
		return detail::string_inf_equal(lhs, lhs+ strlen(lhs), rhs.begin(), rhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator<= (const tiny_string<Size,Al>& lhs, const char* rhs) noexcept {
		return detail::string_inf_equal(lhs.begin(), lhs.end(), rhs, rhs+strlen(rhs));
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator<= (const std::string &lhs, const tiny_string<Size,Al>& rhs)noexcept {
		return detail::string_inf_equal(lhs.data(), lhs.data() + lhs.size(), rhs.begin(), rhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator<= (const tiny_string<Size,Al>& lhs, const std::string& rhs) noexcept {
		return detail::string_inf_equal(lhs.begin(), lhs.end(), rhs.data(), rhs.data() + rhs.size());
	}
	#ifdef SEQ_HAS_CPP_17
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator<= (const std::string_view& lhs, const tiny_string<Size, Al>& rhs)noexcept {
		return detail::string_inf_equal(lhs.data(), lhs.data() + lhs.size(), rhs.begin(), rhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator<= (const tiny_string<Size, Al>& lhs, const std::string_view& rhs) noexcept {
		return detail::string_inf_equal(lhs.begin(), lhs.end(), rhs.data(), rhs.data() + rhs.size());
	}
	#endif



	template<size_t Size, class Al, size_t Size2, class Al2>
	SEQ_ALWAYS_INLINE bool operator> (const tiny_string<Size,Al>& lhs, const tiny_string<Size2,Al2>& rhs) noexcept {
		return detail::string_inf(rhs.begin(), rhs.end(), lhs.begin(), lhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator> (const char* lhs, const tiny_string<Size,Al>& rhs) noexcept {
		return detail::string_inf(rhs.begin(), rhs.end(), lhs, lhs+strlen(lhs));
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator> (const tiny_string<Size,Al>& lhs, const char* rhs) noexcept {
		return detail::string_inf(rhs, rhs+strlen(rhs), lhs.begin(), lhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator> (const std::string &lhs, const tiny_string<Size,Al>& rhs)noexcept {
		return detail::string_inf(rhs.begin(), rhs.end(), rhs.data(), lhs.data() + lhs.size());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator> (const tiny_string<Size,Al>& lhs, const std::string& rhs) noexcept {
		return detail::string_inf(rhs.data(), rhs.data() + lhs.size(), lhs.begin(), lhs.end());
	}
	#ifdef SEQ_HAS_CPP_17
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator> (const std::string_view& lhs, const tiny_string<Size, Al>& rhs)noexcept {
		return detail::string_inf(rhs.begin(), rhs.end(), rhs.data(), lhs.data() + lhs.size());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator> (const tiny_string<Size, Al>& lhs, const std::string_view& rhs) noexcept {
		return detail::string_inf(rhs.data(), rhs.data() + lhs.size(), lhs.begin(), lhs.end());
	}
	#endif


	template<size_t Size, class Al, size_t Size2, class Al2>
	SEQ_ALWAYS_INLINE bool operator>= (const tiny_string<Size,Al>& lhs, const tiny_string<Size2,Al2>& rhs) noexcept {
		return detail::string_inf_equal(rhs.begin(), rhs.end(), lhs.begin(), lhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator>= (const char* lhs, const tiny_string<Size,Al>& rhs) noexcept {
		return detail::string_inf_equal(rhs.begin(), rhs.end(), lhs, lhs+strlen(lhs));
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator>= (const tiny_string<Size,Al>& lhs, const char* rhs) noexcept {
		return detail::string_inf_equal(rhs, rhs+strlen(rhs), lhs.begin(), lhs.end());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator>= (const std::string &lhs, const tiny_string<Size,Al>& rhs)noexcept {
		return detail::string_inf_equal(rhs.begin(), rhs.end(), rhs.data(), lhs.data() + lhs.size());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator>= (const tiny_string<Size,Al>& lhs, const std::string& rhs) noexcept {
		return detail::string_inf_equal(rhs.data(), rhs.data() + lhs.size(), lhs.begin(), lhs.end());
	}
	#ifdef SEQ_HAS_CPP_17
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator>= (const std::string_view& lhs, const tiny_string<Size, Al>& rhs)noexcept {
		return detail::string_inf_equal(rhs.begin(), rhs.end(), rhs.data(), lhs.data() + lhs.size());
	}
	template<size_t Size, class Al>
	SEQ_ALWAYS_INLINE bool operator>= (const tiny_string<Size, Al>& lhs, const std::string_view& rhs) noexcept {
		return detail::string_inf_equal(rhs.data(), rhs.data() + lhs.size(), lhs.begin(), lhs.end());
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
		template<size_t Size, class Al>
		struct GetStaticSize<tiny_string<Size,Al>>
		{
			static constexpr size_t value = Size;
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
		template<>
		struct FindAllocator<view_allocator,view_allocator>
		{
			using allocator_type = std::allocator<char>;
			static auto select(const view_allocator& /*unused*/, const view_allocator& /*unused*/) -> std::allocator<char> {return {};}
		} ;
		template<class Al1>
		struct FindAllocator<Al1, view_allocator>
		{
			using allocator_type = Al1;
			static auto select(const Al1& al1, const view_allocator& /*unused*/) -> Al1 { return al1; }
		};
		template<class Al2>
		struct FindAllocator< view_allocator, Al2>
		{
			using allocator_type = Al2;
			static auto select(const view_allocator&  /*unused*/, const Al2& al2) -> Al2 { return al2; }
		};
		template<class S1, class S2, class Al=void>
		struct FindReturnType
		{
			using type = tiny_string<FindStaticSize<S1, S2>::value, typename FindAllocator<Al,Al>::allocator_type >;
		};
		template<class S1, class S2>
		struct FindReturnType<S1,S2,void>
		{
			using type = tiny_string<FindStaticSize<S1, S2>::value, typename FindAllocator<typename S1::allocator_type, typename S2::allocator_type>::allocator_type >;
		};
	} //end namespace detail

	template<size_t Size, class Al, size_t Size2, class Al2>
	auto operator+ (const tiny_string<Size, Al>& lhs, const tiny_string<Size2, Al2>& rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size2, Al2> >::type
	{
		using find_alloc = detail::FindAllocator<Al, Al2>;
		using ret_type = typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size2, Al2> >::type;
		ret_type ret(lhs.size() + rhs.size(), static_cast<char>(0),
			std::allocator_traits<typename ret_type::allocator_type>::select_on_container_copy_construction(find_alloc::select(lhs.get_allocator(), rhs.get_allocator())));
		memcpy(ret.data(), lhs.data(), lhs.size());
		memcpy(ret.data() + lhs.size(), rhs.data(), rhs.size());
		return ret;
	}

	template<size_t Size, class Al>
	auto operator+ (const tiny_string<Size, Al>& lhs, const std::string& rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size, Al> >::type
	{
		return lhs + tstring_view(rhs.data(), rhs.size());
	}
	#ifdef SEQ_HAS_CPP_17
	template<size_t Size, class Al>
	auto operator+ (const tiny_string<Size, Al>& lhs, const std::string_view& rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size, Al> >::type
	{
		return lhs + tstring_view(rhs.data(), rhs.size());
	}
	#endif

	template<size_t Size, class Al>
	auto operator+ (const tiny_string<Size, Al>& lhs, const char* rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size, Al> >::type
	{
		return lhs + tstring_view(rhs);
	}
	template<size_t Size, class Al>
	auto operator+ (const tiny_string<Size, Al>& lhs,  char rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size, Al> >::type
	{
		return lhs + tstring_view(&rhs,1);
	}

	template<size_t Size, class Al>
	auto operator+ (const std::string& lhs, const tiny_string<Size, Al>& rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size, Al> >::type
	{
		return tstring_view(lhs.data(), lhs.size()) + rhs;
	}
	#ifdef SEQ_HAS_CPP_17
	template<size_t Size, class Al>
	auto operator+ (const std::string_view& lhs, const tiny_string<Size, Al>& rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size, Al> >::type
	{
		return tstring_view(lhs.data(), lhs.size()) + rhs;
	}
	#endif


	template<size_t Size, class Al>
	auto operator+ (const char* lhs, const tiny_string<Size, Al>& rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size, Al> >::type
	{
		return tstring_view(lhs) + rhs;
	}
	template<size_t Size, class Al>
	auto operator+ ( char lhs, const tiny_string<Size, Al>& rhs) -> typename detail::FindReturnType<tiny_string<Size, Al>, tiny_string<Size, Al> >::type
	{
		return tstring_view(&lhs,1) + rhs;
	}

	/// @brief Returns the string data (const char*) for given string object
	auto string_data(const std::string& str) -> const char* { return str.data(); }
	template<size_t S, class Al>
	auto string_data(const tiny_string<S, Al>& str) -> const char* { return str.data(); }
	auto string_data(const char* str) -> const char* { return str; }
	auto string_data(const tstring_view& str) -> const char* { return str.data(); }
	#ifdef SEQ_HAS_CPP_17
	auto string_data(const std::string_view& str) -> const char* { return str.data(); }
	#endif

	/// @brief Returns the string size for given string object
	auto string_size(const std::string& str) -> size_t { return str.size(); }
	template<size_t S, class Al>
	auto string_size(const tiny_string<S, Al>& str) -> size_t { return str.size(); }
	auto string_size(const char* str) -> size_t { return strlen(str); }
	auto string_size(const tstring_view& str) -> size_t { return str.size(); }
	#ifdef SEQ_HAS_CPP_17
	auto string_size(const std::string_view& str) -> size_t { return str.size(); }
	#endif




	/// @brief Detect tiny_string
	template<class T>
	struct is_tiny_string : std::false_type {};
	template<size_t S, class Al>
	struct is_tiny_string<tiny_string<S,Al>> : std::true_type {};

	/// @brief Detect tiny_string, std::string, but not tstring_view
	template<class T>
	struct is_allocated_string : std::false_type {};
	template<size_t S, class Al>
	struct is_allocated_string<tiny_string<S, Al>> : std::true_type {};
	template<>
	struct is_allocated_string<std::string> : std::true_type {} ;
	template<>
	struct is_allocated_string<tstring_view> : std::false_type {} ;

	/// @brief Detect all possible string types (std::string, tstring, tstring_view, std::string_view, const char*, char*
	template<class T>
	struct is_generic_string : std::false_type {};
	template<>
	struct is_generic_string<char*> : std::true_type {} ;
	template<>
	struct is_generic_string<const char*> : std::true_type {} ;
	template<>
	struct is_generic_string<std::string> : std::true_type {} ;
	template<size_t S, class Al>
	struct is_generic_string<tiny_string<S, Al>> : std::true_type {};
	#ifdef SEQ_HAS_CPP_17
	template<>
	struct is_generic_string<std::string_view> : std::true_type {};
	#endif

	/// @brief Detect tstring_view or std::string_view
	template<class T>
	struct is_string_view : std::false_type {};
	template<>
	struct is_string_view<tstring_view> : std::true_type {} ;
	#ifdef SEQ_HAS_CPP_17
	template<>
	struct is_string_view<std::string_view> : std::true_type {};
	#endif


	/// @brief Detect generic string view: tstring_view, std::string_view, char*, const char*
	template<class T>
	struct is_generic_string_view : std::false_type {};
	template<>
	struct is_generic_string_view<tstring_view> : std::true_type {} ;
	template<>
	struct is_generic_string_view<char*> : std::true_type {} ;
	template<>
	struct is_generic_string_view<const char*> : std::true_type {} ;
	#ifdef SEQ_HAS_CPP_17
	template<>
	struct is_generic_string_view<std::string_view> : std::true_type {};
	#endif



	// Specialization of is_relocatable

	template<>
	struct is_relocatable<view_allocator> : std::true_type {} ;

	template<size_t S, class Alloc>
	struct is_relocatable<tiny_string<S, Alloc> > : is_relocatable<Alloc> {};
	
	
	
	

	/**********************************
	* Reading/writing from/to streams
	* ********************************/

	template<class Elem,class Traits,size_t Size, class Alloc>
	inline auto operator>>(std::basic_istream<Elem, Traits>& iss, tiny_string<Size, Alloc>& str) 
		-> typename std::enable_if<!std::is_same<Alloc, view_allocator>::value, std::basic_istream<Elem, Traits> >::type	&
	{	// extract a string
		typedef std::ctype<Elem> c_type;
		typedef std::basic_istream<Elem, Traits> stream_type;
		typedef tiny_string<Size, Alloc> string_type;
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
	inline	auto operator<<(std::basic_ostream<Elem, Traits>& oss, const tiny_string<Size, Alloc>& str) -> std::basic_ostream<Elem, Traits>&
	{	// insert a string
		typedef std::basic_ostream<Elem, Traits> myos;
		typedef tiny_string<Size, Alloc> mystr;
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
	template<size_t Size, class Allocator>
	SEQ_ALWAYS_INLINE void swap(seq::tiny_string<Size, Allocator>& a, seq::tiny_string<Size, Allocator>& b)
	{
		a.swap(b);
	}

	template<size_t Size, class Allocator>
	class hash< seq::tiny_string<Size, Allocator> >
	{
	public:
		using is_transparent = std::true_type;
		auto operator()(const seq::tiny_string<Size, Allocator>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast<const uint8_t*>(str.data()), str.size());
		}
		template<size_t S, class Al>
		auto operator()(const seq::tiny_string<S, Al>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast< const uint8_t*>(str.data()), str.size());
		}
		auto operator()(const std::string& str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast< const uint8_t*>(str.data()), str.size());
		}
		auto operator()(const char * str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast<const uint8_t*>(str), strlen(str));
		}

#ifdef SEQ_HAS_CPP_17
		auto operator()(const std::string_view& str) const noexcept -> size_t
		{
			return seq::hash_bytes_murmur64(reinterpret_cast<const uint8_t*>(str.data()), str.size());
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
