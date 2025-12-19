cmake_minimum_required(VERSION 3.20)
project(or-ml LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(or_ml_core STATIC
    src/mps/mps_parser.cpp
    src/utils/string_utils.cpp
)

target_include_directories(or_ml_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src/mps
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_include_directories(or_ml_core SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/deps/
)

# Main executable
add_executable(or-ml app/or-ml.cpp)
target_link_libraries(or-ml PRIVATE or_ml_core)

# Test executable
add_executable(test_mps_parse_file tests/test_mps_parse_file.cpp)
target_link_libraries(test_mps_parse_file PRIVATE or_ml_core)

enable_testing()
add_test(NAME All_Unit_Tests COMMAND test_mps_parse_file)
