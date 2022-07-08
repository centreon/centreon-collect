project("test" CXX)
cmake_minimum_required(VERSION 3.22)
add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(bench atoi.cc)
target_link_libraries(bench CONAN_PKG::benchmark CONAN_PKG::abseil)
