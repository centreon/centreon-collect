*** Settings ***
Documentation       Centreon Broker RRD metric deletion

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BRRDDM1
    [Documentation]    RRD metrics deletion from metric ids.
    [Tags]    rrd    metric    deletion
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We choose 3 metrics to remove.
    ${metrics}    Ctn Get Metrics To Delete    3
    Log To Console    Metrics to delete ${metrics}

    ${empty}    Create List
    Ctn Remove Graphs    51001    ${empty}    ${metrics}
    ${metrics_str}    Catenate    SEPARATOR=,    @{metrics}
    ${content}    Create List    metrics .* erased from database

    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
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
    Ctn Config Engine    ${1}
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
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${content}    Create List    RRD: new pb data for metric

    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    120
    Should Be True    ${result}    No protobuf metric sent to cbd RRD for 60s.

BRRDDID1
    [Documentation]    RRD metrics deletion from index ids.
    [Tags]    rrd    metric    deletion
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Sleep    1s
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Ctn Get Indexes To Delete    2
    ${metrics}    Ctn Get Metrics Matching Indexes    ${indexes}
    Log To Console    indexes ${indexes} to delete with their metrics

    ${empty}    Create List
    Ctn Remove Graphs    51001    ${indexes}    ${empty}
    ${indexes_str}    Catenate    SEPARATOR=,    @{indexes}
    ${content}    Create List    indexes .* erased from database

    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
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
    Ctn Config Engine    ${1}
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
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Ctn Get Not Existing Indexes    2
    ${metrics}    Ctn Get Not Existing Metrics    2
    Log To Console    indexes ${indexes} and metrics ${metrics} to delete but they do not exist.

    Ctn Remove Graphs    51001    ${indexes}    ${metrics}
    ${content}    Create List    do not appear in the storage database
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    A message telling indexes nor metrics appear in the storage database should appear.

BRRDDMU1
    [Documentation]    RRD metric deletion on table metric with unified sql output
    [Tags]    rrd    metric    deletion unified_sql
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We choose 3 metrics to remove.
    ${metrics}    Ctn Get Metrics To Delete    3
    Log To Console    metrics to delete ${metrics}

    ${empty}    Create List
    Ctn Remove Graphs    51001    ${empty}    ${metrics}
    ${metrics_str}    Catenate    SEPARATOR=,    @{metrics}
    ${content}    Create List    metrics .* erased from database

    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    50
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
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Ctn Get Indexes To Delete    2
    ${metrics}    Ctn Get Metrics Matching Indexes    ${indexes}
    Log To Console    indexes ${indexes} to delete with their metrics

    ${empty}    Create List
    Ctn Remove Graphs    51001    ${indexes}    ${empty}
    ${indexes_str}    Catenate    SEPARATOR=,    @{indexes}
    ${content}    Create List    indexes .* erased from database

    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
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
    Ctn Config Engine    ${1}
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
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Ctn Get Not Existing Indexes    2
    ${metrics}    Ctn Get Not Existing Metrics    2
    Log To Console    indexes ${indexes} and metrics ${metrics} to delete but they do not exist.

    Ctn Remove Graphs    51001    ${indexes}    ${metrics}
    ${content}    Create List    do not appear in the storage database
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    A message telling indexes nor metrics appear in the storage database should appear.

BRRDRM1
    [Documentation]    RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
    [Tags]    rrd    metric    rebuild    grpc
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    rrd    perfdata    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    FOR    ${idx}    IN RANGE    60
        ${index}    Ctn Get Indexes To Rebuild    3
	IF    len(${index}) == 3
            BREAK
	ELSE
	    # If not available, we force checks to have them.
            Ctn Schedule Forced Service Check    host_1    service_1
            Ctn Schedule Forced Service Check    host_1    service_2
            Ctn Schedule Forced Service Check    host_1    service_3
        END
	Sleep    1s
    END
    Ctn Rebuild Rrd Graphs    51001    ${index}    1
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Ctn Get Metrics Matching Indexes    ${index}
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
        ${result}    Ctn Compare Rrd Average Value    ${m}    ${value}
        Should Be True
        ...    ${result}
        ...    Data before RRD rebuild for metric ${m} contained alternatively the metric ID and 0. The expected average is metric_id / 2 = ${value}.
    END

    FOR    ${index_id}    IN    @{index}
        ${value}    Evaluate    ${index_id} %3
        ${result}    Ctn Compare Rrd Status Average Value    ${index_id}    ${value}
        Should Be True
        ...    ${result}
        ...    index_id=${index_id} Data before RRD rebuild contain index_id % 3. The expected average is 100 if modulo==0, 75 if modulo==1, 0 if modulo==2 .
    END

