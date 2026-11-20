*** Settings ***
Documentation       Centreon Broker start/stop tests with bbdo_server and bbdo_client input/output streams in centralized configuration. Only these streams are used instead of grpc and tcp.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BSCSSK1
    [Documentation]    Scenario: Client uses tcp but server expects grpc - connection fails
    ...    Given central broker is configured with a bbdo_server input using tcp on port 5669
    ...    And central broker is configured with a bbdo_client output using tcp to port 5670
    ...    And rrd broker is configured with a bbdo_server input using grpc on port 5670
    ...    When both brokers are started in new generation mode
    ...    Then an error is raised on the client side about corrupted data
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    grpc
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Log    central    tcp    debug
    Ctn Broker Config Log    rrd    grpc    debug
    Ctn Broker Config Log    rrd    tcp    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    rrd    core    error
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True

    # Client cannot connect. It returns an error
    ${content}    Create List    peer tcp://localhost:5670 is sending corrupted data
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the bad connection.

    Ctn Kindly Stop Broker

CBSCSSK2
    [Documentation]    Scenario: Client uses grpc but server expects tcp - connection fails
    ...    Given central broker is configured with a bbdo_server input using grpc on port 5669
    ...    And central broker is configured with a bbdo_client output using grpc to port 5670
    ...    And rrd broker is configured with a bbdo_server input using tcp on port 5670
    ...    When both brokers are started in new generation mode
    ...    Then an error is raised on the client side about invalid protocol header
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    grpc    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    tcp
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Log    central    tcp    debug
    Ctn Broker Config Log    rrd    grpc    debug
    Ctn Broker Config Log    rrd    tcp    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    rrd    core    error
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True

    # Client cannot connect. It returns an error
    ${content}    Create List
    ...    BBDO: invalid protocol header, aborting connection: waiting for message of type 'pb_welcome' but nothing received
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the bad connection.

    Ctn Kindly Stop Broker
