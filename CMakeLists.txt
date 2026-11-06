cmake_minimum_required(VERSION 3.20)
project(SwanCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(swan_frontend
    Compiler/Frontend/Lexer.cpp
)

target_include_directories(swan_frontend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

add_executable(swan-lex
    Tools/swan-lex.cpp
)

target_link_libraries(swan-lex PRIVATE swan_frontend)
