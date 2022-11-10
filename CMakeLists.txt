cmake_minimum_required(VERSION 3.1)

project(seq VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

# project options
option(HEADER_ONLY "Do not build library, use header-only version" OFF)
option(ENABLE_AVX2 "Enable AVX2 support" ON)


# add sources if HEADER_ONLY is false
if(NOT HEADER_ONLY)

	# add sources
	add_library(seq SHARED
		"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/block_codec.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/lz4small.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/transpose.cpp"
	)
	
	# add preprocessor definitions
	add_definitions(-DSEQ_BUILD_LIBRARY)
	#if(BUILD_STATIC_LIB)
	#	message("Build seq static library")
	#	add_definitions(-DSEQ_STATIC_BUILD)
	#endif()
	
	#add linker options
	if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# mingw
		target_link_options(seq PRIVATE -lKernel32 -lpsapi -lBcrypt)
	endif()

	# add sse/avx flags
	if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		if(NOT ENABLE_AVX2)
			target_compile_options(seq PRIVATE /arch:SSE2)
		else()
			target_compile_options(seq PRIVATE /arch:AVX2)
		endif()
	else()
		target_compile_options(seq PRIVATE -march=native)
	endif()
	
	target_include_directories(seq PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq>"
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq/internal>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
						   
else()
	add_library(seq INTERFACE)
	add_definitions(-DSEQ_HEADER_ONLY)
	target_include_directories(seq INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq>"
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq/internal>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
endif()





list(APPEND headers "${CMAKE_CURRENT_SOURCE_DIR}/seq/any.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/bits.hpp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/seq/charconv.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/cvector.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/deque_allocator.hpp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/seq/devector.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/flat_map.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/format.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/hash.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/memory.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/ordered_map.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/pdqsort.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/seq.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/sequence.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/tagged_pointer.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/testing.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/tiered_vector.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/tiny_string.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/type_traits.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/utils.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/block_codec.h"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/lz4small.h"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/transpose.h"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/shuffle_table.h"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/unshuffle_table.h"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/simd.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/block_codec.cpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/lz4small.cpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/transpose.cpp"
					)
					
target_sources(seq INTERFACE "$<BUILD_INTERFACE:${headers}>")

configure_file(seq.pc.in seq.pc @ONLY)



# Installation (only compatible with CMake version >= 3.3)
if(${CMAKE_VERSION} VERSION_GREATER "3.2")
    include(CMakePackageConfigHelpers)

    ## Install include directory and potential natvis file
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/seq"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")



    ## Create and install seqConfig.cmake
    configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/seqConfig.cmake.in"
                                  "${CMAKE_CURRENT_BINARY_DIR}/seqConfig.cmake"
                                  INSTALL_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/seq")

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/seqConfig.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/seq")
			
	install(FILES ${CMAKE_BINARY_DIR}/seq.pc
			DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)


    ## Create local seqTargets.cmake
    export(TARGETS seq NAMESPACE seq:: FILE "${CMAKE_CURRENT_BINARY_DIR}/seqTargets.cmake")

    ## Create and install global seqTargets.cmake
    install(TARGETS seq
            EXPORT seqTargets)

    install(EXPORT seqTargets
            NAMESPACE seq::
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/seq")



    ## Create and install seqConfigVersion.cmake
    # seq is header-only and does not depend on the architecture.
    # Remove CMAKE_SIZEOF_VOID_P from seqConfigVersion.cmake so that a
    # seqConfig.cmake generated for a 64 bit target can be used for 32 bit
    # targets and vice versa.
    set(CMAKE_SIZEOF_VOID_P_BACKUP ${CMAKE_SIZEOF_VOID_P})
    unset(CMAKE_SIZEOF_VOID_P)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/seqConfigVersion.cmake"
                                     COMPATIBILITY SameMajorVersion)
    set(CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P_BACKUP})

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/seqConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/seq")
endif()