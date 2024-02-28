*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             DatabaseLibrary
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
service
    [Documentation]    The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then engine and broker are started and broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that broker doesn't crash anymore.
    [Tags]    MON-18531    MON-35339    MON-35375    MON-24745
    Config Engine    ${1}    ${1}    ${25}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${content}    A message telling check_for_external_commands() should be available.
