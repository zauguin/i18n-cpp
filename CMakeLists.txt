cmake_minimum_required(VERSION 3.20)

project(i18n++
  VERSION 0.0
  DESCRIPTION "gettext for C++"
  LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS off)

include(CTest)

add_subdirectory(external)
add_subdirectory(merge)
add_subdirectory(clang)
add_subdirectory(example)

add_library(i18n_lib INTERFACE)
target_include_directories(i18n_lib INTERFACE ../include)
target_compile_options(i18n_lib INTERFACE "-fplugin=$<TARGET_FILE:plugin>")
target_compile_features(i18n_lib INTERFACE cxx_std_20)

add_custom_target(i18n_internal)
add_dependencies(i18n_internal plugin)
add_dependencies(i18n_lib i18n_internal)

function(target_use_i18n TARGET)
  cmake_parse_arguments(PARSE_ARGV 1 I18N "NODOMAIN" "DOMAIN;COMMENT;BASEPATH;POT_FILE;POT_TARGET" "") 

  if(DEFINED I18N_UNPARSED_ARGUMENTS)
    message(WARNING "Ignoring unexpected arguments ${I18N_UNPARSED_ARGUMENTS}")
  endif()

  target_link_libraries(${TARGET} PRIVATE i18n_lib)
  if(I18N_NODOMAIN)
    target_compile_options(${TARGET} PRIVATE "SHELL:-Xclang -plugin-arg-i18n -Xclang nodomain")
  endif()

  if(DEFINED I18N_DOMAIN)
    target_compile_options(${TARGET} PRIVATE "SHELL:-Xclang -plugin-arg-i18n -Xclang domain=${I18N_DOMAIN}")
  endif()

  if(DEFINED I18N_COMMENT)
    target_compile_options(${TARGET} PRIVATE "SHELL:-Xclang -plugin-arg-i18n -Xclang comment=${I18N_COMMENT}")
  endif()

  if(NOT DEFINED I18N_BASEPATH)
    set(I18N_BASEPATH "${CMAKE_SOURCE_DIR}")
  endif()
  target_compile_options(${TARGET} PRIVATE "SHELL:-Xclang -plugin-arg-i18n -Xclang basepath=${I18N_BASEPATH}")

  if(NOT DEFINED I18N_POT_FILE)
    set(I18N_POT_FILE "${TARGET}.pot")
  endif()

  if(NOT DEFINED I18N_POT_TARGET)
    set(I18N_POT_TARGET "${TARGET}_pot")
  endif()

  get_target_property(srcs ${TARGET} SOURCES)
  foreach(src ${srcs})
    set_source_files_properties(${src} PROPERTIES OBJECT_OUTPUTS "$<FILTER:$<TARGET_OBJECTS:${TARGET}>,INCLUDE,${src}>.poc")
  endforeach()
  add_custom_command(OUTPUT "${I18N_POT_FILE}"
    COMMAND ${CMAKE_COMMAND} -E env "SOURCE_DATE_EPOCH=1500000000" "TZ=UTC"
    $<TARGET_FILE:i18n-merge-pot> "--package=${PROJECT_NAME}" "--version=${PROJECT_VERSION}" "--output=${I18N_POT_FILE}" "$<JOIN:$<TARGET_OBJECTS:${TARGET}>,.poc;>.poc"
    DEPENDS "$<JOIN:$<TARGET_OBJECTS:${TARGET}>,.poc;>.poc"
    COMMAND_EXPAND_LISTS)
  add_custom_target("${I18N_POT_TARGET}" ALL DEPENDS "${I18N_POT_FILE}")
  add_dependencies("${I18N_POT_TARGET}" "${TARGET}")
endfunction()

add_subdirectory(tests)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")
