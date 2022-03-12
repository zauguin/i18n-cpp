cmake_minimum_required(VERSION 3.20)

add_subdirectory(Catch2)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Catch2/extras)
include(Catch)
