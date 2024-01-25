*** Settings ***
Documentation       Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.

Resource            ../resources/import.resource

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
    Ctn Add Item To Broker Conf    module0    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Broker Config Log    central    bbdo    debug
    Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    BBDO: peer is using protocol version 3.0.0 whereas we're using protocol version 2.0.0
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Message about not matching bbdo versions not available

    [Teardown]    Ctn Stop Engine Broker And Save Logs

BEPBBEE2
    [Documentation]    bbdo_version 3 not compatible with sql/storage
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Add Item To Broker Conf    module0    bbdo_version    3.0.0
    Ctn Add Item To Broker Conf    central    bbdo_version    3.0.0
    Ctn Add Item To Broker Conf    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Flush Log    central    0
    Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List
    ...    Configuration check error: bbdo versions >= 3.0.0 need the unified_sql module to be configured.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Message about a missing config of unified_sql not available.
    Ctn Stop Engine

BEPBBEE3
    [Documentation]    bbdo_version 3 generates new bbdo protobuf service status messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbservicestatus.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Add Lua Output To Broker Conf    central    test-protobuf    ${SCRIPTS}test-pbservicestatus.lua
    Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Wait Until Created    /tmp/pbservicestatus.log    1m

    [Teardown]    Ctn Stop Engine Broker And Save Logs

BEPBBEE4
    [Documentation]    bbdo_version 3 generates new bbdo protobuf host status messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbhoststatus.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Add Lua Output To Broker Conf    central    test-protobuf    ${SCRIPTS}test-pbhoststatus.lua
    Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Wait Until Created    /tmp/pbhoststatus.log    1m

    [Teardown]    Ctn Stop Engine Broker And Save Logs

BEPBBEE5
    [Documentation]    bbdo_version 3 generates new bbdo protobuf service messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbservice.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Add Lua Output To Broker Conf    central    test-protobuf    ${SCRIPTS}test-pbservice.lua
    Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Wait Until Created    /tmp/pbservice.log    1m

    [Teardown]    Ctn Stop Engine Broker And Save Logs

BEPBRI1
    [Documentation]    bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbresponsiveinstance.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    read_timeout    2
    Ctn Broker Config Output Set    central    central-broker-unified-sql    instance_timeout    2

    Ctn Add Lua Output To Broker Conf    central    test-protobuf    ${SCRIPTS}test-responsiveinstance.lua
    Clear Retention
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM instances
    ${start}    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine
    Wait Until Created    /tmp/pbresponsiveinstance.log    30s
    ${grep_res}    Grep File    /tmp/pbresponsiveinstance.log    "_type":65582, "category":1, "element":46,
    ${grep_res}    Get Lines Containing String    ${grep_res}    "poller_id":1, "responsive":true
    Should Not Be Empty    ${grep_res}    "responsive":true not found
    Ctn Stop Engine
    FOR    ${index}    IN RANGE    60
        Sleep    1s
        ${grep_res}    Grep File    /tmp/pbresponsiveinstance.log    "_type":65582, "category":1, "element":46,
        ${grep_res}    Get Lines Containing String    ${grep_res}    "poller_id":1, "responsive":false
        IF    len('${grep_res}') > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    "responsive":false not found
    Ctn Kindly Stop Broker    True

BEPBCVS
    [Documentation]    bbdo_version 3 communication of custom variables.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    300
        ${output}    Query
        ...    SELECT c.value FROM customvariables c LEFT JOIN hosts h ON c.host_id=h.host_id WHERE h.name='host_1' && c.name in ('KEY1','KEY_SERV1_1') ORDER BY service_id
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('VAL1',), ('VAL_SERV1',))"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('VAL1',), ('VAL_SERV1',))

    [Teardown]    Ctn Stop Engine Broker And Save Logs    True

BEPB_HOST_DEPENDENCY
    [Documentation]    bbdo_version 3 communication of host dependencies.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Engine Add Cfg File    0    dependencies.cfg
    Ctn Add Host Dependency    0    host_1    host_2
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT dependent_host_id, host_id, dependency_period, inherits_parent, notification_failure_options FROM hosts_hosts_dependencies
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2, 1, '24x7', 1, 'ou'),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2, 1, '24x7', 1, 'ou'),)    host dependency not found in database

    Ctn Config Engine    ${1}
    Ctn Reload Engine

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT dependent_host_id, host_id, dependency_period, inherits_parent, notification_failure_options FROM hosts_hosts_dependencies
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    host dependency not deleted from database

    [Teardown]    Ctn Stop Engine Broker And Save Logs    True

BEPB_SERVICE_DEPENDENCY
    [Documentation]    bbdo_version 3 communication of host dependencies.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Engine Add Cfg File    0    dependencies.cfg
    Ctn Add Service Dependency    0    host_1    host_2    service_1    service_21
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT dependent_host_id, dependent_service_id, host_id, service_id, dependency_period, inherits_parent, notification_failure_options FROM services_services_dependencies;

        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2, 21, 1, 1, '24x7', 1, 'c'),)"    BREAK
    END
    Should Be Equal As Strings
    ...    ${output}
    ...    ((2, 21, 1, 1, '24x7', 1, 'c'),)
    ...    host dependency not found in database

    Ctn Config Engine    ${1}
    Ctn Reload Engine

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT dependent_host_id, host_id, dependency_period, inherits_parent, notification_failure_options FROM hosts_hosts_dependencies
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    host dependency not deleted from database

    [Teardown]    Ctn Stop Engine Broker And Save Logs    True
