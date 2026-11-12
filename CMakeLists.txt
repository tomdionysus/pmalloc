#
# pmalloc - A Heap based memory manager
#
# Copyright (C) 2022-2026 Tom Cully <mail@tomcully.com>
# Licensed under the MIT License - see LICENSE.txt
#

cmake_minimum_required(VERSION 3.14)

# The version lives once, in src/pmalloc.h, so a copy of the two source files carries it too.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/pmalloc.h" _pmalloc_version_lines REGEX "^#define PMALLOC_VERSION_(MAJOR|MINOR|PATCH) ")
foreach(_line IN LISTS _pmalloc_version_lines)
  if(_line MATCHES "^#define PMALLOC_VERSION_(MAJOR|MINOR|PATCH) ([0-9]+)")
    set(_pmalloc_version_${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
  endif()
endforeach()

project(pmalloc
  VERSION ${_pmalloc_version_MAJOR}.${_pmalloc_version_MINOR}.${_pmalloc_version_PATCH}
  DESCRIPTION "A minimal heap memory manager for embedded systems and sub-allocation"
  LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(PMALLOC_TOP_LEVEL ON)
else()
  set(PMALLOC_TOP_LEVEL OFF)
endif()

if(PMALLOC_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  endif()
endif()

option(PMALLOC_BUILD_TESTING "Build pmalloc_tests (Unity)" ${PMALLOC_TOP_LEVEL})
option(PMALLOC_BUILD_EXAMPLES "Build the examples" ${PMALLOC_TOP_LEVEL})
option(PMALLOC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PMALLOC_COVERAGE "Instrument pmalloc and its tests for coverage (GCC or Clang)" OFF)
option(PMALLOC_CHECKS "Validate pointers and regions passed to pmalloc, at the cost of a list walk per call" OFF)
set(PMALLOC_ALIGN "" CACHE STRING "Block alignment in bytes: a power of two, at least the pointer size. Empty for the pointer size")

include(GNUInstallDirs)

# Warnings for pmalloc's own targets only, never for a consumer or the vendored test framework.
add_library(pmalloc_warnings INTERFACE)
if(MSVC)
  target_compile_options(pmalloc_warnings INTERFACE /W4)
  if(PMALLOC_WARNINGS_AS_ERRORS)
    target_compile_options(pmalloc_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(pmalloc_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
  if(PMALLOC_WARNINGS_AS_ERRORS)
    target_compile_options(pmalloc_warnings INTERFACE -Werror)
  endif()
endif()

# The library

# Settings shared by the library and the copy the tests build.
function(pmalloc_configure target)
  target_include_directories(${target} PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(${target} PRIVATE $<BUILD_INTERFACE:pmalloc_warnings>)
  if(NOT PMALLOC_ALIGN STREQUAL "")
    target_compile_definitions(${target} PUBLIC PMALLOC_ALIGN=${PMALLOC_ALIGN})
  endif()
  if(PMALLOC_CHECKS)
    target_compile_definitions(${target} PUBLIC PMALLOC_CHECKS)
  endif()
endfunction()

add_library(pmalloc src/pmalloc.c)
add_library(pmalloc::pmalloc ALIAS pmalloc)
pmalloc_configure(pmalloc)
set_target_properties(pmalloc PROPERTIES VERSION ${PROJECT_VERSION} PUBLIC_HEADER src/pmalloc.h)
target_compile_definitions(pmalloc PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# Examples

if(PMALLOC_BUILD_EXAMPLES)
  foreach(_example basic suballocation)
    add_executable(pmalloc_example_${_example} example/example_${_example}.c)
    set_target_properties(pmalloc_example_${_example} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_link_libraries(pmalloc_example_${_example} PRIVATE pmalloc pmalloc_warnings)
  endforeach()
endif()

# Tests: one binary, run directly as ./pmalloc_tests

if(PMALLOC_BUILD_TESTING)
  add_library(pmalloc_unity STATIC test/vendor/unity/unity.c)
  target_include_directories(pmalloc_unity SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/test/vendor/unity)
  target_compile_definitions(pmalloc_unity PUBLIC UNITY_SUPPORT_64)

  # The tests link their own copy of pmalloc.c, configured with test/pmalloc_test_config.h so every
  # failed check is recorded, and instrumented for coverage when asked.
  add_library(pmalloc_under_test STATIC src/pmalloc.c)
  pmalloc_configure(pmalloc_under_test)
  target_include_directories(pmalloc_under_test PUBLIC ${PROJECT_SOURCE_DIR}/test)
  target_compile_definitions(pmalloc_under_test PUBLIC
    PMALLOC_CONFIG_HEADER="pmalloc_test_config.h"
    $<$<CONFIG:Debug>:DEBUG>)
  if(PMALLOC_COVERAGE)
    target_compile_options(pmalloc_under_test PRIVATE --coverage -O0)
    target_link_options(pmalloc_under_test INTERFACE --coverage)
  endif()

  add_executable(pmalloc_tests
    test/test_main.c
    test/test_support.c
    test/test_addblock.c
    test/test_malloc.c
    test/test_free.c
    test/test_calloc.c
    test/test_realloc.c
    test/test_internals.c
    test/test_checks.c
    test/test_stress.c)
  set_target_properties(pmalloc_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(pmalloc_tests PRIVATE pmalloc_under_test pmalloc_unity pmalloc_warnings)
endif()

# Install and export: find_package(pmalloc 0.3 CONFIG) then link pmalloc::pmalloc

include(CMakePackageConfigHelpers)
install(TARGETS pmalloc
  EXPORT pmallocTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT pmallocTargets NAMESPACE pmalloc:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pmalloc)
configure_package_config_file(cmake/pmallocConfig.cmake.in ${PROJECT_BINARY_DIR}/pmallocConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pmalloc)
write_basic_package_version_file(${PROJECT_BINARY_DIR}/pmallocConfigVersion.cmake COMPATIBILITY SameMinorVersion)
install(FILES ${PROJECT_BINARY_DIR}/pmallocConfig.cmake ${PROJECT_BINARY_DIR}/pmallocConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pmalloc)
