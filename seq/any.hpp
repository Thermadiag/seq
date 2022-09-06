#ifndef SEQ_ANY_HPP
#define SEQ_ANY_HPP



/** @file */

/**\defgroup any Any: type-erasing polymorphic object wrapper used to build heterogeneous containers

The <i>any</i> module provides the seq::hold_any class and related functions to provide a type-erasing polymorphic object wrapper.
*/


#include "format.hpp"
#include "tagged_pointer.hpp"
#include "tiny_string.hpp"

#ifndef SEQ_ANY_MALLOC
#define SEQ_ANY_MALLOC malloc
#endif

#ifndef SEQ_ANY_FREE
#define SEQ_ANY_FREE free
#endif

#ifdef _MSC_VER
// For msvc only disable warning "inherits via dominance" that occurs when using hold_any with custom interface
#pragma warning( disable : 4250  )
#endif


namespace seq
{
	namespace detail
	{
		inline auto build_type_id() noexcept -> int
		{
			// Generate a unique id starting from 20
			static std::atomic<int> cnt = { 21 }; //start index for custom types
			return cnt++;
		}
	} // namespace detail

	/// @brief Returns type Id used by hold_any as a unique type identifier
	/// @tparam T type
	/// @return unique Id for type T
	template<class T>
	auto get_type_id() noexcept -> int
	{
		static int id = detail::build_type_id();
		return id;
	}
	template<> inline constexpr auto get_type_id<char>() noexcept -> int { return 1; }
	template<> inline constexpr auto get_type_id<signed char>() noexcept -> int { return 2; }
	template<> inline constexpr auto get_type_id<short>()noexcept -> int { return 3; }
	template<> inline constexpr auto get_type_id<int>() noexcept -> int { return 4; }
	template<> inline constexpr auto get_type_id<long>() noexcept -> int { return 5; }
	template<> inline constexpr auto get_type_id<long long>() noexcept -> int { return 6; }

	template<> inline constexpr auto get_type_id<unsigned char>() noexcept -> int { return 7; }
	template<> inline constexpr auto get_type_id<unsigned short>() noexcept -> int { return 8; }
	template<> inline constexpr auto get_type_id<unsigned int>() noexcept -> int { return 9; }
	template<> inline constexpr auto get_type_id<unsigned long>()noexcept -> int { return 10; }
	template<> inline constexpr auto get_type_id<unsigned long long>() noexcept -> int { return 11; }

	template<> inline constexpr auto get_type_id<float>() noexcept -> int { return 12; }
	template<> inline constexpr auto get_type_id<double>() noexcept -> int { return 13; }
	template<> inline constexpr auto get_type_id<long double>() noexcept -> int { return 14; }

	template<> inline constexpr auto get_type_id<std::string >() noexcept -> int { return 15; }
	template<> inline constexpr auto get_type_id<tstring >() noexcept -> int { return 16; }
	template<> inline constexpr auto get_type_id<tstring_view >() noexcept -> int { return 17; }
	template<> inline constexpr auto get_type_id<const char* >() noexcept -> int { return 18; }
	template<> inline constexpr auto get_type_id<char* >() noexcept -> int { return 19; }
#ifdef SEQ_HAS_CPP_17
	template<> inline constexpr int get_type_id<std::string_view>() noexcept { return 20; }
#endif

	/// @brief Returns true if given type id corresponds to a signed integral type
	inline auto is_signed_integral_type(int id) noexcept -> bool
	{
		return id != 0 && id <= get_type_id<long long>();
	}
	/// @brief Returns true if given type id corresponds to an unsigned integral type
	inline auto is_unsigned_integral_type(int id) noexcept -> bool
	{
		return id >= get_type_id<unsigned char>() && id <= get_type_id<unsigned long long>();
	}
	/// @brief Returns true if given type id corresponds to an integral type
	inline auto is_integral_type(int id) noexcept -> bool
	{
		return id != 0 && id <= get_type_id<unsigned long long>();
	}
	/// @brief Returns true if given type id corresponds to a floating point type
	inline auto is_floating_point_type(int id) noexcept -> bool
	{
		return id >= get_type_id<float>() && id <= get_type_id<long double>();
	}
	/// @brief Returns true if given type id corresponds to an arithmetic type (floating point or integral)
	inline auto is_arithmetic_type(int id) noexcept -> bool
	{
		return id != 0 && id <= get_type_id<long double>() ;
	}
	/// @brief Returns true if given type id corresponds to a string type (std::string, tstring, tstring_view or const char*)
	inline auto is_string_type(int id) noexcept -> bool
	{
#ifdef SEQ_HAS_CPP_17
		return id >= get_type_id<std::string>() && id <= get_type_id<std::string_view>();
#else
		return id >= get_type_id<std::string>() && id <= get_type_id<char*>();
#endif
	}




	namespace detail
	{
		static auto read_void_p(const void* in) -> void*
		{
			// read pointer address from memory block
			void* res = nullptr;
			memcpy(&res, in, sizeof(void*));
			return res;
		}

		enum AnyTag
		{
			// tags used to describe a type and used by hold_any
			complex_destroy = 1, // not trivially destructible
			complex_copy = 2, // not trivially copiable
			non_relocatable = 4, // not relocatable
			big_size = 8, // big types (no SBO)
			pointer = 16 // pointer type
		};

		
		/////////////////////////////////////////////////////
		// 
		// LOTS of partial specialization because C++11 does not support 'if constexpr ()' ...
		//
		/////////////////////////////////////////////////////

		
		/// @brief Hash type only if is_hashable<T>::value is true
		template<class T, bool Valid = is_hashable<T>::value, class  = void>
		struct HashType
		{
			static SEQ_ALWAYS_INLINE auto apply(const void* in) -> size_t
			{
				return std::hash<T>{}( *static_cast<const T*>(in));
			}
		};
		/// @brief Hash integral types
		template<class T>
		struct HashType<T,true, typename std::enable_if<std::is_integral<T>::value,void>::type >
		{
			static SEQ_ALWAYS_INLINE auto apply(const void* in) -> size_t
			{
				return std::hash<std::uint64_t>{}(static_cast<std::uint64_t>(*static_cast<const T*>(in)));
			}
		};

		/// @brief Specializations for std::string and const char* that use std::hash<tstring>
		template<>
		struct HashType<std::string,true>
		{
			static SEQ_ALWAYS_INLINE auto apply(const void* in) -> size_t
			{
				return std::hash<tstring>{}(*static_cast<const std::string*>(in));
			}
		} ;
		template<>
		struct HashType<const char*, true>
		{
			static SEQ_ALWAYS_INLINE auto apply(const void* in) -> size_t
			{
				return std::hash<tstring>{}(static_cast<const char*>(read_void_p(in)));
			}
		} ;
		template<>
		struct HashType< char*, true>
		{
			static SEQ_ALWAYS_INLINE auto apply(const void* in) -> size_t
			{
				return std::hash<tstring>{}(static_cast<const char*>(read_void_p(in)));
			}
		} ;
		template<class T>
		struct HashType<T, false>
		{
			static SEQ_ALWAYS_INLINE auto apply(const void* /*unused*/) -> size_t
			{
				throw std::bad_function_call();
				return 0;
			}
		};
		
		/// @brief Format type only if is_formattable<T>::value is true
		template<class T, bool Valid = is_formattable<T>::value>
		struct FormatType
		{
			static SEQ_ALWAYS_INLINE void apply(std::string & out, const void* in, const width_format& wfmt, const numeric_format& nfmt)
			{
				auto f = fmt(const_cast<T&>(*static_cast<const T*>(in)));
				f.set_width_format(wfmt);
				f.set_numeric_format(nfmt);
				f.append(out);
			}
		};
		template<class T>
		struct FormatType<T, false>
		{
			static SEQ_ALWAYS_INLINE void apply(std::string&  /*unused*/, const void*  /*unused*/, const width_format&  /*unused*/, const numeric_format&  /*unused*/)
			{
				throw std::bad_function_call();
			}
		};

		/// @brief Cast T to an arithmetic type
		template<class T, bool IsNumeric = std::is_arithmetic<T>::value>
		struct CastArithmetic
		{
			template<class TypeInfo>
			static auto apply(const void* in, const TypeInfo* in_p) -> T
			{
				switch (in_p->type_id()) {
					case get_type_id<char>() : {return static_cast<T>(*static_cast<const char*>(in)); }
					case get_type_id<signed char>() : {return static_cast<T>(*static_cast<const signed char*>(in)); }
					case get_type_id<unsigned char>() : {return static_cast<T>(*static_cast<const unsigned char*>(in)); }
					case get_type_id<short>() : {return static_cast<T>(*static_cast<const short*>(in)); }
					case get_type_id<unsigned short>() : {return static_cast<T>(*static_cast<const unsigned short*>(in)); }
					case get_type_id<int>() : {return static_cast<T>(*static_cast<const int*>(in)); }
					case get_type_id<unsigned int>() : {return static_cast<T>(*static_cast<const unsigned int*>(in)); }
					case get_type_id<long>() : {return static_cast<T>(*static_cast<const long*>(in)); }
					case get_type_id<unsigned long>() : {return static_cast<T>(*static_cast<const unsigned long*>(in)); }
					case get_type_id<long long>() : {return static_cast<T>(*static_cast<const long long*>(in)); }
					case get_type_id<unsigned long long>() : {return static_cast<T>(*static_cast<const unsigned long long*>(in)); }
					case get_type_id<float>() : {return static_cast<T>(*static_cast<const float*>(in)); }
					case get_type_id<double>() : {return static_cast<T>(*static_cast<const double*>(in)); }
					case get_type_id<long double>() : {return static_cast<T>(*static_cast<const long double*>(in)); }
				}
				throw std::bad_cast();
				return T();
			}
		};
		template<class T>
		struct CastArithmetic<T, false>
		{
			template<class TypeInfo>
			static SEQ_ALWAYS_INLINE auto apply(const void*  /*unused*/, const TypeInfo*  /*unused*/) -> T
			{
				throw std::bad_cast();
				return T();
			}
		};

		/// @brief Cast string type to an arithmetic type
		template<class T, bool IsNumeric = std::is_arithmetic<T>::value>
		struct CastString
		{
			template<class String>
			static SEQ_ALWAYS_INLINE auto apply(const String& in) -> T
			{
				T res;
				if (from_chars(string_data(in), string_data(in) + string_size(in), res).ec != std::errc())
					throw std::bad_cast();
				return res;
			}
		};
		template<class T>
		struct CastString<T, false>
		{
			template<class String>
			static SEQ_ALWAYS_INLINE auto apply(const String&  /*unused*/) -> T
			{
				throw std::bad_cast();
				return T();
			}
		};

