*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BEACK1
    [Documentation]    Scenario: Acknowledging a critical service
    ...    Given Engine has a critical service
    ...    When an external command is sent to acknowledge it
    ...    Then the "centreon_storage.acknowledgements" table is updated with this acknowledgement
    ...    And a log in "centreon_storage.logs" concerning this acknowledgement is added.
    ...    When the service is set to OK
    ...    Then the acknowledgement is deleted from the Engine
    ...    But it remains open in the database

    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # The service command is set to CRITICAL to also control active checks
    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    2    (1;1) is critical
    ${result}    Ctn Check Service Status With Timeout
    ...    host_1    service_1
    ...    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    ${start}    Ctn Get Round Current Date
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Acknowledgement With Timeout
    ...    host_1    service_1
    ...    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    ${result}    Ctn Check Acknowledgement In Logs Table    ${start}
    Should Be True    ${result}    Acknowledgement should be in logs table.

    # The service command is set to OK to also control active checks
    Ctn Set Command Status    ${cmd_id}    ${0}

    # Service_1 is set back to OK.
    Ctn Process Service Result Hard    host_1    service_1    0    (1;1) is OK
    ${result}    Ctn Check Service Status With Timeout
    ...    host_1    service_1
    ...    ${0}    60    HARD
    Should Be True    ${result}    Service (1;1) should be OK HARD

    # Acknowledgement is deleted but to see this we have to check in the comments table
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

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

BEACK3
    [Documentation]    Engine has a critical service. An external command is sent to acknowledge it.
    ...                The centreon_storage.acknowledgements table is then updated with this acknowledgement.
    ...                The acknowledgement is removed and the comment in the comments table has its deletion_time
    ...                column updated.
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    processing    error
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
    ...    host_1    service_1    2    (1;1) is critical
    ${result}    Ctn Check Service Status With Timeout
    ...    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Acknowledgement With Timeout
    ...    host_1    service_1    ${d}    2    60    HARD
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

BEACK5
    [Documentation]    Engine has a critical service. An external command is sent to acknowledge it ;
    ...                the acknowledgement is sticky. The centreon_storage.acknowledgements table is
    ...                then updated with this acknowledgement. The service is newly set to WARNING.
    ...                And the acknowledgement in database is still there.
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    trace
    Ctn Broker Config Log    central    sql    debug
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_external_command    trace
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # The service command is set to CRITICAL to also control active checks
    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    2    (1;1) is critical
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1    STICKY
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    # The service command is set to WARNING to also control active checks
    Ctn Set Command Status    ${cmd_id}    ${1}

    # Service_1 is set to WARNING.
    Ctn Process Service Result Hard    host_1    service_1    1    (1;1) is WARNING
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${1}    60    HARD
    Should Be True    ${result}    Service (1;1) should be WARNING HARD

    # Acknowledgement is not deleted.
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    10
    Should Be True    not ${result}    Acknowledgement ${ack_id} should not be deleted.

    Ctn Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments
    # and acknowledgements tables have the deletion_time column filled
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

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

    ${content}    Create List    Still 0 running acknowledgements
    Ctn Find In Log With Timeout    ${engineLog0}    ${d}    ${content}    30


*** Keywords ***
Ctn Clear Acknowledgements
    [Documentation]    This keyword is really useful because each test on acknowledgements adds acknowledgements
    ...                and we don't master the acknowledgement ID.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM acknowledgements
