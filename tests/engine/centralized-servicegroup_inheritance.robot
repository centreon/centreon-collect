*** Settings ***
Documentation       Centreon Engine verify servicegroup inheritance with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CESGI0
    [Documentation]    Given a servicegroup with no fields inheriting from a full template
    ...    When Engine starts with centralized configuration
    ...    Then the servicegroup alias, notes, notes_url, action_url and members are inherited from the template
    [Tags]    engine    servicegroup    MON-151232
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    servicegroup    alias    ["servicegroup_template_1_alias"]

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_template_1    active_checks_enabled    servicegroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_template_1    passive_checks_enabled    servicegroupTemplates.cfg

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroupTemplates.cfg

    # Create service groups
    Ctn Add Service Group    ${0}    ${1}    ["host_1,service_1"]
    Ctn Add Service Group    ${0}    ${2}    ["host_2,service_6"]
    Ctn Add Service Group    ${0}    ${3}    ["host_3,service_11"]
    Ctn Add Service Group    ${0}    ${4}    ["host_4,service_16"]

    # Delete unnecessary fields in service groups:
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_1    alias    servicegroups.cfg
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_1    members    servicegroups.cfg

    # Set servicegroup_1 to use servicegroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    servicegroup_1    use    servicegroup_template_1    servicegroups.cfg

    # Operation in servicegroups
    ${config_values}    Create Dictionary
    ...    members    host_2,service_6
    ...    servicegroup_members    servicegroup_3
    ...    notes    template_notes
    ...    notes_url    template_notes_url
    ...    action_url    template_action_url

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    servicegroup_template_1    ${key}    ${value}    servicegroupTemplates.cfg
    END

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Servicegroup Info Grpc    servicegroup_1

    Should Be Equal As Strings     ${output}[name]    servicegroup_1
    Should Be Equal As Strings     ${output}[alias]    servicegroup_template_1_alias
    Should Be Equal As Strings     ${output}[notes]    template_notes
    Should Be Equal As Strings     ${output}[notesUrl]    template_notes_url
    Should Be Equal As Strings     ${output}[actionUrl]    template_action_url
    Should Not Contain    ${output}[members]    host_1,service_1
    Should Contain    ${output}[members]    host_2,service_6
    Should Contain    ${output}[members]    host_3,service_11

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CESGI1
    [Documentation]    Given a full servicegroup inheriting from a full template
    ...    When Engine starts with centralized configuration
    ...    Then the servicegroup's own fields take precedence over the template's fields
    [Tags]    engine    servicegroup    MON-151232
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    servicegroup    alias    ["servicegroup_template_1_alias"]

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_template_1    active_checks_enabled    servicegroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_template_1    passive_checks_enabled    servicegroupTemplates.cfg

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroupTemplates.cfg

    # Create service groups
    Ctn Add Service Group    ${0}    ${1}    ["host_1,service_1"]
    Ctn Add Service Group    ${0}    ${2}    ["host_2,service_6"]
    Ctn Add Service Group    ${0}    ${3}    ["host_3,service_11"]
    Ctn Add Service Group    ${0}    ${4}    ["host_4,service_16"]

    # Set servicegroup_1 to use servicegroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    servicegroup_1    use    servicegroup_template_1    servicegroups.cfg

    # Operation in servicegroups
    ${config_values}    Create Dictionary
    ...    servicegroup_members    servicegroup_4
    ...    notes    notes
    ...    notes_url    notes_url
    ...    action_url    action_url

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    servicegroup_1    ${key}    ${value}    servicegroups.cfg
    END

    # Operation in servicegroupTemplates
    ${config_values}    Create Dictionary
    ...    members    host_2,service_6
    ...    servicegroup_members    servicegroup_3
    ...    notes    template_notes
    ...    notes_url    template_notes_url
    ...    action_url    template_action_url

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    servicegroup_template_1    ${key}    ${value}    servicegroupTemplates.cfg
    END

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Servicegroup Info Grpc    servicegroup_1

    Should Be Equal As Strings     ${output}[name]    servicegroup_1
    Should Be Equal As Strings     ${output}[alias]    servicegroup_1
    Should Be Equal As Strings     ${output}[notes]    notes
    Should Be Equal As Strings     ${output}[notesUrl]    notes_url
    Should Be Equal As Strings     ${output}[actionUrl]    action_url
    Should Contain    ${output}[members]    host_1,service_1
    Should Contain    ${output}[members]    host_4,service_16
    Should Not Contain    ${output}[members]    host_2,service_6
    Should Not Contain    ${output}[members]    host_3,service_11

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CESGI2
    [Documentation]    Given a servicegroup with no fields inheriting from a full template
    ...    When Broker notifies Engine of the new centralized configuration
    ...    Then the servicegroup alias, notes, notes_url, action_url and members are inherited from the template
    [Tags]    engine    servicegroup    MON-151232
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    servicegroup    alias    ["servicegroup_template_1_alias"]

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_template_1    active_checks_enabled    servicegroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_template_1    passive_checks_enabled    servicegroupTemplates.cfg

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroupTemplates.cfg

    # Create service groups
    Ctn Add Service Group    ${0}    ${1}    ["host_1,service_1"]

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Add Service Group    ${0}    ${2}    ["host_2,service_6"]
    Ctn Add Service Group    ${0}    ${3}    ["host_3,service_11"]
    Ctn Add Service Group    ${0}    ${4}    ["host_4,service_16"]

    # Delete unnecessary fields in service groups:
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_1    alias    servicegroups.cfg
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_1    members    servicegroups.cfg

    # Set servicegroup_1 to use servicegroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    servicegroup_1    use    servicegroup_template_1    servicegroups.cfg

    # Operation in servicegroups
    ${config_values}    Create Dictionary
    ...    members    host_2,service_6
    ...    servicegroup_members    servicegroup_3
    ...    notes    template_notes
    ...    notes_url    template_notes_url
    ...    action_url    template_action_url

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    servicegroup_template_1    ${key}    ${value}    servicegroupTemplates.cfg
    END

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Servicegroup Info Grpc    servicegroup_1

    Should Be Equal As Strings     ${output}[name]    servicegroup_1
    Should Be Equal As Strings     ${output}[alias]    servicegroup_template_1_alias
    Should Be Equal As Strings     ${output}[notes]    template_notes
    Should Be Equal As Strings     ${output}[notesUrl]    template_notes_url
    Should Be Equal As Strings     ${output}[actionUrl]    template_action_url
    Should Not Contain    ${output}[members]    host_1,service_1
    Should Contain    ${output}[members]    host_2,service_6
    Should Contain    ${output}[members]    host_3,service_11

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CESGI3
    [Documentation]    Given a full servicegroup inheriting from a full template
    ...    When Broker notifies Engine of the new centralized configuration
    ...    Then the servicegroup's own fields take precedence over the template's fields
    [Tags]    engine    servicegroup    MON-151232
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    servicegroup    alias    ["servicegroup_template_1_alias"]

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_template_1    active_checks_enabled    servicegroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_template_1    passive_checks_enabled    servicegroupTemplates.cfg

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroupTemplates.cfg

    # Create service groups
    Ctn Add Service Group    ${0}    ${1}    ["host_1,service_1"]

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Add Service Group    ${0}    ${2}    ["host_2,service_6"]
    Ctn Add Service Group    ${0}    ${3}    ["host_3,service_11"]
    Ctn Add Service Group    ${0}    ${4}    ["host_4,service_16"]

    # Set servicegroup_1 to use servicegroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    servicegroup_1    use    servicegroup_template_1    servicegroups.cfg
    Ctn Engine Config Delete Key In Cfg    0    servicegroup_1    members    servicegroups.cfg

    # Operation in servicegroups
    ${config_values}    Create Dictionary
    ...    servicegroup_members    servicegroup_4
    ...    members    host_3,service_11
    ...    notes    notes
    ...    notes_url    notes_url
    ...    action_url    action_url

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    servicegroup_1    ${key}    ${value}    servicegroups.cfg
    END

    # Operation in servicegroupTemplates
    ${config_values}    Create Dictionary
    ...    members    host_1,service_1
    ...    servicegroup_members    servicegroup_2
    ...    notes    template_notes
    ...    notes_url    template_notes_url
    ...    action_url    template_action_url

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    servicegroup_template_1    ${key}    ${value}    servicegroupTemplates.cfg
    END

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Servicegroup Info Grpc    servicegroup_1

    Should Be Equal As Strings     ${output}[name]    servicegroup_1
    Should Be Equal As Strings     ${output}[alias]    servicegroup_1
    Should Be Equal As Strings     ${output}[notes]    notes
    Should Be Equal As Strings     ${output}[notesUrl]    notes_url
    Should Be Equal As Strings     ${output}[actionUrl]    action_url
    Should Not Contain    ${output}[members]    host_1,service_1
    Should Contain    ${output}[members]    host_4,service_16
    Should Not Contain    ${output}[members]    host_2,service_6
    Should Contain    ${output}[members]    host_3,service_11

    Ctn Stop Engine
    Ctn Kindly Stop Broker
