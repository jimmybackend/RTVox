cmake_minimum_required(VERSION 3.12)
project(rtvox_c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

add_library(rtvox_common
    src/rtvox_common.c
)

target_link_libraries(rtvox_common m)

add_executable(rtvoxenc src/rtvoxenc.c)
target_link_libraries(rtvoxenc rtvox_common m)

add_executable(rtvoxdec src/rtvoxdec.c)
target_link_libraries(rtvoxdec rtvox_common m)
