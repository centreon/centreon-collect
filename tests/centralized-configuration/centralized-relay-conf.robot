*** Settings ***
Documentation       Tests concerning relays between central broker and pollers.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CCCRC1
    [Documentation]    Given a topology Poller1 -> Relay1 -> central cbd
    ...    When Engine connects to the relay
    ...    Then the relay sends a ConfigRequest to the central for poller 1
    ...    And the central logs the receipt of that ConfigRequest.
    [Tags]    broker    engine    relay
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files

    Ctn Config Centralized Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Relay    3    4    [{ "name": "module", "output": 5669 }, { "name": "relay", "input": 5669, "output": 5672 }, { "name": "central", "input": 5672 }]
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Broker Config Log    relay3    bbdo    debug
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Relay    3
    Ctn Start Engine    newGeneration=True

    ${content}    Create List    BBDO: received ConfigRequest from relay for poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Central must receive a ConfigRequest from the relay for poller 1

    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Stop Relay    3

CCCRC2
    [Documentation]    Given a topology Poller1 -> Relay3 -> central cbd
    ...    And a poller configuration is pre-created before starting the central broker
    ...    When the central processes the configuration and the relay sends a ConfigRequest
    ...    Then the central sends a non-unknown DiffState to the relay.
    [Tags]    broker    engine    relay
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files

    Ctn Config Centralized Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Relay    3    4    [{ "name": "module", "output": 5669 }, { "name": "relay", "input": 5669, "output": 5672 }, { "name": "central", "input": 5672 }]
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Broker Config Log    relay3    bbdo    debug
    Ctn Clear Retention
    Ctn Notify Broker Of Engine Config Change    0

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Relay    3

    ${content}    Create List    New Engine configuration for poller 1 stored
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Central must process the pre-created configuration for poller 1

    Ctn Start Engine    newGeneration=True

    ${content}    Create List    BBDO: sending DiffState to poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Central must send a non-unknown DiffState to the relay for poller 1

    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Stop Relay    3

CCCRC3
    [Documentation]    Given a topology Poller1 -> Relay3 -> central cbd
    ...    And a poller configuration is pre-created before starting the central broker
    ...    When Engine connects through the relay and the central sends a DiffState
    ...    Then the relay forwards the DiffState to Engine
    ...    And the relay forwards the DiffStateAck back to the central.
    [Tags]    broker    engine    relay
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files

    Ctn Config Centralized Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Relay    3    4    [{ "name": "module", "output": 5669 }, { "name": "relay", "input": 5669, "output": 5672 }, { "name": "central", "input": 5672 }]
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Broker Config Log    relay3    bbdo    info
    Ctn Clear Retention
    Ctn Notify Broker Of Engine Config Change    0

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Relay    3

    ${content}    Create List    New Engine configuration for poller 1 stored
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Central must process the pre-created configuration for poller 1

    Ctn Start Engine    newGeneration=True

    ${content}    Create List    BBDO: received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Central must receive a DiffStateAck from the relay for poller 1

    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Stop Relay    3

CCCRC4
    [Documentation]    Given a topology Poller1 -> Relay3 -> central cbd
    ...    And a poller configuration is pre-created before starting central
    ...    When Engine connects and gets the initial config via relay
    ...    And PHP pushes a new config for poller 1 (5 extra hosts)
    ...    Then the central sends a new DiffState to the relay
    ...    And the central receives a new DiffStateAck.
    [Tags]    broker    engine    relay
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files

    Ctn Config Centralized Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Relay    3    4    [{ "name": "module", "output": 5669 }, { "name": "relay", "input": 5669, "output": 5672 }, { "name": "central", "input": 5672 }]
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    rrd    bbdo    debug
    Ctn Broker Config Log    relay3    bbdo    info
    Ctn Clear Retention
    Ctn Notify Broker Of Engine Config Change    0

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Relay    3
    Ctn Start Engine    newGeneration=True

    ${content}    Create List    BBDO: received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Central must receive the initial DiffStateAck from relay for poller 1

    Ctn Prepare Engine Config    ${1}    ${25}    ${20}
    ${start2}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0

    ${content2}    Create List    BBDO: sending DiffState to poller 1
    ${result2}    Ctn Find In Log With Timeout    ${centralLog}    ${start2}    ${content2}    30
    Should Be True    ${result2}    Central must send a new DiffState to the relay for poller 1

    ${content3}    Create List    BBDO: received diff state ack from poller 1
    ${result3}    Ctn Find In Log With Timeout    ${centralLog}    ${start2}    ${content3}    60
    Should Be True    ${result3}    Central must receive a new DiffStateAck from relay for poller 1

    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Stop Relay    3

