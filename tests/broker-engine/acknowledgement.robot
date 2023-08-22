*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.resource
Library             DatabaseLibrary
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BEACK1
    [Documentation]    Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted from engine but still open on the database.
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug

    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Process Service Check Result    host_1    service_1    2    (1;1) is critical
    ${result}=    Check Service Status With Timeout    host_1    service_1    ${2}    60    SOFT
    Should Be True    ${result}    msg=Service (1;1) should be critical
    Repeat Keyword    2 times    Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}=    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    msg=Service (1;1) should be critical HARD
    ${d}=    Get Current Date    result_format=epoch    exclude_millis=True
    Acknowledge Service Problem    host_1    service_1
    ${ack_id}=    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    msg=No acknowledgement on service (1, 1).

    # Service_1 is set back to OK.
    Repeat Keyword    3 times    Process Service Check Result    host_1    service_1    0    (1;1) is OK
    ${result}=    Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    msg=Service (1;1) should be OK HARD

    # Acknowledgement is deleted but to see this we have to check in the comments table
    ${result}=    check acknowledgement is deleted with timeout    ${ack_id}    30
    Should Be True    ${result}    msg=Acknowledgement ${ack_id} should be deleted.

BEACK2
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted.
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    ${1}
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug

    ${start}=    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    # Time to set the service to CRITICAL HARD.
    Process Service Check Result    host_1    service_1    ${2}    (1;1) is critical
    ${result}=    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    SOFT
    Should Be True    ${result}    msg=Service (1;1) should be critical
    Repeat Keyword    2 times    Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}=    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    HARD
    Should Be True    ${result}    msg=Service (1;1) should be critical HARD
    ${d}=    Get Current Date    result_format=epoch    exclude_millis=True
    Acknowledge Service Problem    host_1    service_1
    ${ack_id}=    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    600    HARD
    Should Be True    ${ack_id} > 0    msg=No acknowledgement on service (1, 1).

    # Service_1 is set back to OK.
    Repeat Keyword    3 times    Process Service Check Result    host_1    service_1    0    (1;1) is OK
    ${result}=    Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    msg=Service (1;1) should be OK HARD

    # Acknowledgement is deleted but to see this we have to check in the comments table
    ${result}=    check acknowledgement is deleted with timeout    ${ack_id}    30
    Should Be True    ${result}    msg=Acknowledgement ${ack_id} should be deleted.

BEACK3
    [Documentation]    Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug

    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Process Service Check Result    host_1    service_1    2    (1;1) is critical
    ${result}=    Check Service Status With Timeout    host_1    service_1    ${2}    60    SOFT
    Should Be True    ${result}    msg=Service (1;1) should be critical
    Repeat Keyword    2 times    Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}=    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    msg=Service (1;1) should be critical HARD
    ${d}=    Get Current Date    result_format=epoch    exclude_millis=True
    Acknowledge Service Problem    host_1    service_1
    ${ack_id}=    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    msg=No acknowledgement on service (1, 1).

    Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables have the deletion_time column filled
    ${result}=    check acknowledgement is deleted with timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    msg=Acknowledgement ${ack_id} should be deleted.

BEACK4
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    ${1}
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug

    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Process Service Check Result    host_1    service_1    ${2}    (1;1) is critical
    ${result}=    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    SOFT
    Should Be True    ${result}    msg=Service (1;1) should be critical
    Repeat Keyword    2 times    Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}=    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    HARD
    Should Be True    ${result}    msg=Service (1;1) should be critical HARD
    ${d}=    Get Current Date    result_format=epoch    exclude_millis=True
    Acknowledge Service Problem    host_1    service_1
    ${ack_id}=    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    600    HARD
    Should Be True    ${ack_id} > 0    msg=No acknowledgement on service (1, 1).

    Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables have the deletion_time column filled
    ${result}=    check acknowledgement is deleted with timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    msg=Acknowledgement ${ack_id} should be deleted.

BEACK5
    [Documentation]    Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    module0    neb    trace
    Broker Config Log    central    sql    debug
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_level_external_command    trace
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Process Service Check Result    host_1    service_1    2    (1;1) is critical
    ${result}=    Check Service Status With Timeout    host_1    service_1    ${2}    60    SOFT
    Should Be True    ${result}    msg=Service (1;1) should be critical
    Repeat Keyword    2 times    Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}=    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    msg=Service (1;1) should be critical HARD
    ${d}=    Get Current Date    result_format=epoch    exclude_millis=True
    Acknowledge Service Problem    host_1    service_1    STICKY
    ${ack_id}=    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    msg=No acknowledgement on service (1, 1).

    # Service_1 is set to WARNING.
    Repeat Keyword    3 times    Process Service Check Result    host_1    service_1    1    (1;1) is WARNING
    ${result}=    Check Service Status With Timeout    host_1    service_1    ${1}    60    HARD
    Should Be True    ${result}    msg=Service (1;1) should be WARNING HARD

    # Acknowledgement is not deleted.
    ${result}=    check acknowledgement is deleted with timeout    ${ack_id}    10
    Should Be True    ${result}==${False}    msg=Acknowledgement ${ack_id} should not be deleted.

    Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables have the deletion_time column filled
    ${result}=    check acknowledgement is deleted with timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    msg=Acknowledgement ${ack_id} should be deleted.

BEACK6
    [Documentation]    Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
    [Tags]    broker    engine    services    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    ${1}
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug

    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # Time to set the service to CRITICAL HARD.
    Process Service Check Result    host_1    service_1    ${2}    (1;1) is critical
    ${result}=    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    SOFT
    Should Be True    ${result}    msg=Service (1;1) should be critical
    Repeat Keyword    2 times    Process Service Check Result    host_1    service_1    2    (1;1) is critical

    ${result}=    Check Service Resource Status With Timeout    host_1    service_1    ${2}    600    HARD
    Should Be True    ${result}    msg=Service (1;1) should be critical HARD
    ${d}=    Get Current Date    result_format=epoch    exclude_millis=True
    Acknowledge Service Problem    host_1    service_1    STICKY
    ${ack_id}=    Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    600    HARD
    Should Be True    ${ack_id} > 0    msg=No acknowledgement on service (1, 1).

    # Service_1 is set to WARNING.
    Repeat Keyword    3 times    Process Service Check Result    host_1    service_1    1    (1;1) is WARNING
    ${result}=    Check Service Status With Timeout    host_1    service_1    ${1}    60    HARD
    Should Be True    ${result}    msg=Service (1;1) should be WARNING HARD

    # Acknowledgement is not deleted.
    ${result}=    check acknowledgement is deleted with timeout    ${ack_id}    10
    Should Be True    ${result}==${False}    msg=Acknowledgement ${ack_id} should not be deleted.

    Remove Service Acknowledgement    host_1    service_1

    # Acknowledgement is deleted but this time, both of comments and acknowledgements tables have the deletion_time column filled
    ${result}=    check acknowledgement is deleted with timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    msg=Acknowledgement ${ack_id} should be deleted.
