*** Settings ***
Resource    ../resources/resources.robot
Suite Setup    Ctn Clean Downtimes Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed

Documentation    Centreon Broker and Engine progressively add services
Library    Process
Library    DatabaseLibrary
Library    OperatingSystem
Library    DateTime
Library    Collections
Library    ../resources/Engine.py
Library    ../resources/Broker.py
Library    ../resources/Common.py

*** Test Cases ***
BEDTMASS1
    [Documentation]    New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.0
    [Tags]    Broker    Engine    services    protobuf
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Engine Config Set Value    ${0}    log_level_functions    trace
    Ctn Engine Config Set Value    ${1}    log_level_functions    trace
    Ctn Engine Config Set Value    ${2}    log_level_functions    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    module2    neb    debug

    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module1    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module2    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # It's time to schedule downtimes
    FOR    ${i}    IN RANGE    ${17}
        Ctn Schedule Host Downtime    ${0}    host_${i + 1}    ${3600}
        Ctn Schedule Host Downtime    ${1}    host_${i + 18}    ${3600}
        Ctn Schedule Host Downtime    ${2}    host_${i + 35}    ${3600}
    END

    ${result}    Ctn Check Number Of Downtimes    ${1050}    ${start}    ${60}
    Should be true    ${result}    We should have 1050 downtimes enabled.

    # It's time to delete downtimes
    FOR    ${i}    IN RANGE    ${17}
        Ctn Delete Host Downtimes    ${0}    host_${i + 1}
        Ctn Delete Host Downtimes    ${1}    host_${i + 18}
        Ctn Delete Host Downtimes    ${2}    host_${i + 35}
    END

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should be true    ${result}    We should have no downtime enabled.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEDTMASS2
    [Documentation]    New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 2.0
    [Tags]    Broker    Engine    services    protobuf
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Engine Config Set Value    ${0}    log_level_functions    trace
    Ctn Engine Config Set Value    ${1}    log_level_functions    trace
    Ctn Engine Config Set Value    ${2}    log_level_functions    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    module2    neb    debug

    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # It's time to schedule downtimes
    FOR    ${i}    IN RANGE    ${17}
        Ctn Schedule Host Downtime    ${0}    host_${i + 1}    ${3600}
        Ctn Schedule Host Downtime    ${1}    host_${i + 18}    ${3600}
        Ctn Schedule Host Downtime    ${2}    host_${i + 35}    ${3600}
    END

    ${result}    Ctn Check Number Of Downtimes    ${1050}    ${start}    ${60}
    Should be true    ${result}    We should have 1050 downtimes enabled.

    # It's time to delete downtimes
    FOR    ${i}    IN RANGE    ${17}
        Ctn Delete Host Downtimes    ${0}    host_${i + 1}
        Ctn Delete Host Downtimes    ${1}    host_${i + 18}
        Ctn Delete Host Downtimes    ${2}    host_${i + 35}
    END

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should be true    ${result}    We should have no downtime enabled.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEDTSVCREN1
    [Documentation]    A downtime is set on a service then the service is renamed. The downtime is still active on the renamed service. The downtime is removed from the renamed service and it is well removed.
    [Tags]    Broker    Engine    services    downtime
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_functions    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    debug

    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # It's time to schedule a downtime
    Ctn Schedule Service Downtime    host_1    service_1    ${3600}

    ${result}    Ctn Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should be true    ${result}    We should have 1 downtime enabled.

    # Let's rename the service service_1
    Ctn Rename Service    ${0}    host_1    service_1    toto_1

    Ctn Reload Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    Ctn Delete Service Downtime Full    ${0}    host_1    toto_1

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should be true    ${result}    We should have no downtime enabled.

    Ctn Stop Engine
    Ctn Kindly Stop Broker


BEDTSVCFIXED
    [Documentation]    A downtime is set on a service, the total number of downtimes is really 1 then we delete this downtime and the number of downtime is 0.
    [Tags]    Broker    Engine    downtime
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_functions    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    debug

    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # It's time to schedule a downtime
    Schedule service downtime  host_1  service_1  ${3600}

    ${result}    Ctn Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should be true    ${result}    We should have 1 downtime enabled.

    Ctn Delete Service Downtime Full    ${0}    host_1    service_1

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should be true    ${result}    We should have no downtime enabled.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEDTHOSTFIXED
    [Documentation]    A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
    [Tags]    Broker    Engine    downtime
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_functions    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # It's time to schedule downtimes
    Ctn Schedule Host Fixed Downtime    ${0}    host_1    ${3600}

    ${result}    Ctn Check Number Of Downtimes    ${21}    ${start}    ${60}
    Should be true    ${result}    We should have 21 downtimes (1 host + 20 services) enabled.

    # It's time to delete downtimes
    Ctn Delete Host Downtimes    ${0}    host_1

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should be true    ${result}    We should have no downtime enabled.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

DTIM
    [Documentation]    New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
    [Tags]    broker    engine    services    host    downtimes    MON-36711
    Ctn Config Engine    ${5}    ${250}    ${20}
    Ctn Engine Config Set Value    ${0}    log_level_functions    trace
    Ctn Engine Config Set Value    ${1}    log_level_functions    trace
    Ctn Engine Config Set Value    ${2}    log_level_functions    trace
    Ctn Engine Config Set Value    ${3}    log_level_functions    trace
    Ctn Engine Config Set Value    ${4}    log_level_functions    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${5}
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.1
    Ctn Broker Config Add Item    module1    bbdo_version    3.0.1
    Ctn Broker Config Add Item    module2    bbdo_version    3.0.1
    Ctn Broker Config Add Item    module3    bbdo_version    3.0.1
    Ctn Broker Config Add Item    module4    bbdo_version    3.0.1
    Ctn Broker Config Add Item    central    bbdo_version    3.0.1
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog4}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # It's time to schedule downtimes
    FOR    ${i}    IN RANGE    ${50}
        Ctn Schedule Host Fixed Downtime    ${0}    host_${i + 1}    ${3600}
        Ctn Schedule Host Fixed Downtime    ${1}    host_${i + 51}    ${3600}
        Ctn Schedule Host Fixed Downtime    ${2}    host_${i + 101}    ${3600}
        Ctn Schedule Host Fixed Downtime    ${3}    host_${i + 151}    ${3600}
        Ctn Schedule Host Fixed Downtime    ${4}    host_${i + 201}    ${3600}
    END

    ${result}    Ctn Check Number Of Downtimes    ${5250}    ${start}    ${60}
    Should be true    ${result}    We should have 5250 downtimes enabled.

    # It's time to delete downtimes
    FOR    ${i}    IN RANGE    ${50}
        Ctn Delete Host Downtimes    ${0}    host_${i + 1}
        Ctn Delete Host Downtimes    ${1}    host_${i + 51}
        Ctn Delete Host Downtimes    ${2}    host_${i + 101}
        Ctn Delete Host Downtimes    ${3}    host_${i + 151}
        Ctn Delete Host Downtimes    ${4}    host_${i + 201}
    END

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    There are still some downtimes enabled.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

*** Keywords ***
Ctn Clean Downtimes Before Suite
    Ctn Clean Before Suite
    
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Execute SQL String    DELETE FROM downtimes WHERE deletion_time IS NULL
