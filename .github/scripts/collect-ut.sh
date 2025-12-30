#!/bin/bash

set +e
test=$1

cd build

sysctl -w kernel.core_pattern=/tmp/core-$test

tests/ut_$test --gtest_output=xml:ut_$test.xml --gtest_filter=grpc*


if [ $? != 0 ]
then
    if [ -f /tmp/core-$test* ]
    then
        gdb -batch -ex "thread apply all bt 30" tests/ut_$test /tmp/core-$test* > ut_$test.core.txt
        ls -l .
        pwd
    fi
    echo exit 1
    exit 1
fi
echo exit 0
exit 0
