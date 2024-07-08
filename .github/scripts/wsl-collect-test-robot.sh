#!/bin/bash
set -e
set -x

export RUN_ENV=docker


#remove git dubious ownership
/usr/bin/git config --global --add safe.directory $PWD

echo "###### git clone opentelemetry-proto  #######"
git clone --depth=1 --single-branch https://github.com/open-telemetry/opentelemetry-proto.git opentelemetry-proto

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot ccc/ccc.robot
