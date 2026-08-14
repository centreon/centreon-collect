*** Settings ***
Documentation       Start and stop Gorgone with pull configuration

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        300s

*** Variables ***
@{process_list}    pull_gorgone_central    pull_gorgone_poller_2

*** Test Cases ***
connect 1 poller to a central with pull configuration
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database/delete_pollers.sql

    Log To Console    \nStarting the Gorgone setup with pull configuration
    Setup Two Gorgone Instances    communication_mode=pull    central_name=pull_gorgone_central    poller_name=pull_gorgone_poller_2

    Restart Poller And Check Connection    pull_gorgone_poller_2    conn_number=2

    Execute Sql String    delete from nagios_server where id=2;    alias=${DBNAME}
    ${body}=    Create List
    ${result}    POST    http://127.0.0.1:8085/api/centreon/nodes/sync    json=${body}

    Sleep    5
    Ctn Check No Error In Logs    pull_gorgone_central
    Ctn Check No Error In Logs    pull_gorgone_poller_2
    Log To Console    End of tests.

*** Keywords ***

Restart Poller And Check Connection
    [Arguments]    ${poller_name}    ${conn_number}=2    ${api_port}=${Capi_port}
    Stop Gorgone And Remove Gorgone Config
    ...    ${poller_name}

    Start Gorgone    debug    ${poller_name}
    Check Poller Is Connected    port=5556    expected_nb=${conn_number}
    Check Poller Communicate     ${conn_number}    api_port=${api_port}