project("test" CXX)
cmake_minimum_required(VERSION 3.16)
add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(bench int64_map.cc)
target_link_libraries(bench CONAN_PKG::benchmark CONAN_PKG::abseil CONAN_PKG::fmt CONAN_PKG::boost)

add_executable(str_repl str_repl.cc)
target_link_libraries(str_repl CONAN_PKG::benchmark CONAN_PKG::abseil CONAN_PKG::boost)
