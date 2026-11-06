cmake_minimum_required(VERSION 3.20)
project(SwanCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(swan_frontend
    Compiler/Frontend/Lexer.cpp
    Compiler/Frontend/Parser.cpp
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

add_library(swan_sema
    Compiler/Sema/Analyzer.cpp
)

target_include_directories(swan_sema
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

target_link_libraries(swan_sema PUBLIC swan_frontend)

add_executable(swan-check
    Tools/swan-check.cpp
)

target_link_libraries(swan-check PRIVATE swan_sema)

add_library(swan_sil
    Compiler/SIL/SIL.cpp
)

target_include_directories(swan_sil
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

target_link_libraries(swan_sil PUBLIC swan_sema)

add_executable(swan-sil
    Tools/swan-sil.cpp
)

target_link_libraries(swan-sil PRIVATE swan_sil)

add_library(swan_llvm_backend
    Compiler/Backend/LLVM/LLVMEmitter.cpp
)

target_include_directories(swan_llvm_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

target_link_libraries(swan_llvm_backend PUBLIC swan_sil)

add_executable(swan-llvm
    Tools/swan-llvm.cpp
)

target_link_libraries(swan-llvm PRIVATE swan_llvm_backend)

add_executable(swan
    Tools/swan.cpp
)

target_link_libraries(swan PRIVATE swan_llvm_backend)

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
    add_test(
        NAME swan-sil-smoke
        COMMAND swan-sil ${CMAKE_CURRENT_SOURCE_DIR}/Tests/sil-smoke.swan
    )
    add_test(
        NAME swan-llvm-smoke
        COMMAND swan-llvm ${CMAKE_CURRENT_SOURCE_DIR}/Tests/sil-smoke.swan
                -o ${CMAKE_CURRENT_BINARY_DIR}/sil-smoke.ll
    )
    add_test(
        NAME swan-driver-smoke
        COMMAND swan emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/Tests/sil-smoke.swan
                -o ${CMAKE_CURRENT_BINARY_DIR}/driver-smoke.ll
    )
endif()
