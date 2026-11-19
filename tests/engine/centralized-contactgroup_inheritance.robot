*** Settings ***
Documentation       Centreon Broker and Engine Verify contactgroup inheritance with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CEBSN5
    [Documentation]    Given a centralized Engine configuration where contactgroup_1 is empty and inherits from a full template
    ...    And the template defines alias, members, and contactgroup_members
    ...    When Engine and Broker are started
    ...    Then contactgroup_1 resolves with the template's alias, members, and sub-groups
    [Tags]    engine    contactgroup    MON-151622
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    contactgroup    alias    ["contactgroup_template_1_alias"]

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_template_1    active_checks_enabled    contactgroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_template_1    passive_checks_enabled    contactgroupTemplates.cfg

    # Add Command :
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroupTemplates.cfg

    # Create contact groups
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1"]
    Ctn Add Contact Group    ${0}    ${3}    ["U2"]
    Ctn Add Contact Group    ${0}    ${4}    ["U3","U4"]

    # Delete unnecessary fields in contactgroup:
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_1    alias    contactgroups.cfg
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_1    members    contactgroups.cfg

    # Set contactgroup_1 to use contactgroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    contactgroup_1    use    contactgroup_template_1    contactgroups.cfg

    # Operation in contactTemplates
    ${config_values}    Create Dictionary
    ...    contactgroup_members    contactgroup_3
    ...    members    U1

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    contactgroup_template_1    ${key}    ${value}    contactgroupTemplates.cfg
    END

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Contactgroup Info Grpc    contactgroup_1

    Should Be Equal As Strings     ${output}[name]    contactgroup_1
    Should Be Equal As Strings     ${output}[alias]    contactgroup_template_1_alias
    Should Not Contain    ${output}[members]    John_Doe
    Should Contain    ${output}[members]    U1
    Should Contain    ${output}[members]    U2

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEBSN6
    [Documentation]    Given a centralized Engine configuration where contactgroup_1 is full and inherits from a full template
    ...    And both the group and template define alias, members, and contactgroup_members
    ...    When Engine and Broker are started
    ...    Then contactgroup_1's own values take precedence over the template's values
    [Tags]    engine    contactgroup    MON-151622
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    contactgroup    alias    ["contactgroup_template_1_alias"]

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_template_1    active_checks_enabled    contactgroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_template_1    passive_checks_enabled    contactgroupTemplates.cfg

    # Add Command :
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Add the necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroupTemplates.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1"]
    Ctn Add Contact Group    ${0}    ${3}    ["U2"]
    Ctn Add Contact Group    ${0}    ${4}    ["U3","U4"]

    # Contactgroup_1 to use contactgroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    contactgroup_1    use    contactgroup_template_1    contactgroups.cfg

    # Operation in contactTemplates
    ${config_values}    Create Dictionary
    ...    contactgroup_members    contactgroup_2

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    contactgroup_1    ${key}    ${value}    contactgroups.cfg
    END

    ${config_values_tmp}    Create Dictionary
    ...    contactgroup_members    contactgroup_4
    ...    members    U2

    FOR    ${key}    ${value}    IN    &{config_values_tmp}
        Ctn Engine Config Set Key Value In Cfg    0    contactgroup_template_1    ${key}    ${value}    contactgroupTemplates.cfg
    END

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Contactgroup Info Grpc    contactgroup_1

    Should Be Equal As Strings     ${output}[name]    contactgroup_1
    Should Be Equal As Strings     ${output}[alias]    contactgroup_1
    Should Contain    ${output}[members]    John_Doe
    Should Contain    ${output}[members]    U1
    Should Not Contain Any   ${output}[members]    U2    U3    U4

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEBSN7
    [Documentation]    Given a centralized Engine started with contactgroup_1 having only one member
    ...    And after start, contactgroup_1 is modified to be empty and inherit from a full template
    ...    When the new configuration is sent to Engine via Broker notification
    ...    Then contactgroup_1 resolves with the template's alias, members, and sub-groups
    [Tags]    engine    contactgroup    MON-151622
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    contactgroup    alias    ["contactgroup_template_1_alias"]

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_template_1    active_checks_enabled    contactgroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_template_1    passive_checks_enabled    contactgroupTemplates.cfg

    # Add Command :
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroupTemplates.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${2}    ["U1"]
    Ctn Add Contact Group    ${0}    ${3}    ["U2"]
    Ctn Add Contact Group    ${0}    ${4}    ["U3","U4"]

    # Delete unnecessary fields in contactgroup:
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_1    alias    contactgroups.cfg
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_1    members    contactgroups.cfg

    # Contactgroup_1 to use contactgroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    contactgroup_1    use    contactgroup_template_1    contactgroups.cfg

    # Operation in contactTemplates
    ${config_values}    Create Dictionary
    ...    contactgroup_members    contactgroup_3
    ...    members    U1

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    contactgroup_template_1    ${key}    ${value}    contactgroupTemplates.cfg
    END

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Contactgroup Info Grpc    contactgroup_1

    Should Be Equal As Strings     ${output}[name]    contactgroup_1
    Should Be Equal As Strings     ${output}[alias]    contactgroup_template_1_alias
    Should Not Contain    ${output}[members]    John_Doe
    Should Contain    ${output}[members]    U1
    Should Contain    ${output}[members]    U2

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CEBSN8
    [Documentation]    Given a centralized Engine started with contactgroup_1 having one member
    ...    And after start, contactgroup_1 is modified to be full and inherit from a full template
    ...    When the new configuration is sent to Engine via Broker notification
    ...    Then contactgroup_1's own values take precedence and template values for overlapping fields are not used
    [Tags]    engine    contactgroup    MON-151622
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    contactgroup    alias    ["contactgroup_template_1_alias"]

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_template_1    active_checks_enabled    contactgroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_template_1    passive_checks_enabled    contactgroupTemplates.cfg

    # Add Command :
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroupTemplates.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${2}    ["U1"]
    Ctn Add Contact Group    ${0}    ${3}    ["U2"]
    Ctn Add Contact Group    ${0}    ${4}    ["U3","U4"]

    # Delete unnecessary fields in contactgroup:
    Ctn Engine Config Delete Key In Cfg    0    contactgroup_1    members    contactgroups.cfg

    # Contactgroup_1 to use contactgroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    contactgroup_1    use    contactgroup_template_1    contactgroups.cfg

    # Operation in contactgroup
    ${config_values}    Create Dictionary
    ...    contactgroup_members    contactgroup_3
    ...    members    U1

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    contactgroup_1    ${key}    ${value}    contactgroups.cfg
    END

    # Operation in contactgroupTemplates
    ${config_values_tmp}    Create Dictionary
    ...    contactgroup_members    contactgroup_4
    ...    members    John_Doe

    FOR    ${key}    ${value}    IN    &{config_values_tmp}
        Ctn Engine Config Set Key Value In Cfg    0    contactgroup_template_1    ${key}    ${value}    contactgroupTemplates.cfg
    END

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Contactgroup Info Grpc    contactgroup_1

    Log To Console    ${output}

    Should Be Equal As Strings     ${output}[name]    contactgroup_1
    Should Be Equal As Strings    ${output}[alias]    contactgroup_1
    Should Not Contain Any    ${output}[members]    John_Doe    U3    U4
    Should Contain    ${output}[members]    U1
    Should Contain    ${output}[members]    U2

    Ctn Stop Engine
    Ctn Kindly Stop Broker
