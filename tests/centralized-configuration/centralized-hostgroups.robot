*** Settings ***
Documentation       Centreon Broker and Engine add Hostgroup

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BECNHG1
    [Documentation]    Scenario: Host group synchronization across 3 pollers in centralized configuration
    ...    Given a centralized engine with 3 pollers
    ...    And broker is configured with RRD, central module, and SQL debug logging
    ...    And database connections are set to 5 for both SQL and perfdata outputs
    ...    When I start the broker and engine with new generation
    ...    And I add a host group containing 3 hosts (host_1, host_2, host_3)
    ...    And I notify broker of the engine configuration change
    ...    Then the logs should confirm membership of all 3 hosts to the host group
    ...    And each host should be properly associated with host group 1 on instance 1
    [Tags]    broker    engine    hostgroup    MON-153802
    Ctn Clear Prot Files
    Ctn Clear Broker Cache
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
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
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    rrd    core    error
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

    Log To Console    Verify that all 12 hosts are in hostgroup_1
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT COUNT(*) FROM hosts_hostgroups WHERE hostgroup_id=1    ==    ${12}    retry_timeout=30s    retry_pause=2s
    Disconnect From Database

    FOR    ${i}    IN RANGE    4
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

BECNHG5
    [Documentation]    Scenario: Host group removal and recreation with same hosts in centralized configuration
    ...    Given 3 pollers and Broker are started in centralized mode
    ...    When hostgroup_1 is created on each poller with different hosts
    ...    Then Broker receives all hostgroups with their respective members
    ...    When hostgroup_1 is removed from poller 1 and hostgroup_2 is created with the same hosts
    ...    Then Broker updates the database to reflect the changes
    ...    And hostgroup_2 contains the 3 hosts from poller 1
    ...    And hostgroup_1 still contains the 6 hosts from pollers 2 and 3

    [Tags]    broker    engine    hostgroup    MON-153802
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module0    core   error
    Ctn Broker Config Log    module0    processing   error
    Ctn Config BBDO3    ${3}
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5

    Ctn Clear Prot Files

    ${start}    Ctn Get Round Current Date

    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]
    Ctn Add Host Group    ${1}    ${1}    ["host_21", "host_22", "host_23"]
    Ctn Add Host Group    ${2}    ${1}    ["host_35", "host_36", "host_37"]
    Ctn Notify Broker Of Engine Config Change    0
    Ctn Notify Broker Of Engine Config Change    1
    Ctn Notify Broker Of Engine Config Change    2

    ${content}    Create List
    ...    enabling membership of host 1 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 21 to host group 1 on instance 2
    ...    enabling membership of host 22 to host group 1 on instance 2
    ...    enabling membership of host 23 to host group 1 on instance 2
    ...    enabling membership of host 35 to host group 1 on instance 3
    ...    enabling membership of host 36 to host group 1 on instance 3
    ...    enabling membership of host 37 to host group 1 on instance 3

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

    Sleep    10s

    Log To Console    New hostgroup_2 for Poller 1
    Ctn Remove Host Group    ${0}    ${1}
    Ctn Add Host Group    ${0}    ${2}    ["host_1", "host_2", "host_3"]
    Log To Console    Notifying Broker about this hostgroup
    Ctn Notify Broker Of Engine Config Change    0

    ${start}    Ctn Get Round Current Date

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    Checking in database that hostgroup_1 has id 1
    Check Query Result    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}    ==    hostgroup_1    retry_timeout=60s    retry_pause=2s
    Log To Console    Checking in database that hostgroup_2 has id 2
    Check Query Result    SELECT name FROM hostgroups WHERE hostgroup_id = ${2}    ==    hostgroup_2    retry_timeout=60s    retry_pause=2s
    Log To Console    Checking in database that hostgroup_2 has now three hosts from poller 1
    Check Query Result    SELECT COUNT(*) FROM hosts_hostgroups WHERE hostgroup_id = ${2}    ==    ${3}    retry_timeout=30s    retry_pause=2s
    Log To Console    Checking in database that hostgroup_1 keeps its hosts from pollers 2 and 3 but not from poller 1
    Check Query Result    SELECT COUNT(*) FROM hosts_hostgroups WHERE hostgroup_id = ${1}    ==    ${6}    retry_timeout=30s    retry_pause=2s
    Disconnect From Database
    Log To Console    End of test

