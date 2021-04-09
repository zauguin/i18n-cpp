cmake_minimum_required(VERSION 3.20)

project(i18n++
  VERSION 0.0
  DESCRIPTION "gettext for C++"
  LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS off)

add_subdirectory(external)
add_subdirectory(merge)
add_subdirectory(clang)
add_subdirectory(example)
