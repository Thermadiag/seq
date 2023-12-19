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

#ifndef SEQ_TAGGED_POINTER_HPP
#define SEQ_TAGGED_POINTER_HPP



/** @file */

/** \addtogroup memory
 *  @{
 */

#include <utility>

#include "bits.hpp"


namespace seq
{
	enum TagPointerType {
		//If a tagged pointer points to a variable on the stack, no choice but to assume an alignment of alignof(T).
		//This might leave much less space for tag (0 for bytes types).
		StackPointer,
		//When pointer to a variable allocated on the heap, we can safely use alignof(std::max_align_t).
		HeapPointer,
		//User defined alignment, use at your own risks 
		CustomAlignment
	};


	namespace detail
	{
		// We need to specialize a structure to get tagged_pointer actual alignment value because msvc 2015 does not support alignof on abstract class...
		template<class T, TagPointerType Type , size_t UserDefinedAlignment >
		struct FindAlignment
		{
			static constexpr std::uintptr_t value = alignof(T);
		};
		template<class T, size_t UserDefinedAlignment >
		struct FindAlignment<T,HeapPointer, UserDefinedAlignment>
		{
			static constexpr std::uintptr_t value = SEQ_DEFAULT_ALIGNMENT;
		};
		template<class T, size_t UserDefinedAlignment >
		struct FindAlignment<T,CustomAlignment, UserDefinedAlignment>
		{
			static constexpr std::uintptr_t value = UserDefinedAlignment;
		};
	}

	
	/// @brief Tagged pointer class.
	///
	/// seq::tagged_pointer uses the low bits of a pointer address to store metadata (or tag value).
	/// The number of bits used for the tag value depends on the TagPointerType flag:
	///		-	StackPointer (default): the tagged_pointer is assumed to point on a stack value or on a value inside an array.
	///			In this case, the tag bits is equal to static_bit_scan_reverse<alignof(T)>::value. For instance, 2 bits for int32_t,
	///			3 bits for int64_t and ... 0 bits for char.
	///		-	HeapPointer: the tagged_pointer is assumed to point on a heap allocated value. Therefore, its alignment is (in theory)
	///			equal to alignof(std::max_align_t). On most platforms, the tag bits is either 3 or 4.
	///		-	CustomAlignment : the tag bits is given by static_bit_scan_reverse<UserDefinedAlignment>::value.
	/// 
	/// tagged_pointer is specialized to work with void pointer.
	/// 
	template< class T, TagPointerType Type = StackPointer, size_t UserDefinedAlignment = 0>
	class tagged_pointer
	{
		static constexpr std::uintptr_t align = detail::FindAlignment<T, Type, UserDefinedAlignment>::value; 

		static_assert( (((align & (align - 1)) == 0)), "alignment must be a non null power of 2");
		static constexpr std::uintptr_t bits = static_bit_scan_reverse<align>::value;
		
		std::uintptr_t d_ptr;

	public:

		/// @brief storage and tag type
		using tag_type = std::uintptr_t;
		using value_type = T;
		using reference = T&;
		using const_reference = const T&;
		using pointer = T*;
		using const_pointer = const T*;

		/// @brief tagged pointer type
		static constexpr TagPointerType type = Type;
		/// @brief number of bits for the tag
		static constexpr std::uintptr_t tag_bits = bits;
		/// @brief mask used to extract the pointer address
		static constexpr std::uintptr_t mask_high = static_cast<std::uintptr_t>(~((1ULL << tag_bits) - 1ULL));
		/// @brief mask used to extract the tag
		static constexpr std::uintptr_t mask_low = static_cast<std::uintptr_t>(((1ULL << tag_bits) - 1ULL));


		/// @brief Construct from pointer
		tagged_pointer(T* ptr = nullptr) noexcept
			:d_ptr(reinterpret_cast<std::uintptr_t>(ptr)) {}
		/// @brief Construct from pointer and tag
		tagged_pointer(T* ptr, tag_type t) noexcept
			:d_ptr(reinterpret_cast<std::uintptr_t>(ptr) | (t & mask_low)) {}
		/// @brief Returns the pointer 
		auto ptr() noexcept -> pointer { return reinterpret_cast<T*>(d_ptr & mask_high); }
		/// @brief Returns the pointer 
		auto ptr() const noexcept -> const_pointer { return reinterpret_cast<T*>(d_ptr & mask_high); }
		/// @brief Returns the tag 
		auto tag() const noexcept -> tag_type { return d_ptr & mask_low; }

