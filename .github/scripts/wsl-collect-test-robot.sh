#!/bin/bash
set -e
set -x

test_file=$1

export RUN_ENV=WSL
export USED_ADDRESS=$2

echo "##### windows host IP #####"
ip route show | grep -i default | awk '{ print $3}'

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot $test_file
