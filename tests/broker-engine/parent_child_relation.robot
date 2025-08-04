*** Settings ***
Documentation       Centreon Engine/Broker verify relation parent child host.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***

EBPN0
    [Documentation]    Verify if child is in queue when parent is down.
    [Tags]    broker    engine    MON-151686
    
    Ctn Config Engine    ${1}    ${5}    ${1}
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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # check if host_2 is child of host_1

    FOR    ${index}    IN RANGE    30
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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

EBPN1
    [Documentation]    verify relation parent child when delete parent.
    [Tags]    broker    engine    MON-151686
    
    Ctn Config Engine    ${1}    ${5}    ${1}
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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1    parentHosts

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    childs:${output}[childHosts]
    Should Contain    ${output}[childHosts]    host_2    childHosts

    FOR    ${index}    IN RANGE    30
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
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
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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

EBPN2
    [Documentation]    verify relation parent child when delete child.
    [Tags]    broker    engine    MON-151686
    
    Ctn Config Engine    ${1}    ${5}    ${1}
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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1    parentHosts

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    childs:${output}[childHosts]
    Should Contain    ${output}[childHosts]    host_2    childHosts

    FOR    ${index}    IN RANGE    30
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
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
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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
    [Documentation]    Given an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
    ...    Engine mustn't crash and log an error on reload.
    [Tags]    engine    MON-168882
    
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    module

    Ctn Clear Retention

    # host_1 is parent of host_2
    Ctn Add Parent To Host    0    host_2    host_1

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    #let's some time to engine to process the parent/child relation
    Sleep   5s

    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1    parentHosts

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    childs:${output}[childHosts]
    Should Contain    ${output}[childHosts]    host_2    childHosts

    # rename the parent host
    Ctn Engine Config Rename Host    ${0}    host_1    host_1_new
    Ctn Engine Config Set Host Value    ${0}    host_2    parents    host_1_new
    Ctn Engine Config Replace Value In Services    ${0}    service_1    host_name    host_1_new

    ${start}    Get Current Date
    Ctn Reload Engine

    ${content}    Create List    Reload configuration finished
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
