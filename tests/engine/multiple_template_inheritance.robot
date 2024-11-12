*** Settings ***
Documentation       Centreon Engine verify multiple template inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EMTI0
    [Documentation]    Verify multiple inheritance host
    [Tags]    broker    engine    hosts    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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
