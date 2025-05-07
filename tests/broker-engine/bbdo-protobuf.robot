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
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    bbdo    trace
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Clear Retention
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
    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbservicestatus.lua
    Ctn Clear Retention
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
    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbhoststatus.lua
    Ctn Clear Retention
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
    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-pbservice.lua
    Ctn Clear Retention
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
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
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
        IF    "${output}" == "(('VAL1',), ('VAL_SERV1',))"    BREAK
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    (('VAL1',), ('VAL_SERV1',))

    [Teardown]    Ctn Stop Engine Broker And Save Logs    True

BEPBHostParent
    [Documentation]    bbdo_version 3 communication of host parent relations
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Add Parent To Host    0    host_1    host_2
    Ctn Add Parent To Host    0    host_3    host_2
    Ctn Config Broker    central
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((1, 2), (3, 2))"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1, 2), (3, 2))    host parent not inserted
    # remove host
    Ctn Config Engine    ${1}
    Ctn Reload Broker    True
    Ctn Reload Engine

    FOR    ${index}    IN RANGE    30
        ${output}    Query
        ...    SELECT child_id, parent_id FROM hosts_hosts_parents
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()    host parent not deleted

    ${content}    Create List    [sql] [error]
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    1
    Should Not Be True    ${result}    sql error

    [Teardown]    Ctn Stop Engine Broker And Save Logs    True

BEPBINST_CONF
    [Documentation]    bbdo_version 3 communication of instance configuration.
    [Tags]    broker    engine    protobuf    bbdo    MON-38007
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    module0    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    ${content}    Create List    muxer centreon-broker-master-rrd event of type 10036 pushed
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    log about pb_instance_configuration not found

    Sleep    2
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    log about pb_instance_configuration not found

    [Teardown]    Ctn Stop Engine Broker And Save Logs

GRPC_CLOUD_FAILURE
    [Documentation]    simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
    [Tags]    broker    engine    bbdo_server    bbdo_client    grpc    cloud    mon-38483

    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    module0    bbdo_client    5669    grpc    localhost
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    module0    grpc    trace
    Ctn Broker Config Log    module0    processing    trace
    Ctn Broker Config Source Log    module0    1
    Ctn Broker Config Log    central    grpc    trace
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Broker Config Output Set    module0    central-module-master-output    encryption    true
    Ctn Broker Config Output Set    module0    central-module-master-output    ca_certificate    /tmp/ca_1234.crt
    Ctn Broker Config Input Set    central    central-broker-master-input    encryption    true
    Ctn Broker Config Input Set    central    central-broker-master-input    private_key    /tmp/server_1234.key
    Ctn Broker Config Input Set    central    central-broker-master-input    ca_certificate    /tmp/ca_1234.crt
    Ctn Broker Config Input Set    central    central-broker-master-input    certificate    /tmp/server_1234.crt

    Ctn Config BBDO3    ${1}
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}

    ${grpc_bbdo_server}    Ctn Create Bbdo Grpc Server    5669

    ${many_connections}    Ctn Wait For Connections    5669    3    20
    Should Not Be True    ${many_connections}    We should have only one connection to fake grpc server

    Call method    ${grpc_bbdo_server}    stop    1
    Sleep    10

    Ctn Start Broker    ${True}

    Ctn Process Service Result Hard    host_1    service_2    2    service critical
    ${result}    Ctn Check Service Status With Timeout    host_1    service_2    2    60    HARD
    Should Be True    ${result}    The service (host_1,service_2) is not CRITICAL as expected

    [Teardown]    Ctn Stop Engine Broker And Save Logs    True

GRPC_RECONNECT
    [Documentation]    We restart broker and engine must reconnect to it and send data
    [Tags]    broker    engine    bbdo_server    bbdo_client    grpc    cloud    mon-38483
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    module0    bbdo_client    5669    grpc    localhost
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker    True
    Ctn Wait For Engine To Be Ready    ${start}
    Ctn Restart Broker    True

    Ctn Process Service Result Hard    host_1    service_2    2    service critical
    ${result}    Ctn Check Service Status With Timeout    host_1    service_2    2    60    HARD
    Should Be True    ${result}    The service (host_1,service_2) is not CRITICAL as expected

    [Teardown]    Ctn Stop Engine Broker And Save Logs    True
