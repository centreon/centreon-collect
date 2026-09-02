*** Settings ***
Documentation       Centreon Broker and Engine add Hostgroup

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
EBNHG1
    [Documentation]    New host group with several pollers and connections to DB
    [Tags]    broker    engine    hostgroup    unstable
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Output Set    central    central-broker-master-sql    connections_count    5
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    connections_count    5
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Sleep    3s
    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1
    ...    enabling membership of host 1 to host group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

EBNHGU1
    [Documentation]    New host group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    hostgroup    unified_sql
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Sleep    3s
    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1
    ...    enabling membership of host 1 to host group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

EBNHGU2
    [Documentation]    New host group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    hostgroup    unified_sql
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Ctn Config BBDO3    3
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Sleep    3s
    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

EBNHGU3
    [Documentation]    New host group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    hostgroup    unified_sql
    Ctn Config Engine    ${4}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${4}

    Ctn Broker Config Log    central    sql    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Ctn Config BBDO3    4
    Ctn Broker Config Log    central    sql    debug

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]
    Ctn Add Host Group    ${1}    ${1}    ["host_21", "host_22", "host_23"]
    Ctn Add Host Group    ${2}    ${1}    ["host_31", "host_32", "host_33"]
    Ctn Add Host Group    ${3}    ${1}    ["host_41", "host_42", "host_43"]

    Sleep    3s
    Ctn Reload Broker
    Ctn Reload Engine

    ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts    1    12    30
    Should Be True    ${result}    We should have 12 hosts members in the hostgroup 1.

    Ctn Config Engine Remove Cfg File    ${0}    hostgroups.cfg

    Sleep    3s
    Ctn Reload Broker
    Ctn Reload Engine
    ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts    1    9    30
    Should Be True    ${result}    We should have 9 hosts members in the hostgroup 1.

EBNHG4
    [Documentation]    New host group with several pollers and connections to DB with broker and rename this hostgroup
    [Tags]    broker    engine    hostgroup
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Output Set    central    central-broker-master-sql    connections_count    5
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    connections_count    5
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Sleep    3s
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

    Ctn Rename Host Group    ${0}    ${1}    test    ["host_1", "host_2", "host_3"]

    Sleep    10s
    ${start}    Get Current Date
    Log To Console    Step-1
    Ctn Reload Broker
    Log To Console    Step0
    Ctn Reload Engine

    Log To Console    Step1
    Log To Console    Step1
    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
        ${output}    Query    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('hostgroup_test',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('hostgroup_test',),)

EBNHGU4_${test_label}
    [Documentation]    New host group with several pollers and connections to DB with broker and rename this hostgroup
    [Tags]    broker    engine    hostgroup
    Ctn Config Engine    ${3}
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    lua    trace
    Ctn Broker Config Log    central    core    trace
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

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    ${start}    Ctn Get Round Current Date
    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${loop_index}    IN RANGE    60
        Log To Console
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}
        ${output}    Query
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}
        Log To Console    ${output}
        ${grep_result}    Grep File    /tmp/lua-engine.log    host_group_name:hostgroup_1
        Sleep    1s

        IF    "${output}" == "(('hostgroup_1', 1), ('hostgroup_1', 2), ('hostgroup_1', 3))" and len("""${grep_result}""") > 10
            BREAK
        END
    END

    Should Be Equal As Strings
    ...    ${output}
    ...    (('hostgroup_1', 1), ('hostgroup_1', 2), ('hostgroup_1', 3))
    ...    host groups not created in database

    Should Be True    len("""${grep_result}""") > 10    hostgroup_1 not found in /tmp/lua-engine.log

    Ctn Rename Host Group    ${0}    ${1}    test    ["host_1", "host_2", "host_3"]

    Sleep    3s
    Ctn Reload Engine
    Ctn Reload Broker

    Log To Console    hostgroup_1 renamed to hostgroup_test

    FOR    ${index}    IN RANGE    60
        Log To Console
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}

        ${output}    Query
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}

        Log To Console    ${output}
        ${grep_result}    Grep File    /tmp/lua-engine.log    host_group_name:hostgroup_test
        Sleep    1s
        IF    "${output}" == "(('hostgroup_test', 1), ('hostgroup_test', 2), ('hostgroup_test', 3))" and len("""${grep_result}""") > 10
            BREAK
        END
    END
    Should Be Equal As Strings
    ...    ${output}
    ...    (('hostgroup_test', 1), ('hostgroup_test', 2), ('hostgroup_test', 3))
    ...    hostgroup_test not found in database

    Should Be True    len("""${grep_result}""") > 10    hostgroup_1 not found in /tmp/lua-engine.log

    # remove hostgroup
    Ctn Config Engine    ${3}
    Ctn Reload Engine
    Ctn Reload Broker

    Log To Console    remove hostgroup

    FOR    ${index}    IN RANGE    60
        Log To Console
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}
        ${output}    Query
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    hostgroup_test not deleted

    # Waiting to observe no host group.
    FOR    ${index}    IN RANGE    60
        Create File    /tmp/lua-engine.log
        Sleep    1s
        ${grep_result}    Grep File    /tmp/lua-engine.log    no host_group_name
        IF    len("""${grep_result}""") > 0    BREAK
    END
    Sleep    10s
    # Do we still have no host group?
    ${grep_result}    Grep File    /tmp/lua-engine.log    host_group_name:
    Should Be True    len("""${grep_result}""") == 0    The hostgroup 1 still exists

    Examples:    Use_BBDO3    test_label    --
    ...    True    BBDO3
    ...    False    BBDO2


