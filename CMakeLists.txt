cmake_minimum_required(VERSION 3.20)
project(MPSParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main app/main.cpp)

target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/src/MPSParse
)
target_include_directories(main SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/deps/
)
