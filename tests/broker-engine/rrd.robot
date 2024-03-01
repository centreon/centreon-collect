*** Settings ***
Documentation       Centreon Broker RRD metric deletion

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Stop Engine Broker And Save Logs


*** Test Cases ***
BRRDDM1
    [Documentation]    RRD metrics deletion from metric ids.
    [Tags]    rrd    metric    deletion
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We choose 3 metrics to remove.
    ${metrics}    Get Metrics To Delete    3
    Log To Console    Metrics to delete ${metrics}

    ${empty}    Create List
    Remove Graphs    51001    ${empty}    ${metrics}
    ${metrics_str}    Catenate    SEPARATOR=,    @{metrics}
    ${content}    Create List    metrics .* erased from database

    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    No log message telling about some metrics deletion.

    # We should have one line, but stored in an array.
    FOR    ${l}    IN    @{result[1]}
        # We check all the metrics are in this line
        FOR    ${m}    IN    @{metrics}
            Should Be True    "${m}" in """${l}"""    ${m} is not in the line ${l}
        END
    END
    FOR    ${m}    IN    @{metrics}
        Log To Console    Waiting for ${VarRoot}/lib/centreon/metrics/${m}.rrd to be deleted
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDWM1
    [Documentation]    We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
    [Tags]    rrd    metric    bbdo3    unified_sql
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${content}    Create List    RRD: new pb data for metric

    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    120
    Should Be True    ${result}    No protobuf metric sent to cbd RRD for 60s.

BRRDDID1
    [Documentation]    RRD metrics deletion from index ids.
    [Tags]    rrd    metric    deletion
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Create Metrics    3

    ${start}    Get Current Date
    Sleep    1s
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Get Indexes To Delete    2
    ${metrics}    Get Metrics Matching Indexes    ${indexes}
    Log To Console    indexes ${indexes} to delete with their metrics

    ${empty}    Create List
    Remove Graphs    51001    ${indexes}    ${empty}
    ${indexes_str}    Catenate    SEPARATOR=,    @{indexes}
    ${content}    Create List    indexes .* erased from database

    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    No log message telling about indexes ${indexes_str} deletion.
    # We should have one line, but stored in an array.
    FOR    ${l}    IN    @{result[1]}
        # We check all the indexes are in this line
        FOR    ${ii}    IN    @{indexes}
            Should Be True    "${ii}" in """${l}"""    ${ii} is not in the line ${l}
        END
    END

    FOR    ${i}    IN    @{indexes}
        Log To Console    Wait for ${VarRoot}/lib/centreon/status/${i}.rrd to be deleted
        Wait Until Removed    ${VarRoot}/lib/centreon/status/${i}.rrd    20s
    END
    FOR    ${m}    IN    @{metrics}
        Log To Console    Wait for ${VarRoot}/lib/centreon/metrics/${m}.rrd to be deleted
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDDMID1
    [Documentation]    RRD deletion of non existing metrics and indexes
    [Tags]    rrd    metric    deletion
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Ctn Get Not Existing Indexes    2
    ${metrics}    Ctn Get Not Existing Metrics    2
    Log To Console    indexes ${indexes} and metrics ${metrics} to delete but they do not exist.

    Remove Graphs    51001    ${indexes}    ${metrics}
    ${content}    Create List    do not appear in the storage database
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    A message telling indexes nor metrics appear in the storage database should appear.

BRRDDMU1
    [Documentation]    RRD metric deletion on table metric with unified sql output
    [Tags]    rrd    metric    deletion unified_sql
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We choose 3 metrics to remove.
    ${metrics}    Get Metrics To Delete    3
    Log To Console    metrics to delete ${metrics}

    ${empty}    Create List
    Remove Graphs    51001    ${empty}    ${metrics}
    ${metrics_str}    Catenate    SEPARATOR=,    @{metrics}
    ${content}    Create List    metrics .* erased from database

    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    50
    Should Be True    ${result[0]}    No log message telling about metrics ${metrics_str} deletion.

    # We should have one line, but stored in an array.
    FOR    ${l}    IN    @{result[1]}
        # We check all the metrics are in this line
        FOR    ${m}    IN    @{metrics}
            Should Be True    "${m}" in """${l}"""    ${m} is not in the line ${l}
        END
    END
    FOR    ${m}    IN    @{metrics}
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDDIDU1
    [Documentation]    RRD metrics deletion from index ids with unified sql output.
    [Tags]    rrd    metric    deletion    unified_sql
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Get Indexes To Delete    2
    ${metrics}    Get Metrics Matching Indexes    ${indexes}
    Log To Console    indexes ${indexes} to delete with their metrics

    ${empty}    Create List
    Remove Graphs    51001    ${indexes}    ${empty}
    ${indexes_str}    Catenate    SEPARATOR=,    @{indexes}
    ${content}    Create List    indexes .* erased from database

    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    No log message telling about indexes ${indexes_str} deletion.
    # We should have one line, but stored in an array.
    FOR    ${l}    IN    @{result[1]}
        # We check all the indexes are in this line
        FOR    ${ii}    IN    @{indexes}
            Should Be True    "${ii}" in """${l}"""    ${ii} is not in the line ${l}
        END
    END

    FOR    ${i}    IN    @{indexes}
        Wait Until Removed    ${VarRoot}/lib/centreon/status/${i}.rrd    20s
    END
    FOR    ${m}    IN    @{metrics}
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDDMIDU1
    [Documentation]    RRD deletion of non existing metrics and indexes
    [Tags]    rrd    metric    deletion    unified_sql
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Ctn Get Not Existing Indexes    2
    ${metrics}    Ctn Get Not Existing Metrics    2
    Log To Console    indexes ${indexes} and metrics ${metrics} to delete but they do not exist.

    Remove Graphs    51001    ${indexes}    ${metrics}
    ${content}    Create List    do not appear in the storage database
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    A message telling indexes nor metrics appear in the storage database should appear.

