cmake_minimum_required(VERSION 3.16)
project(is_mpl VERSION 1.0.0 LANGUAGES CXX)

add_library(is_mpl_core
    src/solver.cpp
)
target_compile_features(is_mpl_core PUBLIC cxx_std_17)
target_include_directories(is_mpl_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_options(is_mpl_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(is_mpl src/io.cpp src/main.cpp)
target_compile_features(is_mpl PRIVATE cxx_std_17)
target_include_directories(is_mpl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(is_mpl PRIVATE is_mpl_core)

include(CTest)
find_package(Python3 COMPONENTS Interpreter)
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(
        NAME is_mpl_correctness
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py"
                "$<TARGET_FILE:is_mpl>"
    )
endif()
