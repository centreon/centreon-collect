*** Settings ***
Documentation       Centreon Engine verify host/service escalation inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***

EESI0
    [Documentation]    Verify service escalation : create service escalation for every service in a service group
    [Tags]    engine    serviceescalation    MON-152874  
    Ctn Config Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1","host_2","service_2","host_3","service_3"]
    Ctn Add Host Group    ${0}    ${1}    ["host_6","host_7"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]

    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_1

    Ctn Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    last_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    notification_interval    1
    Ctn Engine Config Set Value In Escalations    0    esc1    hostgroup    hostgroup_1


    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

EESI1
    [Documentation]    Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Start Engine
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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
    

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

EESI2
    [Documentation]    Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Start Engine
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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
    

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

EESI3
    [Documentation]    Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Reload Engine
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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
    

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

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

EESI4
    [Documentation]    Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Reload Engine
    [Tags]    engine    serviceescalation    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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
    

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

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

EESI5
    [Documentation]    Verfiy host escalation : create host escalation for every host in the hostgroup
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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


    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

EESI6
    [Documentation]    Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Start Engine   
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

EESI7
    [Documentation]    Verify host escalation inheritance : escalation(full) inherit from template (full) , on Start Engine    
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

EESI8
    [Documentation]    Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Reload Engine   
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

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

EESI9
    [Documentation]    Verify host escalation inheritance : escalation(full) inherit from template (full) , on Reload Engine    
    [Tags]    engine    hostescalation    MON-152874
    Ctn Config Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

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

