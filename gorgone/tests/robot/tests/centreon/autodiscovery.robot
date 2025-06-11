*** Settings ***
Documentation       test gorgone autodiscovery module
Library             DatabaseLibrary
Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s


*** Test Cases ***
check autodiscovery ${communication_mode}
    [Documentation]    Check engine autodiscovery module
    ${central}=    Set Variable    ${communication_mode}_gorgone_central_discovery
    ${poller}=    Set Variable    ${communication_mode}_gorgone_poller2_discovery
    @{process_list}    Create List    ${central}    ${poller}
    [Teardown]    Test Teardown    @{process_list}
    Test Setup    ${communication_mode}    @{process_list}

    ${response}=    POST  http://127.0.0.1:8085/api/centreon/autodiscovery/services    data={}
    Log To Console    ${response.json()}
    Dictionary Should Not Contain Key  ${response.json()}    error    api/centreon/statistics/engine api call resulted in an error : ${response.json()}

    Check Services Data Are Present In Database

    #Test Host disco

    Examples:    communication_mode   --
        ...    push_zmq

*** Keywords ***
Check Services Data Are Present In Database
    # check the central node has the data
    Check Row Count    SELECT * FROM service WHERE service_description like 'Disk-%';    equal    5    alias=conf    retry_timeout=5    retry_pause=1
    Check Row Count    SELECT service_description FROM service WHERE service_description like 'Disk-/run/user/1001';    equal    1    alias=conf    retry_timeout=5    retry_pause=1

Test Host disco
    ${http_body}=    Set Variable    {"variables":[],"parameters":{},"content":{"plugins":{"centreon-plugin-Applications-Protocol-Snmp":20250300},"post_execution":{"commands":{"command_line":"/usr/share/centreon/www/modules/centreon-autodiscovery-server/script/run_save_discovered_host --all --job-id=10 --export-conf","action":"COMMAND"}},"execution":{"mode":0},"command_line":"echo '{ \"discovered_items\": 9, \"duration\": 0, \"end_time\": 1749650750, \"results\": [ { \"interface_alias\": \"\", \"interface_description\": \"lo\", \"interface_name\": \"lo\", \"ip\": \"127.0.0.2\", \"netmask\": \"\", \"type\": \"ipv4\" } ], \"start_time\": 1749650750 }'","target":1,"job_id":10}}
    ${response}=    POST  http://127.0.0.1:8085/api/nodes/1/centreon/autodiscovery/hosts   data=${http_body}
    # For now this don't work for 2 reasons :
    # - the json body don't seem correct, in bash you can replace the internal ' by '"'"' to make the curl work
    # - gorgone seem to miss some conf and return me this error :
    # Use of uninitialized value $options{"job_id"} in numeric eq (==) at /centreon-collect/gorgone/gorgone/modules/centreon/autodiscovery/class.pm line 311.
    # Use of uninitialized value $options{"job_id"} in numeric eq (==) at /centreon-collect/gorgone/gorgone/modules/centreon/autodiscovery/class.pm line 311.
    # Use of uninitialized value in concatenation (.) or string at /centreon-collect/gorgone/gorgone/modules/centreon/autodiscovery/class.pm line 373.
    # 2025-06-11 15:14:02 - ERROR - [autodiscovery] -class- host discovery - cannot get host discovery job '' - configuration missing
    # Use of uninitialized value in concatenation (.) or string at /centreon-collect/gorgone/gorgone/modules/centreon/autodiscovery/class.pm line 377.

Test Teardown
    [Arguments]    @{process_list}
    Gorgone Execute Sql    ${ROOT_CONFIG}db-delete-autodiscovery.sql
    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    Stop Mockoon

Test Setup
    [Arguments]    ${communication_mode}    @{process_list}
    #Start Mockoon    ${ROOT_CONFIG}..${/}resources/web-api-mockoon.json
    #Gorgone Execute Sql    ${ROOT_CONFIG}db-delete-autodiscovery.sql
    Gorgone Execute Sql    ${ROOT_CONFIG}db-insert-autodiscovery.sql

    @{central_config}    Create List    ${ROOT_CONFIG}autodiscovery.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List    ${ROOT_CONFIG}actions.yaml
    Setup Two Gorgone Instances    central_config=${central_config}    communication_mode=${communication_mode}    central_name=${process_list}[0]    poller_name=${process_list}[1]    poller_config=${poller_config}


    Connect To Database    pymysql    ${DBNAME}    ${DBUSER}    ${DBPASSWORD}    ${DBHOST}    ${DBPORT}
    ...    alias=conf    autocommit=True
