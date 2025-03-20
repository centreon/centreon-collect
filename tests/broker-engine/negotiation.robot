*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests with new negotiation

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed

Library    Collections

*** Test Cases ***
BESS6
    [Documentation]    Scenario: Verify brokers and engine start and establish connections
    ...    Given the Central Broker, RRD Broker, and Central Engine are started
    ...    When we check the connection between them
    ...    Then the connection should be well established
    ...    And the central broker should have two peers connected: the central engine and the RRD broker
    ...    And the RRD broker should correctly recognize its peer as the central broker
    [Tags]    broker    engine    start-stop    MON-153802
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


BEDW
    [Documentation]    Scenario: Verify Broker configured with cache_config_directory listens to it
    ...    Given the Central Broker is started with cache_config_directory set to a specific Directory
    ...    And the pollers_config_directory is set to its default value: /var/lib/centreon-broker/pollers-configuration.
    ...    When a file of the form <poller_id>.lck is created in the cache_config_directory
    ...    Then Broker logs a message telling the file has been created
    ...    When the corresponding configuration directory doesn't exist
    ...    Then Broker logs a message telling the directory doesn't exist
    [Tags]    broker    engine    MON-153802
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine    ${True}

    ${content}    Create List    Watching for changes in '${VarRoot}/lib/centreon/config'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should log a message when watching for changes in the cache_config_directory

    # We create a file in the cache_config_directory
    Create File    ${VarRoot}/lib/centreon/config/18.lck

    ${content}    Create List    New Engine configuration available, change in '18.lck' detected
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should log a message when a new file of the form <poller_id>.lck is created in the cache_config_directory

    ${content}    Create List    Cannot compute the Engine configuration version for poller '18': No such file or directory
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should raise an error since only the file 18.lck has been created without the corresponding directory
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEDWEN
    [Documentation]    Scenario: Verify Broker configured with cache_config_directory listens to it
    ...    Given the Central Broker is started with cache_config_directory set to a specific Directory
    ...    And the pollers_config_directory is set to /var/lib/centreon-broker/pollers-config.
    ...    When a file of the form <poller_id>.lck is created in the cache_config_directory
    ...    Then Broker logs a message telling the file has been created
    ...    When the corresponding configuration directory doesn't exist
    ...    Then Broker logs a message telling the directory doesn't exist
    [Tags]    broker    engine    MON-153802
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine    ${True}

    ${content}    Create List    Watching for changes in '${VarRoot}/lib/centreon/config'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should log a message when watching for changes in the cache_config_directory

    # We create a file in the cache_config_directory
    Create File    ${VarRoot}/lib/centreon/config/18.lck

    ${content}    Create List    New Engine configuration available, change in '18.lck' detected
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should log a message when a new file of the form <poller_id>.lck is created in the cache_config_directory

    ${content}    Create List    Cannot compute the Engine configuration version for poller '18': No such file or directory
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker raises an error since only the file 18.lck has been created without the corresponding directory
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEDWENF
    [Documentation]    Scenario: Verify Broker configured with cache_config_directory creates the protobuf serialized configuration
    ...    Given the Central Broker is started with cache_config_directory set to a specific Directory
    ...    And the pollers_config_directory is set to /var/lib/centreon-broker/pollers-config.
    ...    When a file of the form <poller_id>.lck is created after the <poller_id> directory is filled correctly
    ...    Then Broker logs a message telling the file has been created
    ...    And Broker dumps a file <poller_id>.prot if the pollers_conf directory
    [Tags]    broker    engine    MON-153802
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Remove Directory    ${VarRoot}/lib/centreon-broker/pollers-configuration    recursive=${True}
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine    ${True}

    ${content}    Create List    Watching for changes in '${VarRoot}/lib/centreon/config'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should log a message when watching for changes in the cache_config_directory

    # We fill the configuration directory with directory "1" in the cache_config_directory
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1
    # We create a file in the cache_config_directory
    Log To Console    File ${VarRoot}/lib/centreon/config/1.lck created
    Create File    ${VarRoot}/lib/centreon/config/1.lck

    Log To Console    Broker should detect the new Engine configuration
    ${content}    Create List    New Engine configuration available, change in '1.lck' detected
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should log a message when a new file of the form <poller_id>.lck is created in the cache_config_directory

    Wait Until Created    ${VarRoot}/lib/centreon-broker/pollers-configuration/1.prot    timeout=30s
    Should Not Exist    ${VarRoot}/lib/centreon-broker/pollers-configuration/diff-1.prot    File 1.prot should not exist
    Should Not Exist    ${VarRoot}/lib/centreon-broker/pollers-configuration/new-1.prot    File 1.prot should not exist
    ${content}    Create List    Reloading from Broker    Starting to reload configuration
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Engine should log about the new configuration

    Ctn Stop Engine
    Ctn Kindly Stop Broker