EESI10
    [Documentation]    Given a service escalation configuration with multiple inheritance templates,
    ...    When the engine is started,
    ...    Then the service escalation attributes are correctly inherited and applied to the services in the service group,
    ...    And services not in the group have no escalation applied.
    [Tags]    engine    serviceescalation    MON-188984
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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
    Ctn Add Contact Group    ${0}    ${11}    ["U4"]

    # create the escalation file
    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_1     service    False
    
    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    use    serviceescalation_template_1, serviceescalation_template_2

    # create the template file
    Ctn Create Template File    ${0}    serviceescalation    contact_groups    ["contactgroup_2", "contactgroup_45"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    passive_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_1    contact_groups    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_2    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_2    passive_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    serviceescalation_template_2    contact_groups    escalationTemplates.cfg
    
    # set the necessary fields in templates and escalation
    FOR    ${key}     ${value}     ${add_to_template}     IN
    ...    servicegroup_name    servicegroup_1     0
    ...    escalation_options    c     0
    ...    escalation_period   24x7    1
    ...    first_notification   2     1
    ...    last_notification    3     1
    ...    notification_interval    2     1
    ...    contact_groups     contactgroup_1     1
        ${template_index}    Ctn Randint    ${0}    ${2}
        IF    ${template_index} == 0 
            #0 add attrib to escalations.cfg
            Ctn Engine Config Set Value In Escalations    0    esc1     ${key}    ${value}
            IF     ${add_to_template} == 1
                #templates must not override contact value
                Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1     ${key}    ${value}1    escalationTemplates.cfg
                Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_2     ${key}    ${value}2    escalationTemplates.cfg
            END
        ELSE IF        ${template_index} == 1 
            #1 add attrib to serviceescalation_template_1
            Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_1    ${key}    ${value}    escalationTemplates.cfg
            IF     ${add_to_template} == 1
                #second template must not override first one
                Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_2    ${key}    ${value}1    escalationTemplates.cfg
            END
        ELSE 
            # add attrib to serviceescalation_template_2
            Ctn Engine Config Set Key Value In Cfg    0    serviceescalation_template_2    ${key}    ${value}    escalationTemplates.cfg
        END
    END

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    FOR     ${host}     ${serv}    IN
    ...    host_1    service_1
    ...    host_2    service_2
    ...    host_3    service_3
        ${output}    Ctn Get Service Escalation Info Grpc    ${host}    ${serv}

        Should Be Equal As Strings    ${output}[host]    ${host}    host
        Should Be Equal As Strings    ${output}[serviceDescription]    ${serv}    serviceDescription
        Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
        Should Not Contain   ${output}[contactGroup]    contactgroup_11    contactGroup11
        Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
        Should Be Equal As Strings    ${output}[escalationOption]    c    escalationOption
        Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
        Should Be Equal As Numbers     ${output}[lastNotification]    3    lastNotification
        Should Be Equal As Numbers     ${output}[notificationInterval]    2    notificationInterval
    END

    ${output}    Ctn Get Service Escalation Info Grpc    host_4    service_4    
    Should Be Empty    ${output}

    ${output}    Ctn Get Service Escalation Info Grpc    host_5    service_5    
    Should Be Empty    ${output}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EESI11
    [Documentation]    Given a host escalation configuration with multiple inheritance templates,
    ...    When the engine is started,
    ...    Then the host escalation attributes are correctly inherited and applied to the hosts in the host group,
    ...    And hosts not in the group have no escalation applied.
    [Tags]    engine    hostescalation    MON-188984
    Ctn Config Engine    ${1}    ${7}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Host Group    ${0}    ${1}    ["host_1","host_2","host_3"]

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]

    Ctn Create Escalations File    0    1    hostgroup_1    contactgroup_1    host    False

    # add inheritance fields in escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    use    hostescalation_template_1,hostescalation_template_2

    # create the template file
    Ctn Create Template File    ${0}    hostescalation    contact_groups    ["contactgroup_2", "contactgroup_45"]
    Ctn Config Engine Add Cfg File    ${0}    escalationTemplates.cfg

    # delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    passive_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_1    contact_groups    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_2    active_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_2    passive_checks_enabled    escalationTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostescalation_template_2    contact_groups    escalationTemplates.cfg
    
    # set the necessary fields in templates and escalation
    FOR    ${key}     ${value}     ${add_to_template}     IN
    ...    hostgroup_name    hostgroup_1     1
    ...    escalation_options    u     0
    ...    escalation_period   24x7    1
    ...    first_notification   2     1
    ...    last_notification    3     1
    ...    notification_interval    2     1
    ...    contact_groups     contactgroup_1     1
        ${template_index}    Ctn Randint    ${0}    ${2}
        IF    ${template_index} == 0 
            #0 add attrib to escalations.cfg
            Ctn Engine Config Set Value In Escalations    0    esc1     ${key}    ${value}
            IF     ${add_to_template} == 1
                #templates must not override contact value
                Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1     ${key}    ${value}1    escalationTemplates.cfg
                Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_2     ${key}    ${value}2    escalationTemplates.cfg
            END
        ELSE IF        ${template_index} == 1 
            #1 add attrib to hostescalation_template_1
            Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_1    ${key}    ${value}    escalationTemplates.cfg
            IF     ${add_to_template} == 1
                #second template must not override first one
                Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_2    ${key}    ${value}1    escalationTemplates.cfg
            END
        ELSE 
            # add attrib to hostescalation_template_2
            Ctn Engine Config Set Key Value In Cfg    0    hostescalation_template_2    ${key}    ${value}    escalationTemplates.cfg
        END
    END



    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    FOR    ${host}    IN    host_1    host_2    host_3
        ${output}    Ctn Get Host Escalation Info Grpc    ${host}

        Should Be Equal As Strings    ${output}[hostName]    ${host}    hostName
        Should Contain   ${output}[contactGroup]    contactgroup_1    contactGroup
        Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
        Should Be Equal As Strings    ${output}[escalationOption]    u    escalationOption
        Should Be Equal As Numbers     ${output}[firstNotification]    2    firstNotification
        Should Be Equal As Numbers     ${output}[lastNotification]    3    lastNotification
        Should Be Equal As Numbers     ${output}[notificationInterval]    2    notificationInterval
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EESI12
    [Documentation]    Verify host escalation configured with host_name directly (not hostgroup_name)
    ...    is properly linked to the host and triggers escalation contacts.
    [Tags]    engine    hostescalation    MON-197927

    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Set Value    0    interval_length    1    True

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_interval    1
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Hosts    0    host_1    check_interval    1
    Ctn Engine Config Set Value In Hosts    0    host_1    retry_interval    1

    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2"]

    # create a escalation with a dummy hostgroup (deleted after)
    Ctn Create Escalations File    0    1    dummy_hostgroup    contactgroup_1    host
    Ctn Engine Config Delete Key In Cfg    0    esc1    hostgroup_name    escalations.cfg

    Ctn Engine Config Set Key Value In Cfg    0    esc1    host_name    host_1    escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    last_notification    0
    Ctn Engine Config Set Value In Escalations    0    esc1    notification_interval    1

    Ctn Config Host Command Status    ${0}    checkh1    2

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    ${output}    Ctn Get Host Escalation Info Grpc    host_1

    Log To Console    "Verify that the escalation host_1 is created"

    Should Be Equal As Strings    ${output}[hostName]    host_1    hostName
    Should Contain    ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Numbers    ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers    ${output}[lastNotification]    0    lastNotification
    Should Be Equal As Numbers    ${output}[notificationInterval]    1    notificationInterval

    # Notification #1 goes to the host's direct contact (John_Doe).
    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    First notification to John_Doe was not sent

    # Notification #2+ comes from the escalation contactgroup_1 (U1, U2).
    ${content}    Create List    HOST NOTIFICATION: U1;host_1;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Escalation notification to U1 was not sent

    ${content}    Create List    HOST NOTIFICATION: U2;host_1;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Escalation notification to U2 was not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EESI13
    [Documentation]    Verify service escalation configured with host_name and service_description directly
    ...    (not servicegroup_name) is properly linked to the service and triggers escalation contacts.
    [Tags]    engine    serviceescalation    MON-197927

    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Set Value    0    interval_length    1    True

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Service_1 notifications: direct contact is John_Doe (first notification),
    # escalation contactgroup_1 fires from notification #2 onwards.
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    1
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    command_44
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1

    Ctn Add Contact Group    ${0}    ${1}    ["U1","U2","U3"]

    Ctn Create Escalations File    0    1    dummy_servicegroup    contactgroup_1    service
    Ctn Engine Config Delete Key In Cfg    0    esc1    servicegroup_name    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    host_name    host_1    escalations.cfg
    Ctn Engine Config Set Key Value In Cfg    0    esc1    service_description    service_1    escalations.cfg
    Ctn Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    last_notification    0
    Ctn Engine Config Set Value In Escalations    0    esc1    notification_interval    1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    
    # Force service_1's check command to return CRITICAL so it reaches HARD CRITICAL.
    Ctn Set Command Status    ${44}    ${2}

    ${output}    Ctn Get Service Escalation Info Grpc    host_1    service_1

    Log To Console    "Verify that the escalation (host_1, service_1) is created"

    Should Be Equal As Strings    ${output}[host]    host_1    host
    Should Be Equal As Strings    ${output}[serviceDescription]    service_1    serviceDescription
    Should Contain    ${output}[contactGroup]    contactgroup_1    contactGroup
    Should Be Equal As Strings    ${output}[escalationPeriod]    24x7    escalationPeriod
    Should Be Equal As Numbers    ${output}[firstNotification]    2    firstNotification
    Should Be Equal As Numbers    ${output}[lastNotification]    0    lastNotification
    Should Be Equal As Numbers    ${output}[notificationInterval]    1    notificationInterval


    # Notification #1 goes to the service's direct contact (John_Doe).
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    First notification to John_Doe was not sent

    # Notification #2+ comes from the escalation contactgroup_1 (U1, U2, U3).
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Escalation notification to U1 was not sent

    ${content}    Create List    SERVICE NOTIFICATION: U2;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Escalation notification to U2 was not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Escalation notification to U3 was not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker
