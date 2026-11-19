*** Settings ***
Documentation       Centreon Engine verify host/service escalation inheritance with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***

CEESI0
    [Documentation]    Given Engine is configured with centralized configuration
    ...    And a service escalation is defined for a service group containing host_1..3/service_1..3
    ...    And a host group containing host_6 and host_7 is defined
    ...    When Broker and Engine are started
    ...    Then each service in the service group gets the escalation applied
    ...    And services outside the service group have no escalation
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Log To Console    step1
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    Log To Console    step2
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Log To Console    step3
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1","host_2","service_2","host_3","service_3"]
    Ctn Add Host Group    ${0}    ${1}    ["host_6","host_7"]

    Log To Console    step4
    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]

    Log To Console    step5
    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_1

    Log To Console    step6
    Ctn Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    last_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    notification_interval    1
    Ctn Engine Config Set Value In Escalations    0    esc1    hostgroup    hostgroup_1

    Log To Console    step7

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Service Escalation Info Grpc    host_1    service_1

    Should Be Equal As Strings    ${output}[host]    host_1    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_1    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,c,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_2    service_2

    Should Be Equal As Strings    ${output}[host]    host_2    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_2    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,c,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_3    service_3

    Should Be Equal As Strings    ${output}[host]    host_3    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_3    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,c,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_6    service_6
    Should Be Empty   ${output}

    ${output}    Ctn Get Service Escalation Info Grpc    host_7    service_7
    Should Be Empty   ${output}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI1
    [Documentation]    Given Engine is configured with centralized configuration
    ...    And a service escalation with no fields inherits from a full service escalation template
    ...    When Broker and Engine are started
    ...    Then services in the service group get the escalation settings from the template on engine start
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    # add necessary files in centengine.cfg
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    # create necessary files:
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1","host_2","service_2","host_3","service_3"]
    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]

    # create the escalation file
    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_1

    # remove all fields in escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    servicegroup_name    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    contact_groups    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_period    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_options    escalations.cfg

    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    use    serviceescalation_template_1

    # create the template file
    Ctn Create Template File    ${0}    serviceescalation    contact_groups    ["contactgroup_1"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    passive_checks_enabled    escalationTemplates.cfg

    # set the necessary fields in templates
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    servicegroup_name    servicegroup_1    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    escalation_options    w,c,r    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    escalation_period    workhours    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg     0    serviceescalation_template_1    first_notification    2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    last_notification    2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    notification_interval    1    escalationTemplates.cfg


    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Service Escalation Info Grpc    host_1    service_1

    Should Be Equal As Strings    ${output}[host]    host_1    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_1    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,c,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_2    service_2

    Should Be Equal As Strings    ${output}[host]    host_2    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_2    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,c,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_3    service_3

    Should Be Equal As Strings    ${output}[host]    host_3    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_3    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,c,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI2
    [Documentation]    Given Engine is configured with centralized configuration
    ...    And a service escalation with full fields inherits from a full service escalation template
    ...    When Broker and Engine are started
    ...    Then the escalation own values take precedence over template values on engine start
    ...    And services outside the service group have no escalation
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    # add necessary files in centengine.cfg
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    # create necessary files:
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1","host_2","service_2","host_3","service_3"]
    Ctn Add Service Group    ${0}    ${2}    ["host_4","service_4","host_5","service_5"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]
    Ctn Add Contact Group    ${0}    ${2}    ["U4"]

    # create the escalation file
    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_1

    # remove all fields in escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_period    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_options    escalations.cfg

    # add fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    last_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    notification_interval    1
    Ctn Engine Config Set Value In Escalations    0    esc1    escalation_period    24x7
    Ctn Engine Config Set Value In Escalations    0    esc1    escalation_options    w,r


    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    use    serviceescalation_template_1

    # create the template file
    Ctn Create Template File    ${0}    serviceescalation    contact_groups    ["contactgroup_2"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    passive_checks_enabled    escalationTemplates.cfg

    # set the necessary fields in templates
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    servicegroup_name    servicegroup_2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    escalation_options    c    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    escalation_period    workhours    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg     0    serviceescalation_template_1    first_notification    3    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    last_notification    3    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    notification_interval    2    escalationTemplates.cfg


    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Service Escalation Info Grpc    host_1    service_1

    Should Be Equal As Strings    ${output}[host]    host_1    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_1    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_2    service_2

    Should Be Equal As Strings    ${output}[host]    host_2    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_2    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_3    service_3

    Should Be Equal As Strings    ${output}[host]    host_3    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_3    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_4    service_4
    Should Be Empty    ${output}

    ${output}    Ctn Get Service Escalation Info Grpc    host_5    service_5
    Should Be Empty    ${output}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI3
    [Documentation]    Given Engine is configured with centralized configuration and started
    ...    And a service escalation with no fields is changed to inherit from a full service escalation template
    ...    When Broker notifies Engine of the new configuration
    ...    Then services in the new template's service group get the escalation settings
    ...    And services in the old service group have no escalation after the configuration change
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info

    # add necessary files in centengine.cfg
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    # create necessary files:
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1","host_2","service_2","host_3","service_3"]
    Ctn Add Service Group    ${0}    ${2}    ["host_4","service_4","host_5","service_5"]
    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]
    Ctn Add Contact Group    ${0}    ${2}    ["U4"]
    # create the escalation file
    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_1

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # remove all fields in escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    servicegroup_name    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    contact_groups    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_period    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_options    escalations.cfg

    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    use    serviceescalation_template_1

    # create the template file
    Ctn Create Template File    ${0}    serviceescalation    contact_groups    ["contactgroup_2"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    passive_checks_enabled    escalationTemplates.cfg

    # set the necessary fields in templates
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    servicegroup_name    servicegroup_2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    escalation_options    w,c,r    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    escalation_period    workhours    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg     0    serviceescalation_template_1    first_notification    2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    last_notification    2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    notification_interval    1    escalationTemplates.cfg


    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Service Escalation Info Grpc    host_1    service_1
    Should Be Empty    ${output}

    ${output}    Ctn Get Service Escalation Info Grpc    host_2    service_2
    Should Be Empty    ${output}

    ${output}    Ctn Get Service Escalation Info Grpc    host_3    service_3
    Should Be Empty    ${output}

    ${output}    Ctn Get Service Escalation Info Grpc    host_4    service_4

    Should Be Equal As Strings    ${output}[host]    host_4    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_4    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_2    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,c,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_5    service_5

    Should Be Equal As Strings    ${output}[host]    host_5    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_5    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_2    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,c,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI4
    [Documentation]    Given Engine is configured with centralized configuration and started
    ...    And a service escalation with full fields is changed to also inherit from a full service escalation template
    ...    When Broker notifies Engine of the new configuration
    ...    Then the escalation own values take precedence over template values after the configuration change
    ...    And services outside the escalation's service group have no escalation
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info

    # add necessary files in centengine.cfg
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    # create necessary files:
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1","host_2","service_2","host_3","service_3"]
    Ctn Add Service Group    ${0}    ${2}    ["host_4","service_4","host_5","service_5"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]
    Ctn Add Contact Group    ${0}    ${2}    ["U4"]

    # create the escalation file
    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_1

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # remove all fields in escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_period    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_options    escalations.cfg

    # add fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    last_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    notification_interval    1
    Ctn Engine Config Set Value In Escalations    0    esc1    escalation_period    workhours
    Ctn Engine Config Set Value In Escalations    0    esc1    escalation_options    w,r


    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    use    serviceescalation_template_1

    # create the template file
    Ctn Create Template File    ${0}    serviceescalation    contact_groups    ["contactgroup_2"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    passive_checks_enabled    escalationTemplates.cfg

    # set the necessary fields in templates
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    servicegroup_name    servicegroup_2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    escalation_options    c    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    escalation_period    none    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg     0    serviceescalation_template_1    first_notification    3    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    last_notification    3    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    notification_interval    2    escalationTemplates.cfg


    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Service Escalation Info Grpc    host_1    service_1

    Should Be Equal As Strings    ${output}[host]    host_1    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_1    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_2    service_2

    Should Be Equal As Strings    ${output}[host]    host_2    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_2    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_3    service_3

    Should Be Equal As Strings    ${output}[host]    host_3    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_3    serviceDescription
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    w,r    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Service Escalation Info Grpc    host_4    service_4
    Should Be Empty    ${output}

    ${output}    Ctn Get Service Escalation Info Grpc    host_5    service_5
    Should Be Empty    ${output}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI5
    [Documentation]    Given Engine is configured with centralized configuration
    ...    And a host escalation is defined for a host group containing host_1, host_2, and host_3
    ...    When Broker and Engine are started
    ...    Then each host in the host group gets the escalation applied
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Host Group    ${0}    ${1}    ["host_1","host_2","host_3"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]

    Ctn Create Escalations File    0    1    hostgroup_1    contactgroup_1    host

    Ctn Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    last_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    notification_interval    1


    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Escalation Info Grpc    host_1

    Should Be Equal As Strings    ${output}[hostName]    host_1    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    all    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_2

    Should Be Equal As Strings    ${output}[hostName]    host_2    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    all    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_3

    Should Be Equal As Strings    ${output}[hostName]    host_3    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    all    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI6
    [Documentation]    Given Engine is configured with centralized configuration
    ...    And a host escalation with no fields inherits from a full host escalation template
    ...    When Broker and Engine are started
    ...    Then hosts in the host group get all escalation settings from the template on engine start
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Host Group    ${0}    ${1}    ["host_1","host_2","host_3"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]

    Ctn Create Escalations File    0    1    hostgroup_1    contactgroup_1    host

    # remove all fields in escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    hostgroup_name    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    contact_groups    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_period    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_options    escalations.cfg

    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    use    hostescalation_template_1

    # create the template file
    Ctn Create Template File    ${0}    hostescalation    contact_groups    ["contactgroup_1"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    passive_checks_enabled    escalationTemplates.cfg

    # set the necessary fields in templates
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    hostgroup_name    hostgroup_1    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    escalation_options    all    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    escalation_period    workhours    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg     0    hostescalation_template_1    first_notification    2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    last_notification    2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    notification_interval    1    escalationTemplates.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Escalation Info Grpc    host_1

    Should Be Equal As Strings    ${output}[hostName]    host_1    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    all    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_2

    Should Be Equal As Strings    ${output}[hostName]    host_2    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    all    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_3

    Should Be Equal As Strings    ${output}[hostName]    host_3    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    all    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI7
    [Documentation]    Given Engine is configured with centralized configuration
    ...    And a host escalation with full fields inherits from a full host escalation template
    ...    When Broker and Engine are started
    ...    Then the escalation own values take precedence over template values for hosts in its group
    ...    And hosts in the template's host group have no escalation on engine start
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Host Group    ${0}    ${1}    ["host_1","host_2","host_3"]
    Ctn Add Host Group    ${0}    ${2}    ["host_4","host_5"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]
    Ctn Add Contact Group    ${0}    ${2}    ["U4"]

    Ctn Create Escalations File    0    1    hostgroup_1    contactgroup_1    host

    # remove all fields in escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_options    escalations.cfg
    Ctn Engine Config Delete Key In Cfg   0    esc1    escalation_period    escalations.cfg

    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    escalation_options    d    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    escalation_period    none    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    first_notification    2    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    last_notification    2    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    notification_interval    1    escalations.cfg

    Ctn Engine Config Set Value In Escalations    0    esc1    use    hostescalation_template_1

    # create the template file
    Ctn Create Template File    ${0}    hostescalation    contact_groups    ["contactgroup_2"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    passive_checks_enabled    escalationTemplates.cfg

    # set the necessary fields in templates
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    hostgroup_name    hostgroup_2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    escalation_options    u    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    escalation_period    24x7    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg     0    hostescalation_template_1    first_notification    3    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    last_notification    3    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    notification_interval    2    escalationTemplates.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Escalation Info Grpc    host_1

    Should Be Equal As Strings    ${output}[hostName]    host_1    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    none    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    d    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_2

    Should Be Equal As Strings    ${output}[hostName]    host_2    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    none    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    d    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_3

    Should Be Equal As Strings    ${output}[hostName]    host_3    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    none    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    d    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_4

    Should Be Empty    ${output}

    ${output}    Ctn Get Host Escalation Info Grpc    host_5

    Should Be Empty    ${output}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI8
    [Documentation]    Given Engine is configured with centralized configuration and started
    ...    And a host escalation with no fields is changed to inherit from a full host escalation template
    ...    When Broker notifies Engine of the new configuration
    ...    Then hosts in the new template's host group get the escalation settings
    ...    And hosts in the old host group have no escalation after the configuration change
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Host Group    ${0}    ${1}    ["host_1","host_2","host_3"]
    Ctn Add Host Group    ${0}    ${2}    ["host_4","host_5"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]
    Ctn Add Contact Group    ${0}    ${2}    ["U4"]

    Ctn Create Escalations File    0    1    hostgroup_1    contactgroup_1    host

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # remove all fields in escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    hostgroup_name    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    contact_groups    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_period    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_options    escalations.cfg

    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    use    hostescalation_template_1

    # create the template file
    Ctn Create Template File    ${0}    hostescalation    contact_groups    ["contactgroup_2"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    passive_checks_enabled    escalationTemplates.cfg

    # set the necessary fields in templates
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    hostgroup_name    hostgroup_2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    escalation_options    all    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    escalation_period    workhours    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg     0    hostescalation_template_1    first_notification    2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    last_notification    2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    notification_interval    1    escalationTemplates.cfg

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Host Escalation Info Grpc    host_1
    Should Be Empty    ${output}

    ${output}    Ctn Get Host Escalation Info Grpc    host_2
    Should Be Empty    ${output}

    ${output}    Ctn Get Host Escalation Info Grpc    host_3
    Should Be Empty    ${output}

    ${output}    Ctn Get Host Escalation Info Grpc    host_4

    Should Be Equal As Strings    ${output}[hostName]    host_4
    Should Contain   ${output}[contactGroup]    contactgroup_2
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours
    Should Be Equal As Strings    ${output}[escalationOption]    all
    Should Be Equal As Numbers     ${output}[firstNotification]    2
    Should Be Equal As Numbers     ${output}[lastNotification]    2
    Should Be Equal As Numbers     ${output}[notificationInterval]    1

    ${output}    Ctn Get Host Escalation Info Grpc    host_5

    Should Be Equal As Strings    ${output}[hostName]    host_5
    Should Contain   ${output}[contactGroup]    contactgroup_2
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours
    Should Be Equal As Strings    ${output}[escalationOption]    all
    Should Be Equal As Numbers     ${output}[firstNotification]    2
    Should Be Equal As Numbers     ${output}[lastNotification]    2
    Should Be Equal As Numbers     ${output}[notificationInterval]    1

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEESI9
    [Documentation]    Given Engine is configured with centralized configuration and started
    ...    And a host escalation with full fields is changed to also inherit from a full host escalation template
    ...    When Broker notifies Engine of the new configuration
    ...    Then the escalation own values take precedence over template values for hosts in its group after the configuration change
    ...    And hosts in the template's host group have no escalation after the configuration change
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Centralized Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Host Group    ${0}    ${1}    ["host_1","host_2","host_3"]
    Ctn Add Host Group    ${0}    ${2}    ["host_4","host_5"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]
    Ctn Add Contact Group    ${0}    ${2}    ["U4"]

    Ctn Create Escalations File    0    1    hostgroup_1    contactgroup_1    host

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # remove all fields in escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_options    escalations.cfg
    Ctn Engine Config Delete Key In Cfg    0    esc1    escalation_period    escalations.cfg

    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    escalation_options    d    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    escalation_period    workhours    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    first_notification    2    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    last_notification    2    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    notification_interval    1    escalations.cfg

    Ctn Engine Config Set Value In Escalations    0    esc1    use    hostescalation_template_1

    # create the template file
    Ctn Create Template File    ${0}    hostescalation    contact_groups    ["contactgroup_2"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    passive_checks_enabled    escalationTemplates.cfg

    # set the necessary fields in templates
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    hostgroup_name    hostgroup_2    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    escalation_options    u    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    escalation_period    24x7    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg     0    hostescalation_template_1    first_notification    3    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    last_notification    3    escalationTemplates.cfg
    Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    notification_interval    2    escalationTemplates.cfg

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Host Escalation Info Grpc    host_1

    Should Be Equal As Strings    ${output}[hostName]    host_1    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    d    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_2

    Should Be Equal As Strings    ${output}[hostName]    host_2    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    d    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_3

    Should Be Equal As Strings    ${output}[hostName]    host_3    hostName
    Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    workhours    escalationPeriod
    Should Be Equal As Strings    ${output}[escalationOption]    d    escalationOption
    Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers     ${output}[lastNotification]    2    lastNotification
    Should Be Equal As Numbers     ${output}[notificationInterval]    1    notificationInterval

    ${output}    Ctn Get Host Escalation Info Grpc    host_4

    Should Be Empty    ${output}

    ${output}    Ctn Get Host Escalation Info Grpc    host_5

    Should Be Empty    ${output}

    Ctn Stop Engine
    Ctn Kindly Stop Broker
