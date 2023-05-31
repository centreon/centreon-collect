*** Settings ***
Documentation       Centreon Broker and Engine communication with or without compression

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             String
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BRGC1
    [Documentation]    Broker good reverse connection
    [Tags]    broker    map    reverse connection
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central_map
    Config Broker    module

    Log To Console    Compression set to
    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.
    Run Reverse Bam    ${50}    ${0.2}

    Kindly Stop Broker
    Stop Engine

    ${content}=    Create List
    ...    New incoming connection 'centreon-broker-master-map-2'
    ...    file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
    ${log}=    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}=    Find In Log With Timeout    ${log}    ${start}    ${content}    40
    Should Be True    ${result}    msg=Connection to map has failed.
    File Should Not Exist
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map*
    ...    msg=There should not exist que map files.

BRCTS1
    [Documentation]    Broker reverse connection too slow
    [Tags]    broker    map    reverse connection
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central_map
    Config Broker    module

    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.
    Run Reverse Bam    ${150}    ${10}

    Kindly Stop Broker
    Stop Engine

    ${content}=    Create List
    ...    New incoming connection 'centreon-broker-master-map-2'
    ...    file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
    ${log}=    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}=    Find In Log With Timeout    ${log}    ${start}    ${content}    40
    Should Be True    ${result}    msg=Connection to map has failed
    File Should Not Exist
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map*
    ...    msg=There should not exist queue map files.

BRCS1
    [Documentation]    Broker reverse connection stopped
    [Tags]    broker    map    reversed
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central_map
    Config Broker    module

    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.
    Kindly Stop Broker
    Stop Engine

    ${content}=    Create List
    ...    New incoming connection 'centreon-broker-master-map-2'
    ...    file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
    ${log}=    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}=    Find In Log With Timeout    ${log}    ${start}    ${content}    40
    Should Not Be True    ${result}    msg=Connection to map has failed
    File Should Not Exist
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map-2
    ...    msg=There should not exist queue map files.

BRCTSMN
    [Documentation]    Broker connected to map with neb filter
    [Tags]    broker    map    reverse connection
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central_map
    Config Broker    module
    Config BBDO3    ${1}

    Broker Config Output Set Json    central    centreon-broker-master-map    filters    {"category": ["neb"]}
    Broker Config Log    central    bbdo    trace
    Broker Config Log    central    core    trace
    Broker Config Log    central    processing    trace
    Broker Config Log    module0    bbdo    info
    ${start}=    Get Round Current Date
    Start Broker
    Start Map
    Sleep    5s

    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.
    # pb_service pb_host pb_service_status pb_host_status
    ${expected_events}=    Create List    65563    65566    65565    65568
    ${categories}=    Create List    1
    ${output}=    Check Map Output    ${categories}    ${expected_events}    120
    Kindly Stop Broker
    Stop Map
    Should Be True    ${output}    msg=Filters badly applied in Broker

    # We should have exactly 1000 pb_service
    ${ret}=    Grep File    /tmp/map-output.log    65563
    ${ret}=    Get Line Count    ${ret}
    Should Be True    ${ret} >= 1000

    # We should have exactly 50 pb_host
    ${ret}=    Grep File    /tmp/map-output.log    65566
    ${ret}=    Get Line Count    ${ret}
    Should Be True    ${ret} >= 50

    Stop Engine

BRCTSMNS
    [Documentation]    Broker connected to map with neb and storage filters
    [Tags]    broker    map    reverse connection
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central_map
    Config Broker    module
    Config BBDO3    ${1}

    Broker Config Output Set Json
    ...    central
    ...    centreon-broker-master-map
    ...    filters
    ...    {"category": ["neb", "storage"]}
    Broker Config Log    central    bbdo    trace
    Broker Config Log    central    core    trace
    Broker Config Log    central    processing    trace
    Broker Config Log    module0    bbdo    info
    ${start}=    Get Round Current Date
    Start Broker
    Start Map
    Sleep    5s

    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.
    # pb_service pb_host pb_service_status pb_host_status pb_metric pb_status pb_index_mapping
    ${expected_events}=    Create List    65563    65566    65565    65568    196617    196618    196619
    ${categories}=    Create List    1    3
    ${output}=    Check Map Output    ${categories}    ${expected_events}    120
    Should Be True    ${output}    msg=Filters badly applied in Broker

    # We should have 1000 pb_service with maybe some BAs
    ${ret}=    Grep File    /tmp/map-output.log    65563
    ${ret}=    Get Line Count    ${ret}
    Should Be True    ${ret} >= 1000

    # We should have exactly 50 pb_host with maybe some meta hosts
    ${ret}=    Grep File    /tmp/map-output.log    65566
    ${ret}=    Get Line Count    ${ret}
    Should Be True    ${ret} >= 50

    # The output file of the map script is cleared.
    Remove File    ${/}tmp${/}map-output.log

    log to console    Second configuration with one more service per host
    # For each host, one service is added (20 -> 21)
    Config Engine    ${1}    ${50}    ${21}
    Reload Engine
    Reload Broker

    # pb_service we changed services 50 added and others moved...
    ${expected_events}=    Create List    65563
    ${categories}=    Create List    1    3
    ${output}=    Check Map Output    ${categories}    ${expected_events}    120
    Should Be True    ${output}    msg=Filters badly applied in Broker

    Kindly Stop Broker
    Stop Map
    Stop Engine
