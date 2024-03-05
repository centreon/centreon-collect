*** Settings ***
Documentation       Centreon Broker and Engine add servicegroup

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Test End


*** Test Cases ***
EBNSG1
    [Documentation]    New service group with several pollers and connections to DB
    [Tags]    broker    engine    servicegroup
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Output Set    central    central-broker-master-sql    connections_count    5
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    connections_count    5

    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Sleep    3s

    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of service (1, 3) to service group 1 on instance 1
    ...    enabling membership of service (1, 2) to service group 1 on instance 1
    ...    enabling membership of service (1, 1) to service group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new service groups not found in logs.

EBNSGU1
    [Documentation]    New service group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    servicegroup    unified_sql
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Broker Config Log    central    sql    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5

    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Sleep    3s

    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of service (1, 3) to service group 1 on instance 1
    ...    enabling membership of service (1, 2) to service group 1 on instance 1
    ...    enabling membership of service (1, 1) to service group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new service groups not found in logs.

EBNSGU2
    [Documentation]    New service group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    servicegroup    unified_sql
    Ctn Config Engine    ${4}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${4}

    Ctn Broker Config Log    central    sql    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module1    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module2    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module3    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    central    sql    debug

    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Ctn Add Service Group    ${1}    ${1}    ["host_14","service_261", "host_14","service_262","host_14", "service_263"]
    Ctn Add Service Group    ${2}    ${1}    ["host_27","service_521", "host_27","service_522","host_27", "service_523"]
    Ctn Add Service Group    ${3}    ${1}    ["host_40","service_781", "host_40","service_782","host_40", "service_783"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${1}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${2}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${3}    servicegroups.cfg
    Sleep    3s
    Ctn Reload Broker
    Ctn Reload Engine
    Sleep    3s

    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    12    30
    Should Be True    ${result}    We should get 12 relations between the servicegroup 1 and services.
    Ctn Config Engine Remove Cfg File    ${0}    servicegroups.cfg
    Ctn Reload Broker
    Ctn Reload Engine

    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    9    30
    Should Be True    ${result}    We should get 9 relations between the servicegroup 1 and services.

*** Keywords ***
Ctn Test End
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Save Logs If Failed
