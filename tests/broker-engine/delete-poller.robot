*** Settings ***
Documentation       Creation of 4 pollers and then deletion of Poller3.

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
EBDP1
    [Documentation]    Four new pollers are started and then we remove Poller3.
    [Tags]    broker    engine    grpc
    Config Engine    ${4}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${4}
    Config BBDO3    ${4}
    Broker Config Log    central    sql    trace
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((4,),)

    Stop Engine
    Kindly Stop Broker
    # Poller3 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${3}    ${50}    ${20}
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the initial service states.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Remove Poller    51001    Poller3
    Sleep    6s

    Stop Engine
    Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP2
    [Documentation]    Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Config Engine    ${3}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Config BBDO3    ${3}
    Broker Config Log    central    sql    trace
    Broker Config Log    central    processing    info
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((3,),)

    ${start_remove}    Get Current Date
    Send Signal To Process    SIGKILL    e0
    Send Signal To Process    SIGKILL    e1
    Send Signal To Process    SIGKILL    e2
    Terminate Process    e0
    Terminate Process    e1
    Terminate Process    e2

    ${content}    Create List    feeder 'central-broker-master-input-\d', connection closed
    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start_remove}    ${content}    60
    Should Be True    ${result}    connection closed not found.

    Log To Console    Reconfiguration of 2 pollers
    # Poller2 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Kindly Stop Broker
    Clear Engine Logs
    Start Engine
    Start Broker

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Remove Poller    51001    Poller2

    Stop Engine
    Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP_GRPC2
    [Documentation]    Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Config Engine    ${3}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Config BBDO3    ${3}
    Config Broker BBDO Input    central    bbdo_server    5669    grpc
    Config Broker BBDO Output    module0    bbdo_client    5669    grpc    localhost
    Config Broker BBDO Output    module1    bbdo_client    5669    grpc    localhost
    Config Broker BBDO Output    module2    bbdo_client    5669    grpc    localhost
    Broker Config Log    central    sql    trace
    Broker Config Log    central    processing    info
    Broker Config Log    central    grpc    info
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((3,),)

    # Let's brutally kill the poller
    ${start_remove}    Get Current Date
    Send Signal To Process    SIGKILL    e0
    Send Signal To Process    SIGKILL    e1
    Send Signal To Process    SIGKILL    e2
    Terminate Process    e0
    Terminate Process    e1
    Terminate Process    e2

    ${content}    Create List    feeder 'central-broker-master-input-\d', connection closed
    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start_remove}    ${content}    60
    Should Be True    ${result}    connection closed not found.

    Log To Console    Reconfiguration of 2 pollers
    # Poller2 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Kindly Stop Broker
    Clear Engine Logs
    Start Engine
    Start Broker

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Remove Poller    51001    Poller2

    Stop Engine
    Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP3
    [Documentation]    Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Config Engine    ${3}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Config BBDO3    ${3}
    Broker Config Log    central    sql    trace
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((3,),)

    # Let's brutally kill the poller
    Send Signal To Process    SIGKILL    e0
    Send Signal To Process    SIGKILL    e1
    Send Signal To Process    SIGKILL    e2
    Terminate Process    e0
    Terminate Process    e1
    Terminate Process    e2

    Log To Console    Reconfiguration of 2 pollers
    # Poller2 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Clear Engine Logs
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Remove Poller    51001    Poller2

    Stop Engine
    Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP4
    [Documentation]    Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
    [Tags]    broker    engine    grpc
    Config Engine    ${4}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${4}
    Config BBDO3    ${4}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    trace
    Broker Config Log    module3    neb    trace
    Broker Config Flush Log    central    0
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((4,),)

    # Let's brutally kill the poller
    ${content}    Create List    processing poller event (id: 4, name: Poller3, running:
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    We want the poller 4 event before stopping broker
    Kindly Stop Broker
    Remove Files    ${centralLog}    ${rrdLog}

    # Generation of many service status but kept in memory on poller3.
    FOR    ${i}    IN RANGE    200
        Process Service Check Result    host_40    service_781    2    service_781 should fail    config3
        Process Service Check Result    host_40    service_782    1    service_782 should fail    config3
    END
    ${content}    Create List
    ...    SERVICE ALERT: host_40;service_781;CRITICAL
    ...    SERVICE ALERT: host_40;service_782;WARNING
    ${result}    Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    Service alerts about service 781 and 782 should be raised

    ${content}    Create List    callbacks: service (40, 781) has no perfdata    service (40, 782) has no perfdata
    ${result}    Find In Log With Timeout    ${moduleLog3}    ${start}    ${content}    60
    Should Be True    ${result}    pb service status on services (40, 781) and (40, 782) should be generated
    Stop Engine

    # Because poller3 is going to be removed, we move its memory file to poller0, 1 and 2.
    Move File
    ...    ${VarRoot}/lib/centreon-engine/central-module-master3.memory.central-module-master-output
    ...    ${VarRoot}/lib/centreon-engine/central-module-master0.memory.central-module-master-output

    # Poller3 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${3}    ${39}    ${20}

    # Restart Broker
    Start Broker

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Remove Poller    51001    Poller3
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    Start Engine
    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${content}    Create List    service status (40, 781) thrown away because host 40 is not known by any poller
    Log To Console    date ${start}
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about these two wrong service status.
    Stop Engine
    Kindly Stop Broker

EBDP5
    [Documentation]    Four new pollers are started and then we remove Poller3.
    [Tags]    broker    engine    grpc
    Config Engine    ${4}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${4}
    Config BBDO3    ${4}
    Broker Config Log    central    sql    trace
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((4,),)

    Stop Engine
    Kindly Stop Broker
    # Poller3 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${3}    ${50}    ${20}
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${remove_time}    Get Current Date
    Remove Poller By Id    51001    ${4}

    # wait unified receive instance event
    ${content}    Create List    central-broker-unified-sql read neb:Instance
    ${result}    Find In Log With Timeout    ${centralLog}    ${remove_time}    ${content}    60
    Should Be True    ${result}    central-broker-unified-sql read neb:Instance is missing

    Stop Engine
    Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP6
    [Documentation]    Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Config Engine    ${3}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Config BBDO3    ${3}
    Broker Config Log    central    sql    trace
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((3,),)

    # Let's brutally kill the poller
    Send Signal To Process    SIGKILL    e0
    Send Signal To Process    SIGKILL    e1
    Send Signal To Process    SIGKILL    e2
    Terminate Process    e0
    Terminate Process    e1
    Terminate Process    e2

    Log To Console    Reconfiguration of 2 pollers
    # Poller2 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Kindly Stop Broker
    Clear Engine Logs
    Start Engine
    Start Broker

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${remove_time}    Get Current Date
    Remove Poller By Id    51001    ${3}

    # wait unified receive instance event
    ${content}    Create List    central-broker-unified-sql read neb:Instance
    ${result}    Find In Log With Timeout    ${centralLog}    ${remove_time}    ${content}    60
    Should Be True    ${result}    central-broker-unified-sql read neb:Instance is missing

    Stop Engine
    Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP7
    [Documentation]    Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Config Engine    ${3}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Config BBDO3    ${3}
    Broker Config Log    central    sql    trace
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((3,),)

    # Let's brutally kill the poller
    Send Signal To Process    SIGKILL    e0
    Send Signal To Process    SIGKILL    e1
    Send Signal To Process    SIGKILL    e2
    Terminate Process    e0
    Terminate Process    e1
    Terminate Process    e2

    Log To Console    Reconfiguration of 2 pollers
    # Poller2 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Clear Engine Logs
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${remove_time}    Get Current Date
    Remove Poller By Id    51001    ${3}

    # wait unified receive instance event
    ${content}    Create List    central-broker-unified-sql read neb:Instance
    ${result}    Find In Log With Timeout    ${centralLog}    ${remove_time}    ${content}    60
    Should Be True    ${result}    central-broker-unified-sql read neb:Instance is missing

    Stop Engine
    Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP8
    [Documentation]    Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
    [Tags]    broker    engine    grpc
    Config Engine    ${4}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${4}
    Config BBDO3    ${4}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    trace
    Broker Config Log    module3    neb    trace
    Broker Config Flush Log    central    0
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((4,),)

    # Let's brutally kill the poller
    ${content}    Create List    processing poller event (id: 4, name: Poller3, running:
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    We want the poller 4 event before stopping broker
    Kindly Stop Broker
    Remove Files    ${centralLog}    ${rrdLog}

    # Generation of many service status but kept in memory on poller3.
    FOR    ${i}    IN RANGE    200
        Process Service Check Result    host_40    service_781    2    service_781 should fail    config3
        Process Service Check Result    host_40    service_782    1    service_782 should fail    config3
    END
    ${content}    Create List
    ...    SERVICE ALERT: host_40;service_781;CRITICAL
    ...    SERVICE ALERT: host_40;service_782;WARNING
    ${result}    Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    Service alerts about service 781 and 782 should be raised

    ${content}    Create List    callbacks: service (40, 781) has no perfdata    service (40, 782) has no perfdata
    ${result}    Find In Log With Timeout    ${moduleLog3}    ${start}    ${content}    60
    Should Be True    ${result}    pb service status on services (40, 781) and (40, 782) should be generated
    Stop Engine

    # Because poller3 is going to be removed, we move its memory file to poller0, 1 and 2.
    Move File
    ...    ${VarRoot}/lib/centreon-engine/central-module-master3.memory.central-module-master-output
    ...    ${VarRoot}/lib/centreon-engine/central-module-master0.memory.central-module-master-output

    # Poller3 is removed from the engine configuration but still there in centreon_storage DB
    Config Engine    ${3}    ${39}    ${20}

    # Restart Broker
    Start Broker
    Remove Poller By Id    51001    ${4}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    Start Engine
    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${content}    Create List    service status (40, 781) thrown away because host 40 is not known by any poller
    Log To Console    date ${start}
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about these two wrong service status.
    Stop Engine
    Kindly Stop Broker
