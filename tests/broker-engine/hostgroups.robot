*** Settings ***
Documentation       Centreon Broker and Engine add Hostgroup

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BENHG1
    [Documentation]    GIVEN a Centreon platform with 3 Engine instances
    ...                AND Broker is configured with RRD, central and module outputs
    ...                AND the central broker has 5 database connections
    ...                WHEN I create a host group containing 3 hosts
    ...                AND I reload both Broker and Engine configurations
    ...                THEN the membership of all 3 hosts to the host group should be logged
    ...                AND all membership entries should appear within 45 seconds
    [Tags]    broker    engine    hostgroup
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
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

BENHGU1
    [Documentation]    GIVEN a Centreon platform with 3 Engine instances
    ...                AND Broker is configured with RRD, central and module outputs
    ...                AND Broker uses unified_sql output for database operations
    ...                AND SQL logging is enabled at info level
    ...                AND the unified_sql output has 5 database connections
    ...                WHEN I create a host group containing 3 hosts
    ...                AND I reload both Broker and Engine configurations
    ...                THEN the membership of all 3 hosts to the host group should be logged
    ...                AND all membership entries should appear within 45 seconds
    [Tags]    broker    engine    hostgroup    unified_sql
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
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

BENHGU2
    [Documentation]    GIVEN a platform with 3 Engine instances and unified_sql output with 5 connections
    ...                AND BBDO3 protocol is enabled
    ...                WHEN I create a host group with 3 hosts and reload configurations
    ...                THEN at least 2 host memberships should be logged within 45 seconds
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

BENHGU3
    [Documentation]    GIVEN a platform with 4 Engine instances and unified_sql output with 5 connections
    ...                AND BBDO3 protocol is enabled with SQL debug logging
    ...                WHEN I create host group 1 across 4 pollers with 3 hosts each and reload
    ...                THEN host group 1 should contain 12 host members within 30 seconds
    ...                WHEN I remove the hostgroups configuration from poller 0 and reload
    ...                THEN host group 1 should contain only 9 host members within 30 seconds
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

BENHG4
    [Documentation]    GIVEN a platform with 3 Engine instances and unified_sql output with 5 connections
    ...    AND detailed logging is enabled on module0 (neb debug, core and processing error)
    ...    WHEN I create host group 1 with 3 hosts and reload configurations
    ...    THEN at least 2 host memberships should be logged within 45 seconds
    ...    WHEN I rename host group 1 to "hostgroup_test" and reload configurations
    ...    THEN the hostgroup name should be updated in database within 60 seconds
    [Tags]    broker    engine    hostgroup
    Ctn Config Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module0    core   error
    Ctn Broker Config Log    module0    processing   error
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    ${start}    Get Current Date
    log to console    Starting date: ${start}
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${3}
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

    Ctn Rename Host Group    ${0}    ${1}    test    ["host_1", "host_2", "host_3"]

    Sleep    10s
    ${start}    Get Current Date
    Ctn Reload Broker
    Ctn Reload Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        Log To Console    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
        ${output}    Query    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('hostgroup_test',),)"    BREAK
    END
    Disconnect From Database
    Should Be Equal As Strings    ${output}    (('hostgroup_test',),)

BENHGU4_${test_label}
    [Documentation]    GIVEN a platform with 3 Engine instances and unified_sql output with 5 connections
    ...    AND detailed trace/debug logging is enabled (sql, lua, core)
    ...    AND a Lua output dumps host groups to /tmp/lua-engine.log
    ...    AND BBDO protocol version is configured based on test parameter
    ...    WHEN I create host group 1 with 3 hosts and reload configurations
    ...    THEN all 3 host memberships should be logged and stored in database within 60 seconds
    ...    AND the hostgroup should appear in the Lua output file
    ...    WHEN I rename host group 1 to "hostgroup_test" and reload configurations
    ...    THEN the hostgroup name should be updated in database within 60 seconds
    ...    AND the renamed hostgroup should appear in the Lua output file
    ...    WHEN I remove the host group configuration and reload
    ...    THEN the hostgroup should be deleted from database within 60 seconds
    ...    AND no hostgroup should appear in the Lua output file after 10 seconds
    [Tags]    broker    engine    hostgroup
    Ctn Config Engine    ${3}
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    lua    trace
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Source Log    central    1
    Ctn Broker Config Source Log    module0    1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    queries_per_transaction    5
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
    Disconnect From Database

    Examples:    Use_BBDO3    test_label    --
    ...    True    BBDO3
    ...    False    BBDO2
