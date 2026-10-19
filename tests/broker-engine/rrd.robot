*** Settings ***
Documentation       Centreon Broker RRD metric deletion

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Stop Engine Broker And Save Logs


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
    Ctn Broker Config Log    rrd    rrd    debug
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
    Ctn Broker Config Log    rrd    rrd    debug
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
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    central    sql    debug
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


BERRDREC1
    [Documentation]    RRD retention startup merge — metric.
    ...
    ...    Given Engine and Broker are started and at least one metric .rrd file is created
    ...    When Broker is stopped and a 2-point MetricRetentionBatch .prot file is planted
    ...    ...    for that metric (timestamps: now-24h and now-12h)
    ...    And Broker is restarted
    ...    Then the RRD stream logs a startup merge message for that metric
    ...    And the merge completes ("merging 2 buffered points")
    ...    And the .prot file is deleted
    [Tags]    rrd    retention    startup_merge    bbdo3
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Flush Log    rrd    0

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # Wait until at least one metric .rrd file is written by the RRD broker.
    # "new pb data for metric" is logged at DEBUG level.
    ${content}    Create List    RRD: new pb data for metric
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    120
    Should Be True    ${result}    No metric data written to RRD within 120 s

    # Pick one metric that has both a .rrd file and a database entry.
    ${metrics}    Ctn Get Metrics To Delete    1
    Should Not Be Empty    ${metrics}    No metrics available for retention startup-merge test
    ${metric_id}    Set Variable    ${metrics[0]}
    Log To Console    Startup merge test using metric ${metric_id}

    # Stop Broker (Engine keeps running but will disconnect).
    Ctn Kindly Stop Broker

    # Plant a 2-point MetricRetentionBatch .prot file dated 24 h and 12 h in the past.
    ${now}    Evaluate    int(time.time())    modules=time
    ${t0}    Evaluate    ${now} - 86400
    ${t1}    Evaluate    ${now} - 43200
    Ctn Create Metric Retention File    ${metric_id}    ${t0}:1.0    ${t1}:2.0

    # Restart Broker.  When central reconnects to the RRD broker, the RRD stream is
    # constructed → _startup_merge() scans the directory → finds the .prot file.
    ${start}    Get Current Date
    Ctn Start Broker

    # Step 1: startup merge must be logged at INFO level.
    ${content}    Create List    RRD: startup merge for recovered metric ${metric_id}
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    Startup merge not logged for metric ${metric_id}

    # Step 2: the merge itself must complete.
    ${content}    Create List    RRD: merging 2 buffered points for metric ${metric_id}
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    Merge did not complete for metric ${metric_id}

    # Step 3: the .prot file must be deleted after the merge.
    File Should Not Exist    ${VarRoot}/lib/centreon/metrics/${metric_id}.prot

BERRDREC2
    [Documentation]    RRD retention startup merge — status.
    ...
    ...    Given Engine and Broker are started and a forced service check has created
    ...    ...    a status .rrd file for service_1 (host_id=1, service_id=1)
    ...    When Broker is stopped and a 2-point StatusRetentionBatch .prot file is planted
    ...    ...    for that index
    ...    And Broker is restarted
    ...    Then the RRD stream logs a startup merge message for that index
    ...    And the merge completes
    ...    And the .prot file is deleted
    [Tags]    rrd    retention    startup_merge    bbdo3    status
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Flush Log    rrd    0
    Ctn Set Services Passive    ${0}    service_1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Force a passive check result so the RRD broker creates the status .rrd.
    Ctn Process Service Result Hard    host_1    service_1    0    output ok for service_1
    ${index}    Ctn Get Service Index    1    1
    Log To Console    Status startup merge test using index ${index}

    # Wait until the status .rrd file actually exists.
    ${content}    Create List    RRD: new pb status data for index ${index}
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    Status .rrd not created for index ${index} within 60 s

    # Stop Broker.
    Ctn Kindly Stop Broker

    # Plant a 2-point StatusRetentionBatch .prot file (OK then CRITICAL, dated in the past).
    ${now}    Evaluate    int(time.time())    modules=time
    ${t0}    Evaluate    ${now} - 86400
    ${t1}    Evaluate    ${now} - 43200
    Ctn Create Status Retention File    ${index}    ${t0}:0    ${t1}:2

    # Restart Broker.
    ${start}    Get Current Date
    Ctn Start Broker

    # Step 1: startup merge logged.
    ${content}    Create List    RRD: startup merge for recovered status ${index}
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    Startup merge not logged for status index ${index}

    # Step 2: merge completes.
    ${content}    Create List    RRD: merging 2 buffered points for status ${index}
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    Status merge did not complete for index ${index}

    # Step 3: .prot file deleted.
    File Should Not Exist    ${VarRoot}/lib/centreon/status/${index}.prot

