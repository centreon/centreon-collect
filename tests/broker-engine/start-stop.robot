*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource
Library             ../resources/lua.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BESS1
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Ctn Start Broker
    Ctn Start Engine

    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS2
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    ${test_direct_grpc}    Ctn Is Using Direct Grpc
    IF    ${test_direct_grpc}
        Pass Execution    Test passes, skipping on direct grpc tests
    END
    Ctn Clear Retention
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    info
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${content}    Create List    SQL: Disabling poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No stop event processed by central cbd
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS2U
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first.
    ...    Unified_sql is used.
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    central    bbdo    info
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test.
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    ${content}    Create List    'central-rrd-master' with id [0-9]+ connected
    ${result}    Ctn Find RegEx In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No rrd cbd connected    
    &{result}    Ctn Get Peers    51001
    Log To Console    ${result}
    ${length}    Get Length    ${result['peers']}
    Should Be Equal As Integers    ${length}    2
    ...    Engine and Broker RRD should be connected to Broker Central
    Ctn Stop Engine
    ${content}    Create List    unified_sql: Disabling poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No stop event processed by central cbd
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    &{result}    Ctn Get Peers    51001
    Log To Console    ${result}
    ${length}    Get Length    ${result['peers']}
    Should Be Equal As Integers    ${length}    1
    ...    RRD Broker should be still connected to Broker Central
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS3
    [Documentation]    Start-Stop Broker/Engine - Engine started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS4
    [Documentation]    Start-Stop Broker/Engine - Engine started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS5
    [Documentation]    Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Engine Config Set Value    ${0}    debug_level    ${-1}
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Broker and Engine seem not connected
    [Teardown]    Ctn Stop Engine Broker And Save Logs
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_GRPC4
    [Documentation]    Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_GRPC5
    [Documentation]    Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Engine Config Set Value    ${0}    debug_level    ${-1}
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_GRPC_COMPRESS1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Change Broker Compression Output    module0    central-module-master-output    yes
    Ctn Change Broker Compression Input    central    centreon-broker-master-input    yes
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection not established between Engine and Broker
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_CRYPTED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Output Grpc Crypto    module0    True    False
    Ctn Add Broker Tcp Input Grpc Crypto    central    True    False
    Ctn Remove Host From Broker Output    module0    central-module-master-output
    Ctn Add Host To Broker Output    module0    central-module-master-output    localhost
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        ${result}    Ctn Check Connections
        Should Be True    ${result}    Connection between Engine and Broker not established
        ${result}    Ctn Check Poller Enabled In Database    1    10
        Should Be True    ${result}    Poller not visible in database
        Ctn Stop Engine
        ${result}    Ctn Check Poller Disabled In Database    1    10
        Should Be True    ${result}    Poller still visible in database
        Ctn Kindly Stop Broker
    END
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_CRYPTED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only server crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Input Grpc Crypto    central    True    False
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_CRYPTED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Output Grpc Crypto    module0    True    False
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_CRYPTED_REVERSED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Output Grpc Crypto    module0    True    True
    Ctn Add Broker Tcp Input Grpc Crypto    central    True    True
    Ctn Add Host To Broker Input    central    central-broker-master-input    localhost
    Ctn Remove Host From Broker Output    module0    central-module-master-output
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        ${result}    Ctn Check Connections
        Should Be True    ${result}    Connection between Engine and Broker not established
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_CRYPTED_REVERSED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only engine server crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Output Grpc Crypto    module0    True    True
    Ctn Add Host To Broker Input    central    central-broker-master-input    localhost
    Ctn Remove Host From Broker Output    module0    central-module-master-output
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    5s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_CRYPTED_REVERSED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Input Grpc Crypto    central    True    True
    Ctn Add Host To Broker Input    central    central-broker-master-input    localhost
    Ctn Remove Host From Broker Output    module0    central-module-master-output
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    5s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESS_ENGINE_DELETE_HOST
    [Documentation]    once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${start}    Get Current Date
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Broker    True
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Ctn Kindly Stop Broker    True
    Ctn Start Broker    True
    Ctn Engine Config Remove Service Host    ${0}    host_16
    Ctn Engine Config Remove Host    ${0}    host_16
    Ctn Reload Engine
    Sleep    2s
    Ctn Kindly Stop Broker    True
    Ctn Stop Engine
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BESSBQ1
    [Documentation]    A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
    [Tags]    broker    engine    start-stop    queue
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    core    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Source Log    central    true
    Ctn Clear Retention
    Ctn Create Bad Queue    central-broker-master.queue.central-broker-master-sql
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    end execute statement

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    120
    Should Be True    ${result}    Services should be updated after the ingestion of the queue file
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

