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

    Test Service Disco
    Test Host disco    ${poller}

    Test Service Disco don't interpret bash
    #Test Host Disco don't interpret bash

    Examples:    communication_mode   --
        ...    push_zmq

*** Keywords ***
Test Service Disco
    ${response}=    POST  http://127.0.0.1:8085/api/centreon/autodiscovery/services    data={}
    Log To Console    ${response.json()}
    Dictionary Should Not Contain Key  ${response.json()}    error    api/centreon/statistics/engine api call resulted in an error : ${response.json()}
    Check Services Data Are Present In Database

Test Service Disco don't interpret bash
    Sleep    1

Check Services Data Are Present In Database
    # check service discovery added services in mysql config database.
    Check Row Count    SELECT * FROM service WHERE service_description like 'Disk-%';    equal    4    alias=conf    retry_timeout=5    retry_pause=1
    Check Row Count    SELECT service_description FROM service WHERE service_description = 'Disk-/home';    equal    1    alias=conf    retry_timeout=5    retry_pause=1

Test Host disco
    [Arguments]    ${poller_name}
    ${http_body}=    Get File    ${ROOT_CONFIG}${/}autodiscovery/host-http-body.json
    ${start_date}=   Get Current Date    increment=-1s
    ${response}=    POST  http://127.0.0.1:8085/api/nodes/1/centreon/autodiscovery/hosts   data=${http_body}
    Check Row Count    select * from mod_host_disco_host;    >    8    alias=conf    retry_timeout=20    retry_pause=1
    # check the poller made the call and not the central.
    ${query}    Create List    Message received external - [COMMAND]
    ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${poller_name}/gorgoned.log    content=${query}    date=${start_date}    timeout=10
    Should Be True    ${logs_poller}    Didn't found the logs in the poller file: /var/log/centreon-gorgone/${poller_name}/gorgoned.log

Test Teardown
    [Arguments]    @{process_list}
    #Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-delete-autodiscovery.sql
    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    Stop Mockoon

Test Setup
    [Arguments]    ${communication_mode}    @{process_list}
    Start Mockoon    ${ROOT_CONFIG}..${/}resources/web-api-mockoon.json

    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-delete-autodiscovery.sql

    @{central_config}    Create List    ${ROOT_CONFIG}autodiscovery${/}configuration-autodiscovery.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List    ${ROOT_CONFIG}actions.yaml
    Setup Two Gorgone Instances    central_config=${central_config}    communication_mode=${communication_mode}    central_name=${process_list}[0]    poller_name=${process_list}[1]    poller_config=${poller_config}
    # this file depend on the nagios_server table, which is created by the gorgone setup.
    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-insert-autodiscovery.sql
    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-insert-autodiscovery-2.sql


    Connect To Database    pymysql    ${DBNAME}    ${DBUSER}    ${DBPASSWORD}    ${DBHOST}    ${DBPORT}
    ...    alias=conf    autocommit=True
