*** Settings ***
Documentation       Centreon Broker and Engine add Hostgroup

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BECNHG1
    [Documentation]    Given a centralized engine with 3 pollers
    ...    And broker is configured with RRD, central module, and SQL debug logging
    ...    And database connections are set to 5 for both SQL and perfdata outputs
    ...    When I start the broker and engine with new generation
    ...    And I add a host group containing 3 hosts (host_1, host_2, host_3)
    ...    And I notify broker of the engine configuration change
    ...    Then the logs should confirm membership of all 3 hosts to the host group
    ...    And each host should be properly associated with host group 1 on instance 1
    [Tags]    broker    engine    hostgroup    MON-153802
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    debug
    Ctn Config BBDO3    ${3}
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${3}
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]
    Ctn Notify Broker Of Engine Config Change    0

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1
    ...    enabling membership of host 1 to host group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

BECNHG3
    [Documentation]    Scenario: Host group synchronization across 4 pollers in centralized configuration
    ...    Given 4 pollers and Broker are started in centralized mode
    ...    When hostgroup_1 is added with 3 hosts per poller (12 total)
    ...    Then Broker receives all 12 hosts as hostgroup_1 members
    ...    When hostgroup configuration files are removed sequentially from each poller
    ...    Then Broker progressively removes corresponding hosts from database
    ...    And hostgroup_1 membership decreases from 12 → 9 → 6 → 3 → 0

    [Tags]    broker    engine    hostgroup    MON-153802
    Ctn Config Centralized Engine    ${4}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${4}

    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Prot Files
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]
    Ctn Add Host Group    ${1}    ${1}    ["host_21", "host_22", "host_23"]
    Ctn Add Host Group    ${2}    ${1}    ["host_31", "host_32", "host_33"]
    Ctn Add Host Group    ${3}    ${1}    ["host_41", "host_42", "host_43"]
    FOR    ${i}    IN RANGE    4
        Ctn Notify Broker Of Engine Config Change    ${i}
    END

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT COUNT(*) FROM hosts_hostgroups WHERE hostgroup_id=1    ==    ${12}    retry_timeout=30s    retry_pause=2s
    Disconnect From Database

    FOR    ${i}    IN RANGE    1
        Log To Console	  Remove hostgroup on poller ${i + 1}
        Ctn Config Engine Remove Cfg File    ${i}    hostgroups.cfg
        Ctn Notify Broker Of Engine Config Change    ${i}

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${expected}    Evaluate    9 - 3 * ${i}
        Check Query Result    SELECT COUNT(*) FROM hosts_hostgroups WHERE hostgroup_id=1    ==    ${expected}    retry_timeout=30s    retry_pause=2s
        Disconnect From Database
    END

BECNHG4
    [Documentation]    Scenario: Host group rename synchronization in centralized configuration
    ...    Given 3 pollers and Broker are started in centralized mode
    ...    When hostgroup_1 is created on poller 1 with hosts: host_1, host_2, host_3
    ...    Then Broker receives hostgroup_1 with its 3 members
    ...    When hostgroup_1 is renamed to hostgroup_test
    ...    Then Broker updates the hostgroup name to hostgroup_test in database
    ...    And the same 3 hosts remain as members of hostgroup_test

    [Tags]    broker    engine    hostgroup    MON-153802
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module0    core   error
    Ctn Broker Config Log    module0    processing   error
    Ctn Config BBDO3    ${3}
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5

    Ctn Clear Prot Files

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]
    Ctn Notify Broker Of Engine Config Change    0

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

    Sleep    10s

    Ctn Rename Host Group    ${0}    ${1}    test    ["host_1", "host_2", "host_3"]
    Ctn Notify Broker Of Engine Config Change    0

    ${start}    Ctn Get Round Current Date

    Log To Console    Step1
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}    ==    hostgroup_test    retry_timeout=60s    retry_pause=2s
    Check Query Result    SELECT COUNT(*) FROM hosts_hostgroups WHERE hostgroup_id = ${1}    ==    ${3}    retry_timeout=30s    retry_pause=2s
    Disconnect From Database

BENHGU4_${test_label}
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