BERRDREC3
    [Documentation]    RRD retention merge with metrics and statuses received during the merge.
    ...
    ...    Given 20 passive services with 5 metrics each, RRD step = 30s
    ...    And their metric and status .rrd files are created (phase A)
    ...    When check results are sent every 5s (phase B, metric value 3, state WARNING)
    ...    And during phase B the RRD broker is stopped, 7 days of retention points
    ...    ...    (metric value 1, state CRITICAL) are planted for each metric and status,
    ...    ...    then the RRD broker is restarted
    ...    Then startup merges run while the central broker sends its backlog and the new data,
    ...    ...    these data are postponed during the merge and replayed at its end
    ...    When check results are sent every 5s (phase C, metric value 4, state OK)
    ...    Then each RRD file contains the planted points, the phase B and the phase C data
    ...    ...    without any missing value
    [Tags]    rrd    retention    merge    bbdo3    MON-206113
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    # check_interval (5) * interval (6) => RRD step of 30s
    Ctn Broker Config Output Set    central    central-broker-unified-sql    interval    6
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Clear Metrics

    ${step}    Set Variable    ${30}
    ${nb_svc}    Set Variable    ${20}
    ${nb_metrics}    Set Variable    ${5}

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Phase A: create the metric and status .rrd files.
    ${a_start}    Evaluate    int(time.time())    modules=time
    FOR    ${i}    IN RANGE    2
        Ctn Process Services Check Result With Value    host_1    1    ${nb_svc}    0    2.0    ${nb_metrics}
        Sleep    5s
    END

    ${metric_ids}    Create List
    ${index_ids}    Create List
    FOR    ${svc}    IN RANGE    1    ${nb_svc + 1}
        ${metrics}    Ctn Get Metrics For Service    ${svc}    metric%
        Should Not Be Equal    ${metrics}    ${None}    No metric found for service_${svc}
        Append To List    ${metric_ids}    @{metrics}
        ${index}    Ctn Get Service Index    1    ${svc}
        Should Not Be Equal    ${index}    ${None}    No index found for service_${svc}
        Append To List    ${index_ids}    ${index}
    END
    Length Should Be    ${metric_ids}    ${{$nb_svc * $nb_metrics}}

    FOR    ${metric_id}    IN    @{metric_ids}
        Wait Until Created    ${VarRoot}/lib/centreon/metrics/${metric_id}.rrd    timeout=60s
    END
    FOR    ${index}    IN    @{index_ids}
        Wait Until Created    ${VarRoot}/lib/centreon/status/${index}.rrd    timeout=60s
    END
    # RRD files are created with a step of 1s, the interval is the RRA resolution.
    ${result}    Ctn Check Rrd Info    ${metric_ids[0]}    rra[0].pdp_per_row    ${step}
    Should Be True    ${result}    The RRD resolution should be ${step}s

    # Planted retention points end two steps before phase A.
    ${p_end}    Evaluate    (${a_start} // ${step} - 2) * ${step}
    ${p_start}    Evaluate    ${p_end} - 7 * 86400

    # Phase B: results sent every 5s, the RRD broker is restarted in the middle.
    ${b_start}    Evaluate    int(time.time())    modules=time
    FOR    ${i}    IN RANGE    36
        Ctn Process Services Check Result With Value    host_1    1    ${nb_svc}    1    3.0    ${nb_metrics}
        IF    ${i} == 2
            Ctn Stop Rrd Broker
            Ctn Create Retention Files Range
            ...    ${metric_ids}    ${index_ids}    ${p_start}    ${p_end}    ${step}    1.0    2
        ELSE IF    ${i} == 8
            ${restart}    Get Current Date
            Ctn Start Rrd Broker
        END
        Sleep    5s
    END
    ${b_end}    Evaluate    int(time.time())    modules=time

    # Phase C: back to OK.
    ${c_start}    Evaluate    int(time.time())    modules=time
    FOR    ${i}    IN RANGE    24
        Ctn Process Services Check Result With Value    host_1    1    ${nb_svc}    0    4.0    ${nb_metrics}
        Sleep    5s
    END
    ${c_end}    Evaluate    int(time.time())    modules=time

    # Merges ran and data were received while they were running.
    ${content}    Create List    RRD: startup merge for recovered metric    RRD: startup merge for recovered status
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${restart}    ${content}    10
    Should Be True    ${result}    No startup merge after the RRD broker restart
    ${result}    ${line}    Ctn Find Regex In Log With Timeout
    ...    ${rrdLog}    ${restart}    ${{ [r"RRD: merge of metric [0-9]+ running, data at [0-9]+ postponed"] }}    10
    Should Be True    ${result}    No metric received during a merge, the test does not check anything
    ${result}    ${line}    Ctn Find Regex In Log With Timeout
    ...    ${rrdLog}    ${restart}    ${{ [r"RRD: merge of status [0-9]+ running, data at [0-9]+ postponed"] }}    10
    Should Be True    ${result}    No status received during a merge, the test does not check anything

    # No metric must be missing.
    FOR    ${metric_id}    IN    @{metric_ids}
        ${result}    Ctn Check Rrd Constant Value    metrics    ${metric_id}    ${p_start}    ${p_end}    1.0
        Should Be True    ${result}    Retention points missing in metric ${metric_id}
        ${result}    Ctn Check Rrd Constant Value    metrics    ${metric_id}    ${b_start}    ${b_end}    3.0
        Should Be True    ${result}    Phase B points missing in metric ${metric_id}
        ${result}    Ctn Check Rrd Constant Value    metrics    ${metric_id}    ${c_start}    ${c_end}    4.0
        Should Be True    ${result}    Phase C points missing in metric ${metric_id}
    END
    # Status RRD values: OK => 100, WARNING => 75, CRITICAL => 0
    FOR    ${index}    IN    @{index_ids}
        ${result}    Ctn Check Rrd Constant Value    status    ${index}    ${p_start}    ${p_end}    0
        Should Be True    ${result}    Retention points missing in status ${index}
        ${result}    Ctn Check Rrd Constant Value    status    ${index}    ${b_start}    ${b_end}    75
        Should Be True    ${result}    Phase B points missing in status ${index}
        ${result}    Ctn Check Rrd Constant Value    status    ${index}    ${c_start}    ${c_end}    100
        Should Be True    ${result}    Phase C points missing in status ${index}
    END


*** Keywords ***
Ctn Stop Rrd Broker
    [Documentation]    Stop only the RRD broker, the central broker keeps its events for it.
    Send Signal To Process    SIGTERM    b2
    ${result}    Wait For Process    b2    timeout=60s
    Should Not Be Equal    ${result}    ${None}    RRD broker not stopped

Ctn Start Rrd Broker
    [Documentation]    Start only the RRD broker with its current configuration.
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2
    ...    stdout=${BROKER_LOG}/central-rrd-stdout.log    stderr=${BROKER_LOG}/central-rrd-stderr.log

Ctn Test Clean
    [Documentation]    Stop engine and broker, then save logs if the test failed.
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Save Logs If Failed