Start_Stop_Engine_Broker_${id}
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    processing    debug
    Ctn Config Broker Sql Output    central    unified_sql
    IF    ${grpc}
        Ctn Change Broker Tcp Output To Grpc    central
        Ctn Change Broker Tcp Output To Grpc    module0
        Ctn Change Broker Tcp Input To Grpc    central
        Ctn Change Broker Tcp Input To Grpc    rrd
    END
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    create feeder central-broker-master-input
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    create feeder not found
    ${result}    Ctn Check Connections
    Should Be True    ${result}    no connection between engine and cbd
    Sleep    5s
    ${start_stop}    Get Current Date
    Ctn Stop Engine
    ${content}    Create List    feeder 'central-broker-master-input-1', connection closed
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_stop}    ${content}    60
    Should Be True    ${result}    connection closed not found

    Examples:    id    grpc    --
    ...    1    False
    ...    2    True
    Ctn Kindly Stop Broker
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

Start_Stop_Broker_Engine_${id}
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    processing    debug
    IF    ${grpc}
        Ctn Change Broker Tcp Output To Grpc    central
        Ctn Change Broker Tcp Output To Grpc    module0
        Ctn Change Broker Tcp Input To Grpc    central
        Ctn Change Broker Tcp Input To Grpc    rrd
    END
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${start}    Ctn Get Round Current Date

    ${result}    Ctn In Bbdo2
    Should Be True    ${result}    We should be in BBDO2 in this test.
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    create feeder central-broker-master-input
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    create feeder not found
    ${result}    Ctn Check Connections
    Should Be True    ${result}    no connection between engine and cbd
    Sleep    5s
    ${stop_broker}    Get Current Date
    Ctn Kindly Stop Broker
    ${content}    Create List    failover central-module-master-output: connection closed
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${stop_broker}    ${content}    60
    Should Be True    ${result}    connection closed not found
    Examples:    id    grpc    --
    ...    1    False
    ...    2    True
    Ctn Stop Engine

BESSG
    [Documentation]    Scenario: Broker handles connection and disconnection with Engine
    ...    Given Broker is configured with only one output that is Graphite
    ...    When the Engine starts and connects to the Broker
    ...    Then the Broker must be able to handle the connection
    ...    When the Engine stops
    ...    Then the Broker must be able to handle the disconnection

    [Tags]    broker    engine    start-stop    MON-161611
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1    3.0.1    True
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Remove Output    central    central-broker-unified-sql
    Ctn Broker Config Remove Output    central    centreon-broker-master-rrd
    Ctn Broker Config Add Output    central    { "name": "graphite-output", "db_host": "localhost", "db_port": "2003", "type": "graphite", "db_password": "", "queries_per_transaction": "1000", "metric_naming": "nagios.host.$HOST$.service.$SERVICE$.perfdata.$METRIC$", "status_naming": "nagios.host.$HOST$.service.$SERVICE$.metadata.state" }
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    ${True}
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    1
    Ctn Stop Engine
    Ctn Kindly Stop Broker    ${True}

BESSCTO
    [Documentation]    Scenario: Service commands time out due to missing Perl Connector
    ...    Given the Engine is configured as usual but without the Perl Connector
    ...    When the Engine executes its service commands
    ...    Then the commands take too long and reach the timeout
    ...    And the Engine starts and stops two times as a result
    [Tags]    engine    start-stop    MON-167816
    Ctn Config Engine    ${1}
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Engine Command Remove Connector    ${0}    *
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1    3.0.1
    FOR    ${i}    IN RANGE    2
      ${start}    Ctn Get Round Current Date
      Ctn Start Broker    ${True}
      Ctn Start Engine
      Ctn Wait For Engine To Be Ready    ${start}    1
      Sleep    60s
      Ctn Stop Engine
      Ctn Kindly Stop Broker    ${True}
    END

