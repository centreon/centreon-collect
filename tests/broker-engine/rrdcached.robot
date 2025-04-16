*** Settings ***
Documentation       Centreon Broker RRD metric rebuild/deletion with rrdcached

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite With rrdcached
Suite Teardown      Ctn Clean After Suite With rrdcached
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BRRDCDDM1
    [Documentation]    RRD metrics deletion from metric ids with rrdcached.
    [Tags]    rrd    metric    deletion    rrdcached
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Add Path To Rrd Output    rrd    ${BROKER_LIB}/rrdcached.sock
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We choose 3 metrics to remove.
    ${metrics}    Ctn Get Metrics To Delete    3
    Log To Console    Metrics to delete ${metrics}

    ${empty}    Create List
    Ctn Remove Graphs    51001    ${empty}    ${metrics}
    ${metrics_str}    Catenate    SEPARATOR=,    @{metrics}
    ${content}    Create List    metrics ${metrics_str} erased from database

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No log message telling about metrics ${metrics_str} deletion.
    FOR    ${m}    IN    @{metrics}
        Log To Console    Waiting for ${VarRoot}/lib/centreon/metrics/${m}.rrd to be deleted
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDCDDID1
    [Documentation]    RRD metrics deletion from index ids with rrdcached.
    [Tags]    rrd    metric    deletion    rrdcached
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Add Path To Rrd Output    rrd    ${BROKER_LIB}/rrdcached.sock
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    rrd    core    error
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Sleep    1s
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Ctn Get Indexes To Delete    2
    ${metrics}    Ctn Get Metrics Matching Indexes    ${indexes}
    Log To Console    indexes ${indexes} to delete with their metrics

    ${empty}    Create List
    Ctn Remove Graphs    51001    ${indexes}    ${empty}
    ${indexes_str}    Catenate    SEPARATOR=,    @{indexes}
    ${content}    Create List    indexes ${indexes_str} erased from database

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No log message telling about indexes ${indexes_str} deletion.
    FOR    ${i}    IN    @{indexes}
        Log To Console    Wait for ${VarRoot}/lib/centreon/status/${i}.rrd to be deleted
        Wait Until Removed    ${VarRoot}/lib/centreon/status/${i}.rrd    20s
    END
    FOR    ${m}    IN    @{metrics}
        Log To Console    Wait for ${VarRoot}/lib/centreon/metrics/${m}.rrd to be deleted
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDCDDMID1
    [Documentation]    RRD deletion of non existing metrics and indexes with rrdcached
    [Tags]    rrd    metric    deletion    rrdcached
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Add Path To Rrd Output    rrd    ${BROKER_LIB}/rrdcached.sock
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
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

BRRDCDDMU1
    [Documentation]    RRD metric deletion on table metric with unified sql output with rrdcached
    [Tags]    rrd    metric    deletion unified_sql    rrdcached
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Add Path To Rrd Output    rrd    ${BROKER_LIB}/rrdcached.sock
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We choose 3 metrics to remove.
    ${metrics}    Ctn Get Metrics To Delete    3
    Log To Console    metrics to delete ${metrics}

    ${empty}    Create List
    Ctn Remove Graphs    51001    ${empty}    ${metrics}
    ${metrics_str}    Catenate    SEPARATOR=,    @{metrics}
    ${content}    Create List    metrics ${metrics_str} erased from database

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    50
    Should Be True    ${result}    No log message telling about metrics ${metrics_str} deletion.
    FOR    ${m}    IN    @{metrics}
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDCDDIDU1
    [Documentation]    RRD metrics deletion from index ids with unified sql output with rrdcached.
    [Tags]    rrd    metric    deletion    unified_sql    rrdcached
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Add Path To Rrd Output    rrd    ${BROKER_LIB}/rrdcached.sock
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${indexes}    Ctn Get Indexes To Delete    2
    ${metrics}    Ctn Get Metrics Matching Indexes    ${indexes}
    Log To Console    indexes ${indexes} to delete with their metrics

    ${empty}    Create List
    Ctn Remove Graphs    51001    ${indexes}    ${empty}
    ${indexes_str}    Catenate    SEPARATOR=,    @{indexes}
    ${content}    Create List    indexes ${indexes_str} erased from database

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No log message telling about indexes ${indexes_str} deletion.
    FOR    ${i}    IN    @{indexes}
        Wait Until Removed    ${VarRoot}/lib/centreon/status/${i}.rrd    20s
    END
    FOR    ${m}    IN    @{metrics}
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDCDDMIDU1
    [Documentation]    RRD deletion of non existing metrics and indexes with rrdcached
    [Tags]    rrd    metric    deletion    unified_sql    rrdcached
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Add Path To Rrd Output    rrd    ${BROKER_LIB}/rrdcached.sock
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
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

BRRDCDRB1
    [Documentation]    RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
    [Tags]    rrd    metric    rebuild    grpc    rrdcached
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Add Path To Rrd Output    rrd    ${BROKER_LIB}/rrdcached.sock
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
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
        ...    Data before RRD rebuild contain index_id % 3. The expected average is 100 if modulo==0, 75 if modulo==1, 50 if modulo==2 .
    END

BRRDCDRBU1
    [Documentation]    RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
    [Tags]    rrd    metric    rebuild    unified_sql    grpc
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Add Path To Rrd Output    rrd    ${BROKER_LIB}/rrdcached.sock
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
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
        ...    Data before RRD rebuild contain index_id % 3. The expected average is 100 if modulo==0, 75 if modulo==1, 50 if modulo==2 .
    END
