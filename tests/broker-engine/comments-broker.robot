*** Settings ***
Documentation       User comments via Broker gRPC (notification_mode = broker).
...                 Counterpart of comments.robot with Broker as the comment authority:
...                 the comments are created and deleted by Broker itself, Engine is
...                 never involved.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
BECMTBRK1
    [Documentation]    Scenario: service comment added and deleted by id through Broker
    ...    Given a BBDO3 platform with notification_mode=broker
    ...    When a service comment is added through the Broker AddServiceComment RPC
    ...    Then an active user comment appears in the comments table with the returned internal_id
    ...    When the comment is deleted through the Broker DeleteComment RPC
    ...    Then the row gets a deletion_time
    [Tags]    broker    engine    services    comments    broker_comments
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    core    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    ${id}    Ctn Broker Add Service Comment    host_1    service_1    comment=a service comment from Broker
    Should Be True    ${id} > 0    Broker should return the internal_id of the new comment
    ${com_id}    Ctn Check Comment    host_1    service_1    ${1}    ${start}    True    30
    Should Be True    ${com_id} == ${id}    The active user comment should carry the returned internal_id (${id}), got ${com_id}

    ${err}    Ctn Broker Delete Comment    ${id}
    Should Be Empty    ${err}    DeleteComment should succeed on a Broker comment
    ${result}    Ctn Check Comment Is Deleted    ${id}    30
    Should Be True    ${result}    Comment ${id} should have a deletion_time.

BECMTBRK2
    [Documentation]    Scenario: all comments of a host and of a service deleted through Broker
    ...    Given a BBDO3 platform with notification_mode=broker
    ...    And two host comments and two service comments added through Broker
    ...    When DeleteAllHostComments is called for the host
    ...    Then the host has no active comment left and the service still has two
    ...    When DeleteAllServiceComments is called for the service
    ...    Then the service has no active comment left
    [Tags]    broker    engine    hosts    services    comments    broker_comments
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    core    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    Ctn Broker Add Host Comment    host_1    comment=first host comment
    Ctn Broker Add Host Comment    host_1    persistent=${True}    comment=second host comment
    Ctn Broker Add Service Comment    host_1    service_1    comment=first service comment
    Ctn Broker Add Service Comment    host_1    service_1    persistent=${True}    comment=second service comment
    ${result}    Ctn Check Active Comments Count    host_1    ${EMPTY}    2    30
    Should Be True    ${result}    host_1 should have 2 active comments
    ${result}    Ctn Check Active Comments Count    host_1    service_1    2    30
    Should Be True    ${result}    service_1 should have 2 active comments

    Ctn Broker Delete All Host Comments    host_1
    ${result}    Ctn Check Active Comments Count    host_1    ${EMPTY}    0    30
    Should Be True    ${result}    host_1 should have no active comment left
    ${result}    Ctn Check Active Comments Count    host_1    service_1    2    5
    Should Be True    ${result}    the service comments must not be touched by the host bulk deletion

    Ctn Broker Delete All Service Comments    host_1    service_1
    ${result}    Ctn Check Active Comments Count    host_1    service_1    0    30
    Should Be True    ${result}    service_1 should have no active comment left

BECMTBRK3
    [Documentation]    Scenario: a comment minted by a poller cannot be deleted through Broker
    ...    Given a BBDO3 platform with notification_mode=broker
    ...    And a service comment added through the Engine external command (poller-minted id)
    ...    When DeleteComment is called on Broker with that id
    ...    Then Broker refuses with FAILED_PRECONDITION and the comment stays active
    [Tags]    broker    engine    services    comments    broker_comments
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    core    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    Ctn Add Svc Comment    host_1    service_1    1    user    a comment minted by the poller
    ${com_id}    Ctn Check Comment    host_1    service_1    ${1}    ${start}    True    30
    Should Be True    ${com_id} > 0    No active service comment was created by Engine.

    ${err}    Ctn Broker Delete Comment    ${com_id}
    Should Be Equal    ${err}    FAILED_PRECONDITION    Broker must refuse to delete a poller-minted comment
    Sleep    3s
    ${result}    Ctn Check Active Comments Count    host_1    service_1    1    5
    Should Be True    ${result}    the poller-minted comment must still be active


*** Keywords ***
Ctn Wait For Broker Acknowledgement Manager
    [Documentation]    Wait until Broker has loaded its notification_mode=broker
    ...    services (the acknowledgement manager is the last one loaded), so the
    ...    Broker gRPC comment endpoints are usable.
    [Arguments]    ${start}    ${timeout}=${60}
    ${content}    Create List    acknowledgement management enabled, acknowledgement manager loaded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    ${timeout}
    Should Be True    ${result}    Broker did not enable notification_mode=broker services in time
