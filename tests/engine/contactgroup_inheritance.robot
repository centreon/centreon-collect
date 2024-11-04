*** Settings ***
Documentation       Centreon Broker and Engine Verify contactgroup inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EBSN5
    [Documentation]    Verify contactgroup inheritance : contactgroup(empty) inherit from template (full) , on Start Engine
    [Tags]    engine    contactgroup    MON-151622
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
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

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Contactgroup Info Grpc    contactgroup_1

    Should Be Equal As Strings     ${output}[name]    contactgroup_1
    Should Be Equal As Strings     ${output}[alias]    contactgroup_template_1_alias
    Should Not Contain    ${output}[members]    John_Doe
    Should Contain    ${output}[members]    U1
    Should Contain    ${output}[members]    U2

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBSN6
    [Documentation]    Verify contactgroup inheritance : contactgroup(full) inherit from template (full) , on Start Engine
    [Tags]    engine    contactgroup    MON-151622
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
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

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Contactgroup Info Grpc    contactgroup_1

    Should Be Equal As Strings     ${output}[name]    contactgroup_1
    Should Be Equal As Strings     ${output}[alias]    contactgroup_1
    Should Contain    ${output}[members]    John_Doe
    Should Contain    ${output}[members]    U1
    Should Not Contain Any   ${output}[members]    U2    U3    U4

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBSN7
    [Documentation]    Verify contactgroup inheritance : contactgroup(empty) inherit from template (full) , on Reload Engine
    [Tags]    engine    contactgroup    MON-151622
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
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

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
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

    # Reload engine
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

    ${output}    Ctn Get Contactgroup Info Grpc    contactgroup_1

    Should Be Equal As Strings     ${output}[name]    contactgroup_1
    Should Be Equal As Strings     ${output}[alias]    contactgroup_template_1_alias
    Should Not Contain    ${output}[members]    John_Doe
    Should Contain    ${output}[members]    U1
    Should Contain    ${output}[members]    U2

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBSN8
    [Documentation]    Verify contactgroup inheritance : contactgroup(full) inherit from template (full) , on Reload Engine
    [Tags]    engine    contactgroup    MON-151622
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
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

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
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

    # Reload engine
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

    ${output}    Ctn Get Contactgroup Info Grpc    contactgroup_1

    Log To Console    ${output}
    
    Should Be Equal As Strings     ${output}[name]    contactgroup_1
    Should Be Equal As Strings    ${output}[alias]    contactgroup_1
    Should Not Contain Any    ${output}[members]    John_Doe    U3    U4
    Should Contain    ${output}[members]    U1
    Should Contain    ${output}[members]    U2

    Ctn Stop Engine
    Ctn Kindly Stop Broker