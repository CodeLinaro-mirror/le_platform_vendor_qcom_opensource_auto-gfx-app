cmake_minimum_required(VERSION 3.22)
project(yxw)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(yxw)

add_subdirectory(yxw)
add_subdirectory(gles)
