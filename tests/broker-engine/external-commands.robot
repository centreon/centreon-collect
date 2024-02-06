*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
BEEXTCMD1
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    300
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD2
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD3
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Host Check Interval    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD4
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Host Check Interval    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD5
    [Documentation]    external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Retry Svc Check Interval    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.retry_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}    Query
            ...    SELECT s.retry_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD6
    [Documentation]    external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Retry Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.retry_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}    Query
            ...    SELECT s.retry_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD7
    [Documentation]    external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Retry Host Check Interval    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT retry_interval FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT retry_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD8
    [Documentation]    external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Retry Host Check Interval    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT retry_interval FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT retry_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD9
    [Documentation]    external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo3.0
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Max Svc Check Attempts    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.max_check_attempts FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}    Query
            ...    SELECT s.max_check_attempts FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT max_check_attempts FROM resources WHERE name='service_1' AND parent_name='host_1'
            ${output}    Query
            ...    SELECT max_check_attempts FROM resources WHERE name='service_1' AND parent_name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD10
    [Documentation]    external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Max Svc Check Attempts    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.max_check_attempts FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}    Query
            ...    SELECT s.max_check_attempts FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD11
    [Documentation]    external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Max Host Check Attempts    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT max_check_attempts FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT max_check_attempts FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT max_check_attempts FROM resources WHERE name='host_1'
            ${output}    Query    SELECT max_check_attempts FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD12
    [Documentation]    external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Max Host Check Attempts    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT max_check_attempts FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT max_check_attempts FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD13
    [Documentation]    external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Host Check Timeperiod    ${use_grpc}    host_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_period FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT check_period FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "(('24x6',),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    (('24x6',),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD14
    [Documentation]    external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Host Check Timeperiod    ${use_grpc}    host_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_period FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT check_period FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "(('24x6',),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    (('24x6',),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD15
    [Documentation]    external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Host Notification Timeperiod    ${use_grpc}    host_1    24x7

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notification_period FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notification_period FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "(('24x7',),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    (('24x7',),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD16
    [Documentation]    external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Host Notification Timeperiod    ${use_grpc}    host_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notification_period FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notification_period FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "(('24x6',),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    (('24x6',),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD17
    [Documentation]    external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Svc Check Timeperiod    ${use_grpc}    host_1    service_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.check_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.check_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "(('24x6',),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    (('24x6',),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD18
    [Documentation]    external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Svc Check Timeperiod    ${use_grpc}    host_1    service_1    24x7

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.check_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.check_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "(('24x7',),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    (('24x7',),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD19
    [Documentation]    external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Svc Notification Timeperiod    ${use_grpc}    host_1    service_1    24x7

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notification_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.notification_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "(('24x7',),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    (('24x7',),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD20
    [Documentation]    external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Change Svc Notification Timeperiod    ${use_grpc}    host_1    service_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notification_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.notification_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "(('24x6',),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    (('24x6',),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD21
    [Documentation]    external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console
        ...    external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0 use_grpc=${use_grpc}
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host And Child Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notifications_enabled FROM resources WHERE name='host_1'
            ${output}    Query    SELECT notifications_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host And Child Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notifications_enabled FROM resources WHERE name='host_1'
            ${output}    Query    SELECT notifications_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD22
    [Documentation]    external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console
        ...    external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0 use_grpc=${use_grpc}
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host And Child Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host And Child Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD23
    [Documentation]    external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Check    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT active_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks_enabled FROM resources WHERE name='host_1'
            ${output}    Query    SELECT active_checks_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Check    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT active_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks_enabled FROM resources WHERE name='host_1'
            ${output}    Query    SELECT active_checks_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD24
    [Documentation]    external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Check    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT active_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Check    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT active_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD25
    [Documentation]    external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    1
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Event Handler    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Event Handler    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD26
    [Documentation]    external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Event Handler    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Event Handler    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD27
    [Documentation]    external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Flap Detection    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT flap_detection FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT flap_detection FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Flap Detection    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT flap_detection FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT flap_detection FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD28
    [Documentation]    external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Flap Detection    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT flap_detection FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT flap_detection FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Flap Detection    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT flap_detection FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT flap_detection FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD29
    [Documentation]    external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console
        ...    external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}    Get Current Date
        Start Broker
        Start Engine
        ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notifications_enabled FROM resources WHERE name='host_1'
            ${output}    Query    SELECT notifications_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notifications_enabled FROM resources WHERE name='host_1'
            ${output}    Query    SELECT notifications_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END
