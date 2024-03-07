*** Settings ***
Resource    ../resources/resources.robot
Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed

Documentation    Centreon Broker and Engine start/stop tests
Library    Process
Library    DateTime
Library    OperatingSystem
Library    DateTime
Library    ../resources/Engine.py
Library    ../resources/Broker.py
Library    ../resources/Common.py

*** Test Cases ***
BESS1
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Broker stopped first
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS2
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    ${result}=    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Ctn Stop Engine
    ${result}=    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BESS3
    [Documentation]    Start-Stop Broker/Engine - Engine started first - Engine stopped first
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Start Engine
    Ctn Start Broker
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    ${result}=    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Ctn Stop Engine
    ${result}=    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BESS4
    [Documentation]    Start-Stop Broker/Engine - Engine started first - Broker stopped first
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Start Engine
    Ctn Start Broker
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    ${result}=    Ctn Check Poller Enabled In Database    1  10
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS5
    [Documentation]    Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Engine Config Set Value    ${0}    debug_level    ${-1}
    Ctn Start Broker
    Ctn Start Engine
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    ${result}=    Ctn Check Poller Enabled In Database    1  10
    Should Be True    ${result}
    Ctn Stop Engine
    ${result}=    Ctn Check Poller Disabled In Database    1  10
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BESS_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Start Engine
    Ctn Start Broker
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    ${result}=    Ctn Check Poller Enabled In Database    1  10
    Should Be True    ${result}
    Ctn Stop Engine
    ${result}=    Ctn Check Poller Disabled In Database    1  10
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BESS_GRPC4
    [Documentation]    Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Start Engine
    Ctn Start Broker
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS_GRPC5
    [Documentation]    Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Engine Config Set Value    ${0}    debug_level    ${-1}
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    ${result}=    Ctn Check Poller Enabled In Database    1  10
    Should Be True    ${result}
    Ctn Stop Engine
    ${result}=    Ctn Check Poller Disabled In Database    1  10
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BESS_GRPC_COMPRESS1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Change Broker Compression Output    module0    central-module-master-output    yes
    Ctn Change Broker Compression Input    central    centreon-broker-master-input    yes
    Ctn Start Broker
    Ctn Start Engine
    ${result}=    Ctn Check Connections
    Should Be True    ${result}
    ${result}=    Ctn Check Poller Enabled In Database    1  10
    Should Be True    ${result}
    Ctn Stop Engine
    ${result}=    Ctn Check Poller Disabled In Database    1  10
    Should Be True    ${result}
    Ctn Kindly Stop Broker


BESS_CRYPTED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Add Broker tcp output grpc crypto    module0    True    False
    Ctn Add Broker tcp input grpc crypto    central    True    False
    Remove Host from broker output    module0    central-module-master-output
    Add Host to broker output    module0    central-module-master-output    localhost
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        ${result}=    Ctn Check Connections
        Should Be True    ${result}
        ${result}=    Ctn Check Poller Enabled In Database    1  10
        Should Be True    ${result}
        Ctn Stop Engine
        ${result}=    Ctn Check Poller Disabled In Database    1  10
        Should Be True    ${result}
        Ctn Kindly Stop Broker
    END

BESS_CRYPTED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only server crypted
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Add Broker tcp input grpc crypto    central    True    False
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_CRYPTED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Add Broker tcp output grpc crypto    module0    True    False
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Add Broker tcp output grpc crypto    module0    True    True
    Ctn Add Broker tcp input grpc crypto    central    True    True
    Add Host to broker input    central    central-broker-master-input    localhost
    Remove Host from broker output    module0    central-module-master-output
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        ${result}=    Ctn Check Connections
        Should Be True    ${result}
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only engine server crypted
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Add Broker tcp output grpc crypto    module0    True    True
    Add Host to broker input    central    central-broker-master-input    localhost
    Remove Host from broker output    module0    central-module-master-output
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    5s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc     central
    Ctn Change Broker Tcp Input To Grpc     rrd
    Ctn Add Broker tcp input grpc crypto    central    True    True
    Add Host to broker input    central    central-broker-master-input    localhost
    Remove Host from broker output    module0    central-module-master-output
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    5s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_ENGINE_DELETE_HOST
    [Documentation]    once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
    [Tags]    Broker    Engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention
    ${start}=    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=An Initial host state on host_1 should be raised before we can start our external commands.
    Ctn Kindly Stop Broker    True
    Ctn Start Broker    True
    engine_config_remove_service_host    ${0}    host_16
    engine_config_remove_host    ${0}    host_16
    Ctn Reload Engine
    Sleep    2s
    Ctn Kindly Stop Broker    True
    Ctn Stop Engine

BESSBQ1
    [Documentation]    A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
    [Tags]    Broker    Engine    start-stop    queue
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
    Ctn Clear Retention
    Ctn Create Bad Queue    central-broker-master.queue.central-broker-master-sql
    ${start}=    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}=    Create List    execute statement 306524174

    ${result}=    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    120
    Should Be True    ${result}    msg=Services should be updated after the ingestion of the queue file
    Ctn Stop Engine
    Ctn Kindly Stop Broker

