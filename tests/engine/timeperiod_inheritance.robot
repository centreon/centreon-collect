*** Settings ***
Documentation       Centreon Engine verify servicegroup inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***

TIMEPERIOD_INHERITANCE
    [Documentation]    Given two timeperiods, one inherit from the second. First onex exclude next hour, checks must not be executed in next 5 minutes
    [Tags]    engine    timeperiod    MON-192707

    Ctn Config Engine    ${1}    ${1}    ${2}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    
    ${start_plus_5minutes}    Get Current Date    increment=300s
    Ctn Create Single Day Time Period    0    short_time_period    ${start_plus_5minutes}    2
    Ctn Engine Config Replace Value In Services    0    service_1    check_period    none
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1

    Ctn Engine Config Add Command    ${0}  echo_command   /bin/echo "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    echo_command
    Ctn Add Template To Timeperiod    0    short_time_period    ["none"]
    Ctn Engine Config Set Value    0    interval_length    10



    ${start_engine}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    Ctn Wait For Engine To Be Ready    ${start_engine}

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    0    180    HARD
    
    Should Not Be True    ${result}       service_1 must remained unknown and not checked 



TIMEPERIOD_INHERITANCE_EXCEPTION
    [Documentation]    Given two timeperiods, one inherit from the second. First one allow H24, second one has only one exception, checks must be executed in next minute
    [Tags]    engine    timeperiod    MON-200794

    Ctn Config Engine    ${1}    ${1}    ${2}
    Ctn Engine Config Set Value    0    log_level_checks    trace
    Ctn Engine Config Set Value    0    log_level_functions    trace
    Ctn Engine Config Set Value    0    log_level_config    trace
    Ctn Engine Config Set Value    0    log_level_runtime    trace
    Ctn Engine Config Set Value    0    log_level_events    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    
    ${start_plus_one_day}    Get Current Date    increment=1d
    Ctn Create Single Day Time Period    0    short_time_period    ${start_plus_one_day}    2
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1

    Ctn Engine Config Add Command    ${0}  echo_command   /bin/echo "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    echo_command
    Ctn Add Template To Timeperiod    0    short_time_period    ["24x7"]
    Ctn Engine Config Set Value    0    interval_length    10



    ${start_engine}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    Ctn Wait For Engine To Be Ready    ${start_engine}

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    0    60    HARD

    Should Be True    ${result}       service_1 must remained unknown and not checked


TIMEPERIOD_MERGE
    [Documentation]    Given a timeperiod with its own calendar exception (active now) and a template
    ...    with a different calendar exception (active yesterday), the merge must preserve the child's
    ...    exception. The service using the child timeperiod as check_period must be checked.
    [Tags]    engine    timeperiod    MON-200794

    Ctn Config Engine    ${1}    ${1}    ${2}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    # child_tp: calendar exception active now (starts 2 minutes ago, lasts 30 minutes)
    ${start_now}    Get Current Date    increment=-120s
    Ctn Create Single Day Time Period    0    child_tp    ${start_now}    30

    # template_tp: calendar exception active yesterday (inactive now)
    ${yesterday}    Get Current Date    increment=-1d
    Ctn Create Single Day Time Period    0    template_tp    ${yesterday}    30

    # child_tp uses template_tp as template: merge must keep child's own exception
    Ctn Add Template To Timeperiod    0    template_tp    ["child_tp"]

    Ctn Engine Config Replace Value In Services    0    service_1    check_period    child_tp
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1

    Ctn Engine Config Add Command    ${0}    echo_command    /bin/echo "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    echo_command
    Ctn Engine Config Set Value    0    interval_length    10

    ${start_engine}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    Ctn Wait For Engine To Be Ready    ${start_engine}

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    0    60    HARD

    Should Be True    ${result}    service_1 must be checked: child_tp exception is active now and must not be overwritten by template_tp's exception during merge

