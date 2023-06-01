*** Settings ***
Documentation       Centreon Broker and Engine log_v2

Resource            ../resources/resources.robot
Library             DatabaseLibrary
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
ENRSCHE1
    [Documentation]    Verify that next check of a rescheduled host is made at last_check + interval_check
    [Tags]    broker    engine    scheduler
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_level_checks    debug
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}=    Get Current Date

    Start Broker
    Start Engine
    ${result}=    Check Connections
    Should Be True    ${result}    msg=Engine and Broker not connected

    ${pid}=    Get Process Id    e0
    ${content}=    Set Variable    Rescheduling next check of host: host_14

    ${result1}    ${result2}=    check reschedule with timeout    ${engineLog0}    ${start}    ${content}    240
    Should Be True    ${result1}    msg=the delta of last_check and next_check is not equal to 60.
    Should Be True    ${result2}    msg=the delta of last_check and next_check is not equal to 300.
    Stop Engine
    Kindly Stop Broker
