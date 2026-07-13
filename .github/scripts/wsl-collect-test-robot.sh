#!/bin/bash

test_file=$1

export RUN_ENV=WSL
export JSON_TEST_PARAMS=$2
export HOST_NAME=`echo $JSON_TEST_PARAMS | jq -r .host`
export PWSH_PATH=`echo $JSON_TEST_PARAMS | jq -r .pwsh_path`
export WINDOWS_PROJECT_PATH=`echo $JSON_TEST_PARAMS | jq -r .current_dir`
export HOST_HOSTNAME=$HOST_NAME

export USED_ADDRESS=`ip route show | grep -i default | awk '{ print $3}'`
#in order to connect to windows we neeed to use windows ip
echo "127.0.0.1       localhost" > /etc/hosts
echo "${USED_ADDRESS}      ${HOST_NAME}" >> /etc/hosts

echo "##### /etc/hosts: ######"
cat /etc/hosts

echo "########################### activate python virtual env ###########################"
python3 -m venv /.venv
source /.venv/bin/activate
# Install Robotframework
apt-get install -y python3 python3-dev python3-pip

/.venv/bin/pip3 install -U robotframework robotframework-databaselibrary robotframework-examples robotframework-httpctrl robotframework-requests
/.venv/bin/pip3 install pymysql python-dateutil grpcio grpcio_tools psutil PyJWT

echo "##### Starting tests ##### with params: $JSON_TEST_PARAMS"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"

robot -e unstable $test_file
result=$?
echo "####################### End of Centreon Collect Robot Tests #######################"

if [ $result -ne 0 ]; then
    echo "Robot tests failed with exit code: $result"
    mkdir -p failed
    echo "Failure detected, saving logs in ${PWD}/../reports"
    
    # Create reports directory if it doesn't exist
    mkdir -p ../reports
    
    # Copy logs if they exist
    [ -d failed ] && cp -rp failed ../reports/windows-cma-failed
    [ -f log.html ] && cp log.html ../reports/windows-cma-log.html
    [ -f output.xml ] && cp output.xml ../reports/windows-cma-output.xml
    [ -f report.html ] && cp report.html ../reports/windows-cma-report.html
    
    echo "Logs saved to ../reports/"
    exit 1
else
    echo "All robot tests passed successfully"
fi