		/// @brief Cast arithmetic type to string type
		template<class String, bool IsString = is_allocated_string<String>::value  >
		struct CastArithmeticToString
		{
			template<class TypeInfo>
			static auto apply(const void* in, const TypeInfo* in_p) -> String
			{
				String res;
				switch (in_p->type_id()) {
					case get_type_id<char>() : { fmt(*static_cast<const char*>(in)).append(res);  break; }
					case get_type_id<signed char>() : { fmt(*static_cast<const signed char*>(in)).append(res);  break; }
					case get_type_id<unsigned char>() : { fmt(*static_cast<const unsigned char*>(in)).append(res);  break; }
					case get_type_id<short>() : { fmt(*static_cast<const short*>(in)).append(res);  break; }
					case get_type_id<unsigned short>() : { fmt(*static_cast<const unsigned short*>(in)).append(res);  break; }
					case get_type_id<int>() : { fmt(*static_cast<const int*>(in)).append(res);  break; }
					case get_type_id<unsigned int>() : { fmt(*static_cast<const unsigned int*>(in)).append(res);  break; }
					case get_type_id<long>() : { fmt(*static_cast<const long*>(in)).append(res);  break; }
					case get_type_id<unsigned long>() : { fmt(*static_cast<const unsigned long*>(in)).append(res);  break; }
					case get_type_id<long long>() : { fmt(*static_cast<const long long*>(in)).append(res);  break; }
					case get_type_id<unsigned long long>() : { fmt(*static_cast<const unsigned long long*>(in)).append(res);  break; }
					case get_type_id<float>() : { fmt(*static_cast<const float*>(in)).append(res);  break; }
					case get_type_id<double>() : { fmt(*static_cast<const double*>(in)).append(res);  break; }
					case get_type_id<long double>() : { fmt(*static_cast<const long double*>(in)).append(res);  break; }
					default: throw std::bad_cast(); break;
				}
				return res;
			}
		};
		template<class String>
		struct CastArithmeticToString<String, false>
		{
			template<class TypeInfo>
			static SEQ_ALWAYS_INLINE auto apply(const void* /*unused*/, const TypeInfo* /*unused*/) -> String
			{
				throw std::bad_cast();
				return String();
			}
		};

		/// @brief Cast string types between them
		template<class String, class InString, bool IsString = is_allocated_string<String>::value && is_generic_string<InString>::value>
		struct CastStringToString
		{
			static SEQ_ALWAYS_INLINE auto apply(const InString& str) -> String
			{
				return String(string_data(str),string_size(str));
			}
		};
		template<class String, class InString>
		struct CastStringToString<String, InString, false>
		{
			static SEQ_ALWAYS_INLINE auto apply(const InString&  /*unused*/) -> String
			{
				throw std::bad_cast();
				return String();
			}
		};

		/// @brief Cast string types to string_view
		template<class String, class InString, bool IsString = is_generic_string_view<String>::value && is_generic_string<InString>::value >
		struct CastStringToStringView
		{
			static SEQ_ALWAYS_INLINE auto apply(const InString& str) -> String
			{
				return String(string_data(str), string_size(str));
			}
		};
		template<class InString>
		struct CastStringToStringView<char*,InString,true>
		{
			static SEQ_ALWAYS_INLINE auto apply(const InString& str) -> char*
			{
				return (char*)string_data(str);
			}
		};
		template<class InString>
		struct CastStringToStringView<const char*, InString, true>
		{
			static SEQ_ALWAYS_INLINE auto apply(const InString& str) -> const char*
			{
				return string_data(str);
			}
		};
		template<class String, class InString>
		struct CastStringToStringView<String, InString, false>
		{
			static SEQ_ALWAYS_INLINE auto apply(const InString&  /*unused*/) -> String
			{
				throw std::bad_cast();
				return String();
			}
		};

		template<class OutPtr, bool IsPtr = std::is_same<OutPtr,void*>::value || std::is_same<OutPtr, const void*>::value>
		struct CastPtrToVoid
		{
			static SEQ_ALWAYS_INLINE auto apply(const void* ptr) -> OutPtr
			{
				return (OutPtr)ptr;
			}
		};
		template<class OutPtr>
		struct CastPtrToVoid<OutPtr,false>
		{
			static SEQ_ALWAYS_INLINE auto apply(const void*  /*unused*/) -> OutPtr
			{
				throw std::bad_cast();
				return OutPtr();
			}
		};


		/// @brief Less than comparison for hold_any::less_than
		template<class T, class Cast, bool IsArithmetic = std::is_arithmetic<T>::value>
		struct CompareLessArithmetic
		{
			template<class U>
			static SEQ_ALWAYS_INLINE auto less(const U& a, const T& b) -> bool { return a < static_cast<Cast>(b); }
		};
		template<class T, class Cast>
		struct CompareLessArithmetic<T, Cast, false>
		{
			template<class U>
			static SEQ_ALWAYS_INLINE auto less(const U& /*unused*/, const T& /*unused*/) -> bool { return false; }
		};
		/// @brief Greater than comparison for hold_any::greater_than
		template<class T, class Cast, bool IsArithmetic = std::is_arithmetic<T>::value>
		struct CompareGreaterArithmetic
		{
			template<class U>
			static SEQ_ALWAYS_INLINE auto greater(const U& a, const T& b) -> bool { return a > static_cast<Cast>(b); }
		};
		template<class T, class Cast>
		struct CompareGreaterArithmetic<T, Cast, false>
		{
			template<class U>
			static SEQ_ALWAYS_INLINE auto greater(const U& /*unused*/, const T& /*unused*/) -> bool { return false; }
		};

		/// @brief Equality comparison for hold_any::equal_to
		template<class T, bool IsString = is_generic_string<T>::value>
		struct CompareEqualString
		{
			static SEQ_ALWAYS_INLINE auto equal(const tstring_view& a, const T& b) -> bool { return a == b; }
		};
		template<class T>
		struct CompareEqualString<T, false>
		{
			static SEQ_ALWAYS_INLINE auto equal(const tstring_view& /*unused*/, const T& /*unused*/) -> bool { return false; }
		};
		/// @brief Less than comparison for hold_any::less_than
		template<class T, bool IsString = is_generic_string<T>::value>
		struct CompareLessString
		{
			static SEQ_ALWAYS_INLINE auto less(const tstring_view& a, const T& b) -> bool { return a < b; }
		};
		template<class T>
		struct CompareLessString<T, false>
		{
			static SEQ_ALWAYS_INLINE auto less(const tstring_view& /*unused*/, const T& /*unused*/) -> bool { return false; }
		};
		/// @brief Greater than comparison for hold_any::greater_than
		template<class T, bool IsString = is_generic_string<T>::value>
		struct CompareGreaterString
		{
			static SEQ_ALWAYS_INLINE auto greater(const tstring_view& a, const T& b) -> bool { return a > b; }
		};
		template<class T>
		struct CompareGreaterString<T, false>
		{
			static SEQ_ALWAYS_INLINE auto greater(const tstring_view& /*unused*/, const T& /*unused*/) -> bool { return false; }
		};
		



		template<class T>
		SEQ_ALWAYS_INLINE void ostream_any(std::ostream& oss, const void* in)
		{
			constexpr_if<is_ostreamable<T>::value>(
				[](auto &oss, const auto& in) {oss << in; },
				[](auto &, const auto& ) {throw std::bad_function_call(); },
				oss, *static_cast<const T*>(in));
		}
		template<class T>
		SEQ_ALWAYS_INLINE void istream_any(std::istream& iss, void* in)
		{
			constexpr_if<is_istreamable<T>::value>(
				[](auto& iss, auto& in) {iss >> in; },
				[](auto& , auto& ) {throw std::bad_function_call(); },
				iss, *static_cast<T*>(in));
		}
		template<class T, bool SupportHash>
		SEQ_ALWAYS_INLINE auto hash_any(const void* in) -> size_t
		{
			return HashType<T, (is_hashable<T>::value && SupportHash)>::apply(in);
			
		}
		template<class T>
		SEQ_ALWAYS_INLINE auto compare_equal_any(const void* a, const void * b) -> bool
		{
			// Comparing types that are not comparable should return false instead of throwing an exception
			return constexpr_if<is_equal_comparable<T>::value>(
				[](const auto& a, const auto& b) {return a == b; },
				[](const auto& , const auto& ) {/*throw std::bad_function_call();*/ return false; },
				* static_cast<const T*>(a), * static_cast<const T*>(b));
		}
		template<class T>
		SEQ_ALWAYS_INLINE auto compare_less_any(const void* a, const void* b) -> bool
		{
			return constexpr_if<is_less_comparable<T>::value>(
				[](const auto& a, const auto& b) {return a < b; },
				[](const auto& , const auto& ) {throw std::bad_function_call(); return false; },
				*static_cast<const T*>(a), *static_cast<const T*>(b));
		}
		template<class T>
		SEQ_ALWAYS_INLINE void format_any(std::string& out, const void* in, const width_format& wfmt, const numeric_format& nfmt)
		{
			FormatType<T>::apply(out, in, wfmt, nfmt);
		}



		template<class T, class any_type_info>
		void copy_any(const any_type_info* in_p, const void* in, const any_type_info* out_p, void* out_storage, unsigned out_storage_size)
		{
			// Copy object to destination buffer, allocating buffer if dst size is not big enough

			if (!std::is_copy_assignable<T>::value && !std::is_copy_constructible<T>::value)
				throw std::bad_function_call();
			
			if (in_p == out_p && std::is_copy_assignable<T>::value)
			{
				//same type, just copy
				//get the dst pointer
				void* dst = sizeof(T) > out_storage_size ? read_void_p(out_storage) : out_storage;
				// might throw, fine
				// use constexpr_if to avoid compile error for non assignable types (but still copy constructible)
				constexpr_if<std::is_copy_assignable<T>::value>(
					[](auto& a, const auto& b) {a = b; },
					[](auto& , const auto& ) {},
					*static_cast<T*>(dst), *static_cast<const T*>(in)
					);
			}
			else
			{
				void* dst = out_storage;
				if (sizeof(T) > out_storage_size) {
					dst = SEQ_ANY_MALLOC(sizeof(T));
					if (!dst)
						throw std::bad_alloc();
					memcpy(out_storage, &dst, sizeof(void*));
				}
				try {
					constexpr_if<std::is_copy_constructible<T>::value>(
						[](void * dst, const auto& in) {new (dst) T(in); },
						[](void * , const auto& ) {},
						dst, *static_cast<const T*>(in)
						);
					//new (dst) T(*static_cast<const T*>(in));
				}
				catch (...) {
					if (sizeof(T) > out_storage_size)
						SEQ_ANY_FREE(dst);
					throw;
				}
			}
		}

		template<class T, class any_type_info>
		void move_any(const any_type_info* in_p, void* in, const any_type_info* out_p, void* out_storage, unsigned out_storage_size)
		{
			// Move object to destination buffer, allocating buffer if dst size is not big enough

			if (in_p == out_p && std::is_move_assignable<T>::value)
			{
				//same type, just copy
				//get the dst pointer
				void* dst = sizeof(T) > out_storage_size ? read_void_p(out_storage) : out_storage;
				// might throw, fine
				// use constexpr_if to avoid compile error for non move assignable types (but still move constructible)
				constexpr_if<std::is_move_assignable<T>::value>(
					[](auto& a,  auto& b) {a = std::move(b); },
					[](auto& ,  auto& ) {},
					*static_cast<T*>(dst), *static_cast<T*>(in)
					);
			}
			else
			{
				void* dst = out_storage;
				if (sizeof(T) > out_storage_size) {
					dst = SEQ_ANY_MALLOC(sizeof(T));
					if (!dst)
						throw std::bad_alloc();
					memcpy(out_storage, &dst, sizeof(void*));
				}
				try {
					new (dst) T(std::move(*static_cast<T*>(in)));
				}
				catch (...) {
					if (sizeof(T) > out_storage_size)
						SEQ_ANY_FREE(dst);
					throw;
				}
			}
		}

