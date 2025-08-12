cmake_minimum_required(VERSION 3.10)
project(gof LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gol main.cpp)
target_compile_options(gol PRIVATE -Wall -Wextra -O2)
