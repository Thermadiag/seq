// Copyright (c) 2021, Matthew Bentley (mattreecebentley@gmail.com) www.plflib.org

// zLib license (https://www.zlib.net/zlib_license.html):
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
// 	claim that you wrote the original software. If you use this software
// 	in a product, an acknowledgement in the product documentation would be
// 	appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
// 	misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef PLF_INDIESORT_H
#define PLF_INDIESORT_H


// Compiler-specific defines used by indiesort:


#if defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)
	#if _MSC_VER < 1900
		#define PLF_NOEXCEPT throw()
	#else
		#define PLF_NOEXCEPT noexcept
	#endif

	#if _MSC_VER >= 1600
		#define PLF_DECLTYPE_SUPPORT
		#define PLF_MOVE_SEMANTICS_SUPPORT
	#endif
	#if _MSC_VER >= 1700
		#define PLF_TYPE_TRAITS_SUPPORT
		#define PLF_ALLOCATOR_TRAITS_SUPPORT
	#endif
	#if _MSC_VER >= 1800
		#define PLF_VARIADICS_SUPPORT // Variadics, in this context, means both variadic templates and variadic macros are supported
	#endif

	#if defined(_MSVC_LANG) && (_MSVC_LANG >= 201703L)
		#define PLF_CONSTEXPR constexpr
	#else
		#define PLF_CONSTEXPR
	#endif

	#define PLF_CONSTFUNC

#elif defined(__cplusplus) && __cplusplus >= 201103L // C++11 support, at least
	#define PLF_MOVE_SEMANTICS_SUPPORT

	#if defined(__GNUC__) && defined(__GNUC_MINOR__) && !defined(__clang__) // If compiler is GCC/G++
		#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4 // 4.2 and below do not support variadic templates or decltype
			#define PLF_VARIADICS_SUPPORT
			#define PLF_DECLTYPE_SUPPORT
		#endif
		#if (__GNUC__ == 4 && __GNUC_MINOR__ < 6) || __GNUC__ < 4
			#define PLF_NOEXCEPT throw()
		#else
			#define PLF_NOEXCEPT noexcept
		#endif
		#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 7) || __GNUC__ > 4
			#define PLF_ALLOCATOR_TRAITS_SUPPORT
		#endif
		#if __GNUC__ >= 5 // GCC v4.9 and below do not support std::is_trivially_copyable
			#define PLF_TYPE_TRAITS_SUPPORT
		#endif
	#elif defined(__clang__) && !defined(__GLIBCXX__) && !defined(_LIBCPP_CXX03_LANG)
		#if __clang_major__ >= 3 // clang versions < 3 don't support __has_feature() or traits
			#define PLF_ALLOCATOR_TRAITS_SUPPORT
			#define PLF_TYPE_TRAITS_SUPPORT

			#if __has_feature(cxx_decltype)
				#define PLF_DECLTYPE_SUPPORT
			#endif
			#if __has_feature(cxx_noexcept)
				#define PLF_NOEXCEPT noexcept
			#else
				#define PLF_NOEXCEPT throw()
			#endif
			#if __has_feature(cxx_rvalue_references) && !defined(_LIBCPP_HAS_NO_RVALUE_REFERENCES)
				#define PLF_MOVE_SEMANTICS_SUPPORT
			#endif
			#if __has_feature(cxx_variadic_templates) && !defined(_LIBCPP_HAS_NO_VARIADICS)
				#define PLF_VARIADICS_SUPPORT
			#endif
		#endif
	#elif defined(__GLIBCXX__) // Using another compiler type with libstdc++ - we are assuming full c++11 compliance for compiler - which may not be true
		#define PLF_DECLTYPE_SUPPORT

		#if __GLIBCXX__ >= 20080606 	// libstdc++ 4.2 and below do not support variadic templates
			#define PLF_VARIADICS_SUPPORT
		#endif
		#if __GLIBCXX__ >= 20160111
			#define PLF_ALLOCATOR_TRAITS_SUPPORT
			#define PLF_NOEXCEPT noexcept
		#elif __GLIBCXX__ >= 20120322
			#define PLF_ALLOCATOR_TRAITS_SUPPORT
			#define PLF_NOEXCEPT noexcept
		#else
			#define PLF_NOEXCEPT throw()
		#endif
		#if __GLIBCXX__ >= 20150422 // libstdc++ v4.9 and below do not support std::is_trivially_copyable
			#define PLF_TYPE_TRAITS_SUPPORT
		#endif
	#elif defined(_LIBCPP_CXX03_LANG) // Special case for checking C++11 support with libCPP
		#define PLF_STACK_NOEXCEPT throw()
		#if !defined(_LIBCPP_HAS_NO_VARIADICS)
			#define PLF_VARIADICS_SUPPORT
   	#endif
	#else // Assume full support for other compilers and standard libraries
		#define PLF_DECLTYPE_SUPPORT
		#define PLF_INITIALIZER_LIST_SUPPORT
		#define PLF_ALLOCATOR_TRAITS_SUPPORT
		#define PLF_VARIADICS_SUPPORT
		#define PLF_TYPE_TRAITS_SUPPORT
		#define PLF_NOEXCEPT noexcept
	#endif

	#if __cplusplus >= 201703L  &&   ((defined(__clang__) && ((__clang_major__ == 3 && __clang_minor__ == 9) || __clang_major__ > 3))   ||   (defined(__GNUC__) && __GNUC__ >= 7)   ||   (!defined(__clang__) && !defined(__GNUC__)))
		#define PLF_CONSTEXPR constexpr
	#else
		#define PLF_CONSTEXPR
	#endif

	#if defined(PLF_LIBRARY_CONSTEXPR_FUNCTIONS) && __cplusplus > 201703L && ((defined(__clang__) && (__clang_major__ >= 10)) || (defined(__GNUC__) && __GNUC__ >= 10) || (!defined(__clang__) && !defined(__GNUC__)))
		#define PLF_CONSTFUNC constexpr
	#else
		#define PLF_CONSTFUNC
	#endif