		struct null_policy {} ;
	}




	

	/// @brief Base class representing a type and related functions
	class alignas(32) any_type_info
	{
		template<class I, size_t S, size_t A>
		friend class hold_any;

		int d_type_id{};
	public:
		/// @brief Retrurns the type id
		auto type_id() const noexcept -> int { return d_type_id; }
		/// @brief Returns the size of underlying type
		virtual auto sizeof_type() const noexcept -> size_t = 0;
		/// @brief Destroy object
		virtual void destroy_any(void* in) const noexcept = 0;
		/// @brief Compare 2 objects of same type for equality
		virtual auto equal_any(const void* a, const void* o) const -> bool = 0;
		/// @brief Compare 2 objects of same type for less than
		virtual auto less_any(const void* a, const void* o) const -> bool = 0;
		/// @brief Hash object of underlying type
		virtual auto hash_any(const void* in) const -> size_t = 0;
		/// @brief Format object of underlying type into output string
		virtual void format_any(std::string& out, const void* in, const width_format& wfmt, const numeric_format& nfmt) const = 0;
		/// @brief Stream object of underlying type to a std::ostream object
		virtual void ostream_any(const void* in, std::ostream& oss) const = 0;
		/// @brief Read object of underlying type from a std::istream object
		virtual void istream_any( void* in, std::istream& iss) const = 0;
		virtual void copy_any(const any_type_info* in_p, const void* in, const any_type_info* out_p, void* out_storage, unsigned out_storage_size) const = 0;
		virtual void move_any(const any_type_info* in_p, void* in, const any_type_info* out_p, void* out_storage, unsigned out_storage_size) const = 0;
	};

	/// @brief Implementation of any_type_info for specific type (Type Erasure)
	template<class T, bool SupportHash = true>
	struct any_typed_type_info : virtual any_type_info
	{
		auto sizeof_type() const noexcept  -> size_t
		{
			return sizeof(T);
		}
		void destroy_any( void* in) const noexcept 
		{
			static_cast<T*>(in)->~T();
		}
		auto equal_any(const void* a, const void* b) const  -> bool
		{
			return detail::compare_equal_any<T>(a, b);
		}
		auto less_any(const void* a, const void* b) const  -> bool
		{
			return detail::compare_less_any<T>(a, b);
		}
		auto hash_any(const void* in) const  -> size_t
		{
			return detail::hash_any<T, SupportHash>(in);
		}
		void format_any(std::string& out,  const void* in, const width_format& wfmt, const numeric_format& nfmt) const 
		{
			detail::format_any<T>(out, in, wfmt, nfmt);
		}
		void ostream_any( const void* in, std::ostream& oss) const 
		{
			return detail::ostream_any<T>(oss, in);
		}
		void istream_any( void* in, std::istream& iss) const 
		{
			return detail::istream_any<T>(iss, in);
		}
		void copy_any(const any_type_info* in_p, const void* in, const any_type_info* out_p, void* out_storage, unsigned out_storage_size) const 
		{
			detail::copy_any<T>(in_p, in, out_p, out_storage, out_storage_size);
		}
		void move_any(const any_type_info* in_p, void* in, const any_type_info* out_p, void* out_storage, unsigned out_storage_size) const 
		{
			detail::move_any<T>(in_p, in, out_p, out_storage, out_storage_size);
		}
	};




	namespace detail
	{

		inline auto get_converters() -> std::vector < std::vector < std::function<void(const void*, void*)> > >&
		{
			// global converter functions
			static std::vector < std::vector < std::function<void(const void*, void*)> > > inst;
			return inst;
		}

		template<class T, class U>
		void default_convert(const void* in, void* out)
		{
			// default conversion function for castable types
			*static_cast<U*>(out) = static_cast<U>(*static_cast<const T*>(in));
		}
		template<class T, class U, class Fun>
		void default_convert_with_functor(const Fun& fun, const void* in, void* out)
		{
			// conversion function using a functor
			*static_cast<U*>(out) = fun(*static_cast<const T*>(in));
		}


		inline auto get_less_comparison() -> std::vector < std::vector < std::function<bool(const void*, const void*)> > >&
		{
			// global comparison functions for less than comparison
			static std::vector < std::vector < std::function<bool(const void*, const void*)> > > inst;
			return inst;
		}
		template<class T, class U>
		auto default_less_comparison(const void* a, const void* b) -> bool
		{
			return *static_cast<const T*>(a) < *static_cast<const U*>(b);
		}
		template<class T, class U, class Fun>
		auto default_less_comparison_with_functor(const Fun& fun, const void* a, const void* b) -> bool
		{
			return fun(*static_cast<const T*>(a), *static_cast<const U*>(b));
		}


		inline auto get_equal_comparison() -> std::vector < std::vector < std::function<bool(const void*, const void*)> > >&
		{
			// global comparison functions for equality comparison
			static std::vector < std::vector < std::function<bool(const void*, const void*)> > > inst;
			return inst;
		}
		template<class T, class U>
		auto default_equal_comparison(const void* a, const void* b) -> bool
		{
			return *static_cast<const T*>(a) == *static_cast<const U*>(b);
		}
		template<class T, class U, class Fun>
		auto default_equal_comparison_with_functor(const Fun& fun, const void* a, const void* b) -> bool
		{
			return fun(*static_cast<const T*>(a), *static_cast<const U*>(b));
		}
	}


	

	/// @brief Base interface for hold_any class.
	/// 
	/// hold_any will directly inherit any_base that provides the following members: 
	///		- any_base::empty() : check for empty any
	///		- any_base::data() : returns underlying object as a void*
	///		- any_base::type() : returns the type as a TypeInfo pointer
	///		- any_base::type_id() : returns the type id (as get_type_id())
	///		- any_base::cast() : cast to template type
	/// 	- any_base::sizeof_type(): returns the size of underlying type
	/// 	- any_base::hash(): returns a hash value (based on std::hash) for underlying object
	/// 
	/// It is possible to inherit any_base in order to provide an extended interface to hold_any (see hold_any documentation for an example).
	/// 
	template<class Derived, class TypeInfo, size_t StaticSize = sizeof(void*), size_t Alignment = alignof(void*)>
	struct any_base : public detail::null_policy
	{
	protected:
		using storage_type = typename std::aligned_storage< StaticSize, Alignment>::type;
		using pointer_type = tagged_pointer<TypeInfo, CustomAlignment, 32>;

		pointer_type d_type_info;
		storage_type d_storage;

		template<class T>
		auto alloc() -> void*
		{
			static constexpr size_t size_T = sizeof(T);
			// compile time tags
			static constexpr size_t tag =
				(std::is_trivially_destructible<T>::value ? 0 : 1) |
				(std::is_trivially_copyable<T>::value ? 0 : 2) |
				(is_relocatable<T>::value ? 0 : 4) |
				(size_T > sizeof(storage_type) ? 8 : 0) |
				(std::is_pointer<T>::value ? 16 : 0);

			// create object storage with allocation if sizeof(T) > StaticSize
			void* d = &d_storage;
			if (tag & detail::big_size) {
				d = SEQ_ANY_MALLOC(size_T);
				if (d == NULL)
					throw std::bad_alloc();
				memcpy(&d_storage, &d, sizeof(void*));
			}
			// set the type tags
			d_type_info.set_tag(tag);
			return d;
		}

		void dealloc() noexcept
		{
			// dealloc (if needed) the object storage and reset tag and pointer
			if (d_type_info.full() & detail::big_size) {
				void* d;
				memcpy(&d, &d_storage, sizeof(void*));
				SEQ_ANY_FREE(d);
			}
			//clear any_type_info pointer
			d_type_info.set_full(0);
		}

		template<class T, class = typename std::enable_if<std::is_reference<T>::value, void>::type>
		auto convert(const TypeInfo*  /*unused*/, const TypeInfo*  /*unused*/, std::uintptr_t  /*unused*/) const -> T
		{
			throw std::bad_cast();
			static typename std::decay<T>::type ref;
			return ref;
		}

		template<class T, class = typename std::enable_if<!std::is_reference<T>::value, void>::type>
		auto convert(const TypeInfo* info, const TypeInfo* out_p, std::uintptr_t tags) const -> typename std::decay<T>::type
		{
			using type = typename std::decay<T>::type;

			// cast pointer to void*
			if ((std::is_same<void*, type>::value || std::is_same<const void*,type>::value) && (tags & detail::pointer))
			{
				return detail::CastPtrToVoid<type>::apply(detail::read_void_p(this->data()));
			}

			// to arithmetic conversion
			if (std::is_arithmetic<type>::value) 
			{
				if (info->type_id() <= get_type_id<long double>()) 
					return detail::CastArithmetic<type>::apply(this->data(), info);
				else if (info->type_id() == get_type_id<std::string>()) 
					return detail::CastString<type>::apply(*static_cast<const std::string*>(this->data()));
				else if (info->type_id() == get_type_id<tstring>()) 
					return detail::CastString<type>::apply(*static_cast<const tstring*>(this->data()));
				else if (info->type_id() == get_type_id<tstring_view>()) 
					return detail::CastString<type>::apply(*static_cast<const tstring_view*>(this->data()));
#ifdef SEQ_HAS_CPP_17
				else if (info->type_id() == get_type_id<std::string_view>()) 
					return detail::CastString<type>::apply(*static_cast<const std::string_view*>(this->data()));
#endif
				else if (info->type_id() == get_type_id<char*>()) 
					return detail::CastString<type>::apply(static_cast<char*>(detail::read_void_p(this->data())));
				else if (info->type_id() == get_type_id<const char*>()) 
					return detail::CastString<type>::apply(static_cast<const char*>(detail::read_void_p(this->data())));
			}

			// convert to tstring_view, only valid for string types

			if (is_generic_string_view<type>::value) 
			{
				if (info->type_id() == get_type_id<std::string>()) 
					return detail::CastStringToStringView< type, std::string>::apply(*static_cast<const std::string*>(this->data()));
				else if (info->type_id() == get_type_id<tstring>()) 
					return detail::CastStringToStringView< type, tstring>::apply(*static_cast<const tstring*>(this->data()));
				else if (info->type_id() == get_type_id<tstring_view>()) 
					return detail::CastStringToStringView< type, tstring_view>::apply(*static_cast<const tstring_view*>(this->data()));
#ifdef SEQ_HAS_CPP_17
				else if (info->type_id() == get_type_id<std::string_view>()) 
					return detail::CastStringToStringView< type, std::string_view>::apply(*static_cast<const std::string_view*>(this->data()));
#endif
				else if (info->type_id() == get_type_id<char*>()) 
					return detail::CastStringToStringView< type, char*>::apply(static_cast<char*>(detail::read_void_p(this->data())));
				else if (info->type_id() == get_type_id<const char*>()) 
					return detail::CastStringToStringView< type, const char*>::apply(static_cast<const char*>(detail::read_void_p(this->data())));
			}

			// to string type conversion
			if (is_allocated_string<type>::value)
			{
				if (info->type_id() == get_type_id<std::string>()) 
					return detail::CastStringToString<type, std::string>::apply(*static_cast<const std::string*>(this->data()));
				else if (info->type_id() == get_type_id<tstring>()) 
					return detail::CastStringToString<type, tstring>::apply(*static_cast<const tstring*>(this->data()));
				else if (info->type_id() == get_type_id<tstring_view>()) 
					return detail::CastStringToString<type, tstring_view>::apply(*static_cast<const tstring_view*>(this->data()));
#ifdef SEQ_HAS_CPP_17
				else if (info->type_id() == get_type_id<std::string_view>()) 
					return detail::CastStringToString<type, std::string_view>::apply(*static_cast<const std::string_view*>(this->data()));
#endif
				else if (info->type_id() == get_type_id<const char*>()) 
					return detail::CastStringToString<type, const char*>::apply(static_cast<const char*>(detail::read_void_p(this->data())));
				else if (info->type_id() == get_type_id< char*>()) 
					return detail::CastStringToString<type,  char*>::apply(static_cast< char*>(detail::read_void_p(this->data())));
				else if (info->type_id() <= get_type_id<long double>()) 
					return detail::CastArithmeticToString<type>::apply(this->data(), info);
			}

			// use registered conversion function
			int in_id = info->type_id();
			if(in_id >= (int) detail::get_converters().size())
				throw std::bad_cast();
			int out_id = out_p->type_id();
			const auto& converts = detail::get_converters()[in_id];//info->d_convert;
			if (out_id >= (int)converts.size() || !converts[out_id])
				throw std::bad_cast();

			type out;
			converts[out_id](this->data(), &out);
			return out;
		}

