*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Run Keywords    Ctn Stop Processes    AND    Ctn Clear Retention
Test Teardown    Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BEACK2
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service.
    ...                An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is
    ...                then updated with this acknowledgement. The service is newly set to OK.
    ...                And the acknowledgement in database is deleted.
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # The service command is set to CRITICAL to also control active checks
    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Ctn Check Service Resource Status With Timeout
    ...    host_1    service_1
    ...    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Acknowledgement With Timeout
    ...    host_1    service_1
    ...    ${d}    2    600    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    # The service command is set to OK to also control active checks
    Ctn Set Command Status    ${cmd_id}    ${0}

    # Service_1 is set back to OK.
    Ctn Process Service Result Hard    host_1    service_1    0    (1;1) is OK
    ${result}    Ctn Check Service Resource Status With Timeout
    ...    host_1    service_1
    ...    ${0}    60    HARD
    Should Be True    ${result}    Service (1;1) should be OK HARD

    # Acknowledgement is deleted but to see this we have to check in the comments table
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

BEACK4
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service. An external command is sent to
    ...                acknowledge it. The centreon_storage.acknowledgements table is then updated with this
    ...                acknowledgement. The acknowledgement is removed and the comment in the comments table has its
    ...                deletion_time column updated.
    [Tags]    broker    engine    services    extcmd    MON-150015
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # The service command is set to CRITICAL to also control active checks
    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard
    ...    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Ctn Check Service Resource Status With Timeout
    ...    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Acknowledgement With Timeout
    ...    host_1    service_1    ${d}    2    600    HARD
    Should Be True
    ...    ${ack_id} > 0
    ...    No acknowledgement on service (1, 1).

    Ctn Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables
    # have the deletion_time column filled
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout
    ...    ${ack_id}    30    BOTH
    Should Be True
    ...    ${result}
    ...    Acknowledgement ${ack_id} should be deleted.

BEACK6
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service. An external command is sent to
    ...    acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is
    ...    then updated with this acknowledgement. The service is newly set to WARNING.
    ...    And the acknowledgement in database is still there.
    [Tags]    broker    engine    services    extcmd    MON-150015
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    trace

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # The service command is set to CRITICAL to also control active checks
    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1    STICKY
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).
    Log To Console    Acknowledgement ${ack_id} on service (1, 1).

    # The service command is set to WARNING to also control active checks
    Ctn Set Command Status    ${cmd_id}    ${1}

    # Service_1 is set to WARNING.
    Ctn Process Service Result Hard    host_1    service_1    1    (1;1) is WARNING
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${1}    60    HARD
    Should Be True    ${result}    Service (1;1) should be WARNING HARD

    # Acknowledgement is not deleted.
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    10
    Should Not Be True    ${result}    Acknowledgement ${ack_id} should not be deleted.

    Ctn Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements
    # tables have the deletion_time column filled.
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

BEACK8
    [Documentation]    Engine has a critical service. It is configured with BBDO 3.
    ...                An external command is sent to acknowledge it ; the acknowledgement is normal.
    ...                The centreon_storage.acknowledgements table is then updated with this acknowledgement.
    ...                The service is newly set to WARNING.
    ...                And the acknowledgement in database is removed (not sticky).
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    module0    neb    trace
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    sql    debug
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_external_command    trace
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    # Ctn Clear Acknowledgements
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # The service command is set to CRITICAL to also control active checks
    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    Service (1;1) is critical HARD
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    ${d}    Ctn Get Round Current Date
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No normal acknowledgement on service (1, 1).
    Log To Console    Normal Acknowledgement ${ack_id} on service (1, 1).

    # Service_1 is set to WARNING.
    # This is for the check command in case of an active check
    Ctn Set Command Status    ${cmd_id}    1
    Ctn Process Service Result Hard    host_1    service_1    1    Service (1;1) is WARNING HARD
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${1}    60    HARD
    Should Be True    ${result}    Service (1;1) should be WARNING HARD

    # Acknowledgement is deleted.
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    60
    Should Be True    ${result}    Normal Acknowledgement ${ack_id} should be deleted.

    Ctn Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements
    # tables have the deletion_time column filled
    ${d}    Get Current Date
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    40
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.


