cmake_minimum_required(VERSION 3.20)

project(i18n++
  VERSION 0.0
  DESCRIPTION "gettext for C++"
  LANGUAGES CXX)

add_subdirectory(external)
add_subdirectory(merge)
add_subdirectory(clang)
