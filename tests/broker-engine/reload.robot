*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BENCV
    [Documentation]    Engine is configured with hosts/services. The first host has no customvariable.
    ...    Then we add a customvariable to the first host and we reload engine.
    ...    Then the host should have this new customvariable defined and centengine should not crash.
    [Tags]    broker    engine    MON-147499
    Ctn Config Engine    ${1}
    Ctn Create Template File    ${0}    host    _CV    ["test1","test2","test3"]
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    use    host_template_1
    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${1}
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    new custom variable 'CV' with value 'test1' on host 1
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The host should have a new customvariable named _CV

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    use    host_template_2,host_template_1
    ${start}    Get Current Date
    Ctn Reload Engine
    ${content}    Create List    new custom variable 'CV' with value 'test2' on host 1
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The host should have a new customvariable named _CV

    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _CV    cv_value
    ${start}    Get Current Date
    Ctn Reload Engine
    ${content}    Create List    new custom variable 'CV' with value 'cv_value' on host 1
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The host should have a new customvariable named _CV


BENCR1
    [Documentation]    Given an engine configured with hosts and services with check commands,
    ...    when I change the check commands in the configuration and reload the engine,
    [Tags]    broker    engine    opentelemetry    MON-193407
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Engine Config Set Value    0    interval_length    10

    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    command_1
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    command_2

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd

    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1

    Ctn Clear Db    resources
    Ctn Clear Retention


    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # check in the db that the command line is stored correctly
    ${result}    Ctn Check Commandline Service With Timeout Rt    host_1    service_1    120   /tmp/var/lib/centreon-engine/check.pl --id 1
    Should Be True    ${result}    command line not found in db

    ${result}    Ctn Check Commandline Host With Timeout Rt    host_1    120   /tmp/var/lib/centreon-engine/check.pl --id 2
    Should Be True    ${result}    command line not found in db

    Sleep    ${5}
    Log To Console    host_1 use command_1
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    command_2
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    command_1

    Ctn Reload Engine


    ${result}    Ctn Check Commandline Service With Timeout Rt    host_1    service_1    120   /tmp/var/lib/centreon-engine/check.pl --id 2
    Should Be True    ${result}    command line not found in db

    ${result}    Ctn Check Commandline Host With Timeout Rt    host_1    120   /tmp/var/lib/centreon-engine/check.pl --id 1
    Should Be True    ${result}    command line not found in db