BEACK9
    [Documentation]    Scenario: the Broker cache exposes acknowledgements through gRPC.
    ...                Given a BBDO3 configuration (unified_sql, so the cache is enabled)
    ...                When a critical service is acknowledged
    ...                Then the GetAcknowledgements gRPC endpoint lists it in the Broker cache
    ...                When the service recovers
    ...                Then the acknowledgement leaves the Broker cache (and is not re-ingested).
    [Tags]    broker    engine    services    extcmd    grpc    cache
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Broker Cache

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${cmd_id}    Ctn Get Service Command Id    ${10}
    Ctn Set Command Status    ${cmd_id}    ${2}
    Ctn Process Service Result Hard    host_1    service_10    ${2}    (1;10) is critical
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_10    ${2}    60    HARD
    Should Be True    ${result}    Service (1;10) should be critical HARD

    ${d}    Ctn Get Round Current Date
    Ctn Acknowledge Service Problem    host_1    service_10
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_10    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 10).

    # The acknowledgement is visible in the Broker cache via gRPC.
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    ${1}    ${10}    ${51001}    30
    Should Be True    ${result}    Acknowledgement (1;10) should be in the Broker cache.
    ${result}    Ctn Check Acknowledgements Count With Timeout    ${1}    ${51001}    30
    Should Be True    ${result}    Exactly one acknowledgement should be in the Broker cache.

    # The service recovers: the acknowledgement leaves the cache and is not re-ingested.
    Ctn Set Command Status    ${cmd_id}    ${0}
    Ctn Process Service Result Hard    host_1    service_10    ${0}    (1;10) is OK
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_10    ${0}    60    HARD
    Should Be True    ${result}    Service (1;10) should be OK HARD

    ${result}    Ctn Check Acknowledgements Count With Timeout    ${0}    ${51001}    30
    Should Be True    ${result}    The Broker cache should hold no acknowledgement after recovery.


BEACK10
    [Documentation]    Scenario: acknowledgements survive a Broker restart (cache persistence).
    ...                Given a BBDO3 configuration and an acknowledged critical service
    ...                When Broker is restarted while Engine keeps running
    ...                Then GetAcknowledgements still lists it (restored from the persisted cache, not re-sent by Engine)
    ...                When the service recovers
    ...                Then the restored acknowledgement is closed and leaves the cache.
    [Tags]    broker    engine    services    extcmd    grpc    cache
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Broker Cache

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${cmd_id}    Ctn Get Service Command Id    ${11}
    Ctn Set Command Status    ${cmd_id}    ${2}
    Ctn Process Service Result Hard    host_1    service_11    ${2}    (1;11) is critical
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_11    ${2}    60    HARD
    Should Be True    ${result}    Service (1;11) should be critical HARD

    ${d}    Ctn Get Round Current Date
    Ctn Acknowledge Service Problem    host_1    service_11
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_11    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 11).
    ${result}    Ctn Check Acknowledgements Count With Timeout    ${1}    ${51001}    30
    Should Be True    ${result}    Exactly one acknowledgement should be in the Broker cache.

    # Restart Broker only; Engine keeps running. The cache is persisted on clean stop.
    Ctn Kindly Stop Broker
    Ctn Start Broker

    # The acknowledgement is restored from the persisted cache (Engine did not re-send it).
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    ${1}    ${11}    ${51001}    60
    Should Be True    ${result}    Acknowledgement (1;11) should be restored in the Broker cache after restart.

    # The restored acknowledgement can still be closed on recovery.
    Ctn Set Command Status    ${cmd_id}    ${0}
    Ctn Process Service Result Hard    host_1    service_11    ${0}    (1;11) is OK
    ${result}    Ctn Check Acknowledgements Count With Timeout    ${0}    ${51001}    60
    Should Be True    ${result}    The restored acknowledgement should be closed after recovery.


*** Keywords ***
Ctn Clear Acknowledgements
    [Documentation]    This keyword is really useful because each test on acknowledgements adds acknowledgements
    ...                and we don't master the acknowledgement ID.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM acknowledgements