	public:

		using type_info_type = TypeInfo;

		/// @brief Returns the underlying object as a void*. Never returns a NULL pointer, even for empty object.
		SEQ_ALWAYS_INLINE auto data() const noexcept -> const void* {
			return (d_type_info.full() & detail::big_size) ? detail::read_void_p(&d_storage) : &d_storage;
		}
		/// @brief Returns the underlying object as a void*. Never returns a NULL pointer, even for empty object.
		SEQ_ALWAYS_INLINE auto data() noexcept -> void* {
			return (d_type_info.full() & detail::big_size) ? detail::read_void_p(&d_storage) : &d_storage;
		}
		/// @brief Returns the type information as a TypeInfo pointer. 
		/// Returns NULL if the object is empty.
		SEQ_ALWAYS_INLINE auto type() noexcept -> TypeInfo* {
			return d_type_info; 
		}
		/// @brief Returns the type information as a TypeInfo pointer. 
		/// Returns NULL if the object is empty.
		SEQ_ALWAYS_INLINE auto type() const noexcept -> const TypeInfo* {
			return d_type_info;
		}
		/// @brief Returns the type id for this object. 
		/// Returns 0 if the object is empty.
		SEQ_ALWAYS_INLINE auto type_id() const noexcept -> int {
			return empty() ? 0 : type()->type_id();
		}
		/// @brief Returns true if this object is empty, false otherwise.
		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool {
			return this->d_type_info.full() == 0;
		}
		/// @brief Returns the size of underlying type, 0 if empty.
		SEQ_ALWAYS_INLINE auto sizeof_type() const noexcept -> size_t {
			return empty() ? 0 : type()->sizeof_type();
		}

		/// @brief Cast any object to given type.
		/// Throw a std::bad_cast on error.
		template<class T>
		SEQ_ALWAYS_INLINE auto cast() -> T
		{
			using type = typename std::decay<T>::type;//
			const TypeInfo* info = this->type();
			if (!info)
				throw std::bad_cast();
			const TypeInfo* out_p = Derived::template get_type<type>();//find_type_info< TypeInfo, type>::find();
			if (out_p == info)
				return (*static_cast<type*>(this->data()));
			
			return convert<T>(info, out_p, d_type_info.full());
		}

		/// @brief Cast any object to given type.
		/// Throw a std::bad_cast on error.
		template<class T>
		SEQ_ALWAYS_INLINE auto cast() const -> const T
		{
			using type = typename std::decay<T>::type;
			const TypeInfo* info = this->type();
			if (this->empty())
				throw std::bad_cast();
			const TypeInfo* out_p = Derived::template get_type<type>();
			if (out_p == info)
				return (*static_cast<const type*>(this->data()));
			
			return convert<T>(info, out_p, d_type_info.full());
		}

		/// @brief Returns the hash value for underlying object based on std::hash.
		/// Might throw a std::bad_function_call.
		SEQ_ALWAYS_INLINE auto hash() const -> size_t
		{
			return empty() ? 0 : this->type()->hash_any(this->data());
		}

	};




	struct any_default_interface
	{
		using type_info = any_type_info;

		template<class T>
		struct typed_type_info : any_typed_type_info<T>
		{};

		template<class Base>
		struct any_interface : public Base
		{
			// inherited by any
		};
	};

	struct any_no_hash_interface
	{
		using type_info = any_type_info;

		template<class T>
		struct typed_type_info : any_typed_type_info<T, false>
		{
		};

