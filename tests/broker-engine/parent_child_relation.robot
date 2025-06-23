*** Settings ***
Documentation       Centreon Engine/Broker verify relation parent child host.

Resource            ../resources/resources.robot
Library             Process
Library             String
Library             DateTime
Library             DatabaseLibrary
Library             OperatingSystem
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             ../resources/specific-duplication.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***

RENAME_PARENT
    [Documentation]    Given an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
    ...    Engine mustn't crash and log an error on reload.
    [Tags]    engine    MON-168884
    
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    module

    Ctn Clear Retention

    # host_1 is parent of host_2
    Ctn Add Parent To Host    0    host_2    host_1

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    #let's some time to engine to process the parent/child relation
    Sleep   5s

    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1    parentHosts

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    childs:${output}[childHosts]
    Should Contain    ${output}[childHosts]    host_2    childHosts

    # rename the parent host
    Ctn Engine Config Rename Host    ${0}    host_1    host_1_new
    Ctn Engine Config Set Host Value    ${0}    host_2    parents    host_1_new
    Ctn Engine Config Replace Value In Services    ${0}    service_1    host_name    host_1_new

    ${start}    Get Current Date
    Ctn Reload Engine

    ${content}    Create List    Reload configuration finished
        ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    Should Be True    ${result}    Engine is Not Ready after 60s!!
    ${output}    Ctn Get Host Info Grpc    ${2}
    Log To Console    parents:${output}[parentHosts]
    Should Contain    ${output}[parentHosts]    host_1_new    parentHosts

    Ctn Stop Engine
