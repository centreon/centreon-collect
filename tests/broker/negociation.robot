*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BBDO_300_310
    [Documentation]    Given a central configured in bbdo 3.0.0 and a rrd cbd configured in 3.1.0, rrd cbd should operate in bbdo 3.0.0
    ...    Then we wait event of type 131074 (bbdo2 ack)
    [Tags]    broker    MON-182381
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module

    Ctn Config BBDO3    nbEngine=1    version=3.0.0

    Ctn Broker Config Add Item    rrd    bbdo_version    3.1.0
    Ctn Broker Config Add Item    central    event_queue_max_size    ${10}
    Ctn Broker Config Add Item    rrd    event_queue_max_size    ${10}
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Log    rrd    bbdo    info

    Ctn Clear Retention
    
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    ${pid1}    Get Process Id    b1
    ${pid2}    Get Process Id    b2
    ${result}    Ctn Check Connection    5670    ${pid1}    ${pid2}
    Should Be True    ${result}    The connection between cbd central and rrd is not established.

    ${content}    Create List    bbdo version downgraded to 3.0.0
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    30
    Should Be True    ${result}    No bbdo downgraded message in rrd logs.

    ${content}    Create List    unserialized 20 bytes for event of type 131074
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No bbdo2 ack received.

    [Teardown]    Ctn Stop Engine Broker And Save Logs

