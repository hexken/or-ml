cmake_minimum_required(VERSION 3.20)
project(or-ml LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(or-ml app/or-ml.cpp src/mps/mps_parser.cpp)

target_include_directories(or-ml PRIVATE
    ${CMAKE_SOURCE_DIR}/src/mps
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_include_directories(or-ml SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/deps/
)
