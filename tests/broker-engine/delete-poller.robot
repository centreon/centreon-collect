*** Settings ***
Documentation       Creation of 4 pollers and then deletion of Poller3.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EBDP1
    [Documentation]    Four new pollers are started and then we remove Poller3.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${4}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    sql    trace
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((4,),)

    Ctn Stop engine
    Ctn Kindly Stop Broker
    # Poller3 is removed from the engine configuration but still there in centreon_storage DB
    Ctn Config Engine    ${3}    ${50}    ${20}
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait for the initial service states.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Ctn Remove Poller    51001    Poller3
    Sleep    6s

    Ctn Stop engine
    Ctn Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP2
    [Documentation]    Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Config BBDO3    ${3}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    processing    info
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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

    ${content}    Create List    feeder 'central-broker-master-input-\\d', connection closed
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start_remove}    ${content}    60
    Should Be True    ${result[0]}    connection closed not found.

    Log To Console    Reconfiguration of 2 pollers
    # Poller2 is removed from the engine configuration but still there in centreon_storage DB
    Ctn Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Ctn Kindly Stop Broker
    Ctn Clear Engine Logs
    Ctn Start engine
    Ctn Start Broker

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Ctn Remove Poller    51001    Poller2

    Ctn Stop engine
    Ctn Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP_GRPC2
    [Documentation]    Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Config BBDO3    ${3}
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    module0    bbdo_client    5669    grpc    localhost
    Ctn Config Broker Bbdo Output    module1    bbdo_client    5669    grpc    localhost
    Ctn Config Broker Bbdo Output    module2    bbdo_client    5669    grpc    localhost
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    processing    info
    Ctn Broker Config Log    central    grpc    info
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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

    ${content}    Create List    feeder 'central-broker-master-input-\\d', connection closed
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start_remove}    ${content}    60
    Should Be True    ${result[0]}    connection closed not found.

    Log To Console    Reconfiguration of 2 pollers
    # Poller2 is removed from the engine configuration but still there in centreon_storage DB
    Ctn Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Ctn Kindly Stop Broker
    Ctn Clear Engine Logs
    Ctn Start engine
    Ctn Start Broker

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Ctn Remove Poller    51001    Poller2

    Ctn Stop engine
    Ctn Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP3
    [Documentation]    Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Config BBDO3    ${3}
    Ctn Broker Config Log    central    sql    trace
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog2}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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
    Ctn Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Ctn Clear Engine Logs
    Ctn Start engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Ctn Remove Poller    51001    Poller2

    Ctn Stop engine
    Ctn Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id, running, deleted, outdated FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP4
    [Documentation]    Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${4}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    neb    trace
    Ctn Broker Config Log    module1    neb    trace
    Ctn Broker Config Log    module2    neb    trace
    Ctn Broker Config Log    module3    neb    trace
    Ctn Broker Config Flush Log    central    0
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${4}

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((4,),)

    # Let's brutally kill the poller
    ${content}    Create List    processing poller event (id: 4, name: Poller3, running:
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    We want the poller 4 event before stopping broker
    Ctn Kindly Stop Broker
    Remove Files    ${centralLog}    ${rrdLog}

    # Generation of many service status but kept in memory on poller3.
    FOR    ${i}    IN RANGE    200
        Ctn Process Service Check Result    host_40    service_781    2    service_781 should fail    config3
        Ctn Process Service Check Result    host_40    service_782    1    service_782 should fail    config3
    END
    ${content}    Create List
    ...    SERVICE ALERT: host_40;service_781;CRITICAL
    ...    SERVICE ALERT: host_40;service_782;WARNING
    ${result}    Ctn Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    Service alerts about service 781 and 782 should be raised

    ${content}    Create List    callbacks: service (40, 781) has no perfdata    service (40, 782) has no perfdata
    ${result}    Ctn Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    pb service status on services (40, 781) and (40, 782) should be generated
    Ctn Stop engine

    # Because poller3 is going to be removed, we move its memory file to poller0, 1 and 2.
    Move File
    ...    ${VarRoot}/lib/centreon-engine/central-module-master3.memory.central-module-master-output
    ...    ${VarRoot}/lib/centreon-engine/central-module-master0.memory.central-module-master-output

    # Poller3 is removed from the engine configuration but still there in centreon_storage DB
    Ctn Config Engine    ${3}    ${39}    ${20}

    # Restart Broker
    Ctn Start Broker

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    Ctn Remove Poller    51001    Poller3
    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    Ctn Start engine
    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${content}    Create List    service status (40, 781) thrown away because host 40 is not known by any poller
    Log To Console    date ${start}
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about these two wrong service status.
    Ctn Stop engine
    Ctn Kindly Stop Broker

EBDP5
    [Documentation]    Four new pollers are started and then we remove Poller3.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${4}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    core    trace
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait until engine listens to external_commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((4,),)

    Ctn Stop engine
    Ctn Kindly Stop Broker
    # Poller3 is removed from the engine configuration but still there in centreon_storage DB
    Ctn Config Engine    ${3}    ${50}    ${20}
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait until engine listens to external_commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${remove_time}    Get Current Date
    Ctn Remove Poller By Id    51001    ${4}

    # wait unified receive instance event
    ${content}    Create List    central-broker-unified-sql read neb:Instance
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${remove_time}    ${content}    60
    Should Be True    ${result}    central-broker-unified-sql read neb:Instance is missing

    Ctn Stop engine
    Ctn Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP6
    [Documentation]    Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Config BBDO3    ${3}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    core    trace
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${3}

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
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
    Ctn Config Engine    ${2}    ${50}    ${20}
    ${start}    Get Current Date
    Ctn Kindly Stop Broker
    Ctn Clear Engine Logs
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${remove_time}    Get Current Date
    Ctn Remove Poller By Id    51001    ${3}

    # wait unified receive instance event
    ${content}    Create List    central-broker-unified-sql read neb:Instance
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${remove_time}    ${content}    60
    Should Be True    ${result}    central-broker-unified-sql read neb:Instance is missing

    Ctn Stop Engine
    Ctn Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBDP7
    [Documentation]    Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Config BBDO3    ${3}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    core    trace
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    Ctn Wait For Engine To Be Ready    ${start}    ${3}

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller2'
        Sleep    1s
        Log To Console    Output with 3 pollers: ${output}
        IF    ${output} != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((3,),)    There are 3 running pollers.

    # Let's brutally kill the pollers
    Send Signal To Process    SIGKILL    e0
    Send Signal To Process    SIGKILL    e1
    Send Signal To Process    SIGKILL    e2
    Terminate Process    e0
    Terminate Process    e1
    Terminate Process    e2

    Log To Console    Reconfiguration of 2 pollers
    # Poller2 is removed from the engine configuration but still there in centreon_storage DB
    Ctn Config Engine    ${2}    ${50}    ${20}
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    ${2}
    ${start}    Get Current Date
    Ctn Clear Engine Logs
    Ctn Start engine
    Ctn Wait For Engine To Be Ready    ${2}

    ${remove_time}    Get Current Date
    Ctn Remove Poller By Id    51001    ${3}

    # wait unified receive instance event
    ${content}    Create List    central-broker-unified-sql read neb:Instance
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${remove_time}    ${content}    60
    Should Be True    ${result}    central-broker-unified-sql read neb:Instance is missing

    Ctn Stop engine
    Ctn Kindly Stop Broker

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id, running, deleted, outdated FROM instances WHERE instance_id=3
        Sleep    1s
        Log To Console    Output= ${output}
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

EBDP8
    [Documentation]    Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
    [Tags]    broker    engine    grpc
    Ctn Config Engine    ${4}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module3    neb    trace
    Ctn Broker Config Flush Log    central    0
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((4,),)

    # We want the poller 3 event handled by broker before stopping broker
    ${content}    Create List    processing poller event (id: 4, name: Poller3, running:
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    We want the poller 4 event before stopping broker
    Ctn Kindly Stop Broker
    Remove Files    ${centralLog}    ${rrdLog}

    # Generation of many service status but kept in memory on poller3 since broker is switched off.
    FOR    ${i}    IN RANGE    200
        Ctn Process Service Check Result    host_40    service_781    2    service_781 should fail    config3
        Ctn Process Service Check Result    host_40    service_782    1    service_782 should fail    config3
    END
    ${content}    Create List
    ...    SERVICE ALERT: host_40;service_781;CRITICAL
    ...    SERVICE ALERT: host_40;service_782;WARNING
    ${result}    Ctn Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    Service alerts about service 781 and 782 should be raised

    ${content}    Create List    callbacks: service (40, 781) has no perfdata    service (40, 782) has no perfdata
    ${result}    Ctn Find In Log With Timeout    ${engineLog3}    ${start}    ${content}    60
    Should Be True    ${result}    pb service status on services (40, 781) and (40, 782) should be generated
    Ctn Stop engine

    # Because poller3 is going to be removed, we move its memory file to poller0, 1 and 2.
    Move File
    ...    ${VarRoot}/lib/centreon-engine/central-module-master3.memory.central-module-master-output
    ...    ${VarRoot}/lib/centreon-engine/central-module-master0.memory.central-module-master-output

    # Poller3 is removed from the engine configuration but still there in centreon_storage DB
    Ctn Config Engine    ${3}    ${39}    ${20}

    # Restart Broker
    Ctn Start Broker
    Ctn Remove Poller By Id    51001    ${4}
    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT instance_id FROM instances WHERE name='Poller3'
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    The Poller3 should be removed from the DB.

    Ctn Start engine
    # Let's wait until engine listens to external_commands.
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands is missing.

    ${content}    Create List    service status (40, 781) thrown away because host 40 is not known by any poller
    Log To Console    date ${start}
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about these two wrong service status.
    Ctn Stop engine
    Ctn Kindly Stop Broker
