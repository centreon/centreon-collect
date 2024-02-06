*** Settings ***
Documentation       Centreon Broker RRD metric deletion from the legacy query made by the php.

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Stop Engine Broker And Save Logs


*** Test Cases ***
BRRDDMDB1
    [Documentation]    RRD metrics deletion from metric ids with a query in centreon_storage.
    [Tags]    rrd    metric    deletion    unified_sql    mysql
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    module
    Broker Config Log    central    grpc    error
    Broker Config Log    central    sql    info
    Broker Config Log    central    core    error
    Broker Config Log    rrd    rrd    debug
    Broker Config Log    rrd    core    error
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Create Metrics    3
    ${start}    Get Current Date    exclude_millis=True
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We choose 3 metrics to remove.
    ${metrics}    Get Metrics To Delete    3
    Log To Console    Metrics to delete ${metrics}

    ${empty}    Create List
    Remove Graphs From Db    ${empty}    ${metrics}
    Reload Broker
    ${metrics_str}    Catenate    SEPARATOR=,    @{metrics}
    ${content}    Create List    metrics ${metrics_str} erased from database

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No log message telling about metrics ${metrics_str} deletion.
    FOR    ${m}    IN    @{metrics}
        Log To Console    Waiting for ${VarRoot}/lib/centreon/metrics/${m}.rrd to be deleted
        Wait Until Removed    ${VarRoot}/lib/centreon/metrics/${m}.rrd    20s
    END

BRRDDIDDB1
    [Documentation]    RRD metrics deletion from index ids with a query in centreon_storage.
    [Tags]    rrd    metric    deletion    unified_sql
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    module
    Broker Config Log    central    sql    info
    Broker Config Log    rrd    rrd    debug
    Broker Config Log    rrd    core    error
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Current Date
    Sleep    1s
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    Log To Console    STEP1
    ${indexes}    Get Indexes To Delete    2
    Log To Console    STEP2
    ${metrics}    Get Metrics Matching Indexes    ${indexes}
    Log To Console    STEP3
    Log To Console    indexes ${indexes} to delete with their metrics

    ${empty}    Create List
    Remove Graphs From Db    ${indexes}    ${empty}
    Reload Broker
    ${indexes_str}    Catenate    SEPARATOR=,    @{indexes}
    ${content}    Create List    indexes ${indexes_str} erased from database

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
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
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    module
    Broker Config Log    rrd    rrd    trace
    Broker Config Log    central    sql    trace
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    ${index}    Get Indexes To Rebuild    3
    Rebuild Rrd Graphs From Db    ${index}
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics to rebuild: ${metrics}
    Reload Broker

    ${content1}    Create List    RRD: Starting to rebuild metrics
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild START

    ${content1}    Create List    RRD: Rebuilding metric
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild DATA

    ${content1}    Create List    RRD: Finishing to rebuild metrics
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    500
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild END
    FOR    ${m}    IN    @{metrics}
        ${value}    Evaluate    ${m} / 2
        ${result}    Compare RRD Average Value    ${m}    ${value}
        Should Be True
        ...    ${result}
        ...    Data before RRD rebuild contain alternatively the metric ID and 0. The expected average is metric_id / 2.
    END

BRRDRBUDB1
    [Documentation]    RRD metric rebuild with a query in centreon_storage and unified sql
    [Tags]    rrd    metric    rebuild    unified_sql    grpc
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    module
    Broker Config Log    rrd    rrd    trace
    Broker Config Log    central    sql    trace
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Config BBDO3    1
    Create Metrics    3

    ${start}    Get Current Date    exclude_millis=True
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    ${index}    Get Indexes To Rebuild    3
    Rebuild Rrd Graphs From Db    ${index}
    Reload Broker
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Get Metrics Matching Indexes    ${index}

    ${content1}    Create List    RRD: Starting to rebuild metrics
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    30
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild START

    ${content1}    Create List    RRD: Rebuilding metric
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    30
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild DATA

    ${content1}    Create List    RRD: Finishing to rebuild metrics
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    500
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild END
    FOR    ${m}    IN    @{metrics}
        ${value}    Evaluate    ${m} / 2
        ${result}    Compare RRD Average Value    ${m}    ${value}
        Should Be True
        ...    ${result}
        ...    Data before RRD rebuild contain alternatively the metric ID and 0. The expected average is metric_id / 2.
    END

BRRDUPLICATE
    [Documentation]    RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
    [Tags]    rrd    metric    rebuild    unified_sql
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    module
    Broker Config Log    rrd    rrd    trace
    Broker Config Log    central    sql    trace
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Create Metrics    3

    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    # We get 3 indexes to rebuild
    ${index}    Get Indexes To Rebuild    3    2
    ${duplicates}    Add Duplicate Metrics
    Rebuild Rrd Graphs From Db    ${index}
    Log To Console    Indexes to rebuild: ${index}
    ${metrics}    Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics to rebuild: ${metrics}
    Reload Broker

    ${content1}    Create List    RRD: Starting to rebuild metrics
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild START

    ${content1}    Create List    RRD: Rebuilding metric
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    45
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild DATA

    ${content1}    Create List    RRD: Finishing to rebuild metrics
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content1}    500
    Should Be True    ${result}    RRD cbd did not receive metrics to rebuild END

    ${result}    Check For Nan Metric    ${duplicates}
    Should Be True    ${result}    at least one metric contains NaN value
