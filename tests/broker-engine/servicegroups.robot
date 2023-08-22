*** Settings ***
Documentation       Centreon Broker and Engine add servicegroup

Resource            ../resources/resources.resource
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
Test Teardown       Test End


*** Test Cases ***
EBNSG1
    [Documentation]    New service group with several pollers and connections to DB
    [Tags]    broker    engine    servicegroup
    Config Engine    ${3}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}

    Broker Config Log    central    sql    info
    Broker Config Output Set    central    central-broker-master-sql    connections_count    5
    Broker Config Output Set    central    central-broker-master-perfdata    connections_count    5

    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    Add service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Sleep    3s

    Reload Broker
    Reload Engine

    ${content}=    Create List
    ...    enabling membership of service (1, 3) to service group 1 on instance 1
    ...    enabling membership of service (1, 2) to service group 1 on instance 1
    ...    enabling membership of service (1, 1) to service group 1 on instance 1

    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    msg=One of the new service groups not found in logs.

EBNSGU1
    [Documentation]    New service group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    servicegroup    unified_sql
    Config Engine    ${3}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module

    Broker Config Log    central    sql    info
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    connections_count    5

    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    Add service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Sleep    3s

    Reload Broker
    Reload Engine

    ${content}=    Create List
    ...    enabling membership of service (1, 3) to service group 1 on instance 1
    ...    enabling membership of service (1, 2) to service group 1 on instance 1
    ...    enabling membership of service (1, 1) to service group 1 on instance 1

    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    msg=One of the new service groups not found in logs.

EBNSGU2
    [Documentation]    New service group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    servicegroup    unified_sql
    Config Engine    ${4}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${4}

    Broker Config Log    central    sql    info
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    module1    bbdo_version    3.0.0
    Broker Config Add Item    module2    bbdo_version    3.0.0
    Broker Config Add Item    module3    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug

    Clear Retention
    Start Broker
    Start Engine
    Add service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Add service Group    ${1}    ${1}    ["host_14","service_261", "host_14","service_262","host_14", "service_263"]
    Add service Group    ${2}    ${1}    ["host_27","service_521", "host_27","service_522","host_27", "service_523"]
    Add service Group    ${3}    ${1}    ["host_40","service_781", "host_40","service_782","host_40", "service_783"]
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Config Engine Add Cfg File    ${1}    servicegroups.cfg
    Config Engine Add Cfg File    ${2}    servicegroups.cfg
    Config Engine Add Cfg File    ${3}    servicegroups.cfg
    Sleep    3s
    Reload Broker
    Reload Engine
    Sleep    3s

    ${result}=    Check Number of relations between servicegroup and services    1    12    30
    Should Be True    ${result}    msg=We should get 12 relations between the servicegroup 1 and services.
    Config Engine Remove Cfg File    ${0}    servicegroups.cfg
    Reload Broker
    Reload Engine

    ${result}=    Check Number of relations between servicegroup and services    1    9    30
    Should Be True    ${result}    msg=We should get 9 relations between the servicegroup 1 and services.

*** Keywords ***
Test End
    Stop Engine
    Kindly Stop Broker
    Save logs If Failed
