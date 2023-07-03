*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
Library             Process
Library             DatabaseLibrary
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             Engine.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed

*** Test Cases ***
not1
    [Documentation]    1 services id configurd and,checking that the non-ok notification is sent.
    [Tags]    broker    engine    services    unified_sql
    Config Engine    ${1}    ${1}    ${1}
    engine_config_set_value    0    enable_notifications    1    True
    engine_config_set_value    0    execute_host_checks    1    True
    engine_config_set_value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    engine_config_add_value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    engine_config_add_command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}=    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.



## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    1s
    END

    ${result}=    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Sleep    1m