#else
	#define PLF_NOEXCEPT throw()
	#define PLF_CONSTEXPR
	#define PLF_CONSTFUNC
#endif




#ifdef PLF_ALLOCATOR_TRAITS_SUPPORT
	#ifdef PLF_VARIADICS_SUPPORT
		#define PLF_CONSTRUCT(the_allocator, allocator_instance, location, ...)	std::allocator_traits<the_allocator>::construct(allocator_instance, location, __VA_ARGS__)
	#else
		#define PLF_CONSTRUCT(the_allocator, allocator_instance, location, data) std::allocator_traits<the_allocator>::construct(allocator_instance, location, data)
	#endif

	#define PLF_ALLOCATE(the_allocator, allocator_instance, size, hint) 		std::allocator_traits<the_allocator>::allocate(allocator_instance, size, hint)
	#define PLF_DEALLOCATE(the_allocator, allocator_instance, location, size) 	std::allocator_traits<the_allocator>::deallocate(allocator_instance, location, size)
#else
	#ifdef PLF_VARIADICS_SUPPORT
		#define PLF_CONSTRUCT(the_allocator, allocator_instance, location, ...)	allocator_instance.construct(location, __VA_ARGS__)
	#else
		#define PLF_CONSTRUCT(the_allocator, allocator_instance, location, data) allocator_instance.construct(location, data)
	#endif

	#define PLF_ALLOCATE(the_allocator, allocator_instance, size, hint)	 		allocator_instance.allocate(size, hint)
	#define PLF_DEALLOCATE(the_allocator, allocator_instance, location, size) 	allocator_instance.deallocate(location, size)
#endif



#ifndef PLF_SORT_FUNCTION
	#include <algorithm> // std::sort
#endif

#include <cassert>	// assert
#include <cstddef> // std::size_t
#include <memory>	// std::allocator
#include <limits>  // std::numeric_limits


#ifdef PLF_TYPE_TRAITS_SUPPORT
	#include <iterator> // std::iterator_traits
	#include <type_traits> // is_same
#endif

#ifdef PLF_MOVE_SEMANTICS_SUPPORT
	#include <utility> // std::move
#endif



namespace plf
{
	// C++11-like functions/structs etc for C++03/98 compatibility:

	template <class element_type>
	struct less
	{
		PLF_CONSTFUNC bool operator() (const element_type &a, const element_type &b) const PLF_NOEXCEPT
		{
			return a < b;
		}
	};



	template <bool condition, class T = void>
	struct enable_if_c
	{
		typedef T type;
	};

	template <class T>
	struct enable_if_c<false, T>
	{};



	template <typename T>
	struct is_pointer
	{
		static const bool value = false;
	};

	template <typename T>
	struct is_pointer<T*>
	{
		static const bool value = true;
	};



