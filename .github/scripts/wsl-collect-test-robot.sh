#!/bin/bash
set -e
set -x

test_file=$1

export RUN_ENV=WSL
export HOST_NAME=$2
export USED_ADDRESS=$3

echo "##### /etc/hosts: ######"
cat /etc/hosts

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot $test_file
