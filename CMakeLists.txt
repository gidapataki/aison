cmake_minimum_required(VERSION 3.16)
project(aison LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# deps

set(JSONCPP_WITH_TESTS                OFF CACHE INTERNAL "")
set(JSONCPP_WITH_POST_BUILD_UNITTEST  OFF CACHE INTERNAL "")
set(JSONCPP_WITH_WARNING_AS_ERROR     OFF CACHE INTERNAL "")
set(JSONCPP_WITH_STRICT_ISO           ON  CACHE INTERNAL "")
set(JSONCPP_WITH_PKGCONFIG_SUPPORT    OFF CACHE INTERNAL "")
set(JSONCPP_WITH_CMAKE_PACKAGE        OFF CACHE INTERNAL "")
set(JSONCPP_WITH_EXAMPLE              OFF CACHE INTERNAL "")
set(JSONCPP_STATIC_WINDOWS_RUNTIME    OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS                 OFF CACHE INTERNAL "")
set(BUILD_STATIC_LIBS                 ON  CACHE INTERNAL "")
set(BUILD_OBJECT_LIBS                 OFF CACHE INTERNAL "")

add_subdirectory(third_party/jsoncpp)

# aison

add_library(aison INTERFACE)
target_include_directories(
  aison INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                  $<INSTALL_INTERFACE:include>)
target_link_libraries(aison INTERFACE jsoncpp_static)

# example

add_executable(example
  example/main.cpp
  example/types.cpp
  example/feature_encoder.cpp
  example/feature_introspect.cpp
  example/feature_variant.cpp
)
target_link_libraries(example PRIVATE aison)

# test

option(AISON_BUILD_TESTS "Build Aison tests" ON)
option(AISON_ENABLE_ASAN "Build with AddressSanitizer" OFF)
option(AISON_ENABLE_UBSAN "Build with UndefinedBehaviorSanitizer" OFF)
option(AISON_ENABLE_LSAN "Build with LeakSanitizer (Linux only)" OFF)


if(AISON_BUILD_TESTS)
  include(CTest)
  enable_testing()

  add_executable(aison-test
      test/main.cpp
      test/utils.cpp
      test/types_basic.cpp
      test/types_variant.cpp
      test/types_custom.cpp
      test/runtime_asserts.cpp
      test/runtime_errors.cpp
  )

  target_include_directories(aison-test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest
  )

  target_link_libraries(aison-test PRIVATE aison)

  if(AISON_ENABLE_ASAN)
    target_compile_options(aison-test PRIVATE -fsanitize=address)
    target_link_options(aison-test PRIVATE -fsanitize=address)
  endif()

  if(AISON_ENABLE_UBSAN)
    target_compile_options(aison-test PRIVATE -fsanitize=undefined)
    target_link_options(aison-test PRIVATE -fsanitize=undefined)
  endif()

  if(AISON_ENABLE_LSAN AND NOT APPLE)
    target_compile_options(aison-test PRIVATE -fsanitize=leak)
    target_link_options(aison-test PRIVATE -fsanitize=leak)
  endif()


  add_test(NAME aison-test COMMAND aison-test)
endif()
