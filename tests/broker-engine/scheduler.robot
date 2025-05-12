*** Settings ***
Documentation       Centreon Broker and Engine log_v2

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
ENRSCHE1
    [Documentation]    Verify that next check of a rescheduled host is made at last_check + interval_check
    [Tags]    broker    engine    scheduler
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_checks    debug
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date

    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${content}    Set Variable    Rescheduling next check of host: host_14

    # We check a retry check rescheduling
    Ctn Process Host Check Result    host_14    1    host_14 is down

    ${result}    Ctn Check Reschedule With Timeout    ${engineLog0}    ${start}    ${content}    True    240
    Should Be True    ${result}    The delta between last_check and next_check is not equal to 60 as expected for a retry check

    # We check a normal check rescheduling
    ${start}    Get Current Date
    ${result}    Ctn Check Reschedule With Timeout    ${engineLog0}    ${start}    ${content}    False    240
    Should Be True    ${result}    The delta between last_check and next_check is not equal to 300 as expected for a normal check

    [Teardown]    Ctn Stop Engine Broker And Save Logs
