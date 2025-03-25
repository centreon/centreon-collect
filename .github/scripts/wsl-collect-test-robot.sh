#!/bin/bash
set -x

test_file=$1

export RUN_ENV=WSL
export JSON_TEST_PARAMS=$2
export USED_ADDRESS=`echo $JSON_TEST_PARAMS | jq -r .ip`
export HOST_NAME=`echo $JSON_TEST_PARAMS | jq -r .host`
export PWSH_PATH=`echo $JSON_TEST_PARAMS | jq -r .pwsh_path`
export WINDOWS_PROJECT_PATH=`echo $JSON_TEST_PARAMS | jq -r .current_dir`



#in order to connect to windows we neeed to use windows ip
echo "127.0.0.1       localhost" > /etc/hosts
echo "${USED_ADDRESS}      ${HOST_NAME}" >> /etc/hosts

echo "##### /etc/hosts: ######"
cat /etc/hosts

echo "##### Starting tests ##### with params: $JSON_TEST_PARAMS"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"

robot -e unstable $test_file

echo "####################### End of Centreon Collect Robot Tests #######################"

if [ -d failed ] ; then
    echo "failure save logs in ${PWD}/../reports"
    cp -rp failed ../reports/windows-cma-failed
    cp log.html ../reports/windows-cma-log.html
    cp output.xml ../reports/windows-cma-output.xml
fi
