cmake_minimum_required(VERSION 3.20)

project(i18n_tests VERSION 0.0.1 LANGUAGES CXX)

add_executable(tests)

find_package(fmt REQUIRED)

target_sources(tests PRIVATE simple.cpp)
target_link_libraries(tests PRIVATE fmt::fmt i18n_lib Catch2::Catch2WithMain)
target_include_directories(tests PRIVATE ../runtime)
target_compile_options(tests PRIVATE "SHELL:-Xclang -plugin-arg-i18n -Xclang nodomain -Xclang -plugin-arg-i18n -Xclang basepath=${CMAKE_SOURCE_DIR}/")
get_target_property(srcs tests SOURCES)
foreach(src ${srcs})
  set_source_files_properties(${src} PROPERTIES OBJECT_OUTPUTS "$<FILTER:$<TARGET_OBJECTS:tests>,INCLUDE,${src}>.poc")
endforeach()
add_custom_command(OUTPUT tests.pot
  COMMAND ${CMAKE_COMMAND} -E env "SOURCE_DATE_EPOCH=1500000000" "TZ=UTC"
    $<TARGET_FILE:merge_pot> "--package=${PROJECT_NAME}" "--version=${PROJECT_VERSION}" "--output=tests.pot" "$<JOIN:$<TARGET_OBJECTS:tests>,.poc >.poc"
  DEPENDS "$<JOIN:$<TARGET_OBJECTS:tests>,.poc >.poc"
  COMMAND_EXPAND_LISTS)
add_custom_target(tests_pot ALL DEPENDS tests.pot)

catch_discover_tests(tests)
add_test(NAME compare_tests_pot COMMAND diff ${CMAKE_CURRENT_SOURCE_DIR}/tests.reference.pot tests.pot)
