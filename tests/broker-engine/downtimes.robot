*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Clean Downtimes Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
BEDTMASS1
    [Documentation]    New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.0
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${3}    ${50}    ${20}
    Engine Config Set Value    ${0}    log_level_functions    trace
    Engine Config Set Value    ${1}    log_level_functions    trace
    Engine Config Set Value    ${2}    log_level_functions    trace
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    debug
    Broker Config Log    module1    neb    debug
    Broker Config Log    module2    neb    debug

    Config BBDO3    3
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # It's time to schedule downtimes
    FOR    ${i}    IN RANGE    ${17}
        Schedule Host Downtime    ${0}    host_${i + 1}    ${3600}
        Schedule Host Downtime    ${1}    host_${i + 18}    ${3600}
        Schedule Host Downtime    ${2}    host_${i + 35}    ${3600}
    END

    ${result}    Check Number Of Downtimes    ${1050}    ${start}    ${60}
    Should Be True    ${result}    We should have 1050 downtimes enabled.

    # It's time to delete downtimes
    FOR    ${i}    IN RANGE    ${17}
        Delete Host Downtimes    ${0}    host_${i + 1}
        Delete Host Downtimes    ${1}    host_${i + 18}
        Delete Host Downtimes    ${2}    host_${i + 35}
    END

    ${result}    Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime enabled.

    Stop Engine
    Kindly Stop Broker

BEDTMASS2
    [Documentation]    New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 2.0
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${3}    ${50}    ${20}
    Engine Config Set Value    ${0}    log_level_functions    trace
    Engine Config Set Value    ${1}    log_level_functions    trace
    Engine Config Set Value    ${2}    log_level_functions    trace
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    debug
    Broker Config Log    module1    neb    debug
    Broker Config Log    module2    neb    debug

    Broker Config Log    central    sql    debug
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # It's time to schedule downtimes
    FOR    ${i}    IN RANGE    ${17}
        Schedule Host Downtime    ${0}    host_${i + 1}    ${3600}
        Schedule Host Downtime    ${1}    host_${i + 18}    ${3600}
        Schedule Host Downtime    ${2}    host_${i + 35}    ${3600}
    END

    ${result}    Check Number Of Downtimes    ${1050}    ${start}    ${60}
    Should Be True    ${result}    We should have 1050 downtimes enabled.

    # It's time to delete downtimes
    FOR    ${i}    IN RANGE    ${17}
        Delete Host Downtimes    ${0}    host_${i + 1}
        Delete Host Downtimes    ${1}    host_${i + 18}
        Delete Host Downtimes    ${2}    host_${i + 35}
    END

    ${result}    Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime enabled.

    Stop Engine
    Kindly Stop Broker

BEDTSVCREN1
    [Documentation]    A downtime is set on a service then the service is renamed. The downtime is still active on the renamed service. The downtime is removed from the renamed service and it is well removed.
    [Tags]    broker    engine    services    downtime
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_level_functions    trace
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    debug

    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # It's time to schedule a downtime
    Schedule Service Downtime    host_1    service_1    ${3600}

    ${result}    Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should Be True    ${result}    We should have 1 downtime enabled.

    # Let's rename the service service_1
    Rename Service    ${0}    host_1    service_1    toto_1

    Reload Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    Delete Service Downtime Full    ${0}    host_1    toto_1

    ${result}    Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime enabled.

    Stop Engine
    Kindly Stop Broker

BEDTSVCFIXED
    [Documentation]    A downtime is set on a service, the total number of downtimes is really 1 then we delete this downtime and the number of downtime is 0.
    [Tags]    broker    engine    downtime
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_level_functions    trace
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    debug

    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # It's time to schedule a downtime
    Schedule Service Downtime    host_1    service_1    ${3600}

    ${result}    Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should Be True    ${result}    We should have 1 downtime enabled.

    Delete Service Downtime Full    ${0}    host_1    service_1

    ${result}    Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime enabled.

    Stop Engine
    Kindly Stop Broker

