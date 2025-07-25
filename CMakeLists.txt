cmake_minimum_required(VERSION 3.16)

project(seq
    VERSION 2.0
    DESCRIPTION "Collection of C++14 original containers"
    HOMEPAGE_URL "https://github.com/Thermadiag/seq"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

# project options
option(SEQ_BUILD_TESTS "Build tests" OFF)
option(SEQ_BUILD_BENCHS "Build benchmarks" OFF)
option(SEQ_NO_WARNINGS "Treat warnings as errors" OFF)


set(SEQ_INSTALL_LIBDIR       "${CMAKE_INSTALL_LIBDIR}")
set(SEQ_INSTALL_DATAROOTDIR  "${CMAKE_INSTALL_DATAROOTDIR}")
set(SEQ_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/seq")


add_library(seq INTERFACE )

target_include_directories(seq INTERFACE
					   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq>"
					   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq/internal>"
					   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
					   
install(TARGETS seq EXPORT seq )
install(EXPORT seq DESTINATION ${SEQ_CMAKEDIR})
	

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
		
