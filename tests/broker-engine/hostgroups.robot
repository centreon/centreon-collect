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
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Engine Config Set Value    ${2}    log_level_config    debug
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    processing    error
    Ctn Broker Config Log    module1    core    error
    Ctn Broker Config Log    module1    processing    error
    Ctn Broker Config Log    module1    core    error
    Ctn Broker Config Log    module2    processing    error
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    processing    error
    Ctn Broker Config Log    central    lua    trace
    Ctn Broker Config Source Log    central    1
    Ctn Broker Config Source Log    module0    1
    Ctn Config Broker Sql Output    central    unified_sql    5
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Ctn Broker Config Add Lua Output    central    test-cache    ${SCRIPTS}test-dump-groups.lua
    Ctn Clear Logs
    Ctn Clear Retention

    Create File    /tmp/lua-engine.log

    IF    ${Use_BBDO3}    Ctn Config BBDO3    ${3}

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${3}

    Ctn Add Host Group    ${0}    ${1}    ["host_1"]
    Ctn Add Host Group    ${1}    ${1}    ["host_18"]
    Ctn Add Host Group    ${2}    ${1}    ["host_35"]

    ${start}    Ctn Get Round Current Date
    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List
    ...    enabling membership of host 1 to host group 1 on instance 1
    ...    enabling membership of host 35 to host group 1 on instance 3
    ...    enabling membership of host 18 to host group 1 on instance 2

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

    FOR    ${loop_index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id
        ...    WHERE h.hostgroup_id = ${1}
        ${output}    Query
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}
        ${grep_result}    Grep File    /tmp/lua-engine.log    host_group_name:hostgroup_1
        Sleep    1s

        Log To Console    ${output}
        Disconnect From Database
        IF    "${output}" == "(('hostgroup_1', 1), ('hostgroup_1', 18), ('hostgroup_1', 35))" and len("""${grep_result}""") > 10
            BREAK
        END
    END

    Log To Console    ${output}
    Should Be Equal As Strings
    ...    ${output}
    ...    (('hostgroup_1', 1), ('hostgroup_1', 18), ('hostgroup_1', 35))
    ...    host groups not created in database

    Should Be True    len("""${grep_result}""") > 10    hostgroup_1 not found in /tmp/lua-engine.log

    ${content}    Create List    host group 'hostgroup_1' of id 1. Currently, this group is used by pollers [0-9]+, [0-9]+, [0-9]+
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result[0]}    The three pollers should be attached to the hostgroup 1.

    Ctn Rename Host Group    ${0}    ${1}    test    ["host_1"]
    Ctn Rename Host Group    ${1}    ${1}    test    ["host_18"]
    Ctn Rename Host Group    ${2}    ${1}    test    ["host_35"]

    Sleep    3s
    Ctn Reload Engine
    Ctn Reload Broker

    Log To Console    hostgroup_1 renamed to hostgroup_test

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id.
        ...    WHERE h.hostgroup_id = ${1}

        ${output}    Query
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}

        Log To Console    ${output}
        ${grep_result}    Grep File    /tmp/lua-engine.log    host_group_name:hostgroup_test
        Sleep    1s
	Disconnect From Database
        IF    "${output}" == "(('hostgroup_test', 1), ('hostgroup_test', 18), ('hostgroup_test', 35))" and len("""${grep_result}""") > 10
            BREAK
        END
    END
    Should Be Equal As Strings
    ...    ${output}
    ...    (('hostgroup_test', 1), ('hostgroup_test', 18), ('hostgroup_test', 35))
    ...    hostgroup_test not found in database with members host_1, host_18, host_35

    Should Be True    len("""${grep_result}""") > 10    hostgroup_1 not found in /tmp/lua-engine.log

    ${content}    Create List    host group 'hostgroup_test' of id 1. Currently, this group is used by pollers [0-9]+, [0-9]+, [0-9]+
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result[0]}    The three pollers should be attached to the hostgroup 1 named now 'test'

    # remove hostgroup
    Ctn Config Engine    ${3}
    Ctn Reload Engine
    Ctn Reload Broker

    Log To Console    remove hostgroup

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id
        ...    WHERE h.hostgroup_id = ${1}
        ${output}    Query
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id WHERE h.hostgroup_id = ${1}
        Log To Console    ${output}
        Sleep    1s
	Disconnect From Database
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
