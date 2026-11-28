cmake_minimum_required(VERSION 3.16)
project(swanc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(swanc
  src/main.cpp
  src/lexer.cpp
  src/parser.cpp
  src/semantic.cpp
  src/codegen.cpp
)

target_include_directories(swanc PRIVATE include)

if(MSVC)
  target_compile_options(swanc PRIVATE /W4)
else()
  target_compile_options(swanc PRIVATE -Wall -Wextra -Wpedantic)
endif()
