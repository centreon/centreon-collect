*** Settings ***
Documentation       Centreon Engine does not modify some conf objects on reload but replaces them.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Run Keywords    Ctn Stop Engine    AND    Ctn Save Logs If Failed


*** Test Cases ***

NOT_MODIFY_HOST_ESCALATION
    [Documentation]    Given an engine with a host escalation configured, we add a contact group to it.
    ...    We expect no error in logs and that configuration is well updated
    [Tags]    engine    immutable conf objects    MON-192369
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Host Group    ${0}    ${1}    ["host_1","host_2","host_3"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]
    Ctn Add Contact Group    ${0}    ${2}    ["U4"]

    Remove File     ${EtcRoot}/centreon-engine/config0/escalations.cfg
    Ctn Create Escalations File    0    1    hostgroup_1    contactgroup_1    host

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    FOR    ${host}    IN    host_1    host_2    host_3
        ${output}    Ctn Get Host Escalation Info Grpc    ${host}
        Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    END


    ${start}    Get Current Date
    Remove File     ${EtcRoot}/centreon-engine/config0/escalations.cfg
    Ctn Create Escalations File    0    1    hostgroup_1    contactgroup_2, contactgroup_1    host
    Ctn Reload Engine
    
    ${content}    Create List    Could not modify
    ${result}    Ctn Find In Log With Timeout     ${engineLog0}    ${start}    ${content}    10
    Should Not Be True    ${result}    Engine has log errors

    FOR    ${host}    IN    host_1    host_2    host_3
        ${output}    Ctn Get Host Escalation Info Grpc    ${host}
        Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
        Should Contain   ${output}[contactGroup]    contactgroup_2    contactGroup
    END


NOT_MODIFY_SERVICE_ESCALATION
    [Documentation]    Given an engine with a service escalation configured, we add a contact group to it.
    ...    We expect no error in logs and that configuration is well updated
    [Tags]    engine    immutable conf objects    MON-192369
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1","host_2","service_2","host_3","service_3"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]
    Ctn Add Contact Group    ${0}    ${2}    ["U4"]

    Remove File     ${EtcRoot}/centreon-engine/config0/escalations.cfg
    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_1    service

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    FOR     ${host}     ${serv}    IN
    ...    host_1    service_1
    ...    host_2    service_2
    ...    host_3    service_3
        ${output}    Ctn Get Service Escalation Info Grpc    ${host}    ${serv}
        Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    END


    ${start}    Get Current Date
    Remove File     ${EtcRoot}/centreon-engine/config0/escalations.cfg
    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_2,contactgroup_1    service
    Ctn Reload Engine
    
    ${content}    Create List    Could not modify
    ${result}    Ctn Find In Log With Timeout     ${engineLog0}    ${start}    ${content}    10
    Should Not Be True    ${result}    Engine has log errors

    FOR     ${host}     ${serv}    IN
    ...    host_1    service_1
    ...    host_2    service_2
    ...    host_3    service_3
        ${output}    Ctn Get Service Escalation Info Grpc    ${host}    ${serv}
        Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
        Should Contain   ${output}[contactGroup]    contactgroup_2    contactGroup
    END


HOST_DEPENDENCY
    [Documentation]    Given an engine with a host dependency configured, we change it.
    ...    We expect no error in logs and that configuration is well updated
    [Tags]    engine        MON-192369
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    
    Remove File    ${EtcRoot}/centreon-engine/config0/dependencies.cfg
    Ctn Add Host Dependency     ${0}     host_1      host_2

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}      Ctn Get Host Info Grpc      ${2}
    Should Be Equal    ${output}[dependencies][0][hostname]     host_1     dependency not resolved

    Remove File    ${EtcRoot}/centreon-engine/config0/dependencies.cfg
    Ctn Add Host Dependency     ${0}     host_3      host_2

    Ctn Reload Engine

    Sleep    2

    ${output}      Ctn Get Host Info Grpc      ${2}
    Should Be Equal    ${output}[dependencies][0][hostname]     host_3     dependency not updated


SERVICE_DEPENDENCY
    [Documentation]    Given an engine with a service dependency configured, we change it.
    ...    We expect no error in logs and that configuration is well updated
    [Tags]    engine        MON-192369
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    
    Remove File    ${EtcRoot}/centreon-engine/config0/dependencies.cfg
    Ctn Add Service Dependency     ${0}     host_1    host_2    service_1    service_2

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}      Ctn Get Service Info Grpc      ${2}    ${2}
    Should Be Equal    ${output}[dependencies][0][hostname]     host_1     dependency not resolved
    Should Be Equal    ${output}[dependencies][0][description]     service_1     dependency not resolved

    Remove File    ${EtcRoot}/centreon-engine/config0/dependencies.cfg
    Ctn Add ServiceDependency     ${0}     host_3    host_2     service_3    service_2

    Ctn Reload Engine

    Sleep    2

    ${output}      Ctn Get Service Info Grpc      ${2}    ${2}
    Should Be Equal    ${output}[dependencies][0][hostname]     host_3     dependency not updated
    Should Be Equal    ${output}[dependencies][0][description]     service_3     dependency not resolved

