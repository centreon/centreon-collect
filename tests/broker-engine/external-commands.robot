*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
Library             DatabaseLibrary
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BEEXTCMD1
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    300
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}=    Query
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}=    Query
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Host Check Interval    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT check_interval FROM hosts WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Host Check Interval    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT check_interval FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Retry Svc Check Interval    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.retry_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}=    Query
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Retry Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.retry_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}=    Query
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Retry Host Check Interval    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT retry_interval FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT retry_interval FROM hosts WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Retry Host Check Interval    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT retry_interval FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT retry_interval FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Max Svc Check Attempts    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.max_check_attempts FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}=    Query
            ...    SELECT s.max_check_attempts FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT max_check_attempts FROM resources WHERE name='service_1' AND parent_name='host_1'
            ${output}=    Query
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Max Svc Check Attempts    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.max_check_attempts FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}=    Query
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Max Host Check Attempts    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT max_check_attempts FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT max_check_attempts FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT max_check_attempts FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT max_check_attempts FROM resources WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Max Host Check Attempts    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT max_check_attempts FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT max_check_attempts FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Host Check Timeperiod    ${use_grpc}    host_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_period FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT check_period FROM hosts WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Host Check Timeperiod    ${use_grpc}    host_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_period FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT check_period FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Host Notification Timeperiod    ${use_grpc}    host_1    24x7

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notification_period FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notification_period FROM hosts WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Host Notification Timeperiod    ${use_grpc}    host_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notification_period FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notification_period FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Svc Check Timeperiod    ${use_grpc}    host_1    service_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.check_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Svc Check Timeperiod    ${use_grpc}    host_1    service_1    24x7

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.check_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Svc Notification Timeperiod    ${use_grpc}    host_1    service_1    24x7

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notification_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Svc Notification Timeperiod    ${use_grpc}    host_1    service_1    24x6

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notification_period FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console
        ...    external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0 use_grpc=${use_grpc}
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host And child Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notifications_enabled FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT notifications_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host And child Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notifications_enabled FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT notifications_enabled FROM resources WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host And child Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host And child Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notify FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Check    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT active_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks_enabled FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT active_checks_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Check    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT active_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks_enabled FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT active_checks_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Check    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT active_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Check    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT active_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT should_be_scheduled FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    1
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Event Handler    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Event Handler    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Event Handler    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Event Handler    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT event_handler_enabled FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Flap Detection    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT flap_detection FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT flap_detection FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Flap Detection    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT flap_detection FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT flap_detection FROM hosts WHERE name='host_1'
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
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Flap detection    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT flap_detection FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT flap_detection FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Flap Detection    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT flap_detection FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT flap_detection FROM hosts WHERE name='host_1'
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
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console
        ...    external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notifications_enabled FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT notifications_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notifications_enabled FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT notifications_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD30
    [Documentation]    external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console
        ...    external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD31
    [Documentation]    external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Svc Checks    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks_enabled FROM resources WHERE name='service_1'
            ${output}=    Query    SELECT active_checks_enabled FROM resources WHERE name='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Svc Checks    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks_enabled FROM resources WHERE name='service_1'
            ${output}=    Query    SELECT active_checks_enabled FROM resources WHERE name='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD32
    [Documentation]    external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Svc Checks    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Svc Checks    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD33
    [Documentation]    external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Svc Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Svc Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD34
    [Documentation]    external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Host Svc Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Host Svc Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD35
    [Documentation]    external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Passive Host Checks    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT passive_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks_enabled FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT passive_checks_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Passive Host Checks    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT passive_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks_enabled FROM resources WHERE name='host_1'
            ${output}=    Query    SELECT passive_checks_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD36
    [Documentation]    external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Passive Host Checks    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT passive_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Passive Host Checks    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT passive_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD37
    [Documentation]    external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Passive Svc Checks    ${use_grpc}    host_1    service_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks_enabled FROM resources WHERE name='service_1'
            ${output}=    Query    SELECT passive_checks_enabled FROM resources WHERE name='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Passive Svc Checks    ${use_grpc}    host_1    service_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks_enabled FROM resources WHERE name='service_1'
            ${output}=    Query    SELECT passive_checks_enabled FROM resources WHERE name='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD38
    [Documentation]    external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Disable Passive Svc Checks    ${use_grpc}    host_1    service_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Enable Passive Svc Checks    ${use_grpc}    host_1    service_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD39
    [Documentation]    external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Stop Obsessing Over Host    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Start Obsessing Over Host    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD40
    [Documentation]    external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Stop Obsessing Over Host    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Start Obsessing Over Host    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD41
    [Documentation]    external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Stop Obsessing Over Svc    ${use_grpc}    host_1    service_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Start Obsessing Over Svc    ${use_grpc}    host_1    service_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD42
    [Documentation]    external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Stop Obsessing Over Svc    ${use_grpc}    host_1    service_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Start Obsessing Over Svc    ${use_grpc}    host_1    service_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_GRPC1
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    300
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_GRPC2
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        # Let's wait for the external command check start
        ${content}    Create List    check_for_external_commands()
        ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message telling check_for_external_commands() should be available.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    60
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_GRPC3
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Host Check Interval    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_GRPC4
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Round Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Host Check Interval    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_REVERSE_GRPC1
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
    [Tags]    broker    engine    services    extcmd    bbdo3
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Broker Config Output Remove    module0    central-module-master-output    host
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Broker Config Input Set    central    central-broker-master-input    host    127.0.0.1
    Broker Config Input Set    rrd    central-rrd-master-input    host    127.0.0.1
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Sleep    1s
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    300
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}=    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_REVERSE_GRPC2
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Broker Config Output Remove    module0    central-module-master-output    host
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Broker Config Input Set    central    central-broker-master-input    host    127.0.0.1
    Broker Config Input Set    rrd    central-rrd-master-input    host    127.0.0.1
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc} reversed
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}=    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_REVERSE_GRPC3
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Output Remove    module0    central-module-master-output    host
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Broker Config Input Set    central    central-broker-master-input    host    127.0.0.1
    Broker Config Input Set    rrd    central-rrd-master-input    host    127.0.0.1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc} reversed
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Host Check Interval    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_REVERSE_GRPC4
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Broker Config Output Remove    module0    central-module-master-output    host
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Broker Config Input Set    central    central-broker-master-input    host    127.0.0.1
    Broker Config Input Set    rrd    central-rrd-master-input    host    127.0.0.1
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc} reversed
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Sleep    1s
        Change Normal Host Check Interval    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}=    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEEXTCMD_COMPRESS_GRPC1
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Change Broker tcp output to grpc    module0
    Change Broker tcp input to grpc    central
    Change Broker Compression Output    module0    central-module-master-output    yes
    Change Broker Compression Input    central    centreon-broker-master-input    yes
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Clear Retention
        ${start}=    Get Current Date
        Start Broker
        Start Engine
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
        Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    300
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}=    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Stop Engine
        Kindly Stop Broker
    END

