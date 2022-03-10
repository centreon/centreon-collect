#!/bin/bash

if [ ! -d cppcheck-build ] ; then
  mkdir cppcheck-build
fi

cppcheck --project=build/compile_commands.json --std=c++14 -j9 -l70 --output-file=cppcheck-report.txt --cppcheck-build-dir=cppcheck-build
