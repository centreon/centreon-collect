*** Settings ***
Documentation       Centreon Broker and Engine anomaly detection

Resource            ../resources/resources.robot
Library             DateTime
Library             Process
Library             OperatingSystem
Library             String
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
STUPID_FILTER
    [Documentation]    Unified SQL is configured with only the bbdo category as filter. An error is raised by broker and broker should run correctly.
    [Tags]    broker    engine    filter
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Config Broker    rrd
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    Broker Config Output Set Json    central    central-broker-unified-sql    filters    {"category": ["bbdo"]}

    ${start}    Get Current Date
    Start Broker    True
    Start Engine

    ${content}    Create List
    ...    The configured write filters for the endpoint 'central-broker-unified-sql' are too restrictive and will be ignored. neb,bbdo,extcmd categories are mandatory.
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling bad filter should be available.

    Stop Engine
    Kindly Stop Broker    True

STORAGE_ON_LUA
    [Documentation]    The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
    [Tags]    broker    engine    filter
    Remove File    /tmp/all_lua_event.log

    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Config Broker    rrd
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    Broker Config Add Lua Output    central    test-filter    ${SCRIPTS}test-log-all-event.lua
    Broker Config Output Set Json    central    test-filter    filters    {"category": [ "storage"]}

    Start Broker    True
    Start Engine

    Wait Until Created    /tmp/all_lua_event.log
    FOR    ${index}    IN RANGE    30
        ${res}    Get File Size    /tmp/all_lua_event.log
        Sleep    1s
        IF    ${res} > 100    BREAK
    END
    ${grep_res}    Grep File    /tmp/all_lua_event.log    "category":[^3]    regexp=True
    Should Be Empty    ${grep_res}    Events of category different than 'storage' found.

    Stop Engine
    Kindly Stop Broker    True

FILTER_ON_LUA_EVENT
    [Documentation]    stream connector with a bad configured filter generate a log error message
    [Tags]    broker    engine    filter
    Remove File    /tmp/all_lua_event.log

    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Config Broker    rrd
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    Broker Config Add Lua Output
    ...    central
    ...    test-filter
    ...    ${SCRIPTS}test-log-all-event.lua

    # We use the possibility of broker to allow to filter by type and not only by category
    Broker Config Output Set Json
    ...    central
    ...    test-filter
    ...    filters
    ...    {"category": [ "storage:pb_metric_mapping"]}

    Start Broker    True
    Start Engine

    Wait Until Created    /tmp/all_lua_event.log
    FOR    ${index}    IN RANGE    30
        # search for pb_metric_mapping
        ${res}    Get File Size    /tmp/all_lua_event.log
        Sleep    1s
        IF    ${res} > 100    BREAK
    END
    ${content}    Get File    /tmp/all_lua_event.log
    @{lines}    Split To lines    ${content}
    FOR    ${line}    IN    @{lines}
        Should Contain
        ...    ${line}
        ...    "_type":196620
        ...    All the lines in all_lua_event.log should contain "_type":196620
    END

    Stop Engine
    Kindly Stop Broker    True

