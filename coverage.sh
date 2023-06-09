#!/bin/bash

soft="all"
if [[ $# -ge 1 ]] ; then
  if [[ "$1" == "engine" || "$1" == "broker" ]] ; then
    echo testing $soft
    soft=$1
  else
    echo Cannot test $1
    exit 1
  fi
  shift
fi

pwd=$PWD
echo "Go to build"
cd build
echo "lcov reset counters"
find . -name '*.gcda' -delete
#/usr/bin/lcov --directory . --zerocounters
echo "Execute tests"
case $soft in
broker)
  tests/ut_broker $*
  ;;
engine)
  tests/ut_engine $*
  ;;
*)
  tests/ut_clib
  tests/ut_engine
  tests/ut_broker
  tests/ut_connector
  ;;
esac

echo "Go back to current directory"
cd $pwd

echo "Execute grcov"
grcov build -s $soft -t html --branch --ignore-not-existing --ignore '*/.conan/*' --ignore '/usr/include/*' --ignore 'tests/*' --ignore 'broker/tests/*' --ignore 'clib/tests/*' --ignore 'connectors/tests/*' --ignore 'build/*' -o test-coverage --threads 8

firefox test-coverage/index.html&