	template< class T > struct remove_pointer                    {typedef T type;};
	template< class T > struct remove_pointer<T*>                {typedef T type;};
	template< class T > struct remove_pointer<T* const>          {typedef T type;};
	template< class T > struct remove_pointer<T* volatile>       {typedef T type;};
	template< class T > struct remove_pointer<T* const volatile> {typedef T type;};



	template <bool flag, class the_type> struct derive_type;

	template <class the_type> struct derive_type<true, the_type>
	{
		typedef typename plf::remove_pointer<the_type>::type type;
	};

	template <class the_type> struct derive_type<false, the_type>
	{
		typedef typename the_type::value_type type;
	};



	// Function-object, used to redirect the sort function to compare element pointers by the elements they point to, and sort the element pointers instead of the elements:

	template <class comparison_function, class iterator_type, class size_type>
	struct random_access_sort_dereferencer
	{
		comparison_function stored_instance;
		const iterator_type stored_first_iterator;

		PLF_CONSTFUNC explicit random_access_sort_dereferencer(const comparison_function &function_instance, const iterator_type first):
			stored_instance(function_instance),
			stored_first_iterator(first)
		{}

		PLF_CONSTFUNC bool operator() (const size_type index1, const size_type index2)
		{
			return stored_instance(*(stored_first_iterator + index1), *(stored_first_iterator + index2));
		}
	};



	template <class iterator_type, class comparison_function, typename size_type>
	PLF_CONSTFUNC void random_access_sort(const iterator_type first, comparison_function compare, const size_type size)
	{
		typedef typename plf::derive_type<plf::is_pointer<iterator_type>::value, iterator_type>::type	element_type;
		typedef typename std::allocator<size_type> 																		size_type_allocator_type;

		size_type_allocator_type size_type_allocator;
		size_type * const sort_array = PLF_ALLOCATE(size_type_allocator_type, size_type_allocator, size, NULL);
		size_type *size_type_pointer = sort_array;

		// Construct pointers to all elements in the sequence:
		for (size_type index = 0; index != size; ++index, ++size_type_pointer)
		{
			PLF_CONSTRUCT(size_type_allocator_type, size_type_allocator, size_type_pointer, index);
		}


		// Now, sort the pointers by the values they point to (std::sort is default sort function if the macro below is not defined):
		#ifndef PLF_SORT_FUNCTION
			std::sort(sort_array, size_type_pointer, plf::random_access_sort_dereferencer<comparison_function, iterator_type, size_type>(compare, first));
		#else
			PLF_SORT_FUNCTION(sort_array, size_type_pointer, plf::random_access_sort_dereferencer<comparison_function, iterator_type, size_type>(compare, first));
		#endif



		// Sort the actual elements via the tuple array:
		size_type index = 0;

		for (size_type *current_index = sort_array; current_index != size_type_pointer; ++current_index, ++index)
		{
			if (*current_index != index)
			{
				size_type destination_index = index;

				#ifdef PLF_MOVE_SEMANTICS_SUPPORT
					element_type end_value = std::move(*(first + destination_index));
				#else
					element_type end_value = *(first + destination_index);
				#endif

				size_type source_index = *current_index;

				do
				{
					#ifdef PLF_MOVE_SEMANTICS_SUPPORT
						*(first + destination_index) = std::move(*(first + source_index));
					#else
						*(first + destination_index) = *(first + source_index);
					#endif

					destination_index = source_index;
					source_index = sort_array[destination_index];
					sort_array[destination_index] = destination_index;
				} while (source_index != index);

				#ifdef PLF_MOVE_SEMANTICS_SUPPORT
					*(first + destination_index) = std::move(end_value);
				#else
					*(first + destination_index) = end_value;
				#endif
			}
		}

		PLF_DEALLOCATE(size_type_allocator_type, size_type_allocator, sort_array, size);
	}



	template <class iterator_type, class comparison_function>
	PLF_CONSTFUNC void call_random_access_sort(const iterator_type first, const iterator_type last, comparison_function compare)
	{
		assert(first <= last);
		const std::size_t size = static_cast<std::size_t>(last - first);

		if (size < 2)
		{
			return;
		}
		else if (size <= std::numeric_limits<unsigned char>::max())
		{
			plf::random_access_sort<iterator_type, comparison_function, unsigned char>(first, compare, static_cast<unsigned char>(size));
		}
		else if (size <= std::numeric_limits<unsigned short>::max())
		{
			plf::random_access_sort<iterator_type, comparison_function, unsigned short>(first, compare, static_cast<unsigned short>(size));
		}
		else if (size <= std::numeric_limits<unsigned int>::max())
		{
			plf::random_access_sort<iterator_type, comparison_function, unsigned int>(first, compare, static_cast<unsigned int>(size));
		}
		else
		{
			plf::random_access_sort<iterator_type, comparison_function, std::size_t>(first, compare, size);
		}
	}



