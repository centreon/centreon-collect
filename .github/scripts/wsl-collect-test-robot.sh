#!/bin/bash
set -x

test_file=$1

export RUN_ENV=WSL
export HOST_NAME=$2
export USED_ADDRESS=$3
export PWSH_PATH=$4
export WINDOWS_PROJECT_PATH=$5


#in order to connect to windows we neeed to use windows ip
echo "127.0.0.1       localhost" > /etc/hosts
echo "${USED_ADDRESS}      ${HOST_NAME}" >> /etc/hosts

echo "##### /etc/hosts: ######"
cat /etc/hosts

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"

robot $test_file

echo "####################### End of Centreon Collect Robot Tests #######################"

if [ -d failed ] ; then
    echo "failure save logs in ${PWD}/../reports"
    cp -rp failed ../reports/windows-cma-failed
    cp log.html ../reports/windows-cma-log.html
    cp output.xml ../reports/windows-cma-output.xml
fi
