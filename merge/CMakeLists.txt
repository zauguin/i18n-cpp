cmake_minimum_required(VERSION 3.20)

project(i18n_PO_merge LANGUAGES CXX)

add_executable(merge_pot)

find_package(fmt REQUIRED)

target_sources(merge_pot PRIVATE main.cpp messages.cpp ../common/write_po.cpp ../common/merge.cpp)
target_link_libraries(merge_pot PRIVATE Boost::boost fmt::fmt)
target_compile_features(merge_pot PRIVATE cxx_std_20)