BEDTHOSTFIXED
    [Documentation]    A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
    [Tags]    broker    engine    downtime
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_level_functions    trace
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    debug
    Config Broker Sql Output    central    unified_sql

    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    log to console    step1
    # It's time to schedule downtimes
    Schedule Host Fixed Downtime    ${0}    host_1    ${3600}
    log to console    step2
    ${content}    Create List    HOST DOWNTIME ALERT: host_1;STARTED; Host has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_1;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_2;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_3;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_4;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_5;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_6;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_7;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_8;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_9;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_10;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_11;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_12;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_13;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_14;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_15;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_16;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_17;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_18;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_19;STARTED; Service has entered a period of scheduled downtime
    ...    SERVICE DOWNTIME ALERT: host_1;service_20;STARTED; Service has entered a period of scheduled downtime
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    We should have 1 host downtime and 20 service downtimes on engine side.

    ${result}    Check Number Of Downtimes    ${21}    ${start}    ${60}
    log to console    step3
    Should Be True    ${result}    We should have 21 downtimes (1 host + 20 services) enabled.
    log to console    step4

    # It's time to delete downtimes
    Delete Host Downtimes    ${0}    host_1
    ${content}    Create List    HOST DOWNTIME ALERT: host_1;CANCELLED; Scheduled downtime for host has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_1;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_2;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_3;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_4;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_5;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_6;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_7;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_8;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_9;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_10;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_11;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_12;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_13;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_14;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_15;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_16;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_17;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_18;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_19;CANCELLED; Scheduled downtime for service has been cancelled.
    ...    SERVICE DOWNTIME ALERT: host_1;service_20;CANCELLED; Scheduled downtime for service has been cancelled.
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    We should have 1 host downtime and 20 service downtimes on engine side.

    ${result}    Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime enabled.

    Stop Engine
    Kindly Stop Broker

DTIM
    [Documentation]    New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
    [Tags]    broker    engine    services    host    downtimes    MON-19339
    Config Engine    ${5}    ${250}    ${20}
    Engine Config Set Value    ${0}    log_level_functions    trace
    Engine Config Set Value    ${1}    log_level_functions    trace
    Engine Config Set Value    ${2}    log_level_functions    trace
    Engine Config Set Value    ${3}    log_level_functions    trace
    Engine Config Set Value    ${4}    log_level_functions    trace
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${5}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    module1    bbdo_version    3.0.1
    Broker Config Add Item    module2    bbdo_version    3.0.1
    Broker Config Add Item    module3    bbdo_version    3.0.1
    Broker Config Add Item    module4    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Config Broker Sql Output    central    unified_sql
    Clear Retention

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog4}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # It's time to schedule downtimes
    FOR    ${i}    IN RANGE    ${50}
        Schedule Host Fixed Downtime    ${0}    host_${i + 1}    ${3600}
        Schedule Host Fixed Downtime    ${1}    host_${i + 51}    ${3600}
        Schedule Host Fixed Downtime    ${2}    host_${i + 101}    ${3600}
        Schedule Host Fixed Downtime    ${3}    host_${i + 151}    ${3600}
        Schedule Host Fixed Downtime    ${4}    host_${i + 201}    ${3600}
    END

    ${result}    Check Number Of Downtimes    ${5250}    ${start}    ${60}
    Should be true    ${result}    We should have 5250 downtimes enabled.

    # It's time to delete downtimes
    FOR    ${i}    IN RANGE    ${50}
        Delete Host Downtimes    ${0}    host_${i + 1}
        Delete Host Downtimes    ${1}    host_${i + 51}
        Delete Host Downtimes    ${2}    host_${i + 101}
        Delete Host Downtimes    ${3}    host_${i + 151}
        Delete Host Downtimes    ${4}    host_${i + 201}
    END

    ${result}    Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    There are still some downtimes enabled.

    Stop Engine
    Kindly Stop Broker


*** Keywords ***
Clean Downtimes Before Suite
    Clean Before Suite

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Execute SQL String    DELETE FROM downtimes WHERE deletion_time IS NULL
