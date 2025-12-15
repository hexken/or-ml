cmake_minimum_required(VERSION 3.20)
project(MPSParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mps_app app/main.cpp)

target_include_directories(mps_app PRIVATE
    ${CMAKE_SOURCE_DIR}/MPSParse
)
