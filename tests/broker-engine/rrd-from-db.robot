*** Settings ***
Documentation       Centreon Broker RRD metric deletion from the legacy query made by the php.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BRRDDMDB1
    [Documentation]    RRD metrics deletion from metric ids with a query in centreon_storage.
    [Tags]    rrd    metric    deletion    unified_sql    mysql
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    module
    Ctn Broker Config Log    central    grpc    error
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3
    ${start}    Get Current Date    exclude_millis=True
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We choose 3 metrics to remove.
    ${metrics}    Ctn Get Metrics To Delete    3
    Log To Console    Metrics to delete ${metrics}

    ${empty}    Create List
    Ctn Remove Graphs From Db    ${empty}    ${metrics}
    Ctn Reload Broker
    ${metrics_str}    Catenate    SEPARATOR=,    @{metrics}
    ${content}    Create List    metrics ${metrics_str} erased from database

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No log message telling about metrics ${metrics_str} deletion.
    FOR    ${m}    IN    @{metrics}
        Log To Console    Waiting for ${VarRoot}/lib/centreon/metrics/${m}.rrd to be deleted
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDDIDDB1
    [Documentation]    RRD metrics deletion from index ids with a query in centreon_storage.
    [Tags]    rrd    metric    deletion    unified_sql
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    rrd    rrd    debug
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Create Metrics    3

    ${start}    Get Current Date
    Sleep    1s
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    Log To Console    STEP1
    ${indexes}    Ctn Get Indexes To Delete    2
    Log To Console    STEP2
    ${metrics}    Ctn Get Metrics Matching Indexes    ${indexes}
    Log To Console    STEP3
    Log To Console    indexes ${indexes} to delete with their metrics

    ${empty}    Create List
    Ctn Remove Graphs From Db    ${indexes}    ${empty}
    Ctn Reload Broker
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

BRRDRBDB1
    [Documentation]    RRD metric rebuild with a query in centreon_storage and unified sql
    [Tags]    rrd    metric    rebuild    unified_sql
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
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

    # We need 3 indexes to rebuild
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
    Ctn Rebuild Rrd Graphs From Db    ${index}
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Ctn Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics to rebuild: ${metrics}
    Ctn Reload Broker

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

BRRDRBUDB1
    [Documentation]    RRD metric rebuild with a query in centreon_storage and unified sql
    [Tags]    rrd    metric    rebuild    unified_sql    grpc
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    module
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Config BBDO3    1
    Ctn Create Metrics    3

    ${start}    Get Current Date    exclude_millis=True
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We need 3 indexes to rebuild
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
    Ctn Rebuild Rrd Graphs From Db    ${index}
    Ctn Reload Broker
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Ctn Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics to rebuild: ${metrics}

    ${content1}    Create List    RRD: Starting to rebuild metrics
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    30
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild START

    ${content1}    Create List    RRD: Rebuilding metric
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    30
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

BRRDUPLICATE
    [Documentation]    RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
    [Tags]    rrd    metric    rebuild    unified_sql
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
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

    # We need 3 indexes to rebuild
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
    ${metrics}    Ctn Get Metrics Matching Indexes    ${index}
    ${duplicates}    Ctn Add Duplicate Metrics    ${metrics}
    Ctn Rebuild Rrd Graphs From Db    ${index}
    Log To Console    Indexes to rebuild: ${index}
    Log To Console    Metrics to rebuild: ${metrics}
    Ctn Reload Broker

    ${content1}    Create List    RRD: Starting to rebuild metrics
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild START

    ${content1}    Create List    RRD: Rebuilding metric
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild DATA

    ${content1}    Create List    RRD: Finishing to rebuild metrics
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    500
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild END

    ${result}    Ctn Check For Nan Metric    ${duplicates}
    Should Be True    ${result}    at least one metric contains NaN value
