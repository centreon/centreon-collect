*** Settings ***
Documentation       Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


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
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    BBDO: peer is using protocol version 3.0.0 whereas we're using protocol version 2.0.0
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Message about not matching bbdo versions not available

    [Teardown]    Stop Engine Broker And Save Logs

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
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List
    ...    Configuration check error: bbdo versions >= 3.0.0 need the unified_sql module to be configured.
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Message about a missing config of unified_sql not available.
    Stop Engine

BEPBBEE3
    [Documentation]    bbdo_version 3 generates new bbdo protobuf service status messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbservicestatus.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbservicestatus.lua
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Wait Until Created    /tmp/pbservicestatus.log    1m

    [Teardown]    Stop Engine Broker And Save Logs

BEPBBEE4
    [Documentation]    bbdo_version 3 generates new bbdo protobuf host status messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbhoststatus.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbhoststatus.lua
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Wait Until Created    /tmp/pbhoststatus.log    1m

    [Teardown]    Stop Engine Broker And Save Logs

BEPBBEE5
    [Documentation]    bbdo_version 3 generates new bbdo protobuf service messages.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbservice.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbservice.lua
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Wait Until Created    /tmp/pbservice.log    1m

    [Teardown]    Stop Engine Broker And Save Logs

BEPBRI1
    [Documentation]    bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
    [Tags]    broker    engine    protobuf    bbdo
    Remove File    /tmp/pbresponsiveinstance.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    1
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    read_timeout    2
    Broker Config Output Set    central    central-broker-unified-sql    instance_timeout    2

    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-responsiveinstance.lua
    Clear Retention
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM instances
    ${start}    Get Current Date
    Start Broker    True
    Start Engine
    Wait Until Created    /tmp/pbresponsiveinstance.log    30s
    ${grep_res}    Grep File    /tmp/pbresponsiveinstance.log    "_type":65582, "category":1, "element":46,
    ${grep_res}    Get Lines Containing String    ${grep_res}    "poller_id":1, "responsive":true
    Should Not Be Empty    ${grep_res}    "responsive":true not found
    Stop Engine
    FOR    ${index}    IN RANGE    60
        Sleep    1s
        ${grep_res}    Grep File    /tmp/pbresponsiveinstance.log    "_type":65582, "category":1, "element":46,
        ${grep_res}    Get Lines Containing String    ${grep_res}    "poller_id":1, "responsive":false
        IF    len('${grep_res}') > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    "responsive":false not found
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
    ${start}    Get Current Date
    Start Broker    True
    Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    300
        ${output}    Query
        ...    SELECT c.value FROM customvariables c LEFT JOIN hosts h ON c.host_id=h.host_id WHERE h.name='host_1' && c.name in ('KEY1','KEY_SERV1_1') ORDER BY service_id
        Log To Console    ${output}
        IF    "${output}" == "(('VAL1',), ('VAL_SERV1',))"    BREAK
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    (('VAL1',), ('VAL_SERV1',))

    [Teardown]    Stop Engine Broker And Save Logs    True

BEPB_HOST_DEPENDENCY                                                            
    [Documentation]    BBDO 3 communication of host dependencies.       
    [Tags]    broker    engine    protobuf    bbdo                              
    Config Engine    ${1}                                                       
    Config Engine Add Cfg File    0    dependencies.cfg                         
    Add Host Dependency    0    host_1    host_2                                
    Config Broker    central                                                    
    Config Broker    module                                                     
    Config BBDO3    ${1}                                                        
    Broker Config Log    central    sql    trace                                
    Config Broker Sql Output    central    unified_sql                          
    Clear Retention                                                             
    ${start}    Get Current Date                                                
    Start Broker    True                                                        
    Start Engine                                                                
                                                                                
    ${result}    Common.Check Host Dependencies    2    1        24x7    1   ou    dp    30
    Should Be True    ${result}    No notification dependency from 2 to 1 with timeperiod 24x7 on 'ou'
                                                                                
    Config Engine    ${1}                                                       
    Reload Engine                                                               
                                                                                
    ${result}    Common.Check No Host Dependencies    30                               
    Should Be True    ${result}    No host dependency should be defined         
                                                                                
    [Teardown]    Stop Engine Broker And Save Logs    True                      

BEPB_SERVICE_DEPENDENCY
    [Documentation]    bbdo_version 3 communication of host dependencies.
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Engine Add Cfg File    0    dependencies.cfg
    Add Service Dependency    0    host_1    host_2    service_1    service_21
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    Start Broker    True
    Start Engine

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

    Config Engine    ${1}
    Reload Engine

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT dependent_host_id, host_id, dependency_period, inherits_parent, notification_failure_options FROM hosts_hosts_dependencies
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    host dependency not deleted from database

    [Teardown]    Stop Engine Broker And Save Logs    True

BEPBHostParent
    [Documentation]    bbdo_version 3 communication of host parent relations
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Add Parent To Host    0    host_1    host_2
    Config Broker    central
    Config BBDO3    ${1}
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    Start Broker    True
    Start Engine
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((1, 2),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1, 2),)    host parent not inserted

    # remove host
    Config Engine    ${1}
    Reload Broker    True
    Reload Engine

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    host parent not deleted

    [Teardown]    Stop Engine Broker And Save Logs    True
