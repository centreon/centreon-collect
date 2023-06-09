*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
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
EBNSVC1
    [Documentation]    New services with several pollers
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${3}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    module1    bbdo_version    3.0.1
    Broker Config Add Item    module2    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    FOR    ${i}    IN RANGE    ${3}
        Sleep    10s
        ${srv_by_host}=    Evaluate    20 + 4 * $i
        log to console    ${srv_by_host} services by host with 50 hosts among 3 pollers.
        Config Engine    ${3}    ${50}    ${srv_by_host}
        Reload Engine
        Reload Broker
        ${nb_srv}=    Evaluate    17 * (20 + 4 * $i)
        ${nb_res}=    Evaluate    $nb_srv + 17
        ${result}=    Check Number Of Resources Monitored by Poller is    ${1}    ${nb_res}    30
        Should Be True    ${result}    msg=Poller 1 should monitor ${nb_srv} services and 17 hosts.
        ${result}=    Check Number Of Resources Monitored by Poller is    ${2}    ${nb_res}    30
        Should Be True    ${result}    msg=Poller 2 should monitor ${nb_srv} services and 17 hosts.
        ${nb_srv}=    Evaluate    16 * (20 + 4 * $i)
        ${nb_res}=    Evaluate    $nb_srv + 16
        ${result}=    Check Number Of Resources Monitored by Poller is    ${3}    ${nb_res}    30
        Should Be True    ${result}    msg=Poller 3 should monitor ${nb_srv} services and 16 hosts.
    END
    Stop Engine
    Kindly Stop Broker
