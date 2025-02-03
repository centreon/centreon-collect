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
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    Ctn Run Reverse Bam    ${50}    ${0.2}

    Ctn Kindly Stop Broker
    Ctn Stop Engine

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
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    Ctn Run Reverse Bam    ${150}    ${10}

    Ctn Kindly Stop Broker
    Ctn Stop Engine

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
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    Ctn Kindly Stop Broker
    Ctn Stop Engine

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
    Ctn Start Broker
    Ctn Start Map
    Sleep    5s

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}

    # pb_service (65563) pb_host (65566) pb_service_status (65565) pb_host_status (65568)
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

    Ctn Stop Engine

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

    Ctn Start Engine
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
