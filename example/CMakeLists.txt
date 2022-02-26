cmake_minimum_required(VERSION 3.20)

project(i18n_example VERSION 0.0.1 LANGUAGES CXX)

add_executable(guessing)

find_package(i18n++ REQUIRED)
find_package(fmt REQUIRED)

target_sources(guessing PRIVATE guessing.cpp)
target_link_libraries(guessing PRIVATE fmt::fmt)
target_use_i18n(guessing NODOMAIN)