CCCRC5
    [Documentation]    Given Engine initially connected to central via Relay3 (poller_id=4)
    ...    When Engine migrates to Relay4 (poller_id=5)
    ...    Then the central sends ConfigRevoke to Relay3
    ...    And serves the configuration to Engine via Relay4.
    [Tags]    broker    engine    relay
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files

    Ctn Config Centralized Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Relay    3    4    [{ "name": "module", "output": 5669 }, { "name": "relay3", "input": 5669, "output": 5672 }, { "name": "central", "input": 5672 }]
    Ctn Config Relay    4    5    [{ "name": "relay4", "input": 5670, "output": 5672 }]
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    relay3    bbdo    info
    Ctn Broker Config Log    relay4    bbdo    info
    Ctn Clear Retention
    Ctn Notify Broker Of Engine Config Change    0

    ${relay3Log}    Set Variable    ${VarRoot}/log/centreon-broker/relay-broker-3.log

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Relay    3

    ${content_init}    Create List    New Engine configuration for poller 1 stored
    ${result_init}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content_init}    30
    Should Be True    ${result_init}    Central must process the pre-created configuration for poller 1

    Ctn Start Engine    newGeneration=True

    ${content}    Create List    BBDO: received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Central must receive the initial DiffStateAck from Relay3

    Ctn Stop Engine
    Ctn Broker Config Output Set    module0    central-module-master-output    port    5670
    Ctn Start Relay    4
    ${start2}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True

    ${content2}    Create List    BBDO: received ConfigRevoke for poller 1
    ${result2}    Ctn Find In Log With Timeout    ${relay3Log}    ${start2}    ${content2}    30
    Should Be True    ${result2}    Relay3 must receive ConfigRevoke for poller 1

    ${content3}    Create List    BBDO: received diff state ack from poller 1
    ${result3}    Ctn Find In Log With Timeout    ${centralLog}    ${start2}    ${content3}    60
    Should Be True    ${result3}    Central must receive DiffStateAck via Relay4

    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Stop Relay    3
    Ctn Stop Relay    4

CCCRC6
    [Documentation]    Given Engine connected via Relay3 with initial config established
    ...    When the central is stopped cleanly and a new config is pushed during the outage
    ...    Then after the central restarts, the relay reconnects and the new DiffState
    ...    is forwarded to Engine via the relay, and central receives a new DiffStateAck.
    [Tags]    broker    engine    relay
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files

    Ctn Config Centralized Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Relay    3    4    [{ "name": "module", "output": 5669 }, { "name": "relay", "input": 5669, "output": 5672 }, { "name": "central", "input": 5672 }]
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    relay3    bbdo    info
    Ctn Clear Retention
    Ctn Notify Broker Of Engine Config Change    0

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Relay    3
    Ctn Start Engine    newGeneration=True

    ${content}    Create List    BBDO: received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Central must receive the initial DiffStateAck from Relay3

    Ctn Stop Engine
    Ctn Kindly Stop Broker

    Ctn Prepare Engine Config    ${1}    ${25}    ${20}
    Ctn Notify Broker Of Engine Config Change    0

    ${start2}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${content2}    Create List    BBDO: received diff state ack from poller 1
    ${result2}    Ctn Find In Log With Timeout    ${centralLog}    ${start2}    ${content2}    90
    Should Be True    ${result2}    Central must receive a DiffStateAck after restart with topology.cache routing

    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Stop Relay    3

CCCRC7
    [Documentation]    Given Engine connected via Relay3 with initial config established
    ...    When GetTopology is called on the central gRPC endpoint
    ...    Then the response contains Relay3 as a direct broker with poller 1 as its poller.
    [Tags]    broker    engine    relay
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files

    Ctn Config Centralized Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Relay    3    4    [{ "name": "module", "output": 5669 }, { "name": "relay", "input": 5669, "output": 5672 }, { "name": "central", "input": 5672 }]
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    config    info
    Ctn Clear Retention
    Ctn Notify Broker Of Engine Config Change    0

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Relay    3
    Ctn Start Engine    newGeneration=True

    ${content}    Create List    BBDO: received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Central must receive the initial DiffStateAck from Relay3

    @{engine_ids}    Create List    ${1}
    ${topo_ok}    Ctn Check Broker Topology    ${4}    ${engine_ids}
    Should Be True    ${topo_ok}    GetTopology must show relay3 (poller_id=4) with poller 1 behind it

    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Stop Relay    3
