#
# Copyright 2024 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#

*** Settings ***
Documentation       test gorgone action module on local and distant target
Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s

# @TODO : I know it's possible to have a remote server managing some poller. For now we don't test this case, but it should be tested and documented.
*** Test Cases ***
action module with ${communication_mode} communcation mode
    [Documentation]    test action on distant node, no whitelist configured
    @{process_list}    Create List    ${communication_mode}_gorgone_central    ${communication_mode}_gorgone_poller_2
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    Run    rm /tmp/actionLogs

    @{central_config}    Create List    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List     ${ROOT_CONFIG}actions.yaml
    Setup Two Gorgone Instances
    ...    central_config=${central_config}
    ...    communication_mode=${communication_mode}
    ...    central_name=${communication_mode}_gorgone_central
    ...    poller_name=${communication_mode}_gorgone_poller_2
    ...    poller_config=${poller_config}

    # first we test the api without waiting for the output of the command.
    # check by default the api launch the query in local
    Test Async Action Module
    # check the central can execute a command and send back the output
    Test Async Action Module    node_path=nodes/1/
    # check a distant poller can execute a command and send back the output
    ${start_date}    Get Current Date    increment=-10s
    Test Async Action Module    node_path=nodes/2/
    # we need to check it is the poller and not the central that have done the action.
    ${log_poller2_query}    Create List    Robot test write with param: for node nodes/2/
    ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${communication_mode}_gorgone_poller_2/gorgoned.log    content=${log_poller2_query}    date=${start_date}    timeout=10
    Should Be True    ${logs_poller}    Didn't found the logs in the poller file : ${logs_poller}

    # Now we test the action api by waiting for the command output in one call.
    # This make gorgone wait for 3 seconds before querying for logs, and wait again 0.5 seconds for log to be received by central.
    # On my machine the sync_wait was at least 0.22 seconds to work sometime, it always worked with 0.5s.
    # In real world where poller is not on the same server the delay will be greater and more random,
    # so the async method should be privileged.
    ${get_params}=    Set Variable    ?log_wait=3000000&sync_wait=500000
    Test Sync Action Module    get_params=${get_params}
    Test Sync Action Module    get_params=${get_params}    node_path=nodes/1/
    Test Sync Action Module    get_params=${get_params}    node_path=nodes/2/
    # we need to check it is the poller and not the central that have done the action.
    ${start_date}    Get Current Date        increment=-10s
    ${log_poller2_query_sync}    Create List    Robot test write with param:${get_params} for node nodes/2/
    ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${communication_mode}_gorgone_poller_2/gorgoned.log    content=${log_poller2_query_sync}    date=${start_date}    timeout=10
    Should Be True    ${logs_poller}    Didn't found the logs in the poller file: ${logs_poller}

    Examples:    communication_mode   --
        ...    push_zmq
        ...    pullwss
        ...    pull

*** Keywords ***
Test Sync Action Module
    [Arguments]    ${get_params}=    ${node_path}=

    ${action_api_result}=    Post Action Endpoint    node_path=${node_path}    get_params=${get_params}
    ${status}    ${logs}    Parse Json Response    ${action_api_result}
    Check Action Api Do Something    ${status}    ${logs}    ${node_path}    ${get_params}


Test Async Action Module
    [Documentation]    This make an api call to write to a dummy file and output a string. as gorgone central and poller and robot are executed on the same host we can access the file to check the result.
    [Arguments]    ${node_path}=${EMPTY}
    ${action_api_result}=    Post Action Endpoint    node_path=${node_path}
    # need to get the data from the token with getlog.
    # this call multiples time the api until the response is available.
    ${status}    ${logs}    Ctn Get Api Log With Timeout    token=${action_api_result.json()}[token]    node_path=${node_path}
    Check Action Api Do Something    ${status}    ${logs}    ${node_path}    ${EMPTY}


Post Action Endpoint
    [Arguments]    ${node_path}=${EMPTY}    ${get_params}=${EMPTY}

    # Ideally, Gorgone should not allow any bash interpretation on command it execute.
    # As there is a whitelist in gorgone, if there was no bash interpretation we could allow only our required binary and be safe.
    # As gorgone always had bash interpretation available, most of the internal use of this module use redirection, pipe or other sh feature.
    ${bodycmd}=    Create Dictionary    command=echo 'Robot test write with param:${get_params} for node ${node_path}' | tee -a /tmp/actionLogs
    ${body}=    Create List    ${bodycmd}
    ${result}    POST    http://127.0.0.1:8085/api/${node_path}core/action/command${get_params}    json=${body}
    RETURN    ${result}


Check Action Api Do Something
    [Arguments]    ${status}    ${logs}    ${node_path}    ${get_params}

    Should Be True    ${status}    No log found in the gorgone api or the command failed.
    # the log api send back a json containing a list of log, with for each logs the token, id, creation time (ctime), status code(code), and data (among other thing)
    # data is a stringified json that need to be evaluated separately.
    ${internal_json}=    Evaluate     json.loads("""${logs}[data]""")    json

    Should Be Equal As Numbers    0    ${internal_json}[result][exit_code]
    Should Be Equal As Strings
    ...    Robot test write with param:${get_params} for node ${node_path}
    ...    ${internal_json}[result][stdout]
    ...    output of the gorgone action api should be the bash command output. 

    ${file_nb_line}=    Run    grep 'Robot test write with param:${get_params} for node ${node_path}\$' /tmp/actionLogs | wc -l
    Should Be Equal    1    ${file_nb_line}    command launched with gorgone api should set only one line in the file per tests
