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

    ${start}    Get Current Date

    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${content}    Set Variable    Rescheduling next check of host: host_14

    # We check a retry check rescheduling
    Process Host Check Result    host_14    1    host_14 is down

    ${result}    Check Reschedule With Timeout    ${engineLog0}    ${start}    ${content}    True    240
    Should Be True    ${result}    The delta between last_check and next_check is not equal to 60 as expected for a retry check

    # We check a normal check rescheduling
    ${start}    Get Current Date
    ${result}    Check Reschedule With Timeout    ${engineLog0}    ${start}    ${content}    False    240
    Should Be True    ${result}    The delta between last_check and next_check is not equal to 300 as expected for a normal check

    [Teardown]    Stop Engine Broker And Save Logs
