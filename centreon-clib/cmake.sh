#!/bin/bash

if [ ! -d build ] ; then
  mkdir build
fi

cd build
CXXFLAGS="-Wall -Wextra" cmake -DWITH_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX_LIB=/usr/lib64 -DWITH_TESTING=On $* ..

#CXX=/usr/bin/clang++ CC=/usr/bin/clang CXXFLAGS="-Wall -Wextra" cmake -DWITH_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX_LIB=/usr/lib64 -DWITH_TESTING=On $* ..

#CXXFLAGS="-Wall -Wextra -O1 -fsanitize=address -fno-omit-frame-pointer" cmake -DWITH_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX_LIB=/usr/lib64 -DWITH_TESTING=On $* ..
