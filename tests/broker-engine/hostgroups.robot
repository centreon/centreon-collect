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
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    Step1
    FOR    ${index}    IN RANGE    60
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
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id
        ...    WHERE h.hostgroup_id = ${1}
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
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id.
        ...    WHERE h.hostgroup_id = ${1}

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
        ...    SELECT name, host_id FROM hostgroups h JOIN hosts_hostgroups hg ON h.hostgroup_id = hg.hostgroup_id
        ...    WHERE h.hostgroup_id = ${1}
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

EBHGLUA
    [Documentation]    A host group is configured through several pollers. A streamconnector checks this hostgroup.
    ...    And poller1 hosts are removed
    ...    And poller2 hosts are removed
    ...    Then when the hostgroup is no more needed, it is removed from the streamconnector cache.
    [Tags]    broker    engine    hostgroup    unified_sql

    Remove File    ${/}tmp${/}test-lua.log

    Ctn Config Engine    ${2}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Broker Config Log    central    lua    debug

    Ctn Config BBDO3    ${2}

    # host_1 and host_2 from poller0 and host_26 from poller1 are added to a hostgroup
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2"]
    Ctn Add Host Group    ${1}    ${1}    ["host_26"]

    ${sc}    Catenate    SEPARATOR=\n
    ...    broker_api_version = 2
    ...
    ...    function init(params)
    ...        broker_log:set_parameters(2, '/tmp/test-lua.log')
    ...        broker_log:info(0, 'lua start test')
    ...    end
    ...
    ...    function write(e)
    ...        if e._type == 65566 or e._type == 65568 then        -- Host or Host Status
    ...            broker_log:info(0, "broker_cache hostgroup name: " .. tostring(broker_cache:get_hostgroup_name(1)))
    ...        end
    ...        return true
    ...    end

    # Create the lua script file
    Create File    /tmp/test-lua.lua    ${sc}

    Ctn Broker Config Add Lua Output    central    test-lua    /tmp/test-lua.lua

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${2}
    Ctn Schedule Forced Host Check    host_4
    Sleep    5s

    ${content}    Create List    broker_cache hostgroup name: hostgroup_1
    ${result}    Ctn Find In Log With Timeout    /tmp/test-lua.log    ${start}    ${content}    30
    Should Be True    ${result}    Host group name not known by the streamconnector.

    Log To Console    hostgroup_1 is removed from poller1
    Ctn Engine Config Remove Host    ${1}    host_26
    Ctn Engine Config Remove Services By Host    ${1}    host_26
    Ctn Engine Config Remove Hostgroup    ${1}    hostgroup_1

    ${start}    Ctn Get Round Current Date
    Ctn Reload Broker
    Ctn Reload Engine

    Ctn Schedule Forced Host Check    host_4
    Sleep    5s

    ${content}    Create List    broker_cache hostgroup name: hostgroup_1
    ${result}    Ctn Find In Log With Timeout    /tmp/test-lua.log    ${start}    ${content}    30
    Should Be True    ${result}    Host group name not known by the streamconnector.

    Log To Console    hostgroup_1 is removed from poller0
    Ctn Engine Config Remove Hostgroup    ${0}    hostgroup_1

    ${start}    Ctn Get Round Current Date
    Ctn Reload Broker
    Ctn Reload Engine

    ${content}    Create List    broker_cache hostgroup name: nil
    ${result}    Ctn Find In Log With Timeout    /tmp/test-lua.log    ${start}    ${content}    30
    Should Be True    ${result}    Host group name still known by the streamconnector.