		template<class Base>
		struct any_interface : public Base
		{
			// inherited by any
		};
	};



	
	/// @brief std::any like class optimized to build heterogeneous containers.
	/// @tparam Interface class Interface, default to any_default_interface.
	/// 
	/// seq::hold_any is a std::any like class optimized to create heterogeneous containers of any types (vectors, deques, hash tables, sorted containers...).
	/// 
	/// The seq library provides the following aliases:
	///		-	seq::any: equivalent to seq::hold_any<seq::any_default_interface>
	///		-	seq::nh_any: equivalent to  seq::hold_any<seq::any_no_hash_interface>
	/// 
	/// 
	/// Interface
	/// ---------
	/// 
	/// hold_any offers a similar interface to std::any and supports additional features:
	///		-	Support for comparison operators <, <=, >, >=, == and !=
	///		-	Support for standard stream operators to/from std::ostream/std::istream
	///		-	hold_any is hashable based on std::hash
	///		-	hold_any can be formatted using the \ref format "formatting" module of seq library
	///		-	hold_any interface can be extended
	/// 
	/// hold_any is very similar to boost.TypeErasure or folly.Poly, but its default implementation provides more features 
	/// in order to be used within most containers (sorted containers, hash tables,...).
	/// 
	/// These features do not modify the requirements of held types: they must be at least move only contructible.
	/// hold_any uses type erasure to provide custom behavior based on held object type, and the additional
	/// functions like streaming or comparison support are implemented if the type supports them. If not, the
	/// corresponding functions with throw a std::bad_function_call exception:
	/// 
	/// \code{.cpp}
	/// 
	/// std::cout << seq::any(2);					// works as expected, print '2'
	/// std::cout << seq::any(std::vector<bool>()); // std::vector<bool> does not define stream operator to std::ostream, throw a std::bad_function_call
	/// 
	/// \endcode
	/// 
	/// The only exception is the hashing support. Indeed, there is now way in C++11 to tell at compile time if a type is hashable or not,
	/// as most implementations of std::hash use static_assert() in the operator() member (undetectable through SFINAE). hold_any
	/// optimistically assumes that all types are hashable. In order to store non hashable types, you must specialize the seq::is_hashable type trait for this type:
	/// 
	/// \code{.cpp}
	/// 
	/// struct test{};
	/// 
	/// template<>
	/// struct seq::is_hashable<test> : std::false_type {};
	/// 
	/// //...
	/// 
	/// seq::any a = test(); // compile as is_hashable is specialized for test class
	/// 
	/// \endcode
	/// 
	/// Otherwise, you can completely disable the hashing support using seq::nh_any (equivalent to seq::hold_any<seq::any_no_hash_interface>) :
	/// 
	/// \code{.cpp}
	/// 
	/// struct test{};
	/// 
	/// seq::nh_any a = test();
	/// 
	/// \endcode
	/// 
	/// 
	/// Casting
	/// -------
	/// 
	/// hold_any can be cast to another type using either hold_any::cast() function or seq::any_cast() (similar to std::any_cast).
	/// When casting to the same type as the underlying object, it is possible to retrieve a reference to the object:
	/// 
	/// \code{.cpp}
	/// 
	/// seq::any a = 2;
	/// int &value1 = a.cast<int&>();
	/// int &value2 = seq::any_cast<int&>(a);
	/// int *value3 = seq::any_cast<int>(&a);
	/// 
	/// \endcode
	/// 
	/// hold_any supports casting to another type. By default, the following conversion are valid:
	///		-	A pointer can be casted to void* or const void*
	///		-	All arithmetic types can be casted between each other
	///		-	Arithmetic types can be casted to std::string or seq::tiny_string
	///		-	All string types can be casted between each other (std::string, seq::tstring, seq::tstring_view, std::string_view, char* and const char*)
	///		-	All string types can be casted to arithmetic types (which can throw a std::bad_cast exception if the string does not represent an arithmetic type)
	/// 
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// any a = "1.2"; // holds a const char*
	///
	/// char* c = a.cast<char*>(); // const char * to char* is valid (although not safe)
	/// void* v = a.cast<void*>(); //conversion to void* is valid (although not safe)
	///
	/// // conversion to the different string types
	/// tstring str = a.cast<tstring>();
	/// tstring_view view = a.cast<tstring_view>();
	/// std::string str2 = a.cast<std::string>();
	///
	/// // conversion to arithmetic
	/// double d = a.cast<double>(); // d holds 1.2
	/// int i = a.cast<int>(); // i holds 1
	///
	/// a = 1.2;
	/// i = a.cast<int>(); //valid, cast double to int
	/// str2 = a.cast<std::string>(); //valid, str2 holds "1.2"
	///
	/// \endcode
	/// 
	/// It is possible to register additional conversion functions for custom types using seq::register_any_conversion():
	/// 
	/// \code{.cpp}
	/// 
	/// struct my_int_pair
	/// {
	///	int a, b;
	///	my_int_pair(int a = 0, int b = 0)
	///		:a(a), b(b) {}
	///
	///	// define conversion operator to std::string
	///	operator std::string() const {
	///		std::string res;
	///		seq::fmt(a).append(res);
	///		seq::fmt(b).append(res);
	///		return res;
	///	}
	/// };
	///
	/// std::string pair_to_string(const std::pair<int, int>& p)
	/// {
	///	std::string res;
	///	seq::fmt(p.first).append(res);
	///	seq::fmt(p.second).append(res);
	///	return res;
	/// }
	///
	/// 
	/// 
	/// // register conversion that already exists 
	/// seq::register_any_conversion<my_int_pair, std::string>();
	/// // register conversion function
	/// seq::register_any_conversion<std::pair<int, int>, std::string>(pair_to_string);
	///
	/// // disable hashing interface
	/// using my_any = seq::nh_any;
	///
	/// my_any a = std::pair<int, int>(1, 2);
	/// my_any b = my_int_pair(1, 2);
	///
	/// std::cout << a.cast<std::string>() << std::endl;
	/// std::cout << b.cast<std::string>() << std::endl;
	/// 
	/// \endcode
	/// 
	/// 
	/// Hashing
	/// -------
	/// 
	/// As said previously, seq::hold_any is hashable and a specialization for std::hash is provided.
	/// This feature is mandatory when inserting any objects into hash table based containers like std::unordered_set.
	/// In order to store non hashable types (that do not specialize std::hash), you must specialize seq::is_hashable type traits for this type
	/// to have seq::is_hashable<Type>::value == false.
	/// 
	/// Another way to store non hashable types is to use seq::nh_any instead of seq::any which disable the hashing support. 
	/// Attempting to call seq::hold_any::hash() member on a non hashable type will throw a std::bad_function_call.
	/// 
	/// The specialization of std::hash for hold_any is transparent to support heterogeneous lookup.
	/// 
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// #include <iostream>
	/// #include "ordered_set.hpp"
	/// #include "tiny_string.hpp"
	/// #include "any.hpp"
	/// 
	/// //...
	/// 
	/// using namespace seq;
	/// 
	/// // build an ordered set than supports heterogeneous lookup 
	/// seq::ordered_set<any,std::hash<any> , std::equal_to<void> > set;
	/// 
	/// set.insert(3);
	/// set.insert(2.5);
	/// set.insert(tstring("hello"));
	/// set.insert(1);
	/// set.insert(std::string("world"));
	/// set.insert("ok");
	///
	/// // print the set content
	/// for (auto val : set)
	///		std::cout << val << std::endl;
	///
	/// assert(set.find(3) != set.end());			// use heterogeneous lookup 
	/// assert(set.find(2.5) != set.end());			// use heterogeneous lookup 
	/// assert(set.find("hello")  != set.end());	// use heterogeneous lookup 
	/// assert(set.find(tstring("world")) != set.end());	// use heterogeneous lookup 
	/// assert(set.find("ok") == set.end()) ;			//"ok" not found has we compare 2 const char* -> pointer comparison, not string comparison
	/// assert(set.find("no") == set.end());			//failed lookup
	/// 
	/// \endcode
	/// 
	/// 
	/// 
	/// Equality comparison
	/// -------------------
	/// 
	/// hold_any objects are equally comparable. This feature is mandatory when inserting any objects into hash table based containers like std::unordered_set.
	/// Two hold_any are considered equal if:
	///		-	They are both empty.
	///		-	They hold the same type and both underlying objects compare equals. If the type does not provide a comparison operator, the operator==() always return false.
	///		-	They both hold an arithmetic value of possibly different types, and these values compare equals.
	///		-	They both hold a string like object (std::string, seq::tstring, seq::tstring_view, std::string_view, char*, const char*) that compare equals.
	///			Note that a const char* can be compared to another string object (like std::string) using string comparison, but comparing two const char* will result in a pointer comparison!
	/// 
	/// It is possible to register a comparison function for unrelated types using seq::register_any_equal_comparison() function.
	/// 
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// seq::any a = 1.0;
	/// seq::any b = 1;
	/// 
	/// seq::any c = "hello";
	/// seq::any d = std::string("hello");
	/// 
	/// assert(a == b);  // compare any(double) and any(int)
	/// assert(a == 1.); // compare any(double) to a double
	/// assert(a == 1);   // compare any(double) to int
	/// assert(a != 1.2); // compare any(double) to a double
	/// assert(a != c); // compare any(double) to any(const char*)
	/// assert(c == d); //compare any(const char*) to any(std::string)
	/// assert(d == "hello"); // compare any(std::string) to const char*
	/// 
	/// 
	/// // define a comparison function between std::pair<int,int> and int
	/// struct equal_pair
	/// {
	///		bool operator()(const std::pair<int,int> & a, int b) const
	///		{
	///			return a.first == b && a.second == b;
	///		}
	/// };
	/// 
	/// seq::register_any_equal_comparison<std::pair<int,int>, int>(equal_pair{});
	/// 
	/// seq::nh_any pair = std::pair<int,int>(2,2);
	/// seq::nh_any integer = 2;
	/// assert(pair == integer);
	/// 
	/// 
	/// \endcode
	/// 
	/// 
	/// Less comparison
	/// ---------------
	/// 
	/// hold_any object can be compared using operators <, >, <=, >=, all based on the <i>less than</i> operator. This is mandatory when inserting any objects into a sorted container like std::set.
	/// A hold_any object A is considered less than another hold_any B if:
	///		-	A is empty and not B.
	///		-	A and B hold the same type and object(A) < object(B). If the type does not provide a less than operator, throw std::bad_function_call.
	///		-	They both hold an arithmetic value of possibly different types, and object(A) < object(B).
	///		-	They both hold a string like object (std::string, seq::tstring, seq::tstring_view, std::string_view, char*, const char*) and object(A) < object(B).
	///  
	/// It is possible to register a comparison function for unrelated types using seq::register_any_less_comparison() function.
	/// 
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// seq::any a = 1;
	/// seq::any b = 2.3;
	/// 
	/// seq::any c = "hello";
	/// seq::any d = std::string("world");
	/// 
	/// assert(a < b);  // compare any(int) and any(double)
	/// assert(a < 2.3); // compare any(int) to a double
	/// assert(a <= 1);   // compare any(int) to int
	/// assert(b >= a);   // compare any(double) to any(int)
	/// assert(c < d); //compare any(const char*) to any(std::string)
	/// assert(d > c); // compare any(std::string) to const char*
	/// 
	/// 
	/// // define a dumy comparison function between std::pair<int,int> and int
	/// struct less_pair
	/// {
	///		bool operator()(const std::pair<int,int> & a, int b) const
	///		{
	///			return a.first < b && a.second < b;
	///		}
	/// };
	/// 
	/// seq::register_any_less_comparison<std::pair<int,int>, int>(less_pair{});
	/// 
	/// seq::nh_any pair = std::pair<int,int>(1,2);
	/// seq::nh_any integer = 3;
	/// assert(pair < integer); 
	/// 
	/// \endcode 
	/// 
	/// 
	/// Small Buffer Optimization
	/// -------------------------
	/// 
	/// hold_any provides a customizable Small Buffer Optimization (SBO) in order to avoid costly memory allocation for small types.
	/// The alignment of the small buffer can also be modified.
	/// 
	/// By default, the small buffer size is equal to sizeof(double) and its alignment is alignof(double). Therefore, on 64 bits machines,
	/// sizeof(seq::any) is 16 bytes.
	/// 
	/// The small buffer size and alignment can be changed through hold_any template parameters:
	/// 
	/// \code{.cpp}
	/// 
	/// // define an any type that can hold a tstring object on the stack
	/// using my_any = seq::hold_any<seq::any_default_interface, sizeof(tstring), alignof(tstring) >;
	/// 
	/// // the tstring object is stored on the stack using the small buffer. This line does not trigger a memory allocation as tstring itself uses Small String Optimization.
	/// my_any a = tstring("hello");
	/// 
	/// \endcode 
	/// 
	/// 
	/// Type information and various optimizations
	/// ------------------------------------------
	/// 
	/// Internally, hold_any uses a tagged pointer to store 3 additional information on the held type object:
	///		-	Tells if the type is relocatable (seq::is_relocatable<Type>::value == true)
	///		-	Tells if the type is trivially copyable (std::is_trivially_copyable<Type>::value == true)
	///		-	Tells if the type is trivially destructible (std::is_trivially_destructible<Type>::value == true)
	/// 
	/// These information are used to optimize the copy, move assignment and destruction of hold_any objects.
	/// 
	/// Note that hold_any itself is NOT relocatable as it might hold a non relocatable type inside its small buffer.
	/// 
	/// 
	/// Move only types
	/// ---------------
	/// 
	/// hold_any supports holding move only types like std::unique_ptr. In this case, the hold_any will silently become a move only object,
	/// and trying to copy the hold_any object will throw a std::bad_function_call exception.
	/// Likewise, the hold_any can only be casted to a reference to this type.
	/// 
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// // create any from unique_ptr
	/// seq::any a = std::unique_ptr<int>(new int(3));
	/// // retrieve a reference to the unique_ptr
	/// std::unique_ptr<int>& val1 = a.cast< std::unique_ptr<int>&>();
	/// std::cout << *val1 << std::endl;
	///
	/// // move any object
	/// seq::any b = std::move(a);
	/// std::unique_ptr<int>& val2 = b.cast< std::unique_ptr<int>&>();
	/// std::cout << *val2 << std::endl;
	///
	/// // try to copy: throw std::bad_function_call as copy operator is disabled for unique_ptr
	/// try {
	/// 	seq::any c = b;
	/// }
	/// catch (const std::bad_function_call&)
	/// {
	/// 	std::cout << "cannot copy any object containing a unique_ptr" << std::endl;
	/// }
	/// 
	/// \endcode 
	/// 
	/// 
	/// Extending hold_any interface
	/// ----------------------------
	/// 
	/// 
	/// hold_any interface is easily extendable to provide additional members or modify its standard behavior.
	/// 
	/// Below is an example of extension that adds the info() member returning a std::type_info object and modify the hash function:
	/// 
	/// \code{.cpp}
	/// 
	/// using namespace seq;
	/// 
	/// // Define a new interface for hold_any
	/// struct NewInterface
	/// {
	/// 	// The base type info class, must inherit seq::any_type_info virtually
	/// 	struct type_info : virtual any_type_info
	/// 	{
	/// 		virtual const std::type_info& info() const = 0;
	/// 	};
	///
	/// 	// Concrete implementation for given type, must inherit type_info and any_typed_type_info<T> (for base implementation of common functions)
	/// 	template<class T>
	/// 	struct typed_type_info : type_info, any_typed_type_info<T>
	/// 	{
	/// 		// implementation of additional info() member
	/// 		virtual const std::type_info& info() const { return typeid(T); }
	/// 
	/// 		// new implementation for the hash function based on the old one
	/// 		virtual size_t hash_any(const void* in) const {
	/// 			return any_typed_type_info<T>{}.hash_any(in)* UINT64_C(0xc4ceb9fe1a85ec53);
	/// 		}
	/// 	};
	/// 
	/// 	// Extended interface to seq::hold_any
	/// 	template<class Base>
	/// 	struct any_interface : Base
	/// 	{
	/// 		//add function to hold_any interface
	/// 		const std::type_info& info() const { return this->type()->info(); }
	/// 	};
	/// };
	/// 
	/// 
	/// using my_any = hold_any< NewInterface>;
	/// 
	/// my_any a  = 2;
	/// 
	/// // print the type name
	/// std::cout<< a.info().name() << std::endl;
	/// // print the hash value
	/// std::cout<< a.hash() << std::endl;
	/// 
	/// 
	/// \endcode 
	/// 
	/// 
	/// Below is a more complex example that transform hold_any into a std::function equivalent:
	/// 
	/// \code{.cpp}
	/// 
	/// // for std::plus and std::multiplies (C++14)
	/// #include <functional>
	/// 
	/// #include "any.hpp"
	/// // for seq::is_invocable
	/// #include "type_traits.hpp"
	/// // for seq::constexpr_if
	/// #include "utils.hpp"
	/// 
	/// namespace seq
	/// {
	/// 	template<class Fun>
	/// 	struct FunInterface;
	///
	/// 	// Provide custom interface to hold_any in order to be callable like std::function
	/// 	template<class R, class... As>
	/// 	struct FunInterface< R(As...)>
	/// 	{
	/// 		// The base type info class, must inherit seq::any_type_info virtually.
	/// 		struct type_info : virtual any_type_info
	/// 		{
	/// 			// Add virtual members() call and target_type()
	/// 			virtual R call(const void* data, As... as) const = 0;
	/// 			virtual const std::type_info& target_type() const = 0;
	/// 		};
	/// 
	/// 		// Concrete implementation for given type, must inherit type_info and any_typed_type_info<T> (for base implementation of common functions)
	/// 		template<class T>
	/// 		struct typed_type_info : type_info, any_typed_type_info<T, false> //disable hashing support
	/// 		{
	/// 			virtual R call(const void* data, As... as) const
	/// 			{
	/// 				// C++11 emulation of if constexpr
	/// 				// Make sure that this interface is still suitable for non invocable types
	/// 				return constexpr_if<is_invocable<T, As ...>::value>(
	/// 					[&as...](const auto& fun) {return fun(std::forward<As>(as)...); },
	/// 					[](const auto&) {throw std::bad_function_call(); return R(); },
	/// 					* static_cast<const T*>(data));
	/// 			}
	/// 			virtual const std::type_info& target_type() const
	/// 			{
	/// 				return typeid(T);
	/// 			}
	/// 		};
	///
	/// 		// Extended interface to seq::hold_any
	/// 		template<class Base>
	/// 		struct any_interface : Base
	/// 		{
	/// 			//add operator() to hold_any interface
	/// 			R operator()(As... as) const
	/// 			{
	/// 				if (this->empty())
	/// 					throw std::bad_function_call();
	/// 				return this->type()->call(this->data(), std::forward<As>(as)...);
	/// 			}
	/// 			//add member target_type() to hold_any interface
	/// 			const std::type_info& target_type() const
	/// 			{
	/// 				return this->empty() ? typeid(void) : this->type()->target_type();
	/// 			}
	/// 		};
	/// 	};
	///
	///		// Define the seq::function type
	/// 	template<class Signature>
	/// 	using function = hold_any< FunInterface< Signature> >;
	///
	/// }
	/// 
	/// 
	/// // dumy function 
	/// int divide(int a, int b) {return a / b;}
	/// 
	/// 
	/// 
	/// // usage
	/// 
	/// seq::function<int(int,int)> plus_fun = std::plus<int>{};					// affect functor
	/// seq::function<int(int,int)> minus_fun = [](int a, int b){return a - b;}	;	// affect lambda
	/// seq::function<int(int,int)> multiplies_fun = std::multiplies<int>{};		// affect functor
	/// seq::function<int(int,int)> divide_fun = divide;							// affect function
	///
	/// assert( plus_fun(1,2) == 3);
	/// assert( minus_fun(2,1) == 1);
	/// assert( multiplies_fun(2,3) == 6);
	/// assert( divide_fun(9,3) == 3);
	/// 
	/// 
	/// \endcode 
	/// 
	template<class Interface = any_default_interface, size_t StaticSize = sizeof(double), size_t Alignment = alignof(double) >
	class hold_any : public Interface::template any_interface< any_base <hold_any<Interface, StaticSize, Alignment>,  typename Interface::type_info, StaticSize, Alignment> >
	{

