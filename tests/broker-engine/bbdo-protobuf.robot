*** Settings ***
Documentation       Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BEPBBEE1
    [Documentation]    central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Log    module0    bbdo    debug
    Broker Config Log    central    bbdo    debug
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    BBDO: peer is using protocol version 3.0.0 whereas we're using protocol version 2.0.0
    ${result}=    Find In Log with timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Message about not matching bbdo versions not available
    Stop Engine
    Kindly Stop Broker

BEPBBEE2
    [Documentation]    bbdo_version 3 not compatible with sql/storage
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Broker Config Flush Log    central    0
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List
    ...    Configuration check error: bbdo versions >= 3.0.0 need the unified_sql module to be configured.
    ${result}=    Find In Log with timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Message about a missing config of unified_sql not available.
    Stop Engine

BEPBBEE3
    [Documentation]    bbdo_version 3 generates new bbdo protobuf service status messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbservicestatus.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbservicestatus.lua
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    Wait Until Created    /tmp/pbservicestatus.log    1m
    Stop Engine
    Kindly Stop Broker

BEPBBEE4
    [Documentation]    bbdo_version 3 generates new bbdo protobuf host status messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbhoststatus.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbhoststatus.lua
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    Wait Until Created    /tmp/pbhoststatus.log    1m
    Stop Engine
    Kindly Stop Broker

BEPBBEE5
    [Documentation]    bbdo_version 3 generates new bbdo protobuf service messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbservice.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbservice.lua
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    Wait Until Created    /tmp/pbservice.log    1m
    Stop Engine
    Kindly Stop Broker

BEPBRI1
    [Documentation]    bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbresponsiveinstance.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    broker_config_output_set    central    central-broker-unified-sql    read_timeout    2
    broker_config_output_set    central    central-broker-unified-sql    instance_timeout    2

    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-responsiveinstance.lua
    Clear Retention
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM instances
    ${start}=    Get Current Date
    Start Broker    True
    Start Engine
    Wait Until Created    /tmp/pbresponsiveinstance.log    30s
    ${grep_res}=    Grep File    /tmp/pbresponsiveinstance.log    "_type":65582, "category":1, "element":46,
    ${grep_res}=    Get Lines Containing String    ${grep_res}    "poller_id":1, "responsive":true
    Should Not Be Empty    ${grep_res}    msg="responsive":true not found
    Stop Engine
    FOR    ${index}    IN RANGE    60
        Sleep    1s
        ${grep_res}=    Grep File    /tmp/pbresponsiveinstance.log    "_type":65582, "category":1, "element":46,
        ${grep_res}=    Get Lines Containing String    ${grep_res}    "poller_id":1, "responsive":false
        IF    len('${grep_res}') > 0            BREAK
    END

    Should Not Be Empty    ${grep_res}    msg="responsive":false not found
    Kindly Stop Broker    True

BEPBCVS
    [Documentation]    bbdo_version 3 communication of custom variables.
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Broker    central
    Config BBDO3    ${1}
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}=    Get Current Date
    Start Broker    True
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    300
        ${output}=    Query
        ...    SELECT c.value FROM customvariables c LEFT JOIN hosts h ON c.host_id=h.host_id WHERE h.name='host_1' && c.name in ('KEY1','KEY_SERV1_1') ORDER BY service_id
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('VAL1',), ('VAL_SERV1',))"            BREAK
    END
    Should Be Equal As Strings    ${output}    (('VAL1',), ('VAL_SERV1',))

    Stop Engine
    Kindly Stop Broker    True
