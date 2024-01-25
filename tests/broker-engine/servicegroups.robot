*** Settings ***
Documentation       Centreon Broker and Engine add servicegroup

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Stop Engine Broker And Save Logs


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
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Sleep    3s

    Reload Broker
    Reload Engine

    ${content}    Create List
    ...    enabling membership of service (1, 3) to service group 1 on instance 1
    ...    enabling membership of service (1, 2) to service group 1 on instance 1
    ...    enabling membership of service (1, 1) to service group 1 on instance 1

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new service groups not found in logs.

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
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Sleep    3s

    Reload Broker
    Reload Engine

    ${content}    Create List
    ...    enabling membership of service (1, 3) to service group 1 on instance 1
    ...    enabling membership of service (1, 2) to service group 1 on instance 1
    ...    enabling membership of service (1, 1) to service group 1 on instance 1

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new service groups not found in logs.

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
    Config BBDO3    4
    Broker Config Log    central    sql    debug

    Clear Retention
    Start Broker
    Start Engine
    Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Add Service Group    ${1}    ${1}    ["host_14","service_261", "host_14","service_262","host_14", "service_263"]
    Add Service Group    ${2}    ${1}    ["host_27","service_521", "host_27","service_522","host_27", "service_523"]
    Add Service Group    ${3}    ${1}    ["host_40","service_781", "host_40","service_782","host_40", "service_783"]
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Config Engine Add Cfg File    ${1}    servicegroups.cfg
    Config Engine Add Cfg File    ${2}    servicegroups.cfg
    Config Engine Add Cfg File    ${3}    servicegroups.cfg
    Sleep    3s
    Reload Broker
    Reload Engine
    Sleep    3s

    ${result}    Check Number Of Relations Between Servicegroup And Services    1    12    30
    Should Be True    ${result}    We should get 12 relations between the servicegroup 1 and services.
    Config Engine Remove Cfg File    ${0}    servicegroups.cfg
    Reload Broker
    Reload Engine

    ${result}    Check Number Of Relations Between Servicegroup And Services    1    9    30
    Should Be True    ${result}    We should get 9 relations between the servicegroup 1 and services.

EBNSGU3_${test_label}
    [Documentation]    New service group with several pollers and connections to DB with broker and rename this servicegroup
    [Tags]    broker    engine    servicegroup unified_sql
    Config Engine    ${3}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}

    Broker Config Log    central    sql    trace
    Broker Config Log    central    lua    trace
    Broker Config Source Log    central    1
    Broker Config Source Log    module0    1
    Config Broker Sql Output    central    unified_sql    5
    Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Broker Config Add Lua Output    central    test-cache    ${SCRIPTS}test-dump-groups.lua
    Clear Retention

    Create File    /tmp/lua-engine.log

    IF    ${Use_BBDO3}    Config BBDO3    ${3}

    Config BBDO3    ${3}

    ${start}    Get Current Date
    Start Broker
    Start Engine
    Sleep    3s
    Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Add Service Group    ${1}    ${1}    ["host_18","service_341", "host_19","service_362","host_19", "service_363"]
    Add Service Group    ${2}    ${1}    ["host_35","service_681", "host_35","service_682","host_36", "service_706"]
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Config Engine Add Cfg File    ${1}    servicegroups.cfg
    Config Engine Add Cfg File    ${2}    servicegroups.cfg

    Reload Broker
    Reload Engine

    ${result}    Check Number Of Relations Between Servicegroup And Services    1    9    30
    Should Be True    ${result}    We should get 9 relations between the servicegroup 1 and services.

    FOR    ${loop_index}    IN RANGE    30
        ${grep_result}    Grep File    /tmp/lua-engine.log    service_group_name:servicegroup_1
        IF    len("""${grep_result}""") > 10    BREAK
        Sleep    1s
    END

    Should Be True    len("""${grep_result}""") > 10    servicegroup_1 not found in /tmp/lua-engine.log

    Rename Service Group    ${0}    servicegroup_1    servicegroup_test
    Rename Service Group    ${1}    servicegroup_1    servicegroup_test
    Rename Service Group    ${2}    servicegroup_1    servicegroup_test

    Reload Engine
    Reload Broker
    ${result}    Check Number Of Relations Between Servicegroup And Services    1    9    30    servicegroup_test
    Should Be True    ${result}    We should get 9 relations between the servicegroup 1 and services.

    Log To Console    \nservicegroup_1 renamed to servicegroup_test

    FOR    ${loop_index}    IN RANGE    30
        ${grep_result}    Grep File    /tmp/lua-engine.log    service_group_name:servicegroup_test
        IF    len("""${grep_result}""") > 10    BREAK
        Sleep    1s
    END

    Should Be True    len("""${grep_result}""") > 10    servicegroup_test not found in /tmp/lua-engine.log

    # remove servicegroup
    Config Engine    ${3}
    Reload Engine
    Reload Broker

    Log To Console    \nremove servicegroup

    ${result}    Check Number Of Relations Between Servicegroup And Services    1    0    30
    Should Be True    ${result}    still a relation between the servicegroup 1 and services.

    # clear lua file
    # this part of test is disable because group erasure is desactivated in macrocache.cc
    # it will be reactivated when global cache will be implemented
    # Create File    /tmp/lua-engine.log
    # Sleep    2s
    # ${grep_result}    Grep File    /tmp/lua-engine.log    no service_group_name 1
    # Should Be True    len("""${grep_result}""") < 10    servicegroup 1 still exist

    Examples:    Use_BBDO3    test_label    --
    ...    True    BBDO3
    ...    False    BBDO2
