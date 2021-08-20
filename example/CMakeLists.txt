cmake_minimum_required(VERSION 3.20)

project(i18n_example VERSION 0.0.1 LANGUAGES CXX)

add_executable(guessing)

find_package(fmt REQUIRED)

target_sources(guessing PRIVATE guessing.cpp)
target_link_libraries(guessing PRIVATE fmt::fmt i18n_lib)
target_include_directories(guessing PRIVATE ../runtime)
target_compile_options(guessing PRIVATE "SHELL:-Xclang -plugin-arg-i18n -Xclang nodomain")
get_target_property(srcs guessing SOURCES)
foreach(src ${srcs})
  set_source_files_properties(${src} PROPERTIES OBJECT_OUTPUTS "$<FILTER:$<TARGET_OBJECTS:guessing>,INCLUDE,${src}>.poc")
endforeach()
add_custom_command(OUTPUT guessing.pot
  COMMAND merge_pot "--package=${PROJECT_NAME}" "--version=${PROJECT_VERSION}" "--output=guessing.pot" "$<JOIN:$<TARGET_OBJECTS:guessing>,.poc;>.poc"
  DEPENDS "$<JOIN:$<TARGET_OBJECTS:guessing>,.poc;>.poc"
  COMMAND_EXPAND_LISTS)
add_custom_target(guessing_pot ALL DEPENDS guessing.pot)
