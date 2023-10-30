*** Settings ***
Documentation       Centreon Broker victoria metrics tests

Resource            ../resources/resources.robot
Library             String
Library             Process
Library             OperatingSystem
Library             DateTime
Library             HttpCtrl.Server
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
VICT_ONE_CHECK_METRIC
    [Documentation]    victoria metrics metric output
    [Tags]    broker    engine    victoria_metrics
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Clear Retention
    Broker Config Log    central    victoria_metrics    trace
    Broker Config Log    central    perfdata    trace
    Broker Config Source Log    central    1
    Config Broker Sql Output    central    unified_sql
    Config Broker Victoria Output
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Start Server    127.0.0.1    8000
    # wait all is started
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99

    ${start}    Get Round Current Date
    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${metric_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${metric_found}    Check Victoria Metric
            ...    ${body}
            ...    ${start}
            ...    unit=%
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    name=metric_taratata
            ...    val=80
            ...    min=5
            ...    max=99
        END
        IF    ${metric_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    AND    Stop Server

VICT_ONE_CHECK_STATUS
    [Documentation]    victoria metrics status output
    [Tags]    broker    engine    victoria_metrics
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Clear Retention
    Broker Config Log    central    victoria_metrics    trace
    Broker Config Log    central    perfdata    trace
    Broker Config Source Log    central    1
    Config Broker Sql Output    central    unified_sql
    Config Broker Victoria Output
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Start Server    127.0.0.1    8000
    # wait all is started
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    # service ok
    ${start}    Get Round Current Date
    Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99

    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${status_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${status_found}    Check Victoria Status
            ...    ${body}
            ...    ${start}
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    val=100
        END
        IF    ${status_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}

    # service warning
    ${start}    Get Round Current Date
    Process Service Result Hard
    ...    host_16
    ...    service_314
    ...    1
    ...    taratata|metric_taratata=80%;50;75;5;99

    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${status_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${status_found}    Check Victoria Status
            ...    ${body}
            ...    ${start}
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    val=75
        END
        IF    ${status_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}

    # service critical

    ${start}    Get Round Current Date
    Process Service Result Hard
    ...    host_16
    ...    service_314
    ...    2
    ...    taratata|metric_taratata=80%;50;75;5;99

    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${status_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${status_found}    Check Victoria Status
            ...    ${body}
            ...    ${start}
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    val=0
        END
        IF    ${status_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    AND    Stop Server

VICT_ONE_CHECK_METRIC_AFTER_FAILURE
    [Documentation]    victoria metrics metric output after victoria shutdown
    [Tags]    broker    engine    victoria_metrics
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Clear Retention
    Broker Config Log    central    victoria_metrics    trace
    Broker Config Log    central    perfdata    trace
    Broker Config Source Log    central    1
    Config Broker Sql Output    central    unified_sql
    Config Broker Victoria Output
    ${start}    Get Current Date
    Start Broker
    Start Engine
    # wait all is started
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99
    ${start}    Get Round Current Date

    ${content}    Create List    [victoria_metrics]    name: "metric_taratata"
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    victoria should add metric in a request

    Start Server    127.0.0.1    8000
    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${metric_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${metric_found}    Check Victoria Metric
            ...    ${body}
            ...    ${start}
            ...    unit=%
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    name=metric_taratata
            ...    val=80
            ...    min=5
            ...    max=99
        END
        IF    ${metric_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    AND    Stop Server
