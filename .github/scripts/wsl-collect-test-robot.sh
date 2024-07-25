#!/bin/bash
set -e
set -x

export RUN_ENV=docker

echo "##### windows host IP #####"
ip route show | grep -i default | awk '{ print $3}'

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot broker-engine/cma.robot
