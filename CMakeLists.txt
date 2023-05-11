cmake_minimum_required(VERSION 3.16)

project(seq
    VERSION 1.1.0
    DESCRIPTION "Collection of C++11 original containers"
    HOMEPAGE_URL "https://github.com/Thermadiag/seq"
    LANGUAGES CXX
)

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
		"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/charconv.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/hash.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/seq/internal/simd.cpp"
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



# Install headers and sources (for header-only mode)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/seq"
		DESTINATION "${CMAKE_INSTALL_PREFIX}/include")

install (TARGETS seq LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
install (TARGETS seq RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

# Configure and install seq.pc
configure_file(seq.pc.in seq.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/seq.pc
			DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)


# Configure and install seqConfig.cmake and seqConfigVersion.cmake
include(CMakePackageConfigHelpers)
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/seqConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/seqConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/seq
  PATH_VARS )
  
# Generate seqConfigVersion.cmake
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/seqConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion )
  
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/seqConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/seqConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/seq )
		