		SEQ_ALWAYS_INLINE auto complex_destroy() const noexcept -> bool { return this->d_type_info.full() & detail::complex_destroy; }
		SEQ_ALWAYS_INLINE auto complex_copy() const noexcept -> bool { return this->d_type_info.full() & detail::complex_copy; }
		SEQ_ALWAYS_INLINE auto big_size() const noexcept -> bool { return this->d_type_info.full() & detail::big_size; }
		SEQ_ALWAYS_INLINE auto non_relocatable() const noexcept -> bool { return this->d_type_info.full() & detail::non_relocatable; }

		template<class T>
		auto get_null_out() -> T* {
			static T t;
			return &t;
		}

		template< class ValueType, class... Args >
		auto emplace_args(Args&&... args) -> typename std::decay<ValueType>::type&
		{
			using type = typename std::decay<ValueType>::type;

			auto* i_t = this->type();
			auto* o_t = get_type<type>();
			type* out = NULL;
			if (o_t == i_t) {
				// same type
				out = static_cast<type*>(this->data());
				*out = type(std::forward<Args>(args)...);

			}
			else {
				// different types, clear this object
				reset();
				// set type info pointer
				this->d_type_info = o_t;
				void* d = NULL;
				try {
					// create memory chunk and tags, might throw
					d = this->template alloc<type>();
					// copy, might throw
					out = new (d) type(std::forward<Args>(args)...);
				}
				catch (...) {
					// free memory chunk if needed
					if (d && sizeof(type) > static_size)
						SEQ_ANY_FREE(d);
					// reset type info pointer and tags
					this->d_type_info.set_full(0);
					throw;
					// just to remove cppcheck warning
					out = get_null_out<type>();
				}
			}

			return *out;
		}

	public:

		using base_type = typename Interface::template any_interface< any_base <hold_any<Interface, StaticSize, Alignment>, typename Interface::type_info, StaticSize, Alignment> >;

		using type_info_type = typename Interface::type_info;
		template<class T>
		using typed_type_info_type =  typename Interface::template typed_type_info<T>;

		/// @brief Returns the type info for given type
		template<class T>
		static auto get_type() -> type_info_type* {
			struct Holder
			{
				typed_type_info_type<T> instance;
				Holder() {
					instance.d_type_id = get_type_id<T>();
				}
			};
			static Holder h;
			return &h.instance;
		}
		
		// max size for Small Buffer Optimization
		static constexpr size_t static_size = sizeof(hold_any::d_storage);


		hold_any()
			:base_type()
		{}

		hold_any(const hold_any& other)
			:base_type()
		{
			//check non empty
			if (!other.big_size() && !other.complex_copy()) {
				// small POD types
				memcpy(&this->d_storage, &other.d_storage, sizeof(this->d_storage));
			}
			else {
				//copy data, might throw
				other.type()->copy_any(other.type(), other.data(), NULL, &this->d_storage, static_size);
			}
			//copy type info
			this->d_type_info = other.d_type_info;
		}


		hold_any(hold_any&& other) noexcept
  			:base_type()
		{
			if (other.big_size() || !other.non_relocatable()) {
				// for big types or relocatable types, simple memcpy and reset other tags
				memcpy(&this->d_storage, &other.d_storage, static_size);
				this->d_type_info = other.d_type_info;
				other.d_type_info.set_full(0);
			}
			else {
				//move data, might throw
				other.type()->move_any(other.type(), other.data(), NULL, &this->d_storage, (static_size));
				// copy tag and info pointer on success
				this->d_type_info = other.d_type_info;
			}
		}

		template<class T, class = typename std::enable_if< !std::is_base_of<detail::null_policy, typename std::decay<T>::type>::value, void>::type >
		hold_any(T&& value)
			:base_type()
		{
			// remove const ref to T
			using type = typename std::decay<T>::type;
			// set the type info pointer
			this->d_type_info = get_type<type>();

			SEQ_ASSERT_DEBUG(this->d_type_info != NULL, "NULL type info");

			void* d = NULL;
			try {
				// returns (and potentially allocate) memory storage, might throw
				d = this->template alloc<type>();
				// construct, might throw
				new (d) type(std::forward<T>(value));
			}
			catch (...) {
				// free memory if needed
				if (d && sizeof(type) > static_size)
					SEQ_ANY_FREE(d);
				// reset type info
				this->d_type_info.set_full(0);
				throw;
			}

		}


		~hold_any()
		{
			reset();
		}

		auto operator=(const hold_any& other) -> hold_any&
		{
			const auto* i_t = this->type();
			const auto* o_t = other.type();

			if (i_t != o_t) {
				// reset this object content if its type is different than other's type
				reset();
				i_t = NULL;
			}

			if (!other.big_size() && !other.complex_copy()) {
				// copy small POD type
				memcpy(&this->d_storage, &other.d_storage, static_size);
			}
			else {
				// copy any type, might throw
				o_t->copy_any(o_t, other.data(), i_t, &this->d_storage, static_size);
			}
			this->d_type_info = other.d_type_info;

			return *this;
		}

		auto operator=(hold_any&& other) noexcept -> hold_any&
  		{
			if (other.empty()) {
				// move empty object
				reset();
				return *this;
			}
			const auto* i_t = this->type();
			const auto* o_t = other.type();

			if (i_t == o_t) {
				// same non null type
				if (!other.non_relocatable()) {
					//relocatable: just swap storage
					std::swap(this->d_storage, other.d_storage);
				}
				else {
					// move, might throw
					o_t->move_any(o_t, other.data(), i_t, &this->d_storage, static_size);
				}
			}
			else {
				// different type, clear this object
				reset();
				// move, might throw
				o_t->move_any(o_t, other.data(), NULL, &this->d_storage, static_size);
				// copy type info with tags
				this->d_type_info = other.d_type_info;
			}
			return *this;
		}

		template<class T, class = typename std::enable_if< !std::is_base_of<detail::null_policy, typename std::decay<T>::type>::value, void>::type >
		auto operator=(T&& value) -> hold_any&
		{
			using type = typename std::decay<T>::type;
			auto* i_t = this->type();
			auto* o_t = get_type<type>();

			if (o_t == i_t) {
				// same type
				*static_cast<type*>(this->data()) = std::forward<T>(value);
			}
			else {
				// different types, clear this object
				reset();
				// set type info pointer
				this->d_type_info = o_t;
				void* d = NULL;
				try {
					// create memory chunk and tags, might throw
					d = this->template alloc<type>();
					// copy, might throw
					new (d) type(std::forward<T>(value));
				}
				catch (...) {
					// free memory chunk if needed
					if (d && sizeof(type) > static_size)
						SEQ_ANY_FREE(d);
					// reset type info pointer and tags
					this->d_type_info.set_full(0);
					throw;
				}
			}

			return *this;
		}

		template< class ValueType, class... Args >
		auto emplace(Args&&... args) -> typename std::decay<ValueType>::type &
		{
			return emplace_args<ValueType>(std::forward<Args>(args)...);
		}

		template< class ValueType, class U, class... Args >
		auto emplace(std::initializer_list<U> il, Args&&... args) -> typename std::decay<ValueType>::type&
		{
			return emplace_args<ValueType>(il, std::forward<Args>(args)...);
		}


		auto has_value() const noexcept -> bool
		{
			return !this->empty();
		}

		explicit operator bool() const noexcept
		{
			return has_value();
		}

		void swap(hold_any& other) 
		{
			// swap cannot be noexcept as there is no guarantee that move assignment of underlying types are noexcept

			if ((!non_relocatable() && !other.non_relocatable()) || (big_size() && other.big_size()))
			{
				// 2 relocatable types or big types: just swap storage and type info
				std::swap(this->d_type_info, other.d_type_info);
				std::swap(this->d_storage, other.d_storage);
			}
			else
			{
				hold_any tmp = std::move(other);
				other = std::move(*this);
				*this = std::move(tmp);
			}
		}

