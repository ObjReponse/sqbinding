cmake_minimum_required(VERSION 3.0)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

project(test
  DESCRIPTION "Test SQB"
  LANGUAGES C CXX
)

add_definitions(-std=c++11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(AddLibrarayToProject_DIR "${CMAKE_SOURCE_DIR}/external")
include(CMakeMacros/AddLibrarayToProject.cmake)
set(ENV{PKG_CONFIG_PATH} "${CMAKE_BINARY_DIR}/root-install/lib64/pkgconfig:$ENV{PKG_CONFIG_PATH}")
set(ENV{LD_LIBRARY_PATH} "${CMAKE_BINARY_DIR}/root-install/lib64:$ENV{LD_LIBRARY_PATH}")

AddLibrarayToProject("squirrel")

add_subdirectory(sqb)

add_executable(${PROJECT_NAME} test.cpp ut/check.h)

target_link_libraries(${PROJECT_NAME} sqb)
