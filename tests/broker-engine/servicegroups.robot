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
    ${test_direct_grpc}    Ctn Is Using Direct Grpc
    IF    ${test_direct_grpc}
        Pass Execution    Test passes, skipping on direct grpc tests
    END

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
    Ctn Wait For Engine To Be Ready    ${start}    ${3}
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
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5

    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${3}

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
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${4}
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

    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    lua    trace
    Ctn Broker Config Source Log    central    1
    Ctn Broker Config Source Log    module0    1
    Ctn Config Broker Sql Output    central    unified_sql    5
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Ctn Broker Config Add Lua Output    central    test-cache    ${SCRIPTS}test-dump-groups.lua
    Ctn Clear Retention

    Create File    /tmp/lua-engine.log

    IF    ${Use_BBDO3}    Ctn Config BBDO3    ${3}

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${3}

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

EBSG_1
    [Documentation]    Scenario: Service group creation and membership updates with unified SQL and BBDO3
    ...    And a service group 1 is defined with 7 members across hosts (including host_1 services 1-5 and host_2 services 6-7)
    ...    When the broker and engine start and the engine becomes ready
    ...    And the engine configuration file servicegroups.cfg is added and the engine is reloaded
    ...    Then the system reports 7 relations between servicegroup 1 and its services
    ...    When service host_1/service_1 is removed and servicegroup_1 members are updated to exclude service_1
    ...    And the engine is reloaded
    ...    Then the system reports 6 relations between servicegroup 1 and its services
    [Tags]    broker    engine    servicegroup    MON-191814
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config BBDO3    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    sql    info

    Ctn Clear Retention
    
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_2","host_1","service_3","host_1","service_4","host_1","service_5","host_2","service_6", "host_2","service_7","host_1","service_1"]

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine
    
    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    7    30
    Should Be True    ${result}    We should get 4 relations between the servicegroup 1 and services.

    # delete the service 1 
    Ctn Remove Service    ${0}    host_1    service_1

    Ctn Engine Config Delete Key In Cfg    0    servicegroup_1    members   servicegroups.cfg
    Ctn Engine Config Set Key Value In Cfg    0    servicegroup_1    members    host_1,service_2,host_1,service_3,host_1,service_4,host_1,service_5,host_2,service_6,host_2,service_7    servicegroups.cfg

    Ctn Reload Engine

    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    6    30
    Should Be True    ${result}    We should get 3 relations between the servicegroup 1 and services.

EBNSG2
    [Documentation]    Scenario: Two pollers are connected, servicegroup 1 is defined on poller 0 with
    ...                host_1/service_1 and host_2/service_2 as members. Both hosts and their services are then
    ...                moved from poller 0's configuration to poller 1's configuration, poller 1 is reloaded first,
    ...                then poller 0. The servicegroup and its service memberships must not disappear from the
    ...                database.
    [Tags]    broker    engine    servicegroup    unified_sql    MON-169103
    Ctn Config Engine    ${2}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    ${2}

    Ctn Broker Config Log    central    sql    trace
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1","host_2","service_2"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${2}

    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    2    30
    Should Be True    ${result}    We should have 2 services members in the servicegroup 1 before the move.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query    SELECT count(*) FROM servicegroups WHERE servicegroup_id = 1
    Should Be Equal As Strings    ${output}    ((1,),)    The servicegroup 1 should exist before the move.

    # Move host_1 and host_2 with their services from poller 0's configuration to poller 1's configuration.
    Ctn Engine Config Move Host To Engine    0    1    host_1
    Ctn Engine Config Move Services To Engine    0    1    host_1
    Ctn Engine Config Move Host To Engine    0    1    host_2
    Ctn Engine Config Move Services To Engine    0    1    host_2

    Ctn Add Service Group    ${1}    ${1}    ["host_1","service_1","host_2","service_2"]
    Ctn Config Engine Add Cfg File    ${1}    servicegroups.cfg

    # Reload the second poller first, then the first poller.
    Ctn Reload Engine    1
    Sleep    1
    Ctn Reload Engine    0

    Sleep    5

    ${result}    Ctn Check Number Of Relations Between Servicegroup And Services    1    2    30
    Should Be True
    ...    ${result}
    ...    host_1/service_1 and host_2/service_2 should still be members of servicegroup 1 in services_servicegroups after the move.

    ${output}    Query    SELECT count(*) FROM servicegroups WHERE servicegroup_id = 1
    Should Be Equal As Strings    ${output}    ((1,),)    The servicegroup 1 should still exist after the move.

    ${output}    Query    SELECT instance_id FROM hosts WHERE name='host_1' AND enabled=1
    Should Be Equal As Strings
    ...    ${output}
    ...    ((2,),)
    ...    host_1 should be enabled and owned by poller 1 (instance_id=2) after the move.

    ${output}    Query    SELECT instance_id FROM hosts WHERE name='host_2' AND enabled=1
    Should Be Equal As Strings
    ...    ${output}
    ...    ((2,),)
    ...    host_2 should be enabled and owned by poller 1 (instance_id=2) after the move.

    ${output}    Query
    ...    SELECT h.instance_id FROM services s JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1' AND s.enabled=1
    Should Be Equal As Strings
    ...    ${output}
    ...    ((2,),)
    ...    service_1 on host_1 should be enabled and owned by poller 1 (instance_id=2) after the move.

    ${output}    Query
    ...    SELECT h.instance_id FROM services s JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_2' AND s.description='service_2' AND s.enabled=1
    Should Be Equal As Strings
    ...    ${output}
    ...    ((2,),)
    ...    service_2 on host_2 should be enabled and owned by poller 1 (instance_id=2) after the move.


