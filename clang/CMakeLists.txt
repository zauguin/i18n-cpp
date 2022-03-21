cmake_minimum_required(VERSION 3.20)

add_executable(i18n-extract)
add_library(plugin MODULE)
add_library(clang_common OBJECT)

find_package(Clang REQUIRED CONFIG)
if(NOT TARGET ZLIB::ZLIB AND TARGET zlib::zlib)
  add_library(ZLIB::ZLIB ALIAS zlib::zlib)
endif()

target_sources(clang_common PRIVATE attr.cpp action.cpp ../common/write_po.cpp)
target_sources(i18n-extract PRIVATE tool.cpp)
target_sources(plugin PRIVATE plugin.cpp)
target_link_libraries(clang_common PUBLIC Boost::headers)
target_link_libraries(plugin PRIVATE clang_common)
target_link_libraries(i18n-extract PRIVATE clang_common)

if(CLANG_LINK_CLANG_DYLIB)
  target_link_libraries(i18n-extract PRIVATE clang-cpp LLVM)
else()
  target_link_libraries(i18n-extract PRIVATE
    clangAST
    clangFrontend
    clangTooling
  )
endif()

set_target_properties(clang_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(plugin PROPERTIES
  PREFIX ""
  OUTPUT_NAME "i18n-clang")
target_include_directories(clang_common PUBLIC ${LLVM_INCLUDE_DIRS})
target_compile_definitions(clang_common PUBLIC ${LLVM_DEFINITIONS})
target_compile_features(clang_common PUBLIC cxx_std_17)
if(${LLVM_ENABLE_RTTI})
  target_compile_options(clang_common PUBLIC -frtti)
else()
  target_compile_options(clang_common PUBLIC -fno-rtti)
endif()
target_link_options(plugin PRIVATE $<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>)

install(TARGETS plugin EXPORT i18n++Targets DESTINATION lib)
install(TARGETS i18n-extract EXPORT i18n++Targets DESTINATION bin)