BAM_STREAM_FILTER
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. we watch its events
    [Tags]    broker    engine    bam    filter
    Clear Commands Status
    Config Broker    module    ${1}
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    core    trace
    Broker Config Log    central    config    trace
    Config BBDO3    ${1}
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create Ba With Services    test    worst    ${svc}
    Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    2
    Start Broker    True
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # monitoring
    FOR    ${cpt}    IN    RANGE 30
        # pb_service
        ${grep_res1}    Grep File    ${centralLog}    centreon-bam-monitoring event of type 1001b written
        # pb_service_status
        ${grep_res2}    Grep File    ${centralLog}    centreon-bam-monitoring event of type 1001d written
        # pb_ba_status
        ${grep_res3}    Grep File    ${centralLog}    centreon-bam-monitoring event of type 60013 written
        # pb_kpi_status
        ${grep_res4}    Grep File    ${centralLog}    centreon-bam-monitoring event of type 6001b written

        # reject KpiEvent
        ${grep_res5}    Grep File
        ...    ${centralLog}
        ...    muxer centreon-bam-monitoring event of type 60015 rejected by write filter
        # reject storage
        ${grep_res6}    Grep File
        ...    ${centralLog}
        ...    muxer centreon-bam-monitoring event of type 3[0-9a-f]{4} rejected by write filter    regexp=True

        IF    len("""${grep_res1}""") > 0 and len("""${grep_res2}""") > 0 and len("""${grep_res3}""") > 0 and len("""${grep_res4}""") > 0 and len("""${grep_res5}""") > 0 and len("""${grep_res6}""") > 0
            BREAK
        END
    END

    Should Not Be Empty    ${grep_res1}    no pb_service event
    Should Not Be Empty    ${grep_res2}    no pb_service_status event
    Should Not Be Empty    ${grep_res3}    no pb_ba_status event
    Should Not Be Empty    ${grep_res4}    no pb_kpi_status event
    Should Not Be Empty    ${grep_res5}    no KpiEvent event
    Should Not Be Empty    ${grep_res6}    no storage event rejected

    # reporting
    # pb_ba_event
    ${grep_res}    Grep File    ${centralLog}    centreon-bam-reporting event of type 60014 written
    Should Not Be Empty    ${grep_res}    no pb_ba_event
    # pb_kpi_event
    ${grep_res}    Grep File    ${centralLog}    centreon-bam-reporting event of type 60015 written
    Should Not Be Empty    ${grep_res}    no pb_kpi_event
    # reject storage
    ${grep_res}    Grep File
    ...    ${centralLog}
    ...    centreon-bam-reporting event of type 3[0-9a-f]{4} rejected by write filter    regexp=True
    Should Not Be Empty    ${grep_res}    no rejected storage event
    # reject neb
    ${grep_res}    Grep File
    ...    ${centralLog}
    ...    centreon-bam-reporting event of type 1[0-9a-f]{4} rejected by write filter    regexp=True
    Should Not Be Empty    ${grep_res}    no rejected neb event

    Stop Engine
    Kindly Stop Broker    True

UNIFIED_SQL_FILTER
    [Documentation]    With bbdo version 3.0.1, we watch events written or rejected in unified_sql
    [Tags]    broker    engine    bam    filter
    Clear Retention
    Config Broker    module    ${1}
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    core    trace
    Config BBDO3    ${1}
    Config Engine    ${1}

    ${start}    Get Current Date
    Start Broker    True
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # one service set to critical in order to have some events
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # de_pb_service de_pb_service_status de_pb_host de_pb_custom_variable de_pb_log_entry de_pb_host_check
    FOR    ${event}    IN    1001b    1001d    1001e    10025    10029    10027
        ${to_search}    Catenate    central-broker-unified-sql event of type    ${event}    written
        ${grep_res}    Grep File    ${centralLog}    ${to_search}
        Should Not Be Empty    ${grep_res}
    END

    Stop Engine
    Kindly Stop Broker    True

