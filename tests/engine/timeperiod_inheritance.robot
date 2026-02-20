*** Settings ***
Documentation       Centreon Engine verify timeperiod inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
ETPI0
    [Documentation]    Verify that a host using a timeperiod that inherits from a template
    ...    timeperiod via the "use" directive correctly resolves the check_period on start.
    ...    The child timeperiod has no day ranges of its own.
    [Tags]    broker    engine    timeperiod    MON-192707
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention

    # Add a parent timeperiod template and a child that inherits from it
    Ctn Create Timeperiod With Template    ${0}

    # Set host to use the child timeperiod as check_period
    Ctn Engine Config Set Value In Hosts    0    host_1    check_period    child_tp

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${1}

    Should Be Equal As Strings
    ...    ${output}[checkPeriod]
    ...    child_tp
    ...    Host check_period should be 'child_tp' inherited from parent template

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ETPI1
    [Documentation]    Verify that a host using a timeperiod that inherits from a template
    ...    timeperiod via the "use" directive correctly resolves the check_period after
    ...    an engine reload.
    [Tags]    broker    engine    timeperiod    MON-192707
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Add a parent timeperiod template and a child that inherits from it
    Ctn Create Timeperiod With Template    ${0}

    # Set host to use the child timeperiod as check_period
    Ctn Engine Config Set Value In Hosts    0    host_1    check_period    child_tp

    ${start}    Ctn Get Round Current Date
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

    Should Be Equal As Strings
    ...    ${output}[checkPeriod]
    ...    child_tp
    ...    Host check_period should be 'child_tp' inherited from parent template after reload

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ETPI2
    [Documentation]    Verify that a child timeperiod overrides the parent alias while still
    ...    inheriting timeranges from the parent via the "use" directive.
    [Tags]    broker    engine    timeperiod    MON-192707
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention

    # Add parent TP and a child that defines its own alias but inherits timeranges
    Ctn Create Timeperiod With Template And Override    ${0}

    # Set host to use the child timeperiod as check_period
    Ctn Engine Config Set Value In Hosts    0    host_1    check_period    child_tp_override

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${1}

    Should Be Equal As Strings
    ...    ${output}[checkPeriod]
    ...    child_tp_override
    ...    Host check_period should be 'child_tp_override'

    Ctn Stop Engine
    Ctn Kindly Stop Broker