	template <class comparison_function, class element_type>
	struct sort_dereferencer
	{
		comparison_function stored_instance;

		PLF_CONSTFUNC explicit sort_dereferencer(const comparison_function &function_instance):
			stored_instance(function_instance)
		{}

		PLF_CONSTFUNC bool operator() (const element_type first, const element_type second)
		{
			return stored_instance(*(first.original_location), *(second.original_location));
		}
	};



	// This struct must be non-local to the function below for C++03 compatibility:

	template <class pointer, typename size_type>
	struct pointer_index_tuple
	{
		pointer original_location;
		size_type original_index;

		PLF_CONSTFUNC pointer_index_tuple(const pointer _item, const size_type _index) PLF_NOEXCEPT:
			original_location(_item),
			original_index(_index)
		{}
	};



	template <class iterator_type, class comparison_function>
	PLF_CONSTFUNC void non_random_access_sort(const iterator_type first, const iterator_type last, comparison_function compare, const std::size_t size)
	{
		if (size < 2)
		{
			return;
		}

		typedef typename plf::derive_type<plf::is_pointer<iterator_type>::value, iterator_type>::type	element_type;
		typedef typename std::size_t																							size_type;
		typedef plf::pointer_index_tuple<element_type *, size_type> 												item_index_tuple;

		typedef typename std::allocator<item_index_tuple> tuple_allocator_type;
		tuple_allocator_type tuple_allocator;

		item_index_tuple * const sort_array = PLF_ALLOCATE(tuple_allocator_type, tuple_allocator, size, NULL);
		item_index_tuple *tuple_pointer = sort_array;

		// Construct pointers to all elements in the sequence:
		size_type index = 0;

		for (iterator_type current_element = first; current_element != last; ++current_element, ++tuple_pointer, ++index)
		{
			#ifdef PLF_VARIADICS_SUPPORT
				PLF_CONSTRUCT(tuple_allocator_type, tuple_allocator, tuple_pointer, &*current_element, index);
			#else
				PLF_CONSTRUCT(tuple_allocator_type, tuple_allocator, tuple_pointer, item_index_tuple(&*current_element, index));
			#endif
		}


		// Now, sort the pointers by the values they point to (std::sort is default sort function if the macro below is not defined):
		#ifndef PLF_SORT_FUNCTION
			std::sort(sort_array, sort_array + size, plf::sort_dereferencer<comparison_function, item_index_tuple>(compare));
		#else
			PLF_SORT_FUNCTION(sort_array, sort_array + size, plf::sort_dereferencer<comparison_function, item_index_tuple>(compare));
		#endif


		// Sort the actual elements via the tuple array:
		index = 0;

		for (item_index_tuple *current_tuple = sort_array; current_tuple != tuple_pointer; ++current_tuple, ++index)
		{
			if (current_tuple->original_index != index)
			{
				#ifdef PLF_MOVE_SEMANTICS_SUPPORT
					element_type end_value = std::move(*(current_tuple->original_location));
				#else
					element_type end_value = *(current_tuple->original_location);
				#endif

				size_type destination_index = index;
				size_type source_index = current_tuple->original_index;

				do
				{
					#ifdef PLF_MOVE_SEMANTICS_SUPPORT
						*(sort_array[destination_index].original_location) = std::move(*(sort_array[source_index].original_location));
					#else
						*(sort_array[destination_index].original_location) = *(sort_array[source_index].original_location);
					#endif

					destination_index = source_index;
					source_index = sort_array[destination_index].original_index;
					sort_array[destination_index].original_index = destination_index;
				} while (source_index != index);

				#ifdef PLF_MOVE_SEMANTICS_SUPPORT
					*(sort_array[destination_index].original_location) = std::move(end_value);
				#else
					*(sort_array[destination_index].original_location) = end_value;
				#endif
			}
		}

		PLF_DEALLOCATE(tuple_allocator_type, tuple_allocator, sort_array, size);
	}



	// Range templates:
	template <class iterator_type, class comparison_function>
	PLF_CONSTFUNC inline void indiesort(const iterator_type first, const iterator_type last, comparison_function compare, const std::size_t size)
	{
		plf::non_random_access_sort(first, last, compare, size);
	}



