*** Settings ***
Documentation       Engine/Broker tests on event_script output

Library             Collections
Library             DatabaseLibrary
Library             DateTime
Library             String
Library             OperatingSystem


Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***

EVENT_SCRIPT_BASIC_SERVICE_CHECK
    [Documentation]    Given an event_script output configured in broker
    ...                When a service check result is processed
    ...                Then the event is passed to the script as JSON
    [Tags]    broker    engine    event_script
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Source Log    central    1
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    event_script    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Event Script Output    neb:Service    ${SCRIPTS}/event_script_test.sh

    Remove File    /tmp/event_script_test.log

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}

    # Send a service check result
    ${start}    Ctn Get Round Current Date
    Ctn Process Service Check Result    host_16    service_314    0    OK|test=42

    # Verify the event_script output received the data
    ${content}    Create List    [event_script]
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    event_script should process the check result

    Sleep    5s

    # Verify the number of lines in event_script_test.log matches neb:Service lines in broker log
    ${event_script_lines}    Get File    /tmp/event_script_test.log
    ${event_script_count}    Get Line Count    ${event_script_lines}
    
    ${broker_content}    Get File    ${centralLog}
    ${neb_service_lines}    Get Regexp Matches    ${broker_content}    .*"cat":"neb","elem":"Service".*
    ${neb_service_count}    Get Length    ${neb_service_lines}
    
    Should Be Equal As Integers    ${event_script_count}    ${neb_service_count}    Event script log lines should match neb:Service occurrences in broker log
    