CBD_RELOAD_AND_FILTERS
    [Documentation]    We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
    [Tags]    broker    engine    filter

    Clear Retention
    Config Broker    module    ${1}
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    config    trace
    Broker Config Log    rrd    rrd    debug
    Config BBDO3    ${1}
    Config Engine    ${1}

    Log To Console    First configuration: all events are sent to rrd.
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Let's wait for storage data written into rrd files
    ${content}    Create List    RRD: new pb status data for index
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    No status from central broker for 1mn.

    # We check that output filters to rrd are set to "all"
    ${content}    Create List
    ...    endpoint applier: filters for endpoint 'centreon-broker-master-rrd' reduced to the needed ones: all
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about the output filters to rrd broker.

    # New configuration
    Broker Config Output Set Json    central    centreon-broker-master-rrd    filters    {"category": [ "storage"]}

    Log To Console    Second configuration: only storage events are sent.
    ${start}    Get Current Date
    Restart Engine
    Reload Broker
    #wait broker reload
    ${content}  Create List  creating endpoint centreon-broker-master-rrd
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No creating endpoint centreon-broker-master-rrd.
    ${start2}    Get Current Date

    # We check that output filters to rrd are set to "storage"
    ${content}    Create List
    ...    create endpoint TCP for endpoint 'centreon-broker-master-rrd'
    ...    endpoint applier: filters
    ...    storage for endpoint 'centreon-broker-master-rrd' applied.
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about the output filters to rrd broker.

    # Let's wait for storage data written into rrd files
    ${content}    Create List    RRD: new pb status data for index
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start2}    ${content}    60
    Should Be True    ${result}    No status from central broker for 1mn.

    # We check that output filters to rrd are set to "storage"
    ${content}    Create List    rrd event of type .* rejected by write filter
    ${result}    Find Regex In Log with Timeout    ${centralLog}    ${start2}    ${content}    120
    Should Be True    ${result}    No event rejected by the rrd output whereas only storage category is enabled.

    Log To Console    Third configuration: all events are sent.
    # New configuration
    Broker Config Output Remove    central    centreon-broker-master-rrd    filters
    ${start}    Get Current Date
    Restart Engine
    Reload Broker
    # wait broker reload
    ${content}  Create List  creating endpoint centreon-broker-master-rrd
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No creating endpoint centreon-broker-master-rrd.
    ${start2}    Get Current Date

    # We check that output filters to rrd are set to "all"
    ${content}    Create List
    ...    endpoint applier: filters for endpoint 'centreon-broker-master-rrd' reduced to the needed ones: all
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about the output filters to rrd broker.
    ${start}    Get Current Date

    # Let's wait for storage data written into rrd files
    ${content}    Create List    RRD: new pb status data for index
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start2}    ${content}    60
    Should Be True    ${result}    No status from central broker for 1mn.

    # We check that output filters to rrd doesn't filter anything
    ${content}    Create List    rrd event of type .* rejected by write filter
    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start2}    ${content}    30
    Should Be Equal As Strings
    ...    ${result[0]}
    ...    False
    ...    Some events are rejected by the rrd output whereas all categories are enabled.

    Stop Engine
    Kindly Stop Broker    True

