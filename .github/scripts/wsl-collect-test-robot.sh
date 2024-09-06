#!/bin/bash
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

robot -t BEOTEL_CENTREON_AGENT_CHECK_HOST $test_file

echo "####################### End of Centreon Collect Robot Tests #######################"

if [ -d failed ] ; then
    echo "failure save logs in ${PWD}/../reports"
    cp -rp failed ../reports/windows-cma-failed
    cp log.html ../reports/windows-cma-log.html
    cp output.xml ../reports/windows-cma-output.xml
fi