BRRDRMU1
    [Documentation]    RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
    [Tags]    rrd    metric    rebuild    unified_sql    grpc
    Ctn Config Engine    ${1}
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
    Ctn Create Metrics    3

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    FOR    ${idx}    IN RANGE    60
        ${index}    Ctn Get Indexes To Rebuild    3
	IF    len(${index}) == 3
            BREAK
	ELSE
	    # If not available, we force checks to have them.
            Ctn Schedule Forced Service Check    host_1    service_1
            Ctn Schedule Forced Service Check    host_1    service_2
            Ctn Schedule Forced Service Check    host_1    service_3
        END
	Sleep    1s
    END
    Ctn Rebuild Rrd Graphs    51001    ${index}    1
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Ctn Get Metrics Matching Indexes    ${index}
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
        ${result}    Ctn Compare Rrd Average Value    ${m}    ${value}
        Should Be True
        ...    ${result}
        ...    Data before RRD rebuild for metric ${m} contained alternatively the metric ID and 0. The expected average is metric_id / 2 = ${value}.
        # 48 = 60(octal)
        ${result}    Ctn Has File Permissions    ${VarRoot}/lib/centreon/metrics/${m}.rrd    48
        Should Be True    ${result}    ${VarRoot}/lib/centreon/metrics/${m}.rrd has not RW group permission
    END

    FOR    ${index_id}    IN    @{index}
        ${value}    Evaluate    ${index_id} %3
        ${result}    Ctn Compare Rrd Status Average Value    ${index_id}    ${value}
        Should Be True
        ...    ${result}
        ...    Data before RRD rebuild contain index_id % 3. The expected average is 100 if modulo==0, 75 if modulo==1, 0 if modulo==2 .
    END

RRD1
    [Documentation]    RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
    [Tags]    rrd    metric    rebuild    unified_sql    grpc
    Ctn Config Engine    ${1}
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
    Ctn Create Metrics    3

    ${start}    Ctn Get Round Current Date
    Run Keywords    Ctn Start Broker    AND    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    ${index}    Ctn Get Not Existing Indexes    3
    Ctn Rebuild Rrd Graphs    51001    ${index}    1
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Ctn Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics to rebuild: ${metrics}
    ${content}    Create List    Metrics rebuild: metrics don't exist
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Central did not send metrics to rebuild

    ${content1}    Create List    mysql_connection: You have an error in your SQL syntax
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Not Be True    ${result}    Database did not receive command to rebuild metrics

BRRDSTATUS
    [Documentation]    We are working with BBDO3. This test checks status are correctly handled independently from their value.
    [Tags]    rrd    status    bbdo3    MON-141934
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Set Services Passive    ${0}    service_1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Process Service Result Hard    host_1    service_1    2    output critical for service_1
    ${index}    Ctn Get Service Index    1    1
    log to console    Service 1:1 has index ${index}
    ${content}    Create List    RRD: new pb status data for index ${index} (state 2)
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    host_1:service_1 is not CRITICAL as expected

    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard    host_1    service_1    1    output warning for service_1
    ${content}    Create List    RRD: new pb status data for index ${index} (state 1)
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    host_1:service_1 is not WARNING as expected

    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard    host_1    service_1    0    output ok for service_1
    ${content}    Create List    RRD: new pb status data for index ${index} (state 0)
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    host_1:service_1 is not OK as expected

    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard    host_1    service_1    3    output UNKNOWN for service_1
    ${content}    Create List    RRD: new pb status data for index ${index} (state 3)
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    host_1:service_1 is not UNKNOWN as expected

    ${content}    Create List    RRD: ignored update non-float value '' in file '${VarRoot}/lib/centreon/status/82884.rrd'
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    1
    Should Be Equal    ${result}    ${False}    We shouldn't have any error about empty value in RRD


BRRDSTATUSRETENTION
    [Documentation]    We are working with BBDO3. This test checks status are not sent twice after Engine reload.
    [Tags]    rrd    status    bbdo3    MON-139747
    Ctn Config Engine    ${1}
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Schedule Forced Service Check    host_1    service_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
    Log To Console    Engine works during 20s
    Sleep    20s

    Log To Console    We modify the check_interval of the service service_1
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1

    ${start}    Ctn Get Round Current Date
    Log To Console    Reloading Engine and waiting for 20s again
    Ctn Reload Engine
    Sleep    20s

    Log To Console    Find in logs if there is an error in rrd.
    ${index}    Ctn Get Service Index    1    1
    ${content}    Create List    RRD: ignored update error in file '${VarRoot}/lib/centreon/status/${index}.rrd': ${VarRoot}/lib/centreon/status/${index}.rrd: illegal attempt to update using time
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    1
    Should Be Equal
    ...    ${result}    ${False}
    ...    No message about an illegal attempt to update the rrd files should appear
    Log To Console    Test finished


*** Keywords ***
Ctn Test Clean
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Save Logs If Failed