CBD_RELOAD_AND_FILTERS_WITH_OPR
    [Documentation]    We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
    [Tags]    broker    engine    filter

    Clear Retention
    Config Broker    module    ${1}
    Config Broker    central
    Config Broker    rrd
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Broker Config Input Set    rrd    central-rrd-master-input    host    localhost
    Broker Config Log    central    config    trace
    Broker Config Log    rrd    rrd    debug
    Config BBDO3    ${1}
    Config Engine    ${1}

    Log To Console    First configuration: all events are sent to rrd.
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Let's wait for storage data written into rrd files
    ${content}    Create List    RRD: new pb status data for index
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    No status from central broker for 1mn.

    # We check that output filters to rrd are set to "all"
    ${content}    Create List
    ...    endpoint applier: filters for endpoint 'centreon-broker-master-rrd' reduced to the needed ones: all
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about the output filters to rrd broker.

    # New configuration
    Broker Config Output Set Json    central    centreon-broker-master-rrd    filters    {"category": [ "storage"]}

    Log To Console    Second configuration: only storage events are sent.
    ${start}    Get Current Date
    Restart Engine
    Reload Broker
    #wait broker reload
    ${content}  Create List  creating endpoint centreon-broker-master-rrd
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No creating endpoint centreon-broker-master-rrd.
    ${start2}    Get Current Date

    # We check that output filters to rrd are set to "storage"
    ${content}    Create List
    ...    create endpoint TCP for endpoint 'centreon-broker-master-rrd'
    ...    endpoint applier: filters
    ...    storage for endpoint 'centreon-broker-master-rrd' applied.
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about the output filters to rrd broker.

    # Let's wait for storage data written into rrd files
    ${content}    Create List    RRD: new pb status data for index
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start2}    ${content}    60
    Should Be True    ${result}    No status from central broker for 1mn.

    # We check that output filters to rrd are set to "storage"
    ${content}    Create List    rrd event of type .* rejected by write filter
    ${result}    Find Regex In Log with Timeout    ${centralLog}    ${start2}    ${content}    120
    Should Be True    ${result}    No event rejected by the rrd output whereas only storage category is enabled.

    Log To Console    Third configuration: all events are sent.
    # New configuration
    Broker Config Output Remove    central    centreon-broker-master-rrd    filters
    ${start}    Get Current Date
    Restart Engine
    Reload Broker
    #wait broker reload
    ${content}  Create List  creating endpoint centreon-broker-master-rrd
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No creating endpoint centreon-broker-master-rrd.
    ${start2}    Get Current Date

    # We check that output filters to rrd are set to "all"
    ${content}    Create List
    ...    endpoint applier: filters for endpoint 'centreon-broker-master-rrd' reduced to the needed ones: all
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No message about the output filters to rrd broker.

    # Let's wait for storage data written into rrd files
    ${content}    Create List    RRD: new pb status data for index
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start2}    ${content}    60
    Should Be True    ${result}    No status from central broker for 1mn.

    # We check that output filters to rrd doesn't filter anything
    ${content}    Create List    rrd event of type .* rejected by write filter
    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start2}    ${content}    30
    Should Be Equal As Strings
    ...    ${result[0]}
    ...    False
    ...    Some events are rejected by the rrd output whereas all categories are enabled.

    Stop Engine
    Kindly Stop Broker    True

SEVERAL_FILTERS_ON_LUA_EVENT
    [Documentation]    Two stream connectors with different filters are configured.
    [Tags]    broker    engine    filter
    Remove File    /tmp/all_lua_event.log
    Remove File    /tmp/all_lua_event-bis.log

    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Config Broker    rrd
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    Broker Config Add Lua Output
    ...    central
    ...    test-filter
    ...    ${SCRIPTS}test-log-all-event.lua
    Broker Config Add Lua Output
    ...    central
    ...    test-filter-bis
    ...    ${SCRIPTS}test-log-all-event-bis.lua

    # We use the possibility of broker to allow to filter by type and not only by category
    Broker Config Output Set Json
    ...    central
    ...    test-filter
    ...    filters
    ...    {"category": [ "storage:pb_metric_mapping"]}

    Broker Config Output Set Json
    ...    central
    ...    test-filter-bis
    ...    filters
    ...    {"category": [ "neb:ServiceStatus"]}

    Start Broker    True
    Start Engine

    Wait Until Created    /tmp/all_lua_event.log
    FOR    ${index}    IN RANGE    30
        # search for pb_metric_mapping
        ${res}    Get File Size    /tmp/all_lua_event.log
        Sleep    1s
        IF    ${res} > 100    BREAK
    END
    ${content}    Get File    /tmp/all_lua_event.log
    @{lines}    Split To lines    ${content}
    FOR    ${line}    IN    @{lines}
        Should Contain
        ...    ${line}
        ...    "_type":196620
        ...    All the lines in all_lua_event.log should contain "_type":196620
    END

    Wait Until Created    /tmp/all_lua_event-bis.log
    FOR    ${index}    IN RANGE    30
        # search for pb_metric_mapping
        ${res}    Get File Size    /tmp/all_lua_event-bis.log
        Sleep    1s
        IF    ${res} > 100    BREAK
    END
    ${content}    Get File    /tmp/all_lua_event-bis.log
    @{lines}    Split To lines    ${content}
    FOR    ${line}    IN    @{lines}
        Should Contain
        ...    ${line}
        ...    "_type":65565
        ...    All the lines in all_lua_event-bis.log should contain "_type":65565
    END
    Stop Engine
    Kindly Stop Broker    True
