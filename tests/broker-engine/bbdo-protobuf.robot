*** Settings ***
Documentation       Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.

Resource            ../resources/resources.robot
Library             Process
Library             DateTime
Library             DatabaseLibrary
Library             OperatingSystem
Library             String
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             ../resources/specific-duplication.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BEPBBEE1
    [Documentation]    central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    BBDO: peer is using protocol version 3.0.0 whereas we're using protocol version 2.0.0
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Message about not matching bbdo versions not available
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPBBEE2
    [Documentation]    bbdo_version 3 not compatible with sql/storage
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List
    ...    Configuration check error: bbdo versions >= 3.0.0 need the unified_sql module to be configured.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Message about a missing config of unified_sql not available.
    Ctn Stop Engine

BEPBBEE3
    [Documentation]    bbdo_version 3 generates new bbdo protobuf service status messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbservicestatus.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbservicestatus.lua
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Wait Until Created    /tmp/pbservicestatus.log    1m
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPBBEE4
    [Documentation]    bbdo_version 3 generates new bbdo protobuf host status messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbhoststatus.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbhoststatus.lua
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Wait Until Created    /tmp/pbhoststatus.log    1m
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPBBEE5
    [Documentation]    bbdo_version 3 generates new bbdo protobuf service messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbservice.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbservice.lua
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Wait Until Created    /tmp/pbservice.log    1m
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPBRI1
    [Documentation]    bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbresponsiveinstance.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Remove Rrd Output    central
    Ctn Broker Config Output Set    central    central-broker-unified-sql    read_timeout    2
    Ctn Broker Config Output Set    central    central-broker-unified-sql    instance_timeout    2

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-responsiveinstance.lua
    Ctn Clear Retention
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM instances
    ${start}    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine
    Wait Until Created    /tmp/pbresponsiveinstance.log    30s
    ${grep_res}    Grep File    /tmp/pbresponsiveinstance.log    "_type":65582, "category":1, "element":46,
    ${grep_res}    Get Lines Containing String    ${grep_res}    "poller_id":1, "responsive":true
    Should Not Be Empty    ${grep_res}    msg="responsive":true not found
    Ctn Stop Engine
    FOR    ${i}    IN RANGE    ${60}
        ${grep_res}    Grep File    /tmp/pbresponsiveinstance.log    "_type":65582, "category":1, "element":46,
        ${grep_res}    Get Lines Containing String    ${grep_res}    "poller_id":1, "responsive":false
        IF    len("""${grep_res}""") > 0    BREAK
        Sleep    1s
    END
    Should Not Be Empty    ${grep_res}    msg="responsive":false not found
    Ctn Kindly Stop Broker    True

BEPBCVS
    [Documentation]    bbdo_version 3 communication of custom variables.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    300
        ${output}    Query
        ...    SELECT c.value FROM customvariables c LEFT JOIN hosts h ON c.host_id=h.host_id WHERE h.name='host_1' && c.name in ('KEY1','KEY_SERV1_1') ORDER BY service_id
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('VAL1',), ('VAL_SERV1',))"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('VAL1',), ('VAL_SERV1',))

    Ctn Stop Engine
    Ctn Kindly Stop Broker    True
