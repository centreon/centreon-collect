*** Settings ***
Documentation       Centreon Engine verify hostgroups inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EHGI0
    [Documentation]    Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Start Engine
    [Tags]    engine    hostgroup    MON-151323
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    hostgroup    alias    ["hostgroup_template_1_alias"]
    
    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_template_1    active_checks_enabled    hostgroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_template_1    passive_checks_enabled    hostgroupTemplates.cfg
    
    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    hostgroupTemplates.cfg

    # Create host group
    Ctn Add Host Group    ${0}    ${1}    ["host_1"]

    # Delete unnecessary fields in hostgroup:
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_1    alias    hostgroups.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_1    members    hostgroups.cfg
    
    # Set hostgroup_1 to use hostgroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    hostgroup_1    use    hostgroup_template_1    hostgroups.cfg

    # Operation in hostgroupTemplates
    ${config_values}    Create Dictionary
    ...    members    host_2
    ...    notes    note_tmpl
    ...    notes_url    note_url_tmpl
    ...    action_url    action_url_tmpl

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    hostgroup_template_1    ${key}    ${value}    hostgroupTemplates.cfg
    END

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Hostgroup Info Grpc    hostgroup_1

    Should Be Equal As Strings     ${output}[name]    hostgroup_1
    Should Be Equal As Strings     ${output}[alias]    hostgroup_template_1_alias
    Should Not Contain    ${output}[members]    host_1
    Should Contain    ${output}[members]    host_2
    Should Be Equal As Strings    ${output}[notes]    note_tmpl
    Should Be Equal As Strings    ${output}[notesUrl]    note_url_tmpl
    Should Be Equal As Strings    ${output}[actionUrl]    action_url_tmpl

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EHGI1
    [Documentation]    Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Start Engine
    [Tags]    engine    hostgroup    MON-151323
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    hostgroup    alias    ["hostgroup_template_1_alias"]
    
    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_template_1    active_checks_enabled    hostgroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_template_1    passive_checks_enabled    hostgroupTemplates.cfg
    
    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    hostgroupTemplates.cfg

    # Create host groups
    Ctn Add Host Group    ${0}    ${1}    ["host_1"]
    
    # Set hostgroup_1 to use hostgroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    hostgroup_1    use    hostgroup_template_1    hostgroups.cfg

    # Operation in hostgroup
    ${config_values}    Create Dictionary
    ...    notes    note
    ...    notes_url    note_url
    ...    action_url    action_url

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    hostgroup_1    ${key}    ${value}    hostgroups.cfg
    END

    # Operation in hostgroupTemplates
    ${config_values}    Create Dictionary
    ...    members    host_2
    ...    notes    note_tmpl
    ...    notes_url    note_url_tmpl
    ...    action_url    action_url_tmpl

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    hostgroup_template_1    ${key}    ${value}    hostgroupTemplates.cfg
    END

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Hostgroup Info Grpc    hostgroup_1

    Should Be Equal As Strings     ${output}[name]    hostgroup_1
    Should Be Equal As Strings     ${output}[alias]    hostgroup_1
    Should Contain    ${output}[members]    host_1
    Should Not Contain    ${output}[members]    host_2
    Should Be Equal As Strings    ${output}[notes]    note
    Should Be Equal As Strings    ${output}[notesUrl]    note_url
    Should Be Equal As Strings    ${output}[actionUrl]    action_url

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EHGI2
    [Documentation]    Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Reload Engine
    [Tags]    engine    hostgroup    MON-151323
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    hostgroup    alias    ["hostgroup_template_1_alias"]
    
    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_template_1    active_checks_enabled    hostgroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_template_1    passive_checks_enabled    hostgroupTemplates.cfg
    
    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    hostgroupTemplates.cfg

    # Create host groups
    Ctn Add Host Group    ${0}    ${1}    ["host_1"]

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Delete unnecessary fields in hostgroup:
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_1    alias    hostgroups.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_1    members    hostgroups.cfg
    
    # Set hostgroup_1 to use hostgroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    hostgroup_1    use    hostgroup_template_1    hostgroups.cfg

    # Operation in hostgroupTemplates
    ${config_values}    Create Dictionary
    ...    members    host_2
    ...    notes    note_tmpl
    ...    notes_url    note_url_tmpl
    ...    action_url    action_url_tmpl

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    hostgroup_template_1    ${key}    ${value}    hostgroupTemplates.cfg
    END

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

    ${output}    Ctn Get Hostgroup Info Grpc    hostgroup_1

    Should Be Equal As Strings     ${output}[name]    hostgroup_1
    Should Be Equal As Strings     ${output}[alias]    hostgroup_template_1_alias
    Should Not Contain    ${output}[members]    host_1
    Should Contain    ${output}[members]    host_2
    Should Be Equal As Strings    ${output}[notes]    note_tmpl
    Should Be Equal As Strings    ${output}[notesUrl]    note_url_tmpl
    Should Be Equal As Strings    ${output}[actionUrl]    action_url_tmpl

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EHGI3
    [Documentation]    Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Reload Engine
    [Tags]    engine    hostgroup    MON-151323
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    hostgroup    alias    ["hostgroup_template_1_alias"]
    
    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_template_1    active_checks_enabled    hostgroupTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    hostgroup_template_1    passive_checks_enabled    hostgroupTemplates.cfg
    
    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    hostgroupTemplates.cfg

    # Create host groups
    Ctn Add Host Group    ${0}    ${1}    ["host_1"]

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    
    # Set hostgroup_1 to use hostgroup_template_1
    Ctn Engine Config Set Key Value In Cfg    0    hostgroup_1    use    hostgroup_template_1    hostgroups.cfg

    # Operation in hostgroup
    ${config_values}    Create Dictionary
    ...    members    host_3
    ...    notes    note
    ...    notes_url    note_url
    ...    action_url    action_url

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    hostgroup_1    ${key}    ${value}    hostgroups.cfg
    END

    # Operation in hostgroupTemplates
    ${config_values}    Create Dictionary
    ...    members    host_2
    ...    notes    note_tmpl
    ...    notes_url    note_url_tmpl
    ...    action_url    action_url_tmpl

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Key Value In Cfg    0    hostgroup_template_1    ${key}    ${value}    hostgroupTemplates.cfg
    END

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

    ${output}    Ctn Get Hostgroup Info Grpc    hostgroup_1

    Should Be Equal As Strings     ${output}[name]    hostgroup_1
    Should Be Equal As Strings     ${output}[alias]    hostgroup_1
    Should Contain    ${output}[members]    host_1
    Should Not Contain    ${output}[members]    host_2
    Should Be Equal As Strings    ${output}[notes]    note
    Should Be Equal As Strings    ${output}[notesUrl]    note_url
    Should Be Equal As Strings    ${output}[actionUrl]    action_url

    Ctn Stop Engine
    Ctn Kindly Stop Broker