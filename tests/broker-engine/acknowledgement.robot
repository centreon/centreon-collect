*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BEACK1
    [Documentation]    Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted from engine but still open on the database.
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical
    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    SOFT
    Should Be True    ${result}    Service (1;1) should be critical
    Repeat Keyword    2 times    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    # Service_1 is set back to OK.
    Ctn Process Service Result Hard    host_1    service_1    0    (1;1) is OK
    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (1;1) should be OK HARD

    # Acknowledgement is deleted but to see this we have to check in the comments table
    ${result}    Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

BEACK2
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted.
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
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Check Result    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    SOFT
    Should Be True    ${result}    Service (1;1) should be critical
    Repeat Keyword    2 times    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    600    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    # Service_1 is set back to OK.
    Ctn Process Service Result Hard    host_1    service_1    0    (1;1) is OK
    ${result}    Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (1;1) should be OK HARD

    # Acknowledgement is deleted but to see this we have to check in the comments table
    ${result}    Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

BEACK3
    [Documentation]    Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical
    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    SOFT
    Should Be True    ${result}    Service (1;1) should be critical
    Repeat Keyword    2 times    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    Ctn Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables have the deletion_time column filled
    ${result}    Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

BEACK4
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
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
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Check Result    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    SOFT
    Should Be True    ${result}    Service (1;1) should be critical
    Repeat Keyword    2 times    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    600    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    Ctn Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables have the deletion_time column filled
    ${result}    Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

BEACK5
    [Documentation]    Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
    [Tags]    broker    engine    services    extcmd
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    trace
    Ctn Broker Config Log    central    sql    debug
    Ctn Set Value In Engine Conf    ${0}    log_v2_enabled    ${1}
    Ctn Set Value In Engine Conf    ${0}    log_level_external_command    trace
    Ctn Set Value In Engine Conf    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical
    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    SOFT
    Should Be True    ${result}    Service (1;1) should be critical
    Repeat Keyword    2 times    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1    STICKY
    ${ack_id}    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    # Service_1 is set to WARNING.
    Ctn Process Service Result Hard    host_1    service_1    1    (1;1) is WARNING
    ${result}    Check Service Status With Timeout    host_1    service_1    ${1}    60    HARD
    Should Be True    ${result}    Service (1;1) should be WARNING HARD

    # Acknowledgement is not deleted.
    ${result}    Check Acknowledgement Is Deleted With Timeout    ${ack_id}    10
    Should Be True    ${result}==${False}    Acknowledgement ${ack_id} should not be deleted.

    Ctn Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables have the deletion_time column filled
    ${result}    Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.

BEACK6
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
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
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Check Result    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    SOFT
    Should Be True    ${result}    Service (1;1) should be critical
    Repeat Keyword    2 times    Ctn Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    ${d}    Get Current Date    result_format=epoch    exclude_millis=True
    Ctn Acknowledge Service Problem    host_1    service_1    STICKY
    ${ack_id}    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    600    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).

    # Service_1 is set to WARNING.
    Ctn Process Service Result Hard    host_1    service_1    1    (1;1) is WARNING
    ${result}    Check Service Status With Timeout    host_1    service_1    ${1}    60    HARD
    Should Be True    ${result}    Service (1;1) should be WARNING HARD

    # Acknowledgement is not deleted.
    ${result}    Check Acknowledgement Is Deleted With Timeout    ${ack_id}    10
    Should Be True    ${result}==${False}    Acknowledgement ${ack_id} should not be deleted.

    Ctn Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables have the deletion_time column filled
    ${result}    Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted.
