cmake_minimum_required(VERSION 3.20)

project(i18n_PO_merge LANGUAGES CXX)

add_executable(i18n-merge-pot)

target_sources(i18n-merge-pot PRIVATE main.cpp messages.cpp ../common/write_po.cpp merge.cpp)
target_link_libraries(i18n-merge-pot PRIVATE boost::boost fmt::fmt)
target_compile_features(i18n-merge-pot PRIVATE cxx_std_20)

install(TARGETS i18n-merge-pot EXPORT i18n++Targets DESTINATION bin)
