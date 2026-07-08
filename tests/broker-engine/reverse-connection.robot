*** Settings ***
Documentation       Centreon Broker and Engine communication with or without compression

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BRGC1
    [Documentation]    Broker good reverse connection
    [Tags]    broker    map    reverse connection
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central_map
    Ctn Config Broker    module

    Log To Console    Compression set to
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    Ctn Run Reverse Bam    ${50}    ${0.2}

    Ctn Kindly Stop Broker
    Ctn Stop engine

    ${content}    Create List
    ...    New incoming connection 'centreon-broker-master-map-2'
    ...    file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log With Timeout    ${log}    ${start}    ${content}    40
    Should Be True    ${result}    Connection to map has failed.
    File Should Not Exist
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map*
    ...    There should not exist que map files.

BRCTS1
    [Documentation]    Broker reverse connection too slow
    [Tags]    broker    map    reverse connection
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central_map
    Ctn Config Broker    module

    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    Ctn Run Reverse Bam    ${150}    ${10}

    Ctn Kindly Stop Broker
    Ctn Stop engine

    ${content}    Create List
    ...    New incoming connection 'centreon-broker-master-map-2'
    ...    file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log With Timeout    ${log}    ${start}    ${content}    40
    Should Be True    ${result}    Connection to map has failed
    @{files}    List Files In Directory    ${VarRoot}/lib/centreon-broker
    Log To Console    ${files}
    File Should Not Exist
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map*
    ...    There should not exist queue map files.

BRCS1
    [Documentation]    Broker reverse connection stopped
    [Tags]    broker    map    reversed
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central_map
    Ctn Config Broker    module

    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    Ctn Kindly Stop Broker
    Ctn Stop engine

    ${content}    Create List
    ...    New incoming connection 'centreon-broker-master-map-2'
    ...    file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log With Timeout    ${log}    ${start}    ${content}    40
    Should Not Be True    ${result}    Connection to map has failed
    File Should Not Exist
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map-2
    ...    There should not exist queue map files.

BRCTSMN
    [Documentation]    Broker connected to map with neb filter
    [Tags]    broker    map    reverse connection
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central_map
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}

    Ctn Broker Config Output Set Json    central    centreon-broker-master-map    filters    {"category": ["neb"]}
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    processing    trace
    Ctn Broker Config Log    module0    bbdo    info
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Map
    Sleep    5s

    Ctn Start engine
    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    # pb_service pb_host pb_service_status pb_host_status
    ${expected_events}    Create List    65563    65566    65565    65568
    ${categories}    Create List    1
    ${output}    Ctn Check Map Output    ${categories}    ${expected_events}    120
    Ctn Kindly Stop Broker
    Ctn Stop Map
    Should Be True    ${output}    Filters badly applied in Broker

    # We should have exactly 1000 pb_service
    ${ret}    Grep File    /tmp/map-output.log    65563
    ${ret}    Get Line Count    ${ret}
    Should Be True    ${ret} >= 1000

    # We should have exactly 50 pb_host
    ${ret}    Grep File    /tmp/map-output.log    65566
    ${ret}    Get Line Count    ${ret}
    Should Be True    ${ret} >= 50

    Ctn Stop engine

BRCTSMNS
    [Documentation]    Broker connected to map with neb and storage filters
    [Tags]    broker    map    reverse connection
    Ctn Clear Metrics
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central_map
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}

    Ctn Broker Config Output Set Json
    ...    central
    ...    centreon-broker-master-map
    ...    filters
    ...    {"category": ["neb", "storage"]}
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    processing    trace
    Ctn Broker Config Log    module0    bbdo    info
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Map
    Sleep    5s

    Ctn Start engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message about check_for_external_commands() should be available.
    # pb_service pb_host pb_service_status pb_host_status pb_metric pb_status pb_index_mapping
    ${expected_events}    Create List    65563    65566    65565    65568    196617    196618    196619
    ${categories}    Create List    1    3
    ${output}    Ctn Check Map Output    ${categories}    ${expected_events}    120
    Should Be True    ${output}    Filters badly applied in Broker

    # We should have 1000 pb_service with maybe some BAs
    ${ret}    Grep File    /tmp/map-output.log    65563
    ${ret}    Get Line Count    ${ret}
    Should Be True    ${ret} >= 1000

    # We should have exactly 50 pb_host with maybe some meta hosts
    ${ret}    Grep File    /tmp/map-output.log    65566
    ${ret}    Get Line Count    ${ret}
    Should Be True    ${ret} >= 50

    # The output file of the map script is cleared.
    Remove File    ${/}tmp${/}map-output.log

    Log To Console    Second configuration with one more service per host
    # For each host, one service is added (20 -> 21)
    Ctn Config Engine    ${1}    ${50}    ${21}
    Ctn Reload Engine
    Ctn Reload Broker

    # pb_service we changed services 50 added and others moved...
    ${expected_events}    Create List    65563
    ${categories}    Create List    1    3
    ${output}    Ctn Check Map Output    ${categories}    ${expected_events}    120
    Should Be True    ${output}    Filters badly applied in Broker

    Ctn Kindly Stop Broker
    Ctn Stop Map
    Ctn Stop Engine

BRTCPWQF
    [Documentation]    Given Broker configured with a map output and a small event queue
    ...    When a map client connects with a tiny receive buffer and stops reading
    ...    Then Broker logs "write queue full => remove oldest event"
    [Tags]    broker    map    reverse connection    tcp
    [Teardown]    Ctn Brtcpwqf Teardown
    ${test_direct_grpc}    Ctn Is Using Direct Grpc
    IF    ${test_direct_grpc}
        Pass Execution    Test passes, skipping on direct grpc tests
    END
    Ctn Config Engine    ${1}    ${500}     ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central_map
    Ctn Config Broker    module
    Ctn Broker Config Remove Output    central     centreon-broker-master-rrd
    Ctn Broker Config Source Log    central    ${True}
    Ctn Broker Config Log    central    tcp    debug
    Ctn Config BBDO3    ${1}
    ${start}    Get Current Date
    Ctn Set Tcp Wmem Small
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}
    Ctn Start Slow Map

    ${random_string}    Generate Random String    2048    [LOWER]
    ${content}    Create List    write queue full => remove oldest event
    FOR    ${i}    IN RANGE   1000
        Ctn Process Service Check Result    host_1    service_1    2    ${random_string}
        Sleep    1ms
    END
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    120
    Should Be True    ${result}    Broker write queue should have been reported as full
    Ctn Kindly Stop Broker
    Ctn Stop Engine


*** Keywords ***
Ctn Brtcpwqf Teardown
    Ctn Restore Tcp Wmem
    Ctn Stop Slow Map
    Ctn Save Logs If Failed
