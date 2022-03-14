cmake_minimum_required(VERSION 3.20)

project(i18n_tests VERSION 0.0.1 LANGUAGES CXX)

add_executable(tests)

find_package(fmt REQUIRED)

target_sources(tests PRIVATE simple.cpp readme.cpp)
target_link_libraries(tests PRIVATE fmt::fmt Catch2::Catch2WithMain)
target_compile_definitions(tests PRIVATE "TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

target_use_i18n(tests NODOMAIN COMMENT L10N:)

catch_discover_tests(tests)
add_test(NAME compare_tests_pot COMMAND diff ${CMAKE_CURRENT_SOURCE_DIR}/tests.reference.pot tests.pot)
