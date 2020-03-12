# Global options.
cmake_minimum_required(VERSION 2.8.12)
project(connectors CXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3")
add_definitions("-D_GLIBCXX_USE_CXX11_ABI=0")

# We will use pkg-config if available.

add_subdirectory(perl)
add_subdirectory(ssh)