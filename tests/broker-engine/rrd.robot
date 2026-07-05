*** Settings ***
Documentation       Centreon Broker RRD metric deletion

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Run Keywords    Ctn Stop Processes    AND    Ctn Clear Retention
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
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

BRRDRMU1
    [Documentation]    Scenario: RRD metric rebuild through the gRPC API with unified_sql output
    ...    Given Broker is configured with a unified_sql output and BBDO3
    ...    And 3 metrics exist in the storage database
    ...    When Broker and Engine are started and connected
    ...    And 3 indexes to rebuild are collected (forcing service checks if needed)
    ...    And a rebuild request is sent for these indexes through the gRPC API
    ...    Then Central sends the metrics to rebuild and RRD starts, rebuilds and finishes them
    ...    And the rebuilt rrd metric files hold the expected average value and RW group permission
    ...    And the rebuilt rrd status files hold the expected average value
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
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Central did not send metrics to rebuild

    ${content1}    Create List    RRD: Starting to rebuild metrics
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    60
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild START

    ${content1}    Create List    RRD: Rebuilding metric
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    60
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
    [Tags]    rrd    status    bbdo3    mon-141934
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

    ${content}    Create List
    ...    RRD: ignored update non-float value '' in file '${VarRoot}/lib/centreon/status/82884.rrd'
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    1
    Should Be Equal    ${result}    ${False}    We shouldn't have any error about empty value in RRD

BRRDSTATUSRETENTION
    [Documentation]    We are working with BBDO3. This test checks status are not sent twice after Engine reload.
    [Tags]    rrd    status    bbdo3    mon-139747
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

    Ctn Schedule Forced Service Check
    ...    host_1
    ...    service_1
    ...    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
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
    ${content}    Create List
    ...    RRD: ignored update error in file '${VarRoot}/lib/centreon/status/${index}.rrd': ${VarRoot}/lib/centreon/status/${index}.rrd: illegal attempt to update using time
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

    # Restart Broker.    When central reconnects to the RRD broker, the RRD stream is
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


*** Keywords ***
Ctn Test Clean
    [Documentation]    Stop engine and broker, then save logs if the test failed.
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Save Logs If Failed