		void reset() noexcept
		{
			// check for non empty
			if (!this->empty()) {
				if (complex_destroy())
					// call object destructor
					this->type()->destroy_any(this->data());
				// dealloc memory if required
				this->dealloc();
			}
		}

		template<class T>
		auto equal_to ( T && other) const -> bool
		{
			using type = typename std::decay<T>::type;

			// one empty: return false
			if (this->empty())
				return false;

			const type_info_type* otype = get_type<type>();

			// same type: use equal_any
			if (this->type() == otype)
				return this->type()->equal_any(this->data(), &other);

			int a_id = this->type_id();

			
			// arithmetic comparison
			if (is_arithmetic_type(a_id) && std::is_arithmetic<type>::value) {
				return constexpr_if< std::is_arithmetic<type>::value >(
					[](auto a, auto b) {return a == b; },
					[](auto , auto ) {return false; },
					this->template cast<long double>(), std::forward<T>(other));
			}

			// string comparison
			if (is_string_type(a_id) && is_generic_string<type>::value ) {
				return detail::CompareEqualString<type>::equal(this->template cast<tstring_view>(), std::forward<T>(other));
			}

			// use registered comparison
			if (a_id >= (int)detail::get_equal_comparison().size())
				return false;
			const auto& converts = detail::get_equal_comparison()[a_id];//info->d_convert;
			int b_id = get_type_id<type>();
			if (b_id >= (int)converts.size() || !converts[b_id])
				return false;
			return converts[b_id](this->data(), &other);
		}

		template<class T>
		auto less_than(T&& other) const -> bool
		{
			using type = typename std::decay<T>::type;

			if (this->empty())
				return true;

			const type_info_type* otype = get_type<type>();

			// same type: use less_any
			if (this->type() == otype)
				return otype->less_any(this->data(), &other);

			int a_id = this->type_id();

			// arithmetic comparison
			if (is_arithmetic_type(a_id) && std::is_arithmetic<type>::value) {
				if (is_integral_type(a_id) && std::is_integral<type>::value) {
					if (is_signed_integral_type(a_id) != std::is_signed<type>::value || is_unsigned_integral_type(a_id)) //different sign: convert to unsigned integral
						return detail::CompareLessArithmetic<type,unsigned long long>::less( this->template cast<unsigned long long>() , other);
					else
						return detail::CompareLessArithmetic<type, long long>::less(this->template cast<long long>(), other);
				}
				return detail::CompareLessArithmetic<type, long double>::less(this->template cast<long double>(), other);
			}

			// string comparison
			if (is_string_type(a_id) && is_generic_string<type>::value ) {
				return detail::CompareLessString<type>::less(this->template cast<tstring_view>(), other);
			}

			// use registered comparison
			int b_id = get_type_id<type>();
			if (a_id >= (int)detail::get_less_comparison().size())
				return a_id < b_id;
			const auto& converts = detail::get_less_comparison()[a_id];//info->d_convert;
			if (b_id >= (int)converts.size() || !converts[b_id])
				return a_id < b_id;
			return converts[b_id](this->data(), &other);
		}

		template<class T>
		auto greater_than(T&& other) const -> bool
		{
			using type = typename std::decay<T>::type;

			if (this->empty())
				return true;

			const type_info_type* otype = get_type<type>();

			// same type: use less_any
			if (this->type() == otype)
				return otype->less_any(&other, this->data());

			int a_id = this->type_id();

			// arithmetic comparison
			if (is_arithmetic_type(a_id) && std::is_arithmetic<type>::value) {
				if (is_integral_type(a_id) && std::is_integral<type>::value) {
					if (is_signed_integral_type(a_id) != std::is_signed<type>::value || is_unsigned_integral_type(a_id)) //different sign: convert to unsigned integral
						return detail::CompareGreaterArithmetic<type, unsigned long long>::greater(this->template cast<unsigned long long>(), other);
					else
						return detail::CompareGreaterArithmetic<type, long long>::greater(this->template cast<long long>(), other);
				}
				return detail::CompareGreaterArithmetic<type, long double>::greater(this->template cast<long double>(), other);
			}

			// string comparison
			if (is_string_type(a_id) && is_generic_string<type>::value ) {
				return detail::CompareGreaterString<type>::greater(this->template cast<tstring_view>(), other);
			}

			// use registered comparison
			int b_id = get_type_id<type>();
			if (b_id >= (int)detail::get_less_comparison().size())
				return b_id < a_id;
			const auto& converts = detail::get_less_comparison()[b_id];//info->d_convert;
			if (a_id >= (int)converts.size() || !converts[a_id])
				return b_id < a_id;
			return converts[a_id](&other, this->data());
		}

		auto operator == (const hold_any & other) const -> bool
		{
			const bool a_empty = this->empty();
			const bool b_empty = other.empty();

			// both empty: equals
			if (a_empty && b_empty)
				return true;
			// one empty: return false
			if (a_empty != b_empty)
				return false;

			// same type: use equal_any
			if (this->type() == other.type() )
				return this->type()->equal_any(this->data(), other.data());

			int a_id = this->type_id();
			int b_id = other.type_id();

			// arithmetic comparison
			if (is_arithmetic_type(a_id) && is_arithmetic_type(b_id)) {
				return this->template cast<long double>() == other.template cast<long double>();
			}

			// string comparison
			if (is_string_type(a_id) && is_string_type(b_id)) {
				return this->template cast<tstring_view>() == other.template cast<tstring_view>();
			}

			// use registered comparison
			if (a_id >= (int)detail::get_equal_comparison().size())
				return false;
			const auto& converts = detail::get_equal_comparison()[a_id];//info->d_convert;
			if (b_id >= (int)converts.size() || !converts[b_id])
				return false;
			return converts[b_id](this->data(),other.data());
		}
		auto operator != (const hold_any& other) const -> bool
		{
			return !(*this == other);
		}
		auto operator < (const hold_any& other) const -> bool
		{
			const bool a_empty = this->empty();
			const bool b_empty = other.empty();

			// both empty
			if (a_empty && b_empty)
				return false;
			// one empty: return true if this is empty, false otherwise
			if (a_empty != b_empty)
				return a_empty;

			// same type: use less_any
			if (this->type() == other.type() )
				return this->type()->less_any(this->data(), other.data());

			int a_id = this->type_id();
			int b_id = other.type_id();

			// arithmetic comparison
			if (is_arithmetic_type(a_id) && is_arithmetic_type(b_id)) {
				if (is_integral_type(a_id) && is_integral_type(b_id)) {
					if(is_signed_integral_type(a_id) != is_signed_integral_type(b_id) || is_unsigned_integral_type(a_id)) //different sign: convert to unsigned integral
						return this->template cast<unsigned long long>() < other.template cast<unsigned long long>();
					else 
						return this->template cast<long long>() < other.template cast<long long>();
				}
				return this->template cast<long double>() < other.template cast<long double>();
			}

			// string comparison
			if (is_string_type(a_id) && is_string_type(b_id)) {
				return this->template cast<tstring_view>() < other.template cast<tstring_view>();
			}

			// use registered comparison
			if (a_id >= (int) detail::get_less_comparison().size())
				return a_id < b_id;
			const auto& converts = detail::get_less_comparison()[a_id];//info->d_convert;
			if (b_id >= (int)converts.size() || !converts[b_id])
				return a_id < b_id;
			return converts[b_id](this->data(), other.data());
		}
		auto operator <= (const hold_any& other) const -> bool
		{
			return !(other < *this);
		}
		auto operator > (const hold_any& other) const -> bool
		{
			return other < *this;
		}
		auto operator >= (const hold_any& other) const -> bool
		{
			return !(*this < other);
		}

	};


	template<class T, class U>
	void register_any_conversion()
	{
		const int in_id = get_type_id<T>();
		const int out_id = get_type_id<U>();
		if ((int)detail::get_converters().size() <= in_id)
			detail::get_converters().resize((size_t)(in_id + 1));
		auto& converts = detail::get_converters()[in_id];
		if ((int)converts.size() <= out_id)
			converts.resize((size_t)(out_id + 1));
		converts[out_id] = std::function<void(const void*, void*)>(detail::default_convert<T, U>);
	}

	template<class T, class U, class Fun>
	void register_any_conversion(Fun  fun)
	{
		const int in_id = get_type_id<T>();
		const int out_id = get_type_id<U>();
		if ((int)detail::get_converters().size() <= in_id)
			detail::get_converters().resize((size_t)(in_id + 1));
		auto& converts = detail::get_converters()[in_id];
		if ((int)converts.size() <= out_id)
			converts.resize((size_t)(out_id + 1));
		converts[out_id] = std::function<void(const void*, void*)>(std::bind(detail::default_convert_with_functor<T, U, Fun>, fun, std::placeholders::_1, std::placeholders::_2));
	}


	template<class T, class U>
	void register_any_less_comparison()
	{
		const int in_id = get_type_id<T>();
		const int out_id = get_type_id<U>();
		if ((int)detail::get_less_comparison().size() <= in_id)
			detail::get_less_comparison().resize((size_t)(in_id + 1));
		auto& converts = detail::get_less_comparison()[in_id];
		if ((int)converts.size() <= out_id)
			converts.resize((size_t)(out_id + 1));
		converts[out_id] = std::function<bool(const void*, const void*)>(detail::default_less_comparison<T, U>);
	}

	template<class T, class U, class Fun>
	void register_any_less_comparison(Fun  fun)
	{
		const int in_id = get_type_id<T>();
		const int out_id = get_type_id<U>();
		if ((int)detail::get_less_comparison().size() <= in_id)
			detail::get_less_comparison().resize((size_t)(in_id + 1));
		auto& converts = detail::get_less_comparison()[in_id];
		if ((int)converts.size() <= out_id)
			converts.resize((size_t)(out_id + 1));
		converts[out_id] = std::function<bool(const void*, const void*)>(std::bind(detail::default_less_comparison_with_functor<T, U, Fun>, fun, std::placeholders::_1, std::placeholders::_2));
	}



	template<class T, class U>
	void register_any_equal_comparison()
	{
		const int in_id = get_type_id<T>();
		const int out_id = get_type_id<U>();
		if ((int)detail::get_equal_comparison().size() <= in_id)
			detail::get_equal_comparison().resize((size_t)(in_id + 1));
		auto& converts = detail::get_equal_comparison()[in_id];
		if ((int)converts.size() <= out_id)
			converts.resize((size_t)(out_id + 1));
		converts[out_id] = std::function<bool(const void*, const void*)>(detail::default_equal_comparison<T, U>);
	}

	template<class T, class U, class Fun>
	void register_any_equal_comparison(Fun  fun)
	{
		const int in_id = get_type_id<T>();
		const int out_id = get_type_id<U>();
		if ((int)detail::get_equal_comparison().size() <= in_id)
			detail::get_equal_comparison().resize((size_t)(in_id + 1));
		auto& converts = detail::get_equal_comparison()[in_id];
		if ((int)converts.size() <= out_id)
			converts.resize((size_t)(out_id + 1));
		converts[out_id] = std::function<bool(const void*, const void*)>(std::bind(detail::default_equal_comparison_with_functor<T, U, Fun>, fun, std::placeholders::_1, std::placeholders::_2));
	}



	// Additional == operators, mandatory for heterogeneous lookup in hash table