BRRDRM1
    [Documentation]    RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
    [Tags]    rrd    metric    rebuild    grpc
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    rrd    perfdata    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    ${index}    Get Indexes To Rebuild    3
    Rebuild Rrd Graphs    51001    ${index}    1
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics to rebuild: ${metrics}
    ${content}    Create List    Metric rebuild: metric    is sent to rebuild
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Central did not send metrics to rebuild

    ${content1}    Create List    RRD: Starting to rebuild metrics
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild START

    ${content1}    Create List    RRD: Rebuilding metric
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild DATA

    ${content1}    Create List    RRD: Finishing to rebuild metrics
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    500
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild END
    FOR    ${m}    IN    @{metrics}
        ${value}    Evaluate    ${m} / 2
        ${result}    Compare RRD Average Value    ${m}    ${value}
        Should Be True
        ...    ${result}
        ...    Data before RRD rebuild contain alternatively the metric ID and 0. The expected average is metric_id / 2.
    END

    FOR    ${index_id}    IN    @{index}
        ${value}    Evaluate    ${index_id} %3
        ${result}    Compare RRD Status Average Value    ${index_id}    ${value}
        Should Be True
        ...    ${result}
        ...    index_id=${index_id} Data before RRD rebuild contain index_id % 3. The expected average is 100 if modulo==0, 75 if modulo==1, 0 if modulo==2 .
    END

BRRDRMU1
    [Documentation]    RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
    [Tags]    rrd    metric    rebuild    unified_sql    grpc
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    ${index}    Get Indexes To Rebuild    3
    Rebuild Rrd Graphs    51001    ${index}    1
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics to rebuild: ${metrics}
    ${content}    Create List    Metric rebuild: metric    is sent to rebuild
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Central did not send metrics to rebuild

    ${content1}    Create List    RRD: Starting to rebuild metrics
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild START

    ${content1}    Create List    RRD: Rebuilding metric
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild DATA

    ${content1}    Create List    RRD: Finishing to rebuild metrics
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    500
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild END
    FOR    ${m}    IN    @{metrics}
        ${value}    Evaluate    ${m} / 2
        ${result}    Compare RRD Average Value    ${m}    ${value}
        Should Be True
        ...    ${result}
        ...    Data before RRD rebuild contain alternatively the metric ID and 0. The expected average is metric_id / 2.
        # 48 = 60(octal)
        ${result}    Has File Permissions    ${VarRoot}/lib/centreon/metrics/${m}.rrd    48
        Should Be True    ${result}    ${VarRoot}/lib/centreon/metrics/${m}.rrd has not RW group permission
    END

    FOR    ${index_id}    IN    @{index}
        ${value}    Evaluate    ${index_id} %3
        ${result}    Compare RRD Status Average Value    ${index_id}    ${value}
        Should Be True
        ...    ${result}
        ...    Data before RRD rebuild contain index_id % 3. The expected average is 100 if modulo==0, 75 if modulo==1, 0 if modulo==2 .
    END

RRD1
    [Documentation]    RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
    [Tags]    rrd    metric    rebuild    unified_sql    grpc
    Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Round Current Date
    Run Keywords    Start Broker    AND    Ctn Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    ${index}    Ctn Get Not Existing Indexes    3
    Rebuild Rrd Graphs    51001    ${index}    1
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics to rebuild: ${metrics}
    ${content}    Create List    Metrics rebuild: metrics don't exist
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Central did not send metrics to rebuild

    ${content1}    Create List    mysql_connection: You have an error in your SQL syntax
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Not Be True    ${result}    Database did not receive command to rebuild metrics


*** Keywords ***
Test Clean
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Save Logs If Failed