		/// @brief Set the pointer value
		void set_ptr(pointer ptr)noexcept { d_ptr = tag() | reinterpret_cast<std::uintptr_t>(ptr); }
		/// @brief Set the tag value
		auto set_tag(tag_type tag)noexcept -> tag_type { d_ptr = tag | (d_ptr & mask_high); return tag; }
		
		void set(pointer ptr, tag_type tag) noexcept {d_ptr = reinterpret_cast<std::uintptr_t>(ptr) | (tag & mask_low);}

		auto full() const noexcept -> std::uintptr_t { return d_ptr; }
		std::uintptr_t set_full(std::uintptr_t p)noexcept { return d_ptr = p; }

		auto split() const noexcept -> std::pair<const T*, unsigned> {
			return std::pair<const T*, unsigned>(ptr(), tag());
		}
		auto split() noexcept -> std::pair<T*, unsigned> {
			return std::pair<T*, unsigned>(ptr(), tag());
		}

		/// @brief cast operator to pointer
		operator pointer() noexcept { return ptr(); }
		/// @brief cast operator to pointer
		operator const_pointer() const noexcept { return ptr(); }

		/// @brief Returns the pointer 
		auto operator->() noexcept -> pointer { return ptr(); }
		/// @brief Returns the pointer 
		auto operator->() const noexcept -> const_pointer { return ptr(); }
		/// @brief Returns a reference to the pointed value
		auto operator*() noexcept -> reference { return *ptr(); }
		/// @brief Returns a reference to the pointed value
		auto operator*() const noexcept -> const_reference { return *ptr(); }
	};



	template< TagPointerType Type, size_t UserDefinedAlignment >
	class tagged_pointer<void, Type, UserDefinedAlignment>
	{
		// Specialization for void* , remove the reference type and related members

		static constexpr std::uintptr_t align = (Type != CustomAlignment ? SEQ_DEFAULT_ALIGNMENT : UserDefinedAlignment);
		static_assert(align > 0 && (((align & (align - 1)) == 0)), "alignment must be a non null power of 2");
		static constexpr std::uintptr_t bits = static_bit_scan_reverse<align>::value;

		std::uintptr_t d_ptr;

	public:

		using tag_type = std::uintptr_t;
		using value_type = void;
		using pointer = void*;
		using const_pointer = const void*;

		static constexpr TagPointerType type = Type;
		static constexpr std::uintptr_t tag_bits = bits;
		static constexpr std::uintptr_t mask_high = static_cast<std::uintptr_t>(~((1ULL << tag_bits) - 1ULL));
		static constexpr std::uintptr_t mask_low = static_cast<std::uintptr_t>(((1ULL << tag_bits) - 1ULL));

		tagged_pointer(void* ptr = nullptr) noexcept
			:d_ptr(reinterpret_cast<std::uintptr_t>(ptr)) {}
		tagged_pointer(void* ptr, tag_type t) noexcept
			:d_ptr(reinterpret_cast<std::uintptr_t>(ptr) | (t & mask_low)) {}
		auto ptr() noexcept -> pointer { return reinterpret_cast<void*>(d_ptr & mask_high); }
		auto ptr() const noexcept -> const_pointer { return reinterpret_cast<void*>(d_ptr & mask_high); }
		auto tag() const noexcept -> tag_type { return d_ptr & mask_low; }
		void set_ptr(pointer ptr)noexcept { d_ptr = tag() | reinterpret_cast<tag_type>(ptr); }
		auto set_tag(tag_type tag)noexcept -> tag_type { d_ptr = tag | (d_ptr & mask_high); return tag; }
		void set(pointer ptr, tag_type tag) noexcept { d_ptr = reinterpret_cast<std::uintptr_t>(ptr) | (tag & mask_low); }
		auto full() const noexcept -> std::uintptr_t { return d_ptr; }
		auto rfull() noexcept -> std::uintptr_t& { return d_ptr; }
		std::uintptr_t set_full(std::uintptr_t p)noexcept { return d_ptr = p; }

		auto split() const noexcept -> std::pair<const void*, unsigned> {
			return std::pair<const void*, unsigned>(ptr(),tag());
		}
		auto split() noexcept -> std::pair<void*, unsigned> {
			return std::pair<void*, unsigned>(ptr(), tag());
		}

		operator pointer() noexcept { return ptr(); }
		operator const_pointer() const noexcept { return ptr(); }
		auto operator->() noexcept -> pointer { return ptr(); }
		auto operator->() const noexcept -> const_pointer { return ptr(); }
	};

}

/** @}*/
//end memory

#endif