BEATOI11
    [Documentation]    external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
    [Tags]    broker    engine    host    extcmd    notification    atoi
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    SEND CUSTOM HOST NOTIFICATION    host_1    1    admin    foobar
    ${content}=    Create List    EXTERNAL COMMAND: SEND_CUSTOM_HOST_NOTIFICATION;host_1;1;admin;foobar
    ${result}=    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=command argument notification_option must be an integer between 0 and 7.
    Stop Engine
    Kindly Stop Broker

BEATOI12
    [Documentation]    external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
    [Tags]    broker    engine    host    extcmd    notification    atoi
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    SEND CUSTOM HOST NOTIFICATION    host_1    8    admin    foobar
    ${content}=    Create List
    ...    Error: could not send custom host notification: '8' must be an integer between 0 and 7
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=command argument notification_option must be an integer between 0 and 7.
    Stop Engine
    Kindly Stop Broker

BEATOI13
    [Documentation]    external command SCHEDULE SERVICE DOWNTIME with duration<0 should fail
    [Tags]    broker    engine    host    extcmd    atoi
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    ${date}=    Get Current Date    result_format=epoch
    SCHEDULE SERVICE DOWNTIME    host_1    service_1    -1
    ${content}=    Create List    Error: could not schedule downtime : duration
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=command argument duration must be an integer >= 0.
    Stop Engine
    Kindly Stop Broker

BEATOI21
    [Documentation]    external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
    [Tags]    broker    engine    host    extcmd    atoi
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Sleep    1s
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    ADD HOST COMMENT    host_1    1    user    comment
    ${content}=    Create List    ADD_HOST_COMMENT
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=the comment with id:1 was not added.
    ${com_id}=    Find Internal Id    ${start}    True    30
    DEL HOST COMMENT    ${com_id}
    ${result}=    Find Internal Id    ${start}    False    30
    Should Be True    ${result}    msg=the comment with id:${com_id} was not deleted.
    Stop Engine
    Kindly Stop Broker

BEATOI22
    [Documentation]    external command DEL_HOST_COMMENT with comment_id<0 should fail
    [Tags]    broker    engine    host    extcmd    atoi
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Sleep    1s
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    ADD HOST COMMENT    host_1    1    user    comment
    ${content}=    Create List    ADD_HOST_COMMENT
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=the comment with id:1 was not added.
    ${com_id}=    Find Internal Id    ${start}    True    30
    DEL HOST COMMENT    -1
    ${content}=    Create List    Error: could not delete comment : comment_id
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=comment_id must be an unsigned integer.
    ${result}=    Find Internal Id    ${start}    True    30
    Should Be True    ${result}    msg=comment with id:-1 was deleted.
    Stop Engine
    Kindly Stop Broker

BEATOI23
    [Documentation]    external command ADD_SVC_COMMENT with persistent=0 should work
    [Tags]    broker    engine    host    extcmd    atoi
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    error
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    ${date}=    Get Current Date    result_format=epoch
    ADD SVC COMMENT    host_1    service_1    0    user    comment
    ${content}=    Create List    ADD_SVC_COMMENT
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=command argument persistent_flag must be 0 or 1.
    Stop Engine
    Kindly Stop Broker