	template<class Interface, size_t S, size_t A, class T>
	auto operator == (const hold_any<Interface,S,A>& a, const T& b) -> bool
	{
		return a.equal_to(b);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator == (const T& b, const hold_any<Interface,S,A>& a) -> bool
	{
		return a.equal_to(b);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator != (const hold_any<Interface,S,A>& a, const T& b) -> bool
	{
		return !a.equal_to(b);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator != (const T& b, const hold_any<Interface,S,A>& a) -> bool
	{
		return !a.equal_to(b);
	}

	template<class Interface, size_t S, size_t A, class T>
	auto operator < (const hold_any<Interface,S,A>& a, const T& b) -> bool
	{
		return a.less_than(b);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator > (const hold_any<Interface,S,A>& a, const T& b) -> bool
	{
		return a.greater_than(b);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator <= (const hold_any<Interface,S,A>& a, const T& b) -> bool
	{
		return !(b < a);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator >= (const hold_any<Interface,S,A>& a, const T& b) -> bool
	{
		return !(a < b);
	}

	template<class Interface, size_t S, size_t A, class T>
	auto operator < (const T& a, const hold_any<Interface,S,A>& b) -> bool
	{
		return b.greater_than(a);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator > (const T& a, const hold_any<Interface,S,A>& b) -> bool
	{
		return b.less_than(a);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator <= (const T& a, const hold_any<Interface,S,A>& b) -> bool
	{
		return !(b < a);
	}
	template<class Interface, size_t S, size_t A, class T>
	auto operator >= (const T& a, const hold_any<Interface,S,A>& b) -> bool
	{
		return !(a < b);
	}

	template<class Interface, size_t S, size_t A>
	auto operator << (std::ostream& oss, const hold_any< Interface,S,A> & a) -> std::ostream& 
	{
		if (a.empty()) 
			throw std::bad_function_call();
		a.type()->ostream_any(a.data(), oss);
		return oss;
	}

	template<class Interface, size_t S, size_t A>
	auto operator >> (std::istream& iss, hold_any< Interface,S,A>& a) -> std::istream&
	{
		if (a.empty()) 
			throw std::bad_function_call();
		a.type()->istream_any(a.data(), iss);
		return iss;
	}

	template<class T, class Interface, size_t S, size_t A>
	auto any_cast(const hold_any<Interface,S,A>& operand) -> T
	{
		return operand.template cast<T>();
	}

	template<class T, class Interface, size_t S, size_t A>
	auto any_cast( hold_any<Interface,S,A>& operand) -> T
	{
		return operand.template cast<T>();
	}

	template<class T, class Interface, size_t S, size_t A>
	auto any_cast(const hold_any<Interface,S,A>* operand) noexcept -> const T*
	{
		if (operand->type() == hold_any<Interface>::template get_type<T>())
			return static_cast<const T*>(operand->data());
		return NULL;
	}

	template<class T, class Interface, size_t S, size_t A>
	auto any_cast( hold_any<Interface,S,A>* operand) noexcept -> T*
	{
		if (operand->type() == hold_any<Interface>::template get_type<T>())
			return static_cast<T*>(operand->data());
		return NULL;
	}

	template< class Any, class T, class... Args >
	auto make_any(Args&&... args) -> Any
	{
		Any res;
		res.template emplace<T>(std::forward<Args>(args)...);
		return res;
	}

	template< class Any, class T, class U, class... Args >
	auto make_any(std::initializer_list<U> il, Args&&... args) -> Any
	{
		Any res;
		res.template emplace<T>(il, std::forward<Args>(args)...);
		return res;
	}
	

	template<class Interface, size_t S, size_t A>
	class ostream_format<hold_any<Interface,S,A> > : 
		public base_ostream_format<hold_any<Interface,S,A>, ostream_format<hold_any<Interface,S,A> > >
	{
		using base_type = base_ostream_format<hold_any<Interface, S, A>, ostream_format<hold_any<Interface, S, A> > >;
	public:

		ostream_format() : base_type() {}
		explicit ostream_format(const hold_any<Interface,S,A>& v) : base_type(v) {}

		auto to_string(std::string& out) const -> size_t
		{
			size_t prev = out.size();
			this->value().type()->format_any(out, this->value().data(), this->width_fmt(), this->numeric_fmt());
			return out.size() - prev;
		}
	};

	using any = hold_any<>;
	using nh_any = hold_any<any_no_hash_interface>;
}

namespace std
{
	// swap overload for hold_any
	template<class Interface, size_t S, size_t A>
	void swap(seq::hold_any<Interface,S,A>& a, seq::hold_any<Interface,S,A>& b)
	{
		a.swap(b);
	}

	// specialization of std hash for hold_any
	template<class Interface, size_t S, size_t A>
	class hash< seq::hold_any<Interface,S,A> >
	{
	public:
		using is_transparent = void;

		auto operator()(const seq::hold_any<Interface,S,A>& a) const -> size_t
		{
			return a.hash();
		}

		template<class T>
		auto operator()(const T* value) const -> size_t
		{
			using type = typename std::decay<T>::type;
			// For any type, use the type info for this type and call hash_any
			const auto* type_inf = seq::hold_any<Interface,S,A>::template get_type<type*>();
			std::uintptr_t p = (std::uintptr_t)value;
			return type_inf->hash_any(&p);
		}

		template<class T>
		auto operator()( const T &value) const -> size_t
		{
			using type = T;
			// For any type, use the type info for this type and call hash_any
			const auto * type_inf = seq::hold_any<Interface,S,A>::template get_type<type>();
			return type_inf->hash_any(&value);
		}
	};
}




/*
#include <boost/any.hpp>
#include "testing.hpp"

template<class Any, class T>
void test_from_value(int repeat, const T& v)
{
	std::vector<Any> vec; vec.reserve(repeat);

	size_t st = seq::detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		vec.emplace_back(v);
	size_t el = seq::detail::msecs_since_epoch() - st;
	std::cout << "create " << typeid(Any).name() << " from " << typeid(T).name() << ": " << el << " ms" << std::endl;
}

template<class Any>
void test_from_any(int repeat, const Any& v)
{
	std::vector<Any> vec; vec.reserve(repeat);

	size_t st = seq::detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		vec.emplace_back(v);
	size_t el = seq::detail::msecs_since_epoch() - st;
	std::cout << "create " << typeid(Any).name() << " from any: " << el << " ms" << std::endl;
}


template<class Any>
void test_from_move(int repeat, const Any& v)
{
	std::vector<Any> vec; vec.reserve(repeat);
	for (int i = 0; i < repeat; ++i)
		vec.emplace_back(v);
	std::vector<Any> vec2; vec2.reserve(repeat);

	size_t st = seq::detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		vec2.emplace_back(std::move(vec[i]));
	size_t el = seq::detail::msecs_since_epoch() - st;
	std::cout << "create " << typeid(Any).name() << " from move: " << el << " ms" << std::endl;
}



template<class T>
void test_cast_same_type_seq(int repeat, const T& v)
{
	std::vector<seq::any> vec; vec.reserve(repeat);
	for (int i = 0; i < repeat; ++i)
		vec.emplace_back(v);

	T tmp;
	size_t st = seq::detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		tmp = vec[i].cast<T>();
	size_t el = seq::detail::msecs_since_epoch() - st;
	std::cout << "cast seq::any to " << typeid(T).name() << ": " << el << " ms" << "      " << tmp << std::endl;
}
template<class T>
void test_cast_same_type_boost(int repeat, const T& v)
{
	std::vector<boost::any> vec; vec.reserve(repeat);
	for (int i = 0; i < repeat; ++i)
		vec.emplace_back(v);

	T tmp;
	size_t st = seq::detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		tmp = boost::any_cast<T>(vec[i]);
	size_t el = seq::detail::msecs_since_epoch() - st;
	std::cout << "cast boost::any to " << typeid(T).name() << ": " << el << " ms" << "      " << tmp << std::endl;
}


template<class Any, class T>
void test_copy_to_null(int repeat, const T& v)
{
	std::vector<Any> vec(repeat);

	size_t st = seq::detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		vec[i] = (v);
	size_t el = seq::detail::msecs_since_epoch() - st;
	std::cout << "copy to null " << typeid(Any).name() << " a " << typeid(T).name() << ": " << el << " ms" << std::endl;
}

template<class Any, class T>
void test_copy_to_same(int repeat, const T& v)
{
	std::vector<Any> vec(repeat, v);

	size_t st = seq::detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		vec[i] = (v);
	size_t el = seq::detail::msecs_since_epoch() - st;
	std::cout << "copy to same " << typeid(Any).name() << " a " << typeid(T).name() << ": " << el << " ms" << std::endl;
}

#include "ordered_map.hpp"
#include <unordered_set>

inline void test_short_string_no_SBO_hashtable(int repeat)
{
	std::vector<tstring> strs;
	for (int i = 0; i < repeat; ++i)
		strs.push_back(generate_random_string<tstring>(14, true));

	seq::ordered_set<any, std::hash<any>, std::equal_to<void> > set;
	size_t st = detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		set.insert(strs[i]);
	size_t el = detail::msecs_since_epoch() - st;
	std::cout << "insert short string no SBO in hash set: " << el << "ms " << set.size() << std::endl;

	//test find
	int sum = 0;
	st = detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		sum += set.find(strs[i])->sizeof_type();
	el = detail::msecs_since_epoch() - st;
	std::cout << "find short string no SBO in hash set: " << el << "ms " << sum << std::endl;
}
inline void test_short_string_SBO_hashtable(int repeat)
{
	using my_any = any;//hold_any<any_base<seq::any_type_info, sizeof(tstring)> >;
	std::vector<tstring> strs;
	for (int i = 0; i < repeat; ++i)
		strs.push_back(generate_random_string<tstring>(14, true));

	seq::ordered_set<my_any, std::hash<my_any>, std::equal_to<void> > set;
	size_t st = detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		set.insert(strs[i]);
	size_t el = detail::msecs_since_epoch() - st;
	std::cout << "insert short string SBO in hash set: " << el << "ms " << set.size() << std::endl;

	//test find
	int sum = 0;
	st = detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		sum += set.find(strs[i])->sizeof_type();
	el = detail::msecs_since_epoch() - st;
	std::cout << "find short string SBO in hash set: " << el << "ms " << sum << std::endl;
}
inline void test_short_string_hashtable(int repeat)
{
	using my_any = any;// hold_any<any_base<seq::any_type_info, sizeof(tstring)> >;
	std::vector<tstring> strs;
	for (int i = 0; i < repeat; ++i)
		strs.push_back(generate_random_string<tstring>(14, true));

	seq::ordered_set<tstring > set;
	size_t st = detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		set.insert(strs[i]);
	size_t el = detail::msecs_since_epoch() - st;
	std::cout << "insert short string in hash set: " << el << "ms " << set.size() << std::endl;

	//test find
	int sum = 0;
	st = detail::msecs_since_epoch();
	for (int i = 0; i < repeat; ++i)
		sum += set.find(strs[i])->size();
	el = detail::msecs_since_epoch() - st;
	std::cout << "find short string in hash set: " << el << "ms " << sum << std::endl;
}
*/
#endif
