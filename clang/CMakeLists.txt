cmake_minimum_required(VERSION 3.20)

add_executable(tool)
add_library(plugin MODULE)
add_library(clang_common OBJECT)

find_package(Clang REQUIRED)

target_sources(clang_common PRIVATE attr.cpp action.cpp ../common/write_po.cpp)
target_sources(tool PRIVATE tool.cpp)
target_sources(plugin PRIVATE plugin.cpp)
target_link_libraries(plugin PRIVATE clang_common)
target_link_libraries(tool PRIVATE clang_common)
target_link_libraries(tool PRIVATE clang-cpp LLVM)
set_target_properties(clang_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(plugin PROPERTIES
  PREFIX ""
  OUTPUT_NAME "i18n")
target_include_directories(clang_common PUBLIC ${LLVM_INCLUDE_DIRS})
target_compile_definitions(clang_common PUBLIC ${LLVM_DEFINITIONS})
target_compile_features(clang_common PUBLIC cxx_std_17)
if(${LLVM_ENABLE_RTTI})
  target_compile_options(clang_common PUBLIC -frtti)
else()
  target_compile_options(clang_common PUBLIC -fno-rtti)
endif()
target_link_options(plugin PRIVATE -fuse-ld=lld)
target_link_options(tool PRIVATE -fuse-ld=lld)

add_library(i18n_lib INTERFACE)
target_include_directories(i18n_lib INTERFACE ../include)
target_compile_options(i18n_lib INTERFACE "-fplugin=$<TARGET_FILE:plugin>")
target_compile_features(i18n_lib INTERFACE cxx_std_20)

add_custom_target(i18n_internal)
add_dependencies(i18n_internal plugin)
add_dependencies(i18n_lib i18n_internal)
