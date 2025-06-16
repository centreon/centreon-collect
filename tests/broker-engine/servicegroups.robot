*** Settings ***
Documentation       Centreon Broker and Engine add servicegroup

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


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
    Ctn Start engine
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
    Ctn Start engine
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
    Ctn Config BBDO3    4
    Ctn Broker Config Log    central    sql    debug

    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start engine
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

EBNSGU3_${test_label}
    [Documentation]    New service group with several pollers and connections to DB with broker and rename this servicegroup
    [Tags]    broker    engine    servicegroup
    Ctn Config Engine    ${3}
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Clear Logs
    Ctn Clear Retention
    Ctn Truncate Resource Host Service

    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    lua    trace
    Ctn Broker Config Source Log    central    1
    Ctn Broker Config Source Log    module0    1
    Ctn Config Broker Sql Output    central    unified_sql    5
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Ctn Broker Config Add Lua Output    central    test-cache    ${SCRIPTS}test-dump-groups.lua

    Create File    /tmp/lua-engine.log

    IF    ${Use_BBDO3}    Ctn Config BBDO3    ${3}

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Ctn Add Service Group    ${1}    ${1}    ["host_18","service_341", "host_19","service_362","host_19", "service_363"]
    Ctn Add Service Group    ${2}    ${1}    ["host_35","service_681", "host_35","service_682","host_36", "service_706"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${1}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${2}    servicegroups.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Reload Broker
    Ctn Reload Engine

    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    9    30
    Should Be True    ${result}    We should get 9 relations between the servicegroup 1 and services.

    FOR    ${loop_index}    IN RANGE    30
        ${grep_result}    Grep File    /tmp/lua-engine.log    service_group_name:servicegroup_1
        IF    len("""${grep_result}""") > 10    BREAK
        Sleep    1s
    END

    Should Be True    len("""${grep_result}""") > 10    servicegroup_1 not found in /tmp/lua-engine.log

    ${content}    Create List    service group 'servicegroup_1' of id 1. Currently, this group is used by pollers [0-9]+, [0-9]+, [0-9]+
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result[0]}    The three pollers should be attached to the servicegroup 1.

    Ctn Rename Service Group    ${0}    servicegroup_1    servicegroup_test
    Ctn Rename Service Group    ${1}    servicegroup_1    servicegroup_test
    Ctn Rename Service Group    ${2}    servicegroup_1    servicegroup_test

    Ctn Reload Engine
    Ctn Reload Broker
    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    9    30    servicegroup_test
    Should Be True    ${result}    We should get 9 relations between the servicegroup 1 and services.

    Log To Console    \nservicegroup_1 renamed to servicegroup_test

    FOR    ${loop_index}    IN RANGE    30
        ${grep_result}    Grep File    /tmp/lua-engine.log    service_group_name:servicegroup_test
        IF    len("""${grep_result}""") > 10    BREAK
        Sleep    1s
    END

    Should Be True    len("""${grep_result}""") > 10    servicegroup_test not found in /tmp/lua-engine.log

    # remove servicegroup
    Ctn Config Engine    ${3}
    Ctn Reload Engine
    Ctn Reload Broker

    Log To Console    \nRemove servicegroup 1

    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    0    30
    Should Be True    ${result}    still a relation between the servicegroup 1 and services.

    # Waiting to observe no service group.
    FOR    ${index}    IN RANGE    60
        Create File    /tmp/lua-engine.log
        Sleep    1s
        ${grep_result}    Grep File    /tmp/lua-engine.log    no service_group_name
        IF    len("""${grep_result}""") > 0    BREAK
    END
    Sleep    10s
    # Do we still have no service group?
    ${grep_result}    Grep File    /tmp/lua-engine.log    service_group_name:
    Should Be True    len("""${grep_result}""") == 0    The servicegroup 1 still exists

    Examples:    Use_BBDO3    test_label    --
    ...    True    BBDO3
    ...    False    BBDO2
