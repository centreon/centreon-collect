*** Settings ***
Documentation       Centreon Engine/Broker verify relation parent child host.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***

BECPN0
    [Documentation]    Feature: Parent-Child Host Dependency Management
    ...    As a monitoring administrator
    ...    I want child host checks to be queued when parent hosts are down
    ...    So that unnecessary checks are avoided

    ...    Scenario: Child host queued when parent is down
    ...        Given host_1 is configured as parent of host_2
    ...        And the monitoring system is running
    ...        When host_1 goes DOWN/HARD
    ...        Then host_2 check should be queued
    ...        And log should contain "Check of child host 'host_2' queued."
    [Tags]    broker    engine    MON-151686    MON-153802

    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Engine Config Set Value    0    log_level_checks    debug
    Ctn Config Broker Sql Output    central    unified_sql    10
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0

    Ctn Clear Retention
    Ctn Clear Db    resources

    # force the check result to 2
    Ctn Config Host Command Status    ${0}    checkh1    2

    # host_1 is parent of host_2
    Ctn Add Parent To Host    0    host_2    host_1

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # check if host_2 is child of host_1
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2, 1),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2, 1),)    host parent not inserted

    # check if host_1 is pending
    ${result}    Ctn Check Host Status    host_1    4    1    True
    Should Be True    ${result}    host_1 should be pending

    ${result}    Ctn Check Host Status    host_2    4    1    True
    Should Be True    ${result}    host_2 should be pending

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    Ctn Process Host Check Result    host_1    0    host_1 UP

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    1s
    END

    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;

    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE HOST should be down in log.

    ${result}    Ctn Check Host Status    host_1    1    1    True
    Should Be True    ${result}    host_1 should be down/hard

    ${content}    Create List
     ...    Check of child host 'host_2' queued.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Check of child host 'host_2' should be queued.

    Disconnect From Database
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BECPN1
    [Documentation]    Feature: Parent Host Deletion Management
    ...    As a monitoring administrator
    ...    I want parent-child relationships to be cleaned up when parent hosts are deleted
    ...    So that orphaned relationships don't exist in the system
    ...
    ...    Scenario: Parent-child relationship cleanup on parent deletion
    ...        Given host_1 is configured as parent of host_2
    ...        And the monitoring system is running
    ...        And the parent-child relationship exists in the database
    ...        When I delete host_1 from the configuration
    ...        And I notify Broker about that change in the engine configuration
    ...        Then host_2 should have no parent hosts
    ...        And the parent-child relationship should be removed from the database
    [Tags]    broker    engine    MON-151686

    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Broker Config Log    rrd    rrd    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Log    module0    core    error

    Ctn Broker Config Log    central    sql    debug
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Config Broker Sql Output    central    unified_sql    10
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0

    Ctn Clear Retention

    # host_1 is parent of host_2
    Ctn Add Parent To Host    0    host_2    host_1

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1    parentHosts

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    childs:${output}[childHosts]
    Should Contain    ${output}[childHosts]    host_2    childHosts

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2, 1),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2, 1),)    the parent link not inserted

    Ctn Engine Config Del Block In Cfg    ${0}    host    host_1    hosts.cfg
    Ctn Engine Config Del Block In Cfg    ${0}    service    host_1    services.cfg
    Ctn Engine Config Delete Value In Hosts    ${0}    host_2    parents

    ${start}    Get Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    Reload differential configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Be Empty    ${output}[parentHosts]

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    the parent link should be deleted

    Disconnect From Database
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BECPN2
    [Documentation]    Feature: Child Host Deletion Management
    ...    As a monitoring administrator
    ...    I want parent-child relationships to be cleaned up when child hosts are deleted
    ...    So that orphaned relationships don't exist in the system
    ...
    ...    Scenario: Parent-child relationship cleanup on child deletion
    ...        Given host_1 is configured as parent of host_2
    ...        And the monitoring system is running
    ...        And the parent-child relationship exists in the database
    ...        When I delete host_2 from the configuration
    ...        And I notify Broker of a change in the engine configuration
    ...        Then host_1 should have no child hosts
    ...        And the parent-child relationship should be removed from the database
    [Tags]    broker    engine    MON-151686

    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Broker Config Log    rrd    rrd    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Log    module0    core    error

    Ctn Broker Config Log    central    sql    debug
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Config Broker Sql Output    central    unified_sql    10
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0

    Ctn Clear Retention

    # host_1 is parent of host_2
    Ctn Add Parent To Host    0    host_2    host_1

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1    parentHosts

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    childs:${output}[childHosts]
    Should Contain    ${output}[childHosts]    host_2    childHosts

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2, 1),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2, 1),)    the parent link not inserted

    Ctn Engine Config Del Block In Cfg    ${0}    host    host_2    hosts.cfg
    Ctn Engine Config Del Block In Cfg    ${0}    service    host_2    services.cfg
    Ctn Engine Config Delete Value In Hosts    ${0}    host_2    parents

    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0

    ${content}    Create List    Reload differential configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    childs:${output}[childHosts]
    Should Be Empty    ${output}[childHosts]

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    the parent link should be deleted

    Disconnect From Database
    Ctn Stop Engine
    Ctn Kindly Stop Broker

RENAME_PARENT
    [Documentation]    Feature: Parent Host Rename Management
    ...    As a monitoring administrator
    ...    I want parent-child relationships to be maintained when parent hosts are renamed
    ...    So that dependencies remain intact after configuration changes
    ...
    ...    Scenario: Parent-child relationship maintained on parent rename
    ...        Given host_1 is configured as parent of host_2
    ...        And the monitoring system is running
    ...        And the parent-child relationship exists
    ...        When I rename host_1 to host_1_new
    ...        And I update host_2 parent reference to host_1_new
    ...        And I reload the engine configuration
    ...        Then host_2 should have host_1_new as parent
    ...        And the engine should not crash
    ...        And the configuration reload should complete successfully
    [Tags]    engine    MON-167683

    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd

    Ctn Clear Retention

    # host_1 is parent of host_2
    Ctn Add Parent To Host    0    host_2    host_1

    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1    parentHosts

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    childs:${output}[childHosts]
    Should Contain    ${output}[childHosts]    host_2    childHosts

    # Rename the parent host
    Ctn Engine Config Rename Host    ${0}    host_1    host_1_new
    Ctn Engine Config Set Host Value    ${0}    host_2    parents    host_1_new
    Ctn Engine Config Replace Value In Services    ${0}    service_1    host_name    host_1_new

    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0

    ${content}    Create List    Reload differential configuration finished
        ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!
    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1_new    parentHosts

    Ctn Stop Engine
    Ctn Kindly Stop Broker
