*** Settings ***
Documentation       Centreon Engine verify multiple template inheritance with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CEMTI0
    [Documentation]    Given a host using a chain of 4 template levels each defining a custom variable
    ...    When Engine starts with centralized configuration
    ...    Then all custom variables from every template level are present on the host
    [Tags]    broker    engine    hosts    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info

    Ctn Clear Retention

    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Template File    ${0}    host    _CV    ["testA", "test2","test3", "test4"]


    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg

    # Operation in host
    Ctn Add Template To Hosts    0    host_template_1    [1]

    # multistage inheritance
    Ctn Engine Config Set Value In Hosts    0    host_template_1    use    host_template_2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_2    use    host_template_3    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_3    use    host_template_4    hostTemplates.cfg

    Ctn Engine Config Set Value In Hosts    0    host_template_2    _CV2    testB    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_3    _CV3    testC    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_4    _CV4    testD    hostTemplates.cfg

    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPCOMMUNITY
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPVERSION

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # Sending the new configuration to Engine.
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Host Info Grpc    ${1}
    Log To Console    ${output}[customVariables]

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY1    VAL1
    Should Be True    ${ret}    customVariables_KEY1:Should Be VAL1

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    CV    testA
    Should Be True    ${ret}    customVariables_CV:Should Be testA

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    CV2    testB
    Should Be True    ${ret}    customVariables_CV2:Should Be testB

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    CV3    testC
    Should Be True    ${ret}    customVariables_CV3:Should Be testC

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    CV4    testD
    Should Be True    ${ret}    customVariables_CV4:Should Be testD

    Ctn Stop Engine
    Ctn Kindly Stop Broker
