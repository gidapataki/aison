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

add_executable(example example/main.cpp)
target_link_libraries(example PRIVATE aison)

# test

if(TRUE)
  include(CTest)
  enable_testing()

  add_executable(aison-test
      test/basic.cpp
      test/schema_with_config.cpp
  )

  target_include_directories(aison-test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest
  )

  target_link_libraries(aison-test PRIVATE aison)


  add_test(NAME aison-test COMMAND aison-test)
endif()