EBNHG5
    [Documentation]    Scenario: Host group creation and membership updates with unified SQL and BBDO3
    ...    And a host group 1 is defined with 7 members across instances (hosts host_1 to host_7)
    ...    When the broker and engine start and the engine becomes ready
    ...    And the engine configuration file hostgroups.cfg is added and the engine is reloaded
    ...    Then the system reports 7 relations between hostgroup 1 and its hosts
    ...    When host host_1 is removed and hostgroup_1 members are updated to exclude host_1
    ...    And the engine is reloaded
    ...    Then the system reports 6 relations between hostgroup 1 and its hosts
    [Tags]    broker    engine    hostgroup    unified_sql    MON-191814
    Ctn Config Engine    ${1}    ${10}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    sql    info

    Ctn Clear Retention

    Ctn Add Host Group    ${0}    ${1}    ["host_1","host_2","host_3","host_4","host_5","host_6","host_7"]
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}

    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts    1    7    30
    Should Be True    ${result}    We should have 7 hosts members in the hostgroup 1.

    # delete host_1 and update hostgroup members
    Ctn Remove Host    ${0}    host_1
    Ctn Remove Service    0    host_1    service_1

    Ctn Engine Config Delete Key In Cfg    0    hostgroup_1    members    hostgroups.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostgroup_1    members    host_2,host_3,host_4,host_5,host_6,host_7    hostgroups.cfg

    Ctn Reload Engine

    ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts    1    6    30
    Should Be True    ${result}    We should have 6 hosts members in the hostgroup 1.

EBNHG6
    [Documentation]    Scenario: Two pollers are connected, hostgroup 1 is defined on poller 0 with host_1 and
    ...                host_2 as members. Both hosts are then moved from poller 0's configuration to poller 1's
    ...                configuration, poller 1 is reloaded first, then poller 0. The hostgroup and its host
    ...                memberships must not disappear from the database.
    [Tags]    broker    engine    hostgroup    unified_sql    MON-169103
    Ctn Config Engine    ${2}    ${5}    ${0}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    ${2}

    Ctn Broker Config Log    central    sql    trace
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2"]

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${2}

    ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts    1    2    30
    Should Be True    ${result}    We should have 2 hosts members in the hostgroup 1 before the move.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query    SELECT count(*) FROM hostgroups WHERE hostgroup_id = 1
    Should Be Equal As Strings    ${output}    ((1,),)    The hostgroup 1 should exist before the move.

    # Move host_1 and host_2 from poller 0's configuration to poller 1's configuration.
    Ctn Engine Config Move Host To Engine    0    1    host_1
    Ctn Engine Config Move Host To Engine    0    1    host_2

    Ctn Add Host Group    ${1}    ${1}    ["host_1", "host_2"]

    # Reload the second poller first, then the first poller.
    Ctn Reload Engine    1
    Sleep    1
    Ctn Reload Engine    0

    Sleep    5

    ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts    1    2    30
    Should Be True
    ...    ${result}
    ...    host_1 and host_2 should still be members of hostgroup 1 in hosts_hostgroups after the move.

    ${output}    Query    SELECT count(*) FROM hostgroups WHERE hostgroup_id = 1
    Should Be Equal As Strings    ${output}    ((1,),)    The hostgroup 1 should still exist after the move.

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
