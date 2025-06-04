*** Settings ***
Documentation       Centreon Broker victoria metrics tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
VICT_ONE_CHECK_METRIC
    [Documentation]    victoria metrics metric output
    [Tags]    broker    engine    victoria_metrics
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    victoria_metrics    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Victoria Output
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Start Server    127.0.0.1    8000
    # wait all is started
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    Ctn Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99

    ${start}    Ctn Get Round Current Date
    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${metric_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${metric_found}    Ctn Check Victoria Metric
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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    AND    Stop Server

VICT_ONE_CHECK_STATUS
    [Documentation]    victoria metrics status output
    [Tags]    broker    engine    victoria_metrics
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    victoria_metrics    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Victoria Output
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Start Server    127.0.0.1    8000
    # wait all is started
    Ctn Wait For Engine To Be Ready    ${start}

    # service ok
    ${start}    Ctn Get Round Current Date
    Ctn Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99

    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${status_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${status_found}    Ctn Check Victoria Status
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

    # Wait for 1s to be sure that the timestamp is different with previous Service Check Result.
    Sleep    1s
    # service warning
    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard
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
            ${status_found}    Ctn Check Victoria Status
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

    Sleep    1s
    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard
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
            ${status_found}    Ctn Check Victoria Status
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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    AND    Stop Server

VICT_ONE_CHECK_METRIC_AFTER_FAILURE
    [Documentation]    victoria metrics metric output after victoria shutdown
    [Tags]    broker    engine    victoria_metrics
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    victoria_metrics    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Victoria Output
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    # wait all is started
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    Ctn Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99
    ${start}    Ctn Get Round Current Date

    ${content}    Create List    [victoria_metrics]    name: "metric_taratata"
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
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
            ${metric_found}    Ctn Check Victoria Metric
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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    AND    Stop Server
