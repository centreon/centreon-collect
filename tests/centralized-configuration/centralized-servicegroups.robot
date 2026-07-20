*** Settings ***
Documentation       Centreon Broker and Engine add servicegroup

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BECNSG1
    [Documentation]    Scenario: Service group creation and synchronization in centralized configuration
    ...    Given 3 pollers and Broker are started in centralized mode
    ...    When a service group is created on poller 1 with 3 services from host_1
    ...    Then Broker receives the service group configuration
    ...    And the 3 services are registered as members of the service group in logs
    [Tags]    broker    engine    servicegroup    MON-153802
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Config BBDO3    ${3}
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Wait For Engine To Be Ready    ${start}    ${3}
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Notify Broker Of Engine Config Change    ${0}

    ${content}    Create List
    ...    enabling membership of service (1:3) to service group 1 on instance 1
    ...    enabling membership of service (1:2) to service group 1 on instance 1
    ...    enabling membership of service (1:1) to service group 1 on instance 1

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new service groups not found in logs.

BECNSG2
    [Documentation]    Feature: Service Groups Management with Unified SQL Database
    ...
    ...    Scenario: Create 4 service groups (3 services each) across 4 pollers, then progressively
    ...    remove servicegroups.cfg files to validate database consistency.
    ...
    ...    Given: 4 Engine pollers + central Broker with unified SQL + BBDO3 + debug logs
    ...    When: Create service groups and add servicegroups.cfg to each poller
    ...    Then: Database should show 12 associations in services_servicegroups table
    ...    When: Remove servicegroups.cfg from pollers sequentially
    ...    Then: Associations should decrease by 3 for each removal (12→9→6→3→0)
    ...
    ...    Validates: Service group associations are correctly maintained during config changes
    [Tags]    broker    engine    servicegroup    unified_sql    MON-153802
    Ctn Config Centralized Engine    ${4}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${4}

    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5

    Ctn Clear Retention
    Ctn Clear Prot Files
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Ctn Add Service Group    ${1}    ${1}    ["host_14","service_261", "host_14","service_262","host_14", "service_263"]
    Ctn Add Service Group    ${2}    ${1}    ["host_27","service_521", "host_27","service_522","host_27", "service_523"]
    Ctn Add Service Group    ${3}    ${1}    ["host_40","service_781", "host_40","service_782","host_40", "service_783"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${1}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${2}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${3}    servicegroups.cfg
    FOR    ${i}    IN RANGE    4
        Ctn Notify Broker Of Engine Config Change    ${i}
    END

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT COUNT(*) FROM services_servicegroups WHERE servicegroup_id=1    ==    ${12}    retry_timeout=30s    retry_pause=2s
    Disconnect From Database

    FOR    ${i}    IN RANGE    4
        Log To Console	  Remove hostgroup on poller ${i + 1}
        Ctn Config Engine Remove Cfg File    ${i}    servicegroups.cfg
        Ctn Notify Broker Of Engine Config Change    ${i}

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${expected}    Evaluate    9 - 3 * ${i}
        Check Query Result    SELECT COUNT(*) FROM services_servicegroups WHERE servicegroup_id=1    ==    ${expected}    retry_timeout=30s    retry_pause=2s
        Disconnect From Database
    END

BECNSG3
    [Documentation]    Test about lua cache. But the centralized configuration currently breaks the broker cache.
    [Tags]    broker    engine    servicegroup    MON-153802
    Ctn Config Centralized Engine    ${3}
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}

    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    lua    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Source Log    central    1
    Ctn Broker Config Source Log    module0    1
    Ctn Broker Config Add Lua Output    central    test-cache    ${SCRIPTS}test-dump-groups.lua
    Ctn Clear Retention
    Ctn Clear Prot Files

    Create File    /tmp/lua-engine.log

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2","host_1", "service_3"]
    Ctn Add Service Group    ${1}    ${1}    ["host_18","service_341", "host_19","service_362","host_19", "service_363"]
    Ctn Add Service Group    ${2}    ${1}    ["host_35","service_681", "host_35","service_682","host_36", "service_706"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${1}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${2}    servicegroups.cfg
    FOR    ${i}    IN RANGE    3
        Ctn Notify Broker Of Engine Config Change    ${i}
    END

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT COUNT(*) FROM services_servicegroups WHERE servicegroup_id=1    ==    ${9}    retry_timeout=30s    retry_pause=2s
    Disconnect From Database

    FOR    ${loop_index}    IN RANGE    30
        ${grep_result}    Grep File    /tmp/lua-engine.log    service_group_name:servicegroup_1
        IF    len("""${grep_result}""") > 10    BREAK
        Sleep    1s
    END

    Should Be True    len("""${grep_result}""") > 10    servicegroup_1 not found in /tmp/lua-engine.log

    Ctn Rename Service Group    ${0}    servicegroup_1    servicegroup_test
    Ctn Rename Service Group    ${1}    servicegroup_1    servicegroup_test
    Ctn Rename Service Group    ${2}    servicegroup_1    servicegroup_test
    FOR    ${i}    IN RANGE    3
        Ctn Notify Broker Of Engine Config Change    ${i}
    END

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT COUNT(*) FROM services_servicegroups WHERE servicegroup_id=1    ==    ${9}    retry_timeout=30s    retry_pause=2s
    Check Query Result    SELECT count(*) FROM servicegroups s JOIN services_servicegroups sg ON s.servicegroup_id = sg.servicegroup_id WHERE s.servicegroup_id=1 AND name='servicegroup_test'    ==    ${9}    retry_timeout=30s    retry_pause=1s
    Disconnect From Database

    Log To Console    \nservicegroup_1 renamed to servicegroup_test

    FOR    ${loop_index}    IN RANGE    30
        ${grep_result}    Grep File    /tmp/lua-engine.log    service_group_name:servicegroup_test
        IF    len("""${grep_result}""") > 10    BREAK
        Sleep    1s
    END

    Should Be True    len("""${grep_result}""") > 10    servicegroup_test not found in /tmp/lua-engine.log

    # remove servicegroup
    Ctn Remove Service Group    ${0}    ${1}
    Ctn Notify Broker Of Engine Config Change    ${0}
    Ctn Remove Service Group    ${1}    ${1}
    Ctn Notify Broker Of Engine Config Change    ${1}
    Ctn Remove Service Group    ${2}    ${1}
    Ctn Notify Broker Of Engine Config Change    ${2}

    Log To Console    \nservice group 1 removed from poller 1 and 2, then 3

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT count(*) FROM servicegroups s JOIN services_servicegroups sg ON s.servicegroup_id = sg.servicegroup_id WHERE s.servicegroup_id=1    ==    ${0}    retry_timeout=30s    retry_pause=1s
    Disconnect From Database

    Log To Console    Waiting to observe no service group in database
    # Waiting to observe no service group.
    FOR    ${index}    IN RANGE    60
        Create File    /tmp/lua-engine.log
        Sleep    1s
        ${grep_result}    Grep File    /tmp/lua-engine.log    no service_group_name
        IF    len("""${grep_result}""") > 0    BREAK
    END

    Log To Console    Still waiting to observe no service group in database, sleeping a bit more
    Sleep    10s
    # Do we still have no service group?
    Log To Console    Checking if there is still no service group in broker cache
    ${grep_result}    Grep File    /tmp/lua-engine.log    service_group_name:
    Should Be True    len("""${grep_result}""") == 0    The servicegroup 1 still exists