BESSCTOWC
    [Documentation]    Scenario: Service commands time out due to missing Perl Connector
    ...    Given the Engine is configured as usual with some commands using the Perl Connector
    ...    When the Engine executes its service commands
    ...    Then the commands take too long and reach the timeout
    ...    And the Engine starts and stops two times as a result

    [Tags]    engine    start-stop    MON-167816
    Ctn Config Engine    ${1}
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1    3.0.1
    FOR    ${i}    IN RANGE    2
      ${start}    Ctn Get Round Current Date
      Ctn Start Broker    ${True}
      Ctn Start Engine
      Ctn Wait For Engine To Be Ready    ${start}    1
      Sleep    60s
      Ctn Stop Engine
      Ctn Kindly Stop Broker    ${True}
    END


HUGE_CONF
    [Documentation]    Given a broker with 3 pollers, we wait that all engine are ready,
    ...    Then once all data are saved in db, we stop broker, we start broker and we test cache content with a lua script. 
    ...    We also check that we don't have cache error in broker logs
    [Tags]    broker    start-stop    MON-195013
    Ctn Config Engine    ${3}    ${5000}    ${20}    ${EMPTY}    ${False}
    Ctn Add All Host_Groups    ${3}    ${10}
    Ctn Add All Service Groups    ${3}    ${10}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Broker Config Source Log    central    1
    Ctn Broker Config Log    central    lua    trace
    Ctn Config BBDO3    ${3}    3.1.0
    Ctn Broker Config Output Set    central    central-broker-unified-sql   store_in_data_bin    no 
    Ctn Clear Retention

    @{random_services}    Ctn Get Random Services    ${10}
    ${target_str}    Evaluate    ",".join(f"{h}:{s}" for h, s in $random_services)
    ${lua_params}    Create Dictionary    name=target_services    type=string    value=${target_str}
    Ctn Broker Config Add Lua Output    central    cache-huge    ${SCRIPTS}/dump_host_service.lua    ${lua_params}
    Ctn Broker Config Output Set Json    central    cache-huge    filters    {"event": ["neb:ServiceStatus"]}

    ${test_start}    Ctn Get Round Current Date
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    ${True}
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${3}
    ${result}    Ctn Check Service Status With Timeout    host_5000    service_100000    4    120    HARD
    Should Be True    ${result}    no service status for service_100000

    FOR    ${try_index}    IN RANGE    2
        Log To Console    round ${try_index}
        Ctn Kindly Stop Broker    ${True}

        Remove File    /tmp/test-huge-cache.log
        Ctn Start Broker    ${True}
        Ctn Process All Services Check Result With Metrics    ${try_index}    output ${try_index}    ${3}
        ${result}    Ctn Check Service Status With Timeout    host_5000    service_100000    ${try_index}    160    ANY
        Should Be True    ${result}    no service status for service_100000 after passive results

        FOR    ${pair}    IN    @{random_services}
            ${host_id}=    Set Variable    ${pair}[0]
            ${service_id}=    Set Variable    ${pair}[1]
            ${host_info}    Ctn Get Host Cache Info    /tmp/test-huge-cache.log    ${host_id}
            Should Not Be Empty    ${host_info}    host_${host_id} not found in lua cache log
            Should Be Equal As Strings    ${host_info["name"]}    host_${host_id}
            Should Be Equal As Numbers    ${host_info["host_id"]}    ${host_id}
            ${serv_info}    Ctn Get Service Cache Info    /tmp/test-huge-cache.log    ${service_id}    60
            Should Not Be Empty    ${serv_info}    service_${service_id} not found in lua cache log
            Should Be Equal As Strings    ${serv_info["description"]}    service_${service_id}
            Should Be Equal As Strings    ${serv_info["host_name"]}    host_${host_id}
            Should Be Equal As Numbers    ${serv_info["host_id"]}    ${host_id}
            Should Be Equal As Numbers    ${serv_info["service_id"]}    ${service_id}
        END
    END

    #cache error?
    ${content}    Create List    error.*global_cache
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${test_start}    ${content}    2
    Should Not Be True    ${result[0]}    Some cache error in logs

    [teardown]    Ctn Stop Engine Broker And Save Logs
