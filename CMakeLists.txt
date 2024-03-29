cmake_minimum_required(VERSION 3.14)
project(pmalloc)

# GoogleTest 
set(CMAKE_CXX_STANDARD 14)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_BINARY_DIR}/lib
)

# pmalloc

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-Os")

# Debug Mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(DEBUG)
endif()

add_library(
  pmalloc
  src/pmalloc.c
)

add_executable(pmalloc_example_basic example/example_basic.c)
target_include_directories(pmalloc_example_basic PUBLIC src)
target_link_libraries(pmalloc_example_basic pmalloc)

add_executable(pmalloc_example_suballocation example/example_suballocation.c)
target_include_directories(pmalloc_example_suballocation PUBLIC src)
target_link_libraries(pmalloc_example_suballocation pmalloc)

enable_testing()

add_executable(
  pmalloc_test
  src/pmalloc_test.cc
)

target_link_libraries(
  pmalloc_test
  GTest::gtest_main
  pmalloc
)

include(GoogleTest)
gtest_discover_tests(pmalloc_test)