	template <class iterator_type, class comparison_function>
	#ifdef PLF_TYPE_TRAITS_SUPPORT
		PLF_CONSTFUNC inline void indiesort(const typename plf::enable_if_c<!(plf::is_pointer<iterator_type>::value || std::is_same<typename std::iterator_traits<iterator_type>::iterator_category, std::random_access_iterator_tag>::value), iterator_type>::type first, const iterator_type last, comparison_function compare)
	#else
		PLF_CONSTFUNC inline void indiesort(const typename plf::enable_if_c<!plf::is_pointer<iterator_type>::value, iterator_type>::type first, const iterator_type last, comparison_function compare)
	#endif
	{
		std::size_t size = 0;
		for (iterator_type temp = first; temp != last; ++temp, ++size) {}
		plf::non_random_access_sort(first, last, compare, size);
	}



	template <class iterator_type, class comparison_function>
	#ifdef PLF_TYPE_TRAITS_SUPPORT
		PLF_CONSTFUNC inline void indiesort(const typename plf::enable_if_c<(plf::is_pointer<iterator_type>::value || std::is_same<typename std::iterator_traits<iterator_type>::iterator_category, std::random_access_iterator_tag>::value), iterator_type>::type first, const iterator_type last, comparison_function compare)
	#else
		PLF_CONSTFUNC inline void indiesort(const typename plf::enable_if_c<plf::is_pointer<iterator_type>::value, iterator_type>::type first, const iterator_type last, comparison_function compare)
	#endif
	{
		plf::call_random_access_sort(first, last, compare);
	}



	template <class iterator_type>
	PLF_CONSTFUNC inline void indiesort(const iterator_type first, const iterator_type last)
	{
		indiesort(first, last, plf::less<typename plf::derive_type<plf::is_pointer<iterator_type>::value, iterator_type>::type>());
	}



	// Container-based templates:

	#ifdef PLF_TYPE_TRAITS_SUPPORT
		template <class container_type, class comparison_function, typename plf::enable_if_c<std::is_same<typename std::iterator_traits<typename container_type::iterator>::iterator_category, std::random_access_iterator_tag>::value, container_type>::type * = nullptr>
		PLF_CONSTFUNC inline void indiesort(container_type &container, comparison_function compare)
		{
			plf::call_random_access_sort(container.begin(), container.end(), compare);
		}
	#endif



	#ifdef PLF_DECLTYPE_SUPPORT
		template <typename T>
		class has_size_function
		{
			private:
				typedef char one;
				struct two { char x[2]; };

				template <typename C> PLF_CONSTFUNC static one test( decltype(&C::size) ) ;
				template <typename C> PLF_CONSTFUNC static two test(...);

			public:
				enum { value = sizeof(test<T>(0)) == sizeof(char) };
		};
	#endif



	#ifdef PLF_TYPE_TRAITS_SUPPORT
		template <class container_type, class comparison_function, typename plf::enable_if_c<!std::is_same<typename std::iterator_traits<typename container_type::iterator>::iterator_category, std::random_access_iterator_tag>::value, container_type>::type * = nullptr>
	#else
		template <class container_type, class comparison_function>
	#endif
	PLF_CONSTFUNC inline void indiesort(container_type &container, comparison_function compare)
	{
		#ifdef PLF_DECLTYPE_SUPPORT
			if PLF_CONSTEXPR (plf::has_size_function<container_type>::value)
			{
				plf::non_random_access_sort(container.begin(), container.end(), compare, static_cast<std::size_t>(container.size()));
			}
			else
		#endif
		{  // If no decltype support, assume container has .size()
			indiesort(container.begin(), container.end(), compare); // call range indiesort
		}
	}



	template <class container_type>
	PLF_CONSTFUNC inline void indiesort(container_type &container)
	{
		indiesort(container, plf::less<typename container_type::value_type>());
	}



}

#undef PLF_DECLTYPE_SUPPORT
#undef PLF_TYPE_TRAITS_SUPPORT
#undef PLF_ALLOCATOR_TRAITS_SUPPORT
#undef PLF_VARIADICS_SUPPORT
#undef PLF_MOVE_SEMANTICS_SUPPORT
#undef PLF_NOEXCEPT
#undef PLF_CONSTEXPR

#undef PLF_CONSTRUCT
#undef PLF_ALLOCATE
#undef PLF_DEALLOCATE

#endif // PLF_INDIESORT_H