cmake_minimum_required(VERSION 3.16)

project(seq
    VERSION 1.3
    DESCRIPTION "Collection of C++14 original containers"
    HOMEPAGE_URL "https://github.com/Thermadiag/seq"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

# project options
option(SEQ_HEADER_ONLY "Do not build library, use header-only version" OFF)
option(SEQ_ENABLE_AVX2 "Enable AVX2 support" ON)
option(SEQ_BUILD_TESTS "Build tests" OFF)
option(SEQ_BUILD_BENCHS "Build benchmarks" OFF)
option(SEQ_NO_WARNINGS "Treat warnings as errors" OFF)
option(SEQ_BUILD_SHARED "Build shared library" ON)
option(SEQ_BUILD_STATIC "Build static library" ON)

# create source file list
file(GLOB SeqSources
     "seq/*.h"
	 "seq/*.hpp"
	 "seq/internal/*.h"
	 "seq/internal/*.hpp"
     "seq/internal/*.cpp" 
)

# Disable header only mode
set(PROJECT_HEADER_ONLY 0)


set(SEQ_INSTALL_LIBDIR       "${CMAKE_INSTALL_LIBDIR}")
set(SEQ_INSTALL_BINDIR       "${CMAKE_INSTALL_BINDIR}")
set(SEQ_INSTALL_DATAROOTDIR  "${CMAKE_INSTALL_DATAROOTDIR}")
set(SEQ_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/seq")



if(SEQ_BUILD_SHARED)
	# add sources
	add_library(seq SHARED 
		${SeqSources}
	)

	# add preprocessor definitions
	target_compile_definitions(seq PRIVATE -DSEQ_BUILD_SHARED_LIBRARY)
	
	#add linker options
	if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# mingw
		target_link_options(seq PRIVATE -lKernel32 -lpsapi -lBcrypt)
	endif()

	if(WIN32)
		target_compile_definitions(seq PRIVATE -D_CRT_SECURE_NO_WARNINGS)
	endif()
	
	# add sse/avx flags
	if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		if(SEQ_ENABLE_AVX2)
			target_compile_options(seq PRIVATE /arch:AVX2)
		endif()
	else()
		target_compile_options(seq PRIVATE -march=native)
	endif()
	
	if(SEQ_NO_WARNINGS)
		if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
			target_compile_options(seq PRIVATE  /WX /W3 )
		elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			target_compile_options(seq PRIVATE -Werror -Wall -Wno-c++98-compat -Wno-c++98-compat-pedantic)
		else()
			target_compile_options(seq PRIVATE -Werror -Wall)
		endif()
	endif()

	target_include_directories(seq PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq>"
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq/internal>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
						   
	
	#test
	install(TARGETS seq EXPORT seq ARCHIVE DESTINATION ${SEQ_INSTALL_LIBDIR} RUNTIME DESTINATION ${SEQ_INSTALL_BINDIR} LIBRARY DESTINATION ${SEQ_INSTALL_LIBDIR})
	install(EXPORT seq DESTINATION ${SEQ_CMAKEDIR})
	
endif()


if(SEQ_BUILD_STATIC)
	# add sources
	add_library(seq_static STATIC 
		${SeqSources}
	)

	# add preprocessor definitions
	target_compile_definitions(seq_static PRIVATE -DSEQ_BUILD_STATIC_LIBRARY)
	
	#add linker options
	if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# mingw
		target_link_options(seq_static PRIVATE -lKernel32 -lpsapi -lBcrypt)
	endif()

	if(WIN32)
		target_compile_definitions(seq_static PRIVATE -D_CRT_SECURE_NO_WARNINGS)
	endif()
	
	# add sse/avx flags
	if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		if(SEQ_ENABLE_AVX2)
			target_compile_options(seq_static PRIVATE /arch:AVX2)
		endif()
	else()
		target_compile_options(seq_static PRIVATE -march=native)
	endif()
	
	if(SEQ_NO_WARNINGS)
		if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
			target_compile_options(seq_static PRIVATE  /WX /W3 )
		elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			target_compile_options(seq_static PRIVATE -Werror -Wall -Wno-c++98-compat -Wno-c++98-compat-pedantic)
		else()
			target_compile_options(seq_static PRIVATE -Werror -Wall)
		endif()
	endif()

	target_include_directories(seq_static  PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq>"
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq/internal>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
						   
						   
	#test
	install(TARGETS seq_static EXPORT seq DESTINATION ${SEQ_INSTALL_LIBDIR} LIBRARY)
	install(EXPORT seq DESTINATION ${SEQ_CMAKEDIR})
	
endif()


# add tests
enable_testing()

# add tests
if(SEQ_BUILD_TESTS)
	add_subdirectory(tests)
endif()

# add benchmarks
if(SEQ_BUILD_BENCHS)
	add_subdirectory(benchs)
endif()

# Install headers and sources (for header-only mode)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/seq"
		DESTINATION "${CMAKE_INSTALL_PREFIX}/include")


# Configure and install seq.pc
configure_file(seq.pc.in seq.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/seq.pc
			DESTINATION ${SEQ_INSTALL_DATAROOTDIR}/pkgconfig)


# Configure and install seq_config.hpp
configure_file(seq_config.hpp.in seq_config.hpp @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/seq_config.hpp
			DESTINATION "${CMAKE_INSTALL_PREFIX}/include/seq")

# Configure and install seqConfig.cmake and seqConfigVersion.cmake
include(CMakePackageConfigHelpers)

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/seqConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/seqConfig.cmake"
  INSTALL_DESTINATION ${SEQ_INSTALL_LIBDIR}/cmake/seq
  PATH_VARS )

  
# Generate seqConfigVersion.cmake
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/seqConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion )
  
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/seqConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/seqConfigVersion.cmake
        DESTINATION ${SEQ_INSTALL_LIBDIR}/cmake/seq )
		
