*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BEEXTCMD30
    [Documentation]    external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console
        ...    external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0 use_grpc=${use_grpc}
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Host Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Host Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT notify FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT notify FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD31
    [Documentation]    external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Host Svc Checks    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks_enabled FROM resources WHERE name='service_1'
            ${output}    Query    SELECT active_checks_enabled FROM resources WHERE name='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Host Svc Checks    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT active_checks_enabled FROM resources WHERE name='service_1'
            ${output}    Query    SELECT active_checks_enabled FROM resources WHERE name='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD32
    [Documentation]    external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Host Svc Checks    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Host Svc Checks    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.active_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.should_be_scheduled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD33
    [Documentation]    external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Host Svc Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Host Svc Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD34
    [Documentation]    external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Host Svc Notifications    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Host Svc Notifications    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.notify FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD35
    [Documentation]    external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Passive Host Checks    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT passive_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks_enabled FROM resources WHERE name='host_1'
            ${output}    Query    SELECT passive_checks_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Passive Host Checks    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT passive_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks_enabled FROM resources WHERE name='host_1'
            ${output}    Query    SELECT passive_checks_enabled FROM resources WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD36
    [Documentation]    external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Passive Host Checks    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT passive_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Passive Host Checks    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT passive_checks FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD37
    [Documentation]    external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Passive Svc Checks    ${use_grpc}    host_1    service_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks_enabled FROM resources WHERE name='service_1'
            ${output}    Query    SELECT passive_checks_enabled FROM resources WHERE name='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Passive Svc Checks    ${use_grpc}    host_1    service_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT passive_checks_enabled FROM resources WHERE name='service_1'
            ${output}    Query    SELECT passive_checks_enabled FROM resources WHERE name='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD38
    [Documentation]    external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Disable Passive Svc Checks    ${use_grpc}    host_1    service_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Enable Passive Svc Checks    ${use_grpc}    host_1    service_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.passive_checks FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD39
    [Documentation]    external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Stop Obsessing Over Host    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Start Obsessing Over Host    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD40
    [Documentation]    external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Stop Obsessing Over Host    ${use_grpc}    host_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Start Obsessing Over Host    ${use_grpc}    host_1

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT obsess_over_host FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD41
    [Documentation]    external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Stop Obsessing Over Svc    ${use_grpc}    host_1    service_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Start Obsessing Over Svc    ${use_grpc}    host_1    service_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD42
    [Documentation]    external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    FOR    ${use_grpc}    IN RANGE    0    1
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Stop Obsessing Over Svc    ${use_grpc}    host_1    service_1

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((0,),)

        Ctn Start Obsessing Over Svc    ${use_grpc}    host_1    service_1

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.obsess_over_service FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((1,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((1,),)
        Disconnect From Database

        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_GRPC1
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    300
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_GRPC2
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc}
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_GRPC3
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    trace
    Ctn Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Change Normal Host Check Interval    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_GRPC4
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc}
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Change Normal Host Check Interval    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_REVERSE_GRPC1
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
    [Tags]    broker    engine    services    extcmd    bbdo3
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Broker Config Output Remove    module0    central-module-master-output    host
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Ctn Broker Config Input Set    central    central-broker-master-input    host    127.0.0.1
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    host    127.0.0.1
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
        Ctn Wait For Engine To Be Ready    ${1}
        #lets time to grpc to start
        Sleep  0.1

        Ctn Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database
	...    pymysql
	...    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    60
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            ${output}    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description='service_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_REVERSE_GRPC2
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Broker Config Output Remove    module0    central-module-master-output    host
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Ctn Broker Config Input Set    central    central-broker-master-input    host    127.0.0.1
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    host    127.0.0.1
    Ctn Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc} reversed
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    15

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
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_REVERSE_GRPC3
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Config BBDO3    1
    Ctn Broker Config Output Remove    module0    central-module-master-output    host
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Ctn Broker Config Input Set    central    central-broker-master-input    host    127.0.0.1
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    host    127.0.0.1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    trace
    Ctn Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc} reversed
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Change Normal Host Check Interval    ${use_grpc}    host_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_REVERSE_GRPC4
    [Documentation]    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Broker Config Output Remove    module0    central-module-master-output    host
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Ctn Broker Config Input Set    central    central-broker-master-input    host    127.0.0.1
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    host    127.0.0.1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 use_grpc=${use_grpc} reversed
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Sleep    1s
        Ctn Change Normal Host Check Interval    ${use_grpc}    host_1    15

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    30
            Log To Console    SELECT check_interval FROM hosts WHERE name='host_1'
            ${output}    Query    SELECT check_interval FROM hosts WHERE name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((15.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((15.0,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEEXTCMD_COMPRESS_GRPC1
    [Documentation]    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Compression Output    module0    central-module-master-output    yes
    Ctn Change Broker Compression Input    central    centreon-broker-master-input    yes
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 use_grpc=${use_grpc}
        Ctn Clear Retention
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}
        Ctn Change Normal Svc Check Interval    ${use_grpc}    host_1    service_1    10

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

        FOR    ${index}    IN RANGE    300
            Log To Console
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            ${output}    Query
            ...    SELECT s.check_interval FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description='service_1' AND h.name='host_1'
            Log To Console    ${output}
            Sleep    1s
            IF    "${output}" == "((10.0,),)"    BREAK
        END
        Should Be Equal As Strings    ${output}    ((10.0,),)
        Disconnect From Database
        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END

BEATOI11
    [Documentation]    external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
    [Tags]    broker    engine    host    extcmd    notification    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    Ctn Send Custom Host Notification    host_1    1    admin    foobar
    ${content}    Create List    EXTERNAL COMMAND: SEND_CUSTOM_HOST_NOTIFICATION;host_1;1;admin;foobar
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    command argument notification_option must be an integer between 0 and 7.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEATOI12
    [Documentation]    external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
    [Tags]    broker    engine    host    extcmd    notification    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    Ctn Send Custom Host Notification    host_1    8    admin    foobar
    ${content}    Create List
    ...    Error: could not send custom host notification: '8' must be an integer between 0 and 7
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    command argument notification_option must be an integer between 0 and 7.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEATOI13
    [Documentation]    external command Schedule Service Downtime with duration<0 should fail
    [Tags]    broker    engine    host    extcmd    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    ${date}    Get Current Date    result_format=epoch
    Ctn Schedule Service Downtime    host_1    service_1    -1
    ${content}    Create List    Error: could not schedule downtime : duration
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    command argument duration must be an integer >= 0.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEATOI21
    [Documentation]    external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
    [Tags]    broker    engine    host    extcmd    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date    exclude_millis=True
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    Ctn Add Host Comment    host_1    1    user    comment
    ${content}    Create List    ADD_HOST_COMMENT
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    the comment with id:1 was not added.
    ${com_id}    Ctn Find Internal Id    ${start}    True    30
    Should Be True    ${com_id}>0    Comment id should be a positive integer.
    Ctn Del Host Comment    ${com_id}
    ${result}    Ctn Find Internal Id    ${start}    False    30
    Should Be True    ${result}    the comment with id:${com_id} was not deleted.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEATOI22
    [Documentation]    external command DEL_HOST_COMMENT with comment_id<0 should fail
    [Tags]    broker    engine    host    extcmd    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Sleep    1s
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    Ctn Add Host Comment    host_1    1    user    comment
    ${content}    Create List    ADD_HOST_COMMENT
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    the comment with id:1 was not added.
    ${com_id}    Ctn Find Internal Id    ${start}    True    30
    Ctn Del Host Comment    -1
    ${content}    Create List    Error: could not delete comment : comment_id
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    comment_id must be an unsigned integer.
    ${result}    Ctn Find Internal Id    ${start}    True    30
    Should Be True    ${result}    comment with id:-1 was deleted.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEATOI23
    [Documentation]    external command ADD_SVC_COMMENT with persistent=0 should work
    [Tags]    broker    engine    host    extcmd    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    error
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    ${date}    Get Current Date    result_format=epoch
    Ctn Add Svc Comment    host_1    service_1    0    user    comment
    ${content}    Create List    ADD_SVC_COMMENT
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    command argument persistent_flag must be 0 or 1.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BECUSTOMHOSTVAR
    [Documentation]    external command CHANGE_CUSTOM_HOST_VAR on SNMPVERSION
    [Tags]    broker    engine    host    extcmd    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    ${date}    Get Current Date    result_format=epoch
    Ctn Change Custom Host Var Command    host_1    SNMPVERSION    789456

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    300
        Log To Console
        ...    SELECT c.value FROM customvariables c LEFT JOIN hosts h ON c.host_id=h.host_id WHERE h.name='host_1' && c.name='SNMPVERSION'
        ${output}    Query
        ...    SELECT c.value FROM customvariables c LEFT JOIN hosts h ON c.host_id=h.host_id WHERE h.name='host_1' && c.name='SNMPVERSION'
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('789456',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('789456',),)
    Disconnect From Database

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BECUSTOMSVCVAR
    [Documentation]    external command CHANGE_CUSTOM_SVC_VAR on CRITICAL
    [Tags]    broker    engine    host    extcmd    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    ${date}    Get Current Date    result_format=epoch
    Ctn Change Custom Svc Var Command    host_1    service_1    CRITICAL    456123

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    300
        Log To Console
        ...    SELECT c.value FROM customvariables c JOIN hosts h ON c.host_id=h.host_id JOIN services s ON c.service_id = s.service_id and h.host_id = s.service_id WHERE h.name='host_1' && s.description = 'service_1' && c.name='CRITICAL'
        ${output}    Query
        ...    SELECT c.value FROM customvariables c JOIN hosts h ON c.host_id=h.host_id JOIN services s ON c.service_id = s.service_id and h.host_id = s.service_id WHERE h.name='host_1' && s.description = 'service_1' && c.name='CRITICAL'
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('456123',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('456123',),)
    Disconnect From Database

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BESERVCHECK
    [Documentation]    external command CHECK_SERVICE_RESULT
    [Tags]    broker    engine    host    extcmd    atoi
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    UPDATE services set command_line='toto', next_check=0 where service_id=1 and host_id=1
    Ctn Schedule Forced Service Check    host_1    service_1
    ${command_id}    Ctn Get Service Command Id    1
    ${result}    Ctn Check Service Check With Timeout
    ...    host_1
    ...    service_1
    ...    30
    ...    ${VarRoot}/lib/centreon-engine/check.pl --id ${command_id}
    Should Be True    ${result}    service table not updated
    Disconnect From Database

BEHOSTCHECK
    [Documentation]    Given Engine and Broker configured to work with BBDO 3
    ...    When a schedule forced host check command on host host_1 is launched
    ...    Then the result appears in the centreon_storage resources table
    [Tags]    broker    engine    host    extcmd    bbdo3
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Schedule Forced Host Check    host_1
    ${result}    Ctn Check Host Check With Timeout    host_1    ${start}    30
    Should Be True    ${result}    last_check column in resources table not updated.


BE_BACKSLASH_CHECK_RESULT
    [Documentation]    external command PROCESS_SERVICE_CHECK_RESULT with \:
    [Tags]    broker    engine    services    extcmd   MON-51121
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    Ctn Broker Config Log    central    sql    debug
    FOR    ${use_grpc}    IN RANGE    0    2
        Log To Console    external command PROCESS_SERVICE_CHECK_RESULT  use_grpc=${use_grpc}
        Ctn Clear Retention
        ${start}    Get Current Date
        Ctn Start Broker
        Ctn Start Engine
        Ctn Wait For Engine To Be Ready    ${start}  ${1}

        ${start}    Ctn Get Round Current Date
        Ctn Process Service Check Result    host_1    service_1    0    output ok D: \\: Total: 1.205TB - Used: 1.203TB (100%) - Free: 2.541GB (0%) ${use_grpc}  config0  ${use_grpc}

        ${result}    Ctn Check Service Output Resource Status With Timeout
        ...    host_1
        ...    service_1
        ...    35
        ...    ${start}
        ...    0
        ...    HARD
        ...    output ok D: \\: Total: 1.205TB - Used: 1.203TB (100%) - Free: 2.541GB (0%) ${use_grpc}
        Should Be True    ${result}    resources table not updated


        Ctn Stop Engine
        Ctn Kindly Stop Broker
    END
