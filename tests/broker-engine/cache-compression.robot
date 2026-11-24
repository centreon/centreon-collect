*** Settings ***
Documentation     Compression of the files Broker writes (cache_compression parameter): its
...               cache file and the configuration files of the pollers. They are compressed
...               with zstd by default and read back from their header, whatever the parameter
...               is: a plain file, as written by the previous versions, is still read.

Resource          ../resources/import.resource

Suite Setup       Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup        Ctn Stop Processes
Test Teardown     Ctn Save Logs If Failed


*** Test Cases ***
BECACHECMP1
    [Documentation]    Scenario: Broker writes a plain cache file when cache_compression is disabled
    ...    Given Broker is configured with cache_compression set to false
    ...    And a DOWN HARD host acknowledged through Broker
    ...    When Broker is restarted
    ...    Then the cache file it wrote is a plain BrokerCache message
    ...    And the acknowledgement has survived the restart
    [Tags]    broker    engine    cache    acknowledgement
    Ctn Config Acknowledged Host Platform
    Ctn Broker Config Add Item    central    cache_compression    ${False}
    Ctn Start And Acknowledge Host 1

    Ctn Restart Broker And Wait
    ${format}    Ctn Get Broker Cache File Format
    Should Be Equal    ${format}    plain    The cache file should not be compressed
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    0    51001    60
    Should Be True    ${result}    The acknowledgement should survive the Broker restart

    Ctn Kindly Stop Broker
    Ctn Stop Engine

BECACHECMP2
    [Documentation]    Scenario: a plain cache file is read by a Broker compressing its cache, and conversely
    ...    Given Broker is configured with cache_compression set to false
    ...    And a DOWN HARD host acknowledged through Broker
    ...    And Broker is stopped, leaving a plain cache file as the previous versions write it
    ...    When Broker is started with the default configuration, which compresses the cache
    ...    Then it reads the plain cache file and the acknowledgement is still there
    ...    When Broker is stopped
    ...    Then the cache file is compressed with zstd
    ...    When Broker is started with cache_compression set to false
    ...    Then it reads the compressed cache file and the acknowledgement is still there
    ...    When Broker is stopped
    ...    Then the cache file is plain again
    [Tags]    broker    engine    cache    acknowledgement
    Ctn Config Acknowledged Host Platform
    Ctn Broker Config Add Item    central    cache_compression    ${False}
    Ctn Start And Acknowledge Host 1

    Ctn Kindly Stop Broker
    ${format}    Ctn Get Broker Cache File Format
    Should Be Equal    ${format}    plain    The cache file should not be compressed

    # Default configuration: compression enabled.
    Ctn Broker Config Remove Item    central    cache_compression
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Wait For Broker Acknowledgement Manager    ${start}
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    0    51001    60
    Should Be True    ${result}    The acknowledgement should be read from the plain cache file
    Ctn Kindly Stop Broker
    ${format}    Ctn Get Broker Cache File Format
    Should Be Equal    ${format}    zstd    The cache file should be compressed by default

    Ctn Broker Config Add Item    central    cache_compression    ${False}
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Wait For Broker Acknowledgement Manager    ${start}
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    0    51001    60
    Should Be True    ${result}    The acknowledgement should be read from the compressed cache file
    Ctn Kindly Stop Broker
    ${format}    Ctn Get Broker Cache File Format
    Should Be Equal    ${format}    plain    The cache file should not be compressed any more

    Ctn Stop Engine

BECACHECMP3
    [Documentation]    Scenario: the poller configuration files of Broker follow cache_compression
    ...    Given a centralized configuration with one poller
    ...    And Broker configured with cache_compression set to false
    ...    When the configuration is pushed and acknowledged
    ...    Then 1.prot is a plain file
    ...    When Broker is restarted with the default configuration, which compresses
    ...    And a changed configuration is pushed
    ...    Then the plain 1.prot is read to compute the difference, and the poller acknowledges it
    ...    And 1.prot is now compressed with zstd
    ...    And it holds the configuration the poller runs, as its state.prot does.
    [Tags]    broker    engine    cache    config    centralized
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    cache_compression    ${False}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Logs
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Push Configuration And Wait    ${start}    ${0}

    VAR    ${dir}    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration
    ${format}    Ctn Get Proto File Format    ${dir}/1.prot
    Should Be Equal    ${format}    plain    1.prot should not be compressed

    # Default configuration: compression enabled.
    Ctn Kindly Stop Broker
    Ctn Broker Config Remove Item    central    cache_compression
    Ctn Start Broker    newGeneration=True
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    7
    ${start}    Ctn Get Round Current Date
    Ctn Push Configuration And Wait    ${start}    ${0}

    ${format}    Ctn Get Proto File Format    ${dir}/1.prot
    Should Be Equal    ${format}    zstd    1.prot should be compressed by default
    ${result}    Ctn Check State Configurations Are Equal
    ...    ${dir}/1.prot
    ...    ${VarRoot}/lib/centreon-engine/config0/state.prot
    Should Be True    ${result}    1.prot should hold the configuration the poller runs

    Ctn Stop Engine
    Ctn Kindly Stop Broker


*** Keywords ***
Ctn Config Acknowledged Host Platform
    [Documentation]    Configure one poller in BBDO3 with Broker as the acknowledgement
    ...    authority, host_1 passive, and no cache left by a previous test.
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    cache    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Set Hosts Passive    ${0}    host_1
    Ctn Clear Retention
    Ctn Clear Broker Cache
    Ctn Clear Logs

Ctn Start And Acknowledge Host 1
    [Documentation]    Start Broker and Engine, put host_1 DOWN HARD and acknowledge it
    ...    through Broker.
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    FOR    ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 is DOWN
    END
    ${result}    Ctn Check Host Status    host_1    1    1    ${True}    60
    Should Be True    ${result}    host_1 should be DOWN HARD

    Ctn Broker Acknowledge Host Problem    host_1    sticky=${True}
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    0    51001    30
    Should Be True    ${result}    The host acknowledgement should be in the Broker cache

Ctn Restart Broker And Wait
    [Documentation]    Stop Broker cleanly, so that it writes its cache file, and start it again.
    ${start}    Get Current Date
    Ctn Kindly Stop Broker
    Ctn Start Broker
    Ctn Wait For Broker Acknowledgement Manager    ${start}

Ctn Wait For Broker Acknowledgement Manager
    [Documentation]    Wait until Broker has loaded its acknowledgement manager
    ...    (notification_mode=broker) so the gRPC acknowledgement endpoints are
    ...    usable.
    [Arguments]    ${start}    ${timeout}=${60}
    ${content}    Create List    acknowledgement management enabled, acknowledgement manager loaded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    ${timeout}
    Should Be True    ${result}    Broker did not enable acknowledgement management (notification_mode=broker) in time
