*** Settings ***
Documentation       Tests on flapping services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed

*** Test Cases ***
BEFL1
    [Documentation]    The flapping is enabled on the service (1;1). When it goes flapping,
    ...    the flapping information is well sent to Broker. Then the service is stabilized
    ...    and the flapping information is disabled on Broker side.
    ...    And during all that test, the RRD broker should not have any error.
    [Tags]    broker    engine    services    extcmd    MON-150015
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Engine Config Set Value    0    enable_flap_detection    1
    Ctn Engine Config Set Value    0    log_level_checks    debug
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_options    all
    Ctn Engine Config Set Value In Services    0    service_1    low_flap_threshold    5
    Ctn Engine Config Set Value In Services    0    service_1    high_flap_threshold    10

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${start}    Get Current Date
    FOR    ${idx}    IN RANGE    1    30
	Ctn Process Service Check Result    host_1    service_1    0    (1;1) is OK ${idx}
        Sleep    1s
	Ctn Process Service Check Result    host_1    service_1    2    (1;1) is CRITICAL ${idx}
        Sleep    1s
    END
    ${content}    Create List    Checking service 'service_1' on host 'host_1' for flapping    Service is flapping
    ${result}    Ctn Find In Log    engineLog   ${start}    ${content}
    Should Be True    ${result}    The service (1;1) should have flapped.

    Sleep    1s
    ${start}    Get Current Date
    FOR    ${idx}    IN RANGE    1    30
	${start}    Get Current Date
	Ctn Process Service Check Result    host_1    service_1    0    (1;1) is OK ${idx}
        Sleep    1s
	${result}    Ctn Find In Log    engineLog   ${start}    ${content}
	IF    ${result}
	    Log To Console    Flapping detected after ${idx} iterations
	    BREAK
	END
    END
    ${content}    Create List    Checking service 'service_1' on host 'host_1' for flapping    Service is not flapping
    ${result}    Ctn Find In Log    engineLog   ${start}    ${content}
    Should Be True    ${result}    The service (1;1) should not flap anymore.

    # Do we have RRD errors?
    ${result}    Grep File    ${rrdLog}    "ignored update error in file"
    Should Be Empty
    ...    ${result}
    ...    There should not be any error in cbd RRD of kind 'ignored update error in file'
