#!/bin/bash

set -e
set -x

conan install `dirname $0`/../..

if test -f "/bin/cmake3";
then
	cmake3 `dirname $0`/../..
else
	cmake `dirname $0`/../..
fi

make -j8

./bin/clib-ut --gtest_output="xml:./clib.xml"
#./bin/ccc_ut --gtest_output="xml:./connectors.xml"
./bin/cce_ut --gtest_output="xml:./engine.xml"
./bin/ccb_ut --gtest_output="xml:./broker.xml"
./bin/cbwd_ut --gtest_output="xml:./watchdog.xml"
