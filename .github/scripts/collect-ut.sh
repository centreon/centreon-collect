#!/bin/bash

# Usage: collect-ut.sh <component>: run build/tests/ut_<component> with a private /tmp,
# so that the unit tests of several components can run at the same time.

set +e
test=$1

cd build

# The pattern is global to the kernel: %e keeps the cores of concurrent tests apart,
# and /var/tmp stays outside the private /tmp of the test.
mkdir -p /var/tmp
sysctl -w kernel.core_pattern=/var/tmp/core-%e.%p > /dev/null

echo
echo "---------------------------   Execute tests/ut_$test   ---------------------------------"
echo

# Some tests of different components use the same /tmp paths (/tmp/toto, /tmp/test.txt...).
unshare --mount --propagation private sh -c 'mount -t tmpfs tmpfs /tmp && exec "$@"' sh \
    tests/ut_$test --gtest_output=xml:ut_$test.xml


if [ $? != 0 ]
then
    for core in /var/tmp/core-ut_$test.*
    do
        if [ -f "$core" ]
        then
            gdb -batch -ex "thread apply all bt 30" tests/ut_$test "$core" > ut_$test.core.txt
        fi
    done
    exit 1
fi
exit 0
