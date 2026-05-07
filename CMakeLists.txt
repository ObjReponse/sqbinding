cmake_minimum_required(VERSION 3.0...3.20)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

project(sqb)

find_package(squirrel REQUIRED)

file(GLOB CppFiles    src/*.cpp)
file(GLOB HeaderFiles src/*.h)

add_library(${PROJECT_NAME}  ${CppFiles} ${HeaderFiles})

target_compile_definitions(${PROJECT_NAME} PUBLIC SQBINDING_EXISTS)

target_include_directories(${PROJECT_NAME} PUBLIC
  src
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  squirrel::squirrel_static squirrel::sqstdlib_static
)
