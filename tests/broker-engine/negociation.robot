*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed

Library    Collections

*** Test Cases ***
BESS6
    [Documentation]    Start-Stop Broker/Engine - Central and RRD Brokers and Engine
    ...    are started. Then, we check that the connection is well established between
    ...    them.
    ...    And then we use the gRPC API to check that the central broker has two peers
    ...    connected to it: the central engine and the rrd broker.
    ...    And we also check that the rrd broker knows correctly its peer that is
    ...    the central broker.
    [Tags]    broker    engine    start-stop    MON-15671
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    # While we not get two peers connected, we recheck the peers list
    ${count}        Set Variable    0
    FOR    ${idx}    IN RANGE    0    20
        ${result}    Ctn Get Peers    51001
	Log To Console    ${result}
        # We check that result contains a 'peers' key
        IF    ${result} is not None and "peers" not in ${result}
            Log To Console    No peers found in the result, let's wait a bit more
            Sleep    1s
            Continue For
        END
	Log To Console    ${result}
        ${count}    Evaluate    len(${result['peers']})
        IF    ${count} == 2
            BREAK
        END
        Sleep    1s
    END
    Should Be Equal As Integers    ${count}    2    Two peers should be connected to the central broker.

    # We define a variable to count the number of peers found
    ${count}        Set Variable    0
    FOR                ${peer}    IN    @{result['peers']}
      IF    "${peer['brokerName']}" == "central-module-master0"
        ${count}    Evaluate    ${count} + 1
        Should Be Equal As Strings    ${peer['brokerName']}    central-module-master0    Broker name should be central-module-master0
        Should Be Equal As Strings    ${peer['pollerName']}    Poller0    Poller name should be Poller0
        Should Be Equal As Integers    ${peer['id']}    1    On the poller instance, Poller id should be 1
        Should Be True    "type" in ${peer}    Poller type should be defined as ENGINE.
        Should Be Equal As Strings    ${peer['type']}    ENGINE    Poller type should be ENGINE
      ELSE IF    "${peer['brokerName']}" == "central-rrd-master"
        ${count}    Evaluate    ${count} + 1
        Should Be Equal As Strings    ${peer['brokerName']}    central-rrd-master    Broker name should be central-rrd-master
        Should Be Equal As Strings    ${peer['pollerName']}    Central    Poller name should be Central
        Should Be Equal As Integers    ${peer['id']}    1    On the central instance, Central id should be 1
        Should Be True    "type" in ${peer}    Poller type should be defined as BROKER.
        Should Be Equal As Strings    ${peer['type']}    BROKER    Central type should be BROKER
      ELSE
        Fail    Peer '${peer['brokerName']}' name should not be found
      END
    END

    Should Be Equal As Integers    ${count}    2    Two peers should be connected to the central broker.

    # While we not get two peers connected, we recheck the peers list
    FOR    ${idx}    IN RANGE    0    20
        ${result}    Ctn Get Peers    51002
	Log To Console    ${result}
        # We check that result contains a 'peers' key
        IF    "peers" in ${result}
            Log To Console    peers found in the result
            BREAK
        END
    END

    Should Be True    "peers" in ${result}    RRD cbd should know about its peers.
    Log To Console    ${result}
    Should Be Equal As Strings    ${result['peers'][0]['brokerName']}    central-broker-master    From the RRD cbd, Poller peer name should be central-broker-master
    Should Be Equal As Strings    ${result['peers'][0]['pollerName']}    Central    From the RRD cbd, peer Broker name should be Central
    Should Be Equal As Integers    ${result['peers'][0]['id']}    1    From the RRD cbd, peer id should be 1
    Should Be Equal As Strings    ${result['peers'][0]['type']}    BROKER    From the RRD cbd, peer type should be BROKER because it is the central cbd instance.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BESS7
    [Documentation]    Start-Stop Broker/Engine - Central and RRD Brokers and Engine
    ...    are started. We wait for the connections to be established.
    ...    Then we stop the central engine and check on the gRPC API of the
    ...    central broker that the central engine is not connected anymore but
    ...    the rrd broker is still connected.
    ...    Then the central engine is started again and we check that it is back.
    ...	   Then we stop the rrd broker and check on the gRPC API of the central
    ...    broker that the rrd broker is not connected anymore.
    ...    And we start the rrd broker again and check that it is back.
    [Tags]    broker    engine    start-stop    MON-15671
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}
    ...    Connection between Engine and Broker not established

    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}
    # We should have two peers connected to the central broker
    Should Be True    "peers" in ${result}
    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    2
    ...    Two peers should be connected to the central broker.

    # We stop the central Engine
    Ctn Stop Engine

    # We wait for the central engine to be disconnected
    FOR    ${idx}    IN RANGE    0    20
        ${result}    Ctn Get Peers    51001
	Log To Console    ${result}
        # We check that there is only one peer connected
        IF   len(${result['peers']}) == 1
            BREAK
        END
        Sleep    1s
    END

    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    1
    ...    Only one peer should be connected to the central broker.
    Should Be Equal As Strings    ${result['peers'][0]['type']}    BROKER
    ...    The peer type should be BROKER

    # We start the central Engine
    Ctn Start Engine

    # We wait for the central engine to be connected
    FOR    ${idx}    IN RANGE    0    20
        ${result}    Ctn Get Peers    51001
	Log To Console    ${result}
        # We check that there is two peers connected
        IF   len(${result['peers']}) == 2
            BREAK
        END
        Sleep    1s
    END

    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    2
    ...    Two peers should be connected to the central broker.
    ${lst}    Create List    ${result['peers'][0]['type']}    ${result['peers'][1]['type']}
    # Let's sort the list
    Sort List    ${lst}
    Should Be Equal As Strings   ${lst}    ['BROKER', 'ENGINE']
    ...    The peer types should be BROKER ENGINE

    # We stop the rrd Broker
    Send Signal To Process    SIGTERM    b2

    # We wait for the rrd broker to be disconnected
    FOR    ${idx}    IN RANGE    0    20
        ${result}    Ctn Get Peers    51001
	Log To Console    ${result}
        # We check that there is only one peer connected
        IF   len(${result['peers']}) == 1
            BREAK
        END
        Sleep    1s
    END

    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    1
    ...    Only one peer should be connected to the central broker.
    Should Be Equal As Strings    ${result['peers'][0]['type']}    ENGINE

    # We start the rrd Broker
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2    stdout=${BROKER_LOG}/central-rrd-stdout.log    stderr=${BROKER_LOG}/central-rrd-stderr.log

    # We wait for the rrd broker to be connected
    FOR    ${idx}    IN RANGE    0    20
        ${result}    Ctn Get Peers    51001
	Log To Console    ${result}
        # We check that there is two peers connected
        IF   len(${result['peers']}) == 2
            BREAK
        END
        Sleep    1s
    END

    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    2
    ...    Two peers should be connected to the central broker.
    ${lst}    Create List    ${result['peers'][0]['type']}    ${result['peers'][1]['type']}
    # Let's sort the list
    Sort List    ${lst}
    Should Be Equal As Strings   ${lst}    ['BROKER', 'ENGINE']
    ...    The peer types should be BROKER ENGINE

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BESS8
    [Documentation]    Start-Stop Broker/Engine - Central and RRD Brokers and Engine
    ...    are started with extended negociation.
    ...    The connection is established for the first time, so Broker doesn't know
    ...    it. So when it is time to send the Instance message, Engine sends also
    ...    an EngineConfiguration message and then waits for the EngineConfiguration
    ...    answered by the Broker.
    ...    Since this is the first time, Engine should send its configuration as usual.
    [Tags]    broker    engine    start-stop    MON-15671
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    ${content}    Create List    BBDO: engine configuration sent to peer 'central-broker-master' with version
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that Engine is sending its configuration should be available in centengine.log

    ${content}    Create List
    ...    BBDO: received engine configuration from Engine peer 'central-module-master0'
    ...    BBDO: engine configuration for 'central-module-master0' is outdated
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that Broker received the configuration from Engine should be available in central.log. And this configuration should be outdated.

    ${content}    Create List
    ...    BBDO: engine configuration from peer 'central-broker-master' received as expected
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should send a response to the EngineConfiguration.

    ${content}    Create List
    ...    init: sending poller configuration
    ...    init: beginning host dump
    ...    init: end of host dump
    ...    init: beginning service dump
    ...    init: end of services dump
    ...    init: sending initial instance configuration loading event

    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Engine should send its full configuration.

    # Once the configuration is sent, Broker must copy the cache configuration
    # from the php cache.
    ${content}    Create List
    ...    unified_sql: processing Pb instance configuration (poller 1)
    ...    unified_sql: New engine configuration, broker directories updated
    ...    Poller 1 configuration updated in '${VarRoot}/lib/centreon-broker/pollers-configuration/1'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should update its poller configuration.

    ${result}    Ctn Check Poller Enabled In Database    1    30    ${True}
    Should Be True    ${result}    Poller not visible in resources table

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BESS9
    [Documentation]    Start-Stop Broker/Engine - Central and RRD Brokers and Engine
    ...    are started with extended negociation.
    ...    The connection has already been established, so Broker knows
    ...    it. Then when it is time to send the Instance message, Engine sends also
    ...    an EngineConfiguration message and then waits for the EngineConfiguration
    ...    answered by the Broker.
    ...    Here, Broker already knows the configuration, Engine doesn't send it.
    [Tags]    broker    engine    start-stop    MON-15671
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1

    # We simulate that Broker already knows the configuration
    Remove Directory    ${VarRoot}/lib/centreon-broker/pollers-configuration    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config/pollers-configuration/1
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon-broker/pollers-configuration/1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    ${content}    Create List    BBDO: engine configuration sent to peer 'central-broker-master' with version
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that Engine is sending its configuration should be available in centengine.log

    ${content}    Create List
    ...    BBDO: received engine configuration from Engine peer 'central-module-master0'
    ...    BBDO: engine configuration for 'central-module-master0' is up to date
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that Broker received the configuration from Engine should be available in central.log. And this configuration should be up to date.

    ${content}    Create List
    ...    BBDO: engine configuration from peer 'central-broker-master' received as expected
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should send a response to the EngineConfiguration.

    ${content}    Create List
    ...    init: No need to send poller configuration
    ...    init: sending initial instance configuration loading event
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Engine should send a partial configuration.

    ${content}    Create List
    ...    init: beginning host dump
    ...    init: beginning service dump
    ${result}    Ctn Find In Log    ${engineLog0}    ${start}    ${content}
    Should Not Be True    ${result[0]}    Engine should not send its full configuration.

    # Once the configuration is sent, Broker must copy the cache configuration
    # from the php cache.
    ${content}    Create List
    ...    unified_sql: processing Pb instance configuration (poller 1)
    ...    unified_sql: Engine configuration already known by Broker
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should update its poller configuration.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BESS10
    [Documentation]    Start-Stop Broker/Engine - Central and RRD Brokers and Engine
    ...    are started with extended negociation.
    ...    A first start is made and then a second one.
    ...    Since the connection has already been established, Broker knows
    ...    it. And after the InstanceConfiguration message, Broker must
    ...    enable the poller's resources by its own.
    [Tags]    broker    engine    start-stop    MON-15671
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    # We simulate the php cache directory here
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # For the first connection, Engine still sends its configuration, so the
    # resources table is well updated.
    ${result}    Ctn Check Poller Enabled In Database    1    30    ${True}
    Should Be True    ${result}    Poller not visible in resources table (first connection)

    ${hosts_services}    Ctn Get Hosts Services Count    1    50    1000
    Should Be Equal As Strings    ${hosts_services}    (50, 1000)
    ...    Hosts and services count should be (50, 1000) initially
    Ctn Restart Engine

    # For the second connection, Engine does not send its configuration, so the
    # resources table is updated by broker alone.
    ${result}    Ctn Check Poller Enabled In Database    1    30    ${True}
    Should Be True    ${result}    Poller not visible in resources table (second connection)

    ${content}    Create List
    ...    Enabling 50 hosts in hosts table
    ...    Enabling 1000 services in services table
    ${new_hosts_services}    Ctn Get Hosts Services Count    1    50    1000
    Should Be Equal    ${hosts_services}    ${new_hosts_services}
    ...    Hosts and services count should be the same before and after Engine restart
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BENE_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first
    ...    Broker stopped first - Extended negociation enabled.
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BENE_GRPC2
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
    Ctn Config BBDO3    1    3.1.0
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    1
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}
    # We should have two peers with brokerName in [ 'central-module-master0', 'central-rrd-master' ]
    Should Be True    "peers" in ${result}
    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    2
    ...    Two peers should be connected to the central broker.
    ${content}    Create List    ${result['peers'][0]['brokerName']}    ${result['peers'][1]['brokerName']}
    ${expected}    Create List	  central-module-master0    central-rrd-master
    Lists Should Be Equal    ${content}    ${expected}    ignore_order=True

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BENE_GRPC3
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
    Ctn Config BBDO3    1    3.1.0
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    Ctn Wait For Engine To Be Ready    ${start}    1
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}
    # We should have two peers with brokerName in [ 'central-module-master0', 'central-rrd-master' ]
    Should Be True    "peers" in ${result}
    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    2
    ...    Two peers should be connected to the central broker.
    ${content}    Create List    ${result['peers'][0]['brokerName']}    ${result['peers'][1]['brokerName']}
    ${expected}    Create List	  central-module-master0    central-rrd-master
    Lists Should Be Equal    ${content}    ${expected}    ignore_order=True

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BENE_GRPC4
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
    Ctn Config BBDO3    1    3.1.0
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    Ctn Wait For Engine To Be Ready    ${start}    1
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}
    # We should have two peers with brokerName in [ 'central-module-master0', 'central-rrd-master' ]
    Should Be True    "peers" in ${result}
    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    2
    ...    Two peers should be connected to the central broker.
    ${content}    Create List    ${result['peers'][0]['brokerName']}    ${result['peers'][1]['brokerName']}
    ${expected}    Create List	  central-module-master0    central-rrd-master
    Lists Should Be Equal    ${content}    ${expected}    ignore_order=True

    Ctn Kindly Stop Broker
    Ctn Stop Engine

BENE_GRPC5
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
    Ctn Config BBDO3    1    3.1.0
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    1
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established

    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}
    # We should have two peers with brokerName in [ 'central-module-master0', 'central-rrd-master' ]
    Should Be True    "peers" in ${result}
    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    2
    ...    Two peers should be connected to the central broker.
    ${content}    Create List    ${result['peers'][0]['brokerName']}    ${result['peers'][1]['brokerName']}
    ${expected}    Create List	  central-module-master0    central-rrd-master
    Lists Should Be Equal    ${content}    ${expected}    ignore_order=True

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BENE_GRPC_COMPRESS1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Change Broker Compression Output    module0    central-module-master-output    yes
    Ctn Change Broker Compression Input    central    centreon-broker-master-input    yes
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    1

    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}
    # We should have two peers with brokerName in [ 'central-module-master0', 'central-rrd-master' ]
    Should Be True    "peers" in ${result}
    ${count}    Evaluate    len(${result['peers']})
    Should Be Equal As Integers    ${count}    2
    ...    Two peers should be connected to the central broker.
    ${content}    Create List    ${result['peers'][0]['brokerName']}    ${result['peers'][1]['brokerName']}
    ${expected}    Create List	  central-module-master0    central-rrd-master
    Lists Should Be Equal    ${content}    ${expected}    ignore_order=True

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection not established between Engine and Broker
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BENE_CRYPTED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
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
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -e /tmp/etc/centreon-engine/config0    disambiguous=True
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        Ctn Start Engine
	Ctn Wait For Engine To Be Ready    ${start}    1
        ${result}    Ctn Check Connections
        Should Be True    ${result}    Connection not established between Engine and Broker

	${result}    Ctn Get Peers    51001
	Log To Console    ${result}
	# We should have two peers with brokerName in [ 'central-module-master0', 'central-rrd-master' ]
	Should Be True    "peers" in ${result}
	${count}    Evaluate    len(${result['peers']})
	Should Be Equal As Integers    ${count}    2
	...    Two peers should be connected to the central broker.
	${content}    Create List    ${result['peers'][0]['brokerName']}    ${result['peers'][1]['brokerName']}
	${expected}    Create List	  central-module-master0    central-rrd-master
	Lists Should Be Equal    ${content}    ${expected}    ignore_order=True

        ${result}    Ctn Check Poller Enabled In Database    1    10
        Should Be True    ${result}
        Ctn Stop Engine
        ${result}    Ctn Check Poller Disabled In Database    1    10
        Should Be True    ${result}
        Ctn Kindly Stop Broker
    END
