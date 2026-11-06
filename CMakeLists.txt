cmake_minimum_required(VERSION 3.20)
project(SwanCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(swan_frontend
    Compiler/Frontend/Lexer.cpp
    Compiler/Frontend/Parser.cpp
    Compiler/Sema/Analyzer.cpp
)

target_include_directories(swan_frontend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

add_executable(swan-lex
    Tools/swan-lex.cpp
)

target_link_libraries(swan-lex PRIVATE swan_frontend)

add_executable(swan-parse
    Tools/swan-parse.cpp
)

target_link_libraries(swan-parse PRIVATE swan_frontend)

add_executable(swan-check
    Tools/swan-check.cpp
)

target_link_libraries(swan-check PRIVATE swan_frontend)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME swan-parser-smoke
        COMMAND swan-parse ${CMAKE_CURRENT_SOURCE_DIR}/Tests/lexer-smoke.swan
    )
    add_test(
        NAME swan-sema-smoke
        COMMAND swan-check ${CMAKE_CURRENT_SOURCE_DIR}/Tests/semantic-smoke.swan
    )
    add_test(
        NAME swan-sema-diagnostics
        COMMAND swan-check ${CMAKE_CURRENT_SOURCE_DIR}/Tests/semantic-errors.swan
    )
    set_tests_properties(swan-sema-diagnostics PROPERTIES WILL_FAIL TRUE)
endif()
