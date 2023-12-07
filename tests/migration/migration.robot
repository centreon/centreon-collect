*** Settings ***
Documentation       Centreon Broker and Engine are configured in bbdo2 with sql/storage outputs. Then we change these outputs to unified_sql. The we change bbdo2 to bbdo3. And we make all the way in reverse order.

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
MIGRATION
    [Documentation]    Migration bbdo2 with sql/storage to bbdo2 with unified_sql and then to bbdo3 with unified_sql and then to bbdo2 with unified_sql and then to bbdo2 with sql/storage
    [Tags]    broker    engine    services    protobuf

    Log To Console    Pure legacy mode
    Config Engine    ${3}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Broker Config Log    central    sql    debug
    Broker Config Log    central    core    error
    Broker Config Log    rrd    rrd    trace
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine

    ${contentCentral}    Create List    SQL: processing service status event
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${contentCentral}    60
    Should Be True    ${result}    No service status processed by the sql output for 60s
    ${contentRRD}    Create List    RRD: output::write
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${contentRRD}    60
    Should Be True    ${result}    No metric sent to rrd cbd for 30s

    Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date

    Log To Console    Move to BBDO 2.0.0 with unified_sql
    Kindly Stop Broker
    Start Broker
    Stop Engine
    Start Engine
    Sleep    2s

    ${contentCentral}    Create List    SQL: processing service status event
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${contentCentral}    60
    Should Be True    ${result}    No service status processed by the unified_sql output for 60s
    ${contentRRD}    Create List    RRD: output::write
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${contentRRD}    30
    Should Be True    ${result}    No metric sent to rrd cbd by unified_sql for 30s

    Config BBDO3    3
    ${start}    Get Current Date

    Log To Console    Move to BBDO 3.0.0 with unified_sql
    Kindly Stop Broker
    Start Broker
    Stop Engine
    Start Engine
    Sleep    2s

    ${contentCentral}    Create List    status check result output:
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${contentCentral}    60
    Should Be True    ${result}    No pb service status processed by the unified_sql output with BBDO3 for 60s
    ${contentRRD}    Create List    RRD: output::write
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${contentRRD}    30
    Should Be True    ${result}    No metric sent to rrd cbd by unified_sql for 30s

    Broker Config Remove Item    module0    bbdo_version
    Broker Config Remove Item    module1    bbdo_version
    Broker Config Remove Item    module2    bbdo_version
    Broker Config Remove Item    central    bbdo_version
    Broker Config Remove Item    rrd    bbdo_version
    ${start}    Get Current Date

    Log To Console    Move back to BBDO 2.0.0 with unified_sql
    Kindly Stop Broker
    Start Broker
    Stop Engine
    Start Engine
    Sleep    2s

    ${contentCentral}    Create List    SQL: processing service status event
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${contentCentral}    60
    Should Be True    ${result}    No service status processed by the unified_sql output for 60s
    ${contentRRD}    Create List    RRD: output::write
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${contentRRD}    30
    Should Be True    ${result}    No metric sent to rrd cbd by unified_sql for 30s

    Log To Console    Move back to BBDO 2.0.0 with sql/storage
    Config Broker Sql Output    central    sql/perfdata
    Kindly Stop Broker
    Start Broker
    Stop Engine
    Start Engine
    Sleep    2s

    ${contentCentral}    Create List    SQL: processing service status event
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${contentCentral}    200
    Should Be True    ${result}    No service status processed by the sql output for 200s
    ${contentRRD}    Create List    RRD: output::write
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${contentRRD}    30
    Should Be True    ${result}    No metric sent to rrd cbd for 30s

    [Teardown]    Stop Engine Broker And Save Logs
