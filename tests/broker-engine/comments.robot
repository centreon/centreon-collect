*** Settings ***
Documentation       Centreon Engine and Broker comments lifecycle (creation, deletion, bulk, flapping, downtime, retention)

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Comments Test Setup
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
BECMT_DEL_SVC
    [Documentation]    Scenario: deleting a service comment by id
    ...    Given a service comment has been added by external command
    ...    When a DEL_SVC_COMMENT external command is sent with its internal_id
    ...    Then the matching row in the "comments" table gets a deletion_time
    [Tags]    broker    engine    services    extcmd    comments
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Clear Logs

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Add Svc Comment    host_1    service_1    1    user    a persistent service comment
    ${com_id}    Ctn Check Comment    host_1    service_1    ${1}    ${start}    True    30
    Should Be True    ${com_id}>0    No active service comment was created.

    Ctn Del Svc Comment    ${com_id}
    ${result}    Ctn Check Comment Is Deleted    ${com_id}    30
    Should Be True    ${result}    Service comment ${com_id} should have a deletion_time.

BECMT_DEL_ALL
    [Documentation]    Scenario: bulk deletion of comments
    ...    Given several comments on a host and on a service
    ...    When DEL_ALL_HOST_COMMENTS / DEL_ALL_SVC_COMMENTS are sent
    ...    Then every matching active comment gets a deletion_time
    [Tags]    broker    engine    extcmd    comments
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Clear Logs

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Add Host Comment    host_1    1    user    host comment 1
    Ctn Add Host Comment    host_1    1    user    host comment 2
    Ctn Add Svc Comment    host_1    service_1    1    user    svc comment 1
    Ctn Add Svc Comment    host_1    service_1    1    user    svc comment 2

    ${result}    Ctn Check Active Comments Count    host_1    ${EMPTY}    ${2}    30
    Should Be True    ${result}    Two active host comments were expected.
    ${result}    Ctn Check Active Comments Count    host_1    service_1    ${2}    30
    Should Be True    ${result}    Two active service comments were expected.

    Ctn Del All Host Comments    host_1
    ${result}    Ctn Check Active Comments Count    host_1    ${EMPTY}    ${0}    30
    Should Be True    ${result}    All host comments should be deleted.

    Ctn Del All Svc Comments    host_1    service_1
    ${result}    Ctn Check Active Comments Count    host_1    service_1    ${0}    30
    Should Be True    ${result}    All service comments should be deleted.

BECMT_FLAPPING
    [Documentation]    Scenario: a flapping service owns a comment
    ...    Given flap detection is enabled on a service
    ...    When the service flaps
    ...    Then a flapping comment is created in the "comments" table
    ...    When the service state stabilizes and flapping stops
    ...    Then the flapping comment gets a deletion_time
    [Tags]    broker    engine    services    comments    flapping
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Engine Config Set Value    0    enable_flap_detection    1
    Ctn Set Services Passive    ${0}    service_1
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    low_flap_threshold    10
    Ctn Engine Config Set Value In Services    0    service_1    high_flap_threshold    20
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_options    all
    Ctn Clear Retention
    Ctn Clear Logs

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # generate flapping
    FOR    ${index}    IN RANGE    21
        Ctn Process Service Result Hard    host_1    service_1    2    flapping
        Ctn Process Service Check Result    host_1    service_1    0    flapping
        Sleep    1s
    END

    ${result}    Ctn Check Service Flapping    host_1    service_1    30    5    50
    Should Be True    ${result}    The service (host_1,service_1) is not flapping as expected.

    ${com_id}    Ctn Check Comment    host_1    service_1    ${3}    ${start}    True    30
    Should Be True    ${com_id}>0    No flapping comment was created for the service.

    # stabilize the service so that flapping stops
    FOR    ${index}    IN RANGE    25
        Ctn Process Service Check Result    host_1    service_1    0    stable
        Sleep    1s
    END

    ${result}    Ctn Check Comment Is Deleted    ${com_id}    60
    Should Be True    ${result}    The flapping comment ${com_id} should be deleted once flapping stops.

BECMT_DOWNTIME
    [Documentation]    Scenario: a downtime owns a comment
    ...    Given a fixed downtime is scheduled on a service
    ...    Then a downtime comment is created in the "comments" table
    ...    When the downtime is deleted
    ...    Then the downtime comment gets a deletion_time
    [Tags]    broker    engine    services    comments    downtime
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Clear Logs

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600
    ${com_id}    Ctn Check Comment    host_1    service_1    ${2}    ${start}    True    30
    Should Be True    ${com_id}>0    No downtime comment was created for the service.

    Ctn Delete Service Downtime Full    ${0}    host_1    service_1
    ${result}    Ctn Check Comment Is Deleted    ${com_id}    30
    Should Be True    ${result}    The downtime comment ${com_id} should be deleted with its downtime.

BECMT_RETENTION
    [Documentation]    Scenario: a persistent comment survives an Engine restart
    ...    Given a persistent host comment
    ...    When Engine is restarted (retention preserved)
    ...    Then the comment is still active in the "comments" table
    [Tags]    broker    engine    comments    retention
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Clear Logs

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # a persistent host comment
    Ctn Add Host Comment    host_1    1    user    persistent comment surviving restart
    ${com_id}    Ctn Check Comment    host_1    ${EMPTY}    ${1}    ${start}    True    30
    Should Be True    ${com_id}>0    No persistent host comment was created.

    # restart Engine, keeping retention
    Ctn Stop Engine
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # the persistent comment must still be active after the restart
    ${result}    Ctn Check Comment Is Deleted    ${com_id}    5
    Should Be True    not ${result}    The persistent host comment should survive the restart.


BECMT_RETENTION_ACK
    [Documentation]    Scenario: an acknowledgement comment is still deletable after a restart
    ...    Given a service is acknowledged (a non-persistent ack comment is created)
    ...    When Engine is restarted (retention preserved)
    ...    And the service goes back to OK so the acknowledgement is cleared
    ...    Then the ack comment is deleted, proving its id survived the restart on the notifier
    [Tags]    broker    engine    comments    retention
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Clear Logs

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    2    (1;1) is critical
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD.
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Comment    host_1    service_1    ${4}    ${start}    True    30
    Should Be True    ${ack_id}>0    No acknowledgement comment was created.

    # restart Engine, keeping retention (the ack comment id must be restored)
    Ctn Stop Engine
    Remove File    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # explicitly remove the acknowledgement -> the ack comment must be deleted,
    # which only works if its id was restored on the notifier from retention.
    Ctn Remove Service Acknowledgement    host_1    service_1
    ${result}    Ctn Check Comment Is Deleted    ${ack_id}    30
    Should Be True    ${result}    The ack comment ${ack_id} should be deleted (its id must survive the restart).


*** Keywords ***
Ctn Comments Test Setup
    [Documentation]    Stop all processes and empty the comments table for a clean slate.
    Ctn Stop Processes
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM comments
    Disconnect From Database
