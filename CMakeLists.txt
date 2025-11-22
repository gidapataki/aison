cmake_minimum_required(VERSION 3.16)
project(aison LANGUAGES CXX)

execute_process(COMMAND ln -fsn "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/build/current")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# deps

option(AISON_USE_BUNDLED_JSONCPP "Use bundled jsoncpp submodule" OFF)
option(AISON_REQUIRE_EXTERNAL_JSONCPP "Fail if external jsoncpp is not found" OFF)

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

set(AISON_JSONCPP_TARGET "")
if(NOT AISON_USE_BUNDLED_JSONCPP)
  find_package(jsoncpp 1.9.5 CONFIG QUIET)
  if(jsoncpp_FOUND)
    if(TARGET jsoncpp_lib)
      set(AISON_JSONCPP_TARGET jsoncpp_lib)
    elseif(TARGET jsoncpp_static)
      set(AISON_JSONCPP_TARGET jsoncpp_static)
    elseif(TARGET JsonCpp::JsonCpp)
      set(AISON_JSONCPP_TARGET JsonCpp::JsonCpp)
    endif()
  endif()
  if(AISON_JSONCPP_TARGET STREQUAL "" AND AISON_REQUIRE_EXTERNAL_JSONCPP)
    message(FATAL_ERROR
      "External jsoncpp requested but not found. Install jsoncpp/1.9.5 (e.g., via Conan) "
      "or set AISON_USE_BUNDLED_JSONCPP=ON.")
  endif()
endif()

if(AISON_JSONCPP_TARGET STREQUAL "")
  add_subdirectory(third_party/jsoncpp)
  set(AISON_JSONCPP_TARGET jsoncpp_static)
endif()

# aison

add_library(aison INTERFACE)
target_include_directories(
  aison INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                  $<INSTALL_INTERFACE:include>)
target_link_libraries(aison INTERFACE ${AISON_JSONCPP_TARGET})

# example

add_executable(example
  example/main.cpp
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